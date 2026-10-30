#include "DcBlocker.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>

namespace dsp
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

/** sin(w0) / (2Q) at Q = 1/sqrt(2) — the maximally flat, still-monotone Q — is sin(w0) * sqrt(2)/2. */
constexpr double kAlphaPerSin = 0.70710678118654752440;

/** ln(1e-6), the level A1 holds the output to, as an exponent for the envelope bound. */
constexpr double kLogMicro = -13.815510557964274;

/** Headroom on that bound: a step also drives the direct path, and a Butterworth pair overshoots by
    4.3%, so the crossing lands later than the pure exponential envelope crosses it. */
constexpr double kSettlingMargin = 1.25;
}

DcBlocker::DcBlocker(double cornerIn) noexcept : cornerHzValue(cornerIn)
{
}

void DcBlocker::prepare(double sampleRate, int channels)
{
    assert(sampleRate > 2.0 * cornerHzValue && channels > 0);

    const double w0 = 2.0 * kPi * cornerHzValue / sampleRate;
    const double cosine = std::cos(w0);
    const double alpha = std::sin(w0) * kAlphaPerSin;
    const double inverseA0 = 1.0 / (1.0 + alpha);

    /* b0 = (1 + cos w0) / (2 (1 + alpha)) and b1, b2 derived from it: b0 + b1 + b2 is then zero
       with no rounding left over, because doubling is exact and adding a value to its own negation
       is exact. */
    b0 = (1.0 + cosine) * 0.5 * inverseA0;
    b1 = -2.0 * b0;
    b2 = b0;
    a1 = -2.0 * cosine * inverseA0;
    a2 = (1.0 - alpha) * inverseA0;

    /* A complex pole pair's radius is sqrt(a2), and the transient envelope falls by that factor per
       sample, so the bound is where ln(1e-6) of it has gone. */
    const double poleRadius = std::sqrt(a2);
    settling = poleRadius > 0.0 && poleRadius < 1.0
                   ? static_cast<int>(kLogMicro / std::log(poleRadius) * kSettlingMargin + 0.5)
                   : 0;

    state.assign(static_cast<std::size_t>(channels), State{});
}

void DcBlocker::reset() noexcept
{
    for (auto& channelState : state)
        channelState = State{};
}
}
