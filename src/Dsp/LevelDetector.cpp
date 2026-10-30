#include "LevelDetector.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace dsp
{
namespace
{
/** A one-pole with a time constant of timeConstantMs at this rate: the state reaches 1 - 1/e of a
    step in that many milliseconds. A constant of 0 means instantaneous, which is what a 0 ms attack
    asks for, and the poles are the plain amplitude-domain ones on purpose — with attack and release
    set equal the two coefficients are then equal, and the follower is a linear filter rather than a
    detector with a crest-dependent bias. */
double poleFor(double timeConstantMs, double sampleRate) noexcept
{
    const double samples = timeConstantMs * sampleRate / 1000.0;
    return samples > 0.0 ? 1.0 - std::exp(-1.0 / samples) : 1.0;
}
}

LevelDetector::LevelDetector(double integrationMsIn, double attackMsIn, double releaseMsIn) noexcept
    : integrationMsValue(integrationMsIn), attackMsValue(attackMsIn), releaseMsValue(releaseMsIn)
{
}

void LevelDetector::prepare(double sampleRate, int channels)
{
    assert(sampleRate > 0.0 && channels >= 0);
    assert(integrationMsValue > 0.0 && attackMsValue >= 0.0 && releaseMsValue >= 0.0);

    sampleRateValue = sampleRate;
    window = static_cast<std::size_t>(integrationMsValue * sampleRate / 1000.0 + 0.5);
    if (window < 1)
        window = 1;

    /* The inner mean-square stage is not a fourth control: it exists to take the ripple out of the
       follower's input, and the only scale it has to be right against is the faster of the two time
       constants the caller did choose. Divisor and lag are what D-014's measurement of criteria 1
       and 3 sitting in opposite directions is settled on. */
    innerMsValue = std::min(attackMsValue, releaseMsValue) / innerTimeConstantDivisor;
    innerCoefficient = poleFor(innerMsValue, sampleRate);
    attackCoefficient = poleFor(attackMsValue, sampleRate);
    releaseCoefficient = poleFor(releaseMsValue, sampleRate);

    const std::size_t count = static_cast<std::size_t>(channels);
    channelState.assign(count, Channel{});
    history.assign(window * count, 0.0f);
    peaks.assign(window * count, Peak{});
}

void LevelDetector::reset() noexcept
{
    for (auto& state : channelState)
        state = Channel{};
    for (auto& sample : history)
        sample = 0.0f;
    for (auto& entry : peaks)
        entry = Peak{};
}

std::size_t LevelDetector::silenceSamples(double fromReading) const noexcept
{
    assert(sampleRateValue > 0.0);
    if (!(fromReading > envelopeFloor))
        return 0;

    /* The published reading decays exponentially at 1/releaseMs, so it reaches the floor after
       tau · ln(from / floor) samples — plus the inner stage's own area delay, which is 2 · tau_inner
       because its target falls as exp(-u/2) and ∫(1 - exp(-u/2)) du = 2. The last sample of margin is
       for the comparison itself: the bound is checked against a state the recursion reached in
       floating point, and a reading on the boundary to within a rounding step would otherwise decide
       the flush on evaluation order. */
    const double releaseTimeSamples = releaseMsValue * sampleRateValue / 1000.0;
    const double innerTimeSamples = innerMsValue * sampleRateValue / 1000.0;
    const double needed
        = releaseTimeSamples * std::log(fromReading / envelopeFloor) + 2.0 * innerTimeSamples;
    return static_cast<std::size_t>(std::ceil(needed)) + 1;
}
}
