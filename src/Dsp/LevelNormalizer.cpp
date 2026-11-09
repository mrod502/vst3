#include "LevelNormalizer.hpp"

#include <cassert>
#include <cstddef>

namespace dsp
{
void LevelNormalizer::prepare(double sampleRate, int channels, int hopSamples)
{
    assert(sampleRate > 0.0 && channels > 0 && hopSamples > 0);

    hop = static_cast<std::size_t>(hopSamples);
    hopInverse = 1.0f / static_cast<float>(hopSamples);

    /* 2 · gainRangeDb crossed in fullTravelMs is 360 dB/s, so the cap per hop is 0.96 dB at
       H = 128 / 48 kHz and 3.84 dB at H = 512 — the same 200 ms of full travel either way, which is the
       whole reason the constant is a time rather than a per-hop figure. */
    slewPerHop = 2.0 * gainRangeDb * 1000.0 * static_cast<double>(hopSamples)
                 / (fullTravelMs * sampleRate);

    /* One hop of window, from the rate being prepared at. envelope() ignores it — this buys the per-hop
       RMS and peak readings a caller may want for a meter, and nothing more, so a wider window would be
       footprint spent on a reading no consumer has. */
    detection.emplace(1000.0 * static_cast<double>(hopSamples) / sampleRate);
    detection->a.prepare(sampleRate, channels);
    detection->b.prepare(sampleRate, channels);
    detection->conv.prepare(sampleRate, channels);

    channelState.assign(static_cast<std::size_t>(channels), Channel{});
}

void LevelNormalizer::reset() noexcept
{
    if (detection.has_value())
    {
        detection->a.reset();
        detection->b.reset();
        detection->conv.reset();
    }
    for (Channel& state : channelState)
        state = Channel{};
}
}
