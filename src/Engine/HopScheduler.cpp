#include "HopScheduler.hpp"

#include <algorithm>
#include <cassert>

namespace engine
{
HopScheduler::HopScheduler(Engine& engineToUse) noexcept : engine(&engineToUse) {}

void HopScheduler::prepare(const SchedulerConfig& configToUse)
{
    config = configToUse;
    const int hop = config.engine.hop;
    const int channels = config.channels;
    assert(hop > 0 && channels > 0 && config.maxHostBlock > 0);

    // Capacity bounds. An input ring holds at most hop - 1 samples left over after the last
    // hop plus one whole callback. The output ring holds the pre-fill plus one callback's
    // production before that callback drains it, so its peak occupancy is
    // latencySamples + maxHostBlock. CircularBuffer rounds both up to a power of two.
    const std::size_t inputCapacity = static_cast<std::size_t>(config.maxHostBlock + hop);
    const std::size_t outputCapacity =
        static_cast<std::size_t>(config.engine.latencySamples + config.maxHostBlock);

    silence.assign(static_cast<std::size_t>(std::max(config.maxHostBlock, hop)), 0.0f);

    inputA.clear();
    inputB.clear();
    output.clear();
    inputA.reserve(static_cast<std::size_t>(channels));
    inputB.reserve(static_cast<std::size_t>(std::max(config.sidechainChannels, 0)));
    output.reserve(static_cast<std::size_t>(channels));
    for (int channel = 0; channel < channels; ++channel)
    {
        inputA.emplace_back(inputCapacity);
        output.emplace_back(outputCapacity);
    }
    for (int channel = 0; channel < config.sidechainChannels; ++channel)
        inputB.emplace_back(inputCapacity);

    const std::size_t span = static_cast<std::size_t>(hop) * static_cast<std::size_t>(channels);
    hopBufferA.assign(span, 0.0f);
    hopBufferB.assign(span, 0.0f);
    hopBufferOut.assign(span, 0.0f);

    hopA.resize(static_cast<std::size_t>(channels));
    hopB.resize(static_cast<std::size_t>(channels));
    hopOut.resize(static_cast<std::size_t>(channels));
    for (int channel = 0; channel < channels; ++channel)
    {
        const std::size_t offset = static_cast<std::size_t>(channel) * static_cast<std::size_t>(hop);
        hopA[channel] = hopBufferA.data() + offset;
        hopOut[channel] = hopBufferOut.data() + offset;
        hopB[channel] = channel < config.sidechainChannels
                            ? hopBufferB.data() + offset
                            : silence.data();
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
    assert(numChannels == config.channels);
    assert(numSamples <= config.maxHostBlock);

    const std::size_t hop = static_cast<std::size_t>(config.engine.hop);
    const std::size_t arrived = static_cast<std::size_t>(numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
        inputA[channel].write(aIn[channel], arrived);
    for (int channel = 0; channel < config.sidechainChannels; ++channel)
        inputB[channel].write(bIn != nullptr ? bIn[channel] : silence.data(), arrived);

    while (holdsFullHop())
    {
        for (int channel = 0; channel < numChannels; ++channel)
            inputA[channel].read(hopBufferA.data()
                                     + static_cast<std::size_t>(channel) * hop,
                                 hop);
        for (int channel = 0; channel < config.sidechainChannels; ++channel)
            inputB[channel].read(hopBufferB.data()
                                     + static_cast<std::size_t>(channel) * hop,
                                 hop);

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
