#include "MixEngine.hpp"

#include <DcBlocker.hpp>
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

void MixEngine::fillCoefficients() noexcept
{
    const std::size_t hop = static_cast<std::size_t>(config.hop);

    for (std::size_t i = 0; i < hop; ++i)
    {
        const float rawBlend = blendRamp.step();

        /* The dry bus follows the sign of p, so a travel from -1 to +1 has to hand over from A to B
           somewhere. Landing exactly on p = 0 makes that hand-over silent: w_dry(0) == 0, so no
           sample of the outgoing bus is dropped and none of the incoming one is picked up early.
           The sign is tracked on the un-snapped value, or the sample after the landing would look
           like another crossing and pin the dial at zero. */
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
}
}
