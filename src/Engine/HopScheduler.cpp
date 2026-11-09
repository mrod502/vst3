#include "HopScheduler.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace engine
{
namespace
{
/** Whether one B index of a resolved map can be turned into a pointer by a scheduler built with
    @p sideWidth B rings. Consolidation needs at least one source to average. */
constexpr bool validSidechainSource(std::uint16_t source, int sideWidth) noexcept
{
    return source < sideWidth || source == kSilentSource
           || (source == kConsolidatedSource && sideWidth > 0);
}
}

HopScheduler::HopScheduler(Engine& engineToUse) noexcept : engine(&engineToUse) {}

void HopScheduler::prepare(const SchedulerConfig& configToUse)
{
    const int hop = configToUse.engine.hop;
    const int sources = configToUse.channels;
    const int sideSources = std::max(configToUse.sidechainChannels, 0);
    const int processed = configToUse.map.channels;

    /* An index naming a ring that is about not to exist means the caller resolved the map against
       widths other than the ones it passed. Rejected here, on the message thread, rather than on the
       first hop of a live callback, which is where the same mismatch wrote past hopBufferB in a layout
       the shipped gate accepts (F-014). Thrown rather than asserted because prepare is the one place a
       failure can be reported — the same choice GrainConvolutionEngine::prepare makes — and an abort
       is not observable to a leg running in the same process. Validated before anything is assigned,
       so a rejected prepare leaves the previous configuration standing rather than a half-written one
       the callback would then read. */
    if (processed <= 0 || processed > kMaxChannels)
        throw std::invalid_argument("hop scheduler needs a resolved channel map");
    for (int slot = 0; slot < processed; ++slot)
        if (configToUse.map.a[slot] >= sources
            || !validSidechainSource(configToUse.map.b[slot], sideSources))
            throw std::invalid_argument("channel map names a source outside its side");

    assert(hop > 0 && sources > 0 && configToUse.maxHostBlock > 0);
    assert(configToUse.engine.maxChannels >= processed);

    config = configToUse;

    // Capacity bounds. An input ring holds at most hop - 1 samples left over after the last
    // hop plus one whole callback. The output ring holds the pre-fill plus one callback's
    // production before that callback drains it, so its peak occupancy is
    // latencySamples + maxHostBlock. CircularBuffer rounds both up to a power of two.
    const std::size_t inputCapacity = static_cast<std::size_t>(config.maxHostBlock + hop);
    const std::size_t outputCapacity =
        static_cast<std::size_t>(config.engine.latencySamples + config.maxHostBlock);

    silence.assign(static_cast<std::size_t>(std::max(config.maxHostBlock, hop)), 0.0f);

    /* Sizing is per source on the way in and per processed channel on the way out: the fold is the
       case where the two differ, and sizing B scratch by the processed count is what overflows. */
    inputA.clear();
    inputB.clear();
    output.clear();
    inputA.reserve(static_cast<std::size_t>(sources));
    inputB.reserve(static_cast<std::size_t>(sideSources));
    output.reserve(static_cast<std::size_t>(processed));
    for (int channel = 0; channel < sources; ++channel)
        inputA.emplace_back(inputCapacity);
    for (int channel = 0; channel < sideSources; ++channel)
        inputB.emplace_back(inputCapacity);
    for (int channel = 0; channel < processed; ++channel)
        output.emplace_back(outputCapacity);

    const std::size_t hopSpan = static_cast<std::size_t>(hop);
    hopBufferA.assign(hopSpan * static_cast<std::size_t>(sources), 0.0f);
    hopBufferB.assign(hopSpan * static_cast<std::size_t>(sideSources), 0.0f);
    hopBufferOut.assign(hopSpan * static_cast<std::size_t>(processed), 0.0f);
    foldBuffer.assign(hopSpan, 0.0f);
    foldScale = sideSources > 0 ? 1.0f / static_cast<float>(sideSources) : 0.0f;

    hopA.resize(static_cast<std::size_t>(processed));
    hopB.resize(static_cast<std::size_t>(processed));
    hopOut.resize(static_cast<std::size_t>(processed));
    consolidates = false;
    for (int slot = 0; slot < processed; ++slot)
    {
        const std::size_t offset = static_cast<std::size_t>(slot) * hopSpan;
        hopA[slot] = hopBufferA.data() + static_cast<std::size_t>(config.map.a[slot]) * hopSpan;
        hopOut[slot] = hopBufferOut.data() + offset;

        const std::uint16_t source = config.map.b[slot];
        if (source == kConsolidatedSource)
        {
            hopB[slot] = foldBuffer.data();
            consolidates = true;
        }
        else if (source == kSilentSource)
            hopB[slot] = silence.data();
        else
            hopB[slot] = hopBufferB.data() + static_cast<std::size_t>(source) * hopSpan;
    }

    engine->prepare(config.engine);
    prefillOutput();
    prepared = true;
}

void HopScheduler::reset() noexcept
{
    for (auto& ring : inputA)
        ring.clear();
    for (auto& ring : inputB)
        ring.clear();
    for (auto& ring : output)
        ring.clear();

    hops = 0;
    engine->reset();
    prefillOutput();
}

void HopScheduler::process(const float* const* aIn, const float* const* bIn,
                           float* const* out, int numChannels, int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    assert(prepared);
    assert(numChannels == config.map.channels);
    assert(numSamples <= config.maxHostBlock);

    const std::size_t hop = static_cast<std::size_t>(config.engine.hop);
    const std::size_t arrived = static_cast<std::size_t>(numSamples);

    /* Ingest bounds by the source widths — that is how many pointers the caller holds — while the
       hop and the drain below bound by the processed count the Engine is driven with. */
    for (int channel = 0; channel < config.channels; ++channel)
        inputA[channel].write(aIn[channel], arrived);
    for (int channel = 0; channel < config.sidechainChannels; ++channel)
        inputB[channel].write(bIn != nullptr ? bIn[channel] : silence.data(), arrived);

    while (holdsFullHop())
    {
        for (int channel = 0; channel < config.channels; ++channel)
            inputA[channel].read(hopBufferA.data()
                                     + static_cast<std::size_t>(channel) * hop,
                                 hop);
        for (int channel = 0; channel < config.sidechainChannels; ++channel)
            inputB[channel].read(hopBufferB.data()
                                     + static_cast<std::size_t>(channel) * hop,
                                 hop);

        if (consolidates)
            foldSidechain();

        engine->processHop(hopA.data(), hopB.data(), hopOut.data(), numChannels);

        for (int channel = 0; channel < numChannels; ++channel)
            output[channel].write(hopBufferOut.data()
                                      + static_cast<std::size_t>(channel) * hop,
                                  hop);
        ++hops;
    }

    for (int channel = 0; channel < numChannels; ++channel)
    {
        // Short delivery means the latency model or a capacity bound is wrong, not something
        // to paper over with fresh zeros (T2).
        const std::size_t delivered = output[channel].read(out[channel], arrived);
        assert(delivered == arrived);
    }
}

void HopScheduler::foldSidechain() noexcept
{
    const std::size_t hop = foldBuffer.size();
    const std::size_t sources = static_cast<std::size_t>(config.sidechainChannels);
    const float* const side = hopBufferB.data();

    /* Ascending source order with the reciprocal applied once, after the sum, so the mean is one
       multiply per sample whatever chB is — including 3, where 1/3 is not representable and a
       per-term scale would not be bit-identical to the reference a leg mirrors this loop against. */
    for (std::size_t i = 0; i < hop; ++i)
    {
        float sum = 0.0f;
        for (std::size_t channel = 0; channel < sources; ++channel)
            sum += side[channel * hop + i];
        foldBuffer[i] = sum * foldScale;
    }
}

bool HopScheduler::holdsFullHop() const noexcept
{
    const std::size_t hop = static_cast<std::size_t>(config.engine.hop);
    for (const auto& ring : inputA)
        if (ring.availableRead() < hop)
            return false;
    for (const auto& ring : inputB)
        if (ring.availableRead() < hop)
            return false;
    return true;
}

void HopScheduler::prefillOutput() noexcept
{
    std::size_t remaining = static_cast<std::size_t>(config.engine.latencySamples);
    while (remaining > 0)
    {
        const std::size_t chunk = std::min(remaining, silence.size());
        for (auto& ring : output)
            ring.write(silence.data(), chunk);
        remaining -= chunk;
    }
}
}
