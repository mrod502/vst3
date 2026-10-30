#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

namespace dsp
{
/** Total DC removal for a stream that has to keep its band: M3 §4.3's blocker, on the convolved path.

    Second-order Butterworth high-pass, transposed direct form II, at the corner passed to the
    constructor (cornerHz is the shipped value).

    Second order because a first-order stage cannot be flat where flatness is required: at eight
    times its corner a first-order high-pass sits 0.067 dB below unity, against item 02 criterion A2's
    0.05 dB, where the Butterworth response is 0.0011 dB there — 10·log10(4096/4097), measured to
    -0.00106 dB — and still monotone, so nothing peaks above the corner either (A3). Q = 1/sqrt(2) is
    the highest Q with that monotonicity.

    b1 is written as -2*b0 and b2 as b0, not as three separate evaluations, so b0 + b1 + b2 is exactly
    zero in double: DC rejection is then a property of the coefficients and the recursion only has to
    hold what they already promise, rather than converge towards a small residual.

    The state is double, not float. The pole radius is sqrt(a2), about 0.99985 at the shipped corner,
    and rounding a float state that slow leaves a floor of the order of 1e-7 of the input level — a
    residue that scales with the signal, where A1 asks for 1e-6 of the DC offset it is holding back.

    Threading: prepare allocates and belongs off the audio thread; processChannel touches only the
    state of the channel it names and allocates nothing. State advances one sample at a time, so the
    same stream renders bit-identically whatever chunk it arrives in (A6) and one channel's content
    cannot reach another's (A7). */
class DcBlocker final
{
public:
    /** The shipped -3 dB corner, Hz. D-013 records the anchor measurement it was picked against. */
    static constexpr double cornerHz = 5.0;

    /** @param cornerIn the -3 dB corner; below half the lowest sample rate the caller prepares at. */
    explicit DcBlocker(double cornerIn = cornerHz) noexcept;

    /** Sizes the per-channel state for the largest configuration the caller will drive. */
    void prepare(double sampleRate, int channels);

    void reset() noexcept;

    /** count samples of one channel, in place. */
    void processChannel(std::size_t channel, float* samples, std::size_t count) noexcept;

    double corner() const noexcept { return cornerHzValue; }

    /** Samples of constant input after which the output is at or below 1e-6 of that input: the
        documented bound criteria A1 and A5 settle against, derived from the realised poles. */
    int settlingSamples() const noexcept { return settling; }

private:
    double cornerHzValue = cornerHz;
    double b0 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    int settling = 0;

    struct State
    {
        double previous1 = 0.0;
        double previous2 = 0.0;
    };
    std::vector<State> state;
};

inline void DcBlocker::processChannel(std::size_t channel, float* samples,
                                      std::size_t count) noexcept
{
    assert(channel < state.size());
    State& channelState = state[channel];

    for (std::size_t i = 0; i < count; ++i)
    {
        const double x = static_cast<double>(samples[i]);
        const double y = b0 * x + channelState.previous1;
        channelState.previous1 = b1 * x - a1 * y + channelState.previous2;
        channelState.previous2 = b2 * x - a2 * y;
        samples[i] = static_cast<float>(y);
    }
}
}
