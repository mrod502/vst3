#include "MixEngine.hpp"

#include <DcBlocker.hpp>
#include <LevelNormalizer.hpp>
#include <SoftClipper.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace engine
{
namespace
{
int rampLengthSamples(double milliseconds, double sampleRate) noexcept
{
    return std::max(1, static_cast<int>(milliseconds * sampleRate * 0.001 + 0.5));
}
}

MixEngine::MixEngine(std::unique_ptr<Engine> innerToUse) noexcept : inner(std::move(innerToUse))
{
}

MixEngine::MixEngine(std::unique_ptr<Engine> innerToUse,
                     std::unique_ptr<dsp::DcBlocker> wetBlockerToUse,
                     std::unique_ptr<dsp::SoftClipper> outputClipperToUse) noexcept
    : inner(std::move(innerToUse)),
      wetBlocker(std::move(wetBlockerToUse)),
      outputClipper(std::move(outputClipperToUse))
{
}

MixEngine::~MixEngine() = default;

void MixEngine::prepare(const EngineConfig& configToUse)
{
    config = configToUse;
    inner->prepare(configToUse);
    if (wetBlocker != nullptr)
        wetBlocker->prepare(config.sampleRate, config.maxChannels);
    if (levelNormalizer != nullptr)
        levelNormalizer->prepare(config.sampleRate, config.maxChannels, config.hop);

    blendRamp.length = rampLengthSamples(blendRampMs, config.sampleRate);
    gainRamp.length = rampLengthSamples(outputGainRampMs, config.sampleRate);
    bypassRamp.length = rampLengthSamples(bypassRampMs, config.sampleRate);
    snapRamps();

    scratch.assign(static_cast<std::size_t>(config.maxChannels)
                       * static_cast<std::size_t>(config.hop),
                   0.0f);
    scratchPtrs.assign(static_cast<std::size_t>(config.maxChannels), nullptr);
    for (int channel = 0; channel < config.maxChannels; ++channel)
        scratchPtrs[static_cast<std::size_t>(channel)] =
            scratch.data()
            + static_cast<std::size_t>(channel) * static_cast<std::size_t>(config.hop);

    const std::size_t hop = static_cast<std::size_t>(config.hop);
    coefA.assign(hop, 0.0f);
    coefB.assign(hop, 0.0f);
    coefWet.assign(hop, 0.0f);

    /* The publication's position restarts with the transport: the ring above the seam is rebuilt at the
       same moment, so a continuing counter would be a gap nobody can tell from a wrap. */
    traceHop = 0;
}

void MixEngine::installTraceSink(TraceSink sink) noexcept
{
    trace = sink;
}

void MixEngine::snapRamps() noexcept
{
    blendRamp.snap();
    gainRamp.snap();
    bypassRamp.snap();
    previousBlend = blendRamp.value;
}

void MixEngine::reset()
{
    inner->reset();
    if (wetBlocker != nullptr)
        wetBlocker->reset();
    if (levelNormalizer != nullptr)
        levelNormalizer->reset();
    snapRamps();
}

int MixEngine::latencySamples() const
{
    return inner->latencySamples();
}

int MixEngine::wetDelaySamples() const
{
    return inner->wetDelaySamples();
}

void MixEngine::setBlendTarget(float dialPosition) noexcept
{
    blendRamp.retarget(dialPosition);
}

void MixEngine::setOutputGainTarget(float decibels) noexcept
{
    gainRamp.retarget(std::pow(10.0, static_cast<double>(decibels) / 20.0));
}

void MixEngine::setBypassTarget(bool bypassed) noexcept
{
    bypassRamp.retarget(bypassed ? 0.0f : 1.0f);
}

void MixEngine::installLevelNormalizer(std::unique_ptr<dsp::LevelNormalizer> stage) noexcept
{
    /* Asserting on the prepare that already happened rather than on the stage pointer: config holds the
       rate and hop the stage would have to have been sized for, so a re-install that would silently keep
       the old hop is caught by the same test. */
    assert(config.sampleRate <= 0.0 && "installLevelNormalizer must run before prepare");
    levelNormalizer = std::move(stage);
}

void MixEngine::setNormalisationAmountTarget(float amount) noexcept
{
    if (levelNormalizer != nullptr)
        levelNormalizer->setAmount(amount);
}

void MixEngine::setSilenceFloorTarget(float floorDb) noexcept
{
    if (levelNormalizer != nullptr)
        levelNormalizer->setFloorDb(floorDb);
}

void MixEngine::fillCoefficients() noexcept
{
    const std::size_t hop = static_cast<std::size_t>(config.hop);

    for (std::size_t i = 0; i < hop; ++i)
    {
        const float rawBlend = blendRamp.step();

        /* The dry bus follows the sign of p, so a travel from -1 to +1 hands over somewhere; landing exactly on
           p = 0 makes it silent, since w_dry(0) == 0 drops no outgoing sample and picks up no incoming one
           early. The sign is tracked on the un-snapped value, or the next sample looks like another crossing. */
        const bool crossed = (previousBlend < 0.0f) != (rawBlend < 0.0f);
        previousBlend = rawBlend;
        const float p = crossed ? 0.0f : rawBlend;

        const MixWeights weights = mixLawWeights(p);
        const float gain = gainRamp.step();
        const float beta = bypassRamp.step();
        const float dryToA = p < 0.0f ? 1.0f : 0.0f;

        coefA[i] = gain * (beta * weights.dry * dryToA + (1.0f - beta));
        coefB[i] = gain * (beta * weights.dry * (1.0f - dryToA));
        coefWet[i] = gain * (beta * weights.wet);
    }
}

void MixEngine::processHop(const float* const* a, const float* const* b, float* const* out,
                           int numChannels)
{
    assert(numChannels > 0 && numChannels <= config.maxChannels);

    /* The sum needs the wet hop, A and B readable at the same time, so the inner engine writes
       scratch rather than out. Engine's contract that out is not aliased with a or b is what lets
       the read-then-write below need no third buffer. */
    inner->processHop(a, b, scratchPtrs.data(), numChannels);

    /* Decision C's first stage: the convolved stream only. A and B reach the sum unfiltered, which
       is what keeps D-010's bit-exact dial endpoints exact. */
    const std::size_t hop = static_cast<std::size_t>(config.hop);
    if (wetBlocker != nullptr)
    {
        for (int channel = 0; channel < numChannels; ++channel)
            wetBlocker->processChannel(static_cast<std::size_t>(channel),
                                       scratchPtrs[static_cast<std::size_t>(channel)], hop);
    }

    /* Then the level stage, on the blocked stream and before the law: after the blocker, so DC cannot inflate
       the reading it corrects for, and before the sum, so `coefWet`'s exact zero at |p| = 1 still hands through
       the dry bus. It reads A and B inside the hop seam, where Engine's contract guarantees `out` is unaliased. */
    if (levelNormalizer != nullptr)
    {
        for (int channel = 0; channel < numChannels; ++channel)
            levelNormalizer->processChannel(static_cast<std::size_t>(channel), a[channel], b[channel],
                                            scratchPtrs[static_cast<std::size_t>(channel)], hop);
    }

    fillCoefficients();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* const wet = scratchPtrs[static_cast<std::size_t>(channel)];
        for (std::size_t i = 0; i < hop; ++i)
            out[channel][i] = coefA[i] * a[channel][i] + coefB[i] * b[channel][i]
                              + coefWet[i] * wet[i];
    }

    /* And the second one, after the gain folded into the coefficients: this is the last thing the
       output sees before the host, so nothing downstream of it can exceed the ceiling. */
    if (outputClipper != nullptr)
    {
        for (int channel = 0; channel < numChannels; ++channel)
            outputClipper->processChannel(out[channel], hop);
    }

    /* The publication goes last so the readings are the ones this hop was decided from and its position
       matches the hop the host just got. A chain with no level stage has no readings and publishes
       nothing: an absent trace is honest, a zeroed one is a flat line that looks like a signal. */
    if (levelNormalizer != nullptr && !trace.empty())
        trace.publish(makeTraceSnapshot(numChannels));
}

TraceSnapshot MixEngine::makeTraceSnapshot(int numChannels) noexcept
{
    const std::size_t count = static_cast<std::size_t>(
        numChannels < kMaxChannels ? numChannels : kMaxChannels);

    TraceSnapshot snapshot;
    snapshot.hopIndex = traceHop;
    snapshot.channels = static_cast<std::uint32_t>(count);

    for (std::size_t channel = 0; channel < count; ++channel)
    {
        snapshot.reading[channel] = TraceReading {
            levelNormalizer->envelopeA(channel),
            levelNormalizer->envelopeB(channel),
            levelNormalizer->envelopeConv(channel),
            levelNormalizer->gainDb(channel),
        };
    }

    ++traceHop;
    return snapshot;
}
}
