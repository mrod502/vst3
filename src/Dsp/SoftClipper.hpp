#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dsp
{
/** Output safety stage: exactly the input up to 0 dBFS, then a monotone tanh ramp to a hard ceiling.

    Memoryless by contract (M3 item 2 decision A): nothing but the current sample feeds the output,
    so there is nothing to latch, ring or drift subnormal, silence is 0.0f on the sample it arrives,
    and output q depends on input q alone — which is what keeps the stage out of the latency model
    and lets M3 §4 item 7 resolve to a proof that nothing moved. The one member is a diagnostic count
    of the non-finite samples that were replaced; nothing reads it back into the signal.

    knee is exactly unity so the stage is invisible to any signal a host could already handle, and so
    the dial endpoints of D-010 stay bit-exact through it at full scale (item 02 criteria B1 and C5).
    The ceiling therefore has to sit above full scale: identity up to |x| = 1 and a continuous first
    derivative at the knee leave no monotone shape bounded by 1, and a hard clamp at unity fails that
    derivative. D-012 records both figures, what they buy, and what they cost.

    Threading: one instance is driven by the thread that calls processChannel. The counter is atomic
    so a read-out on another thread needs no lock; it is relaxed because nothing synchronises on it. */
class SoftClipper final
{
public:
    /** |x| at and below which the output is the input, bit for bit. 0 dBFS. */
    static constexpr float knee = 1.0f;

    /** The bound |out| cannot be exceeded, +6 dB. */
    static constexpr float ceiling = 2.0f;

    /** The transfer function on finite input, with no counter: identity to the knee, then
        knee + (ceiling - knee) * tanh((|x| - knee) / (ceiling - knee)) taken with the sign of x.
        Odd, monotone non-decreasing, and C2 across the knee — tanh's second derivative is zero at
        the origin, so the ramp joins the identity line without a curvature step. */
    static float shape(float in) noexcept;

    /** shape(), plus the non-finite rule: NaN and ±inf become 0.0f and are counted. The very next
        finite sample behaves normally, because there is no state for it to be affected by. */
    float process(float in) noexcept;

    /** process() over count samples in place. Allocates nothing. */
    void processChannel(float* samples, std::size_t count) noexcept;

    /** Samples replaced because they were not finite, since the last resetNonFiniteCount(). */
    std::uint64_t nonFiniteSamples() const noexcept
    {
        return replaced.load(std::memory_order_relaxed);
    }

    void resetNonFiniteCount() noexcept { replaced.store(0, std::memory_order_relaxed); }

private:
    std::atomic<std::uint64_t> replaced{0};
};

inline float SoftClipper::shape(float in) noexcept
{
    const float magnitude = std::abs(in);
    if (magnitude <= knee)
        return in;

    const float eased =
        knee + (ceiling - knee) * std::tanh((magnitude - knee) / (ceiling - knee));

    /* The clamp keeps the documented ceiling a property of this class rather than of tanh's
       asymptote: a different shape function could approach from above and never trip a test that
       only ever feeds it the curve it already has. */
    const float bounded = eased < ceiling ? eased : ceiling;
    return in < 0.0f ? -bounded : bounded;
}

inline float SoftClipper::process(float in) noexcept
{
    if (std::isfinite(in))
        return shape(in);

    replaced.fetch_add(1, std::memory_order_relaxed);
    return 0.0f;
}

inline void SoftClipper::processChannel(float* samples, std::size_t count) noexcept
{
    for (std::size_t i = 0; i < count; ++i)
        samples[i] = process(samples[i]);
}
}
