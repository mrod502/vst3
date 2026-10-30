#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsp
{
/** Reads the level of each channel of a block: a windowed RMS, a windowed peak and an
    attack/release envelope, all live at once. M3 §4 item 1's detector.

    Measurement only (D-014). No target, no floor, no gate, no gain, no idea which bus it is pointed
    at — every one of those is the normalizer's decision and all of them are still open
    (`plan/M3-impl-plan/INDEX.md` §9). Both candidate measurement schemes stay available because
    choosing between them is open too: the windowed pair measures something as long as a grain or as
    short as a hop, the envelope is the smoothed alternative.

    All three readings are in the same amplitude domain as the input — 1.0 is whatever 1.0 is at the
    input — and all three are defined to be *correct*, not merely bounded:

    * `windowedRms(c)` is the square root of the mean square over the last `windowSamples()` samples.
      Samples before the object was prepared count as silence, so it settles over one window, and a
      stationary sine of amplitude A settles to A/sqrt(2) — not to 0.6366·A, which is what
      mean-rectifying reads.
    * `windowedPeak(c)` is the largest magnitude in the same window. Same warm-up, and the same answer
      as a zero-padded window because magnitudes are non-negative.
    * `envelope(c)` is an attack/release follower of a smoothed RMS. It is two stages, because one
      cannot do both jobs (D-014): a follower whose input is the raw square is biased by the ripple in
      that input, so it reads a stationary sine high, while a follower slow enough to average the
      ripple cannot follow a step at the rate the attack constant names. Stage one is a linear
      first-order mean-square smoother with the time constant `innerTimeConstantMs()`, derived rather
      than configured; stage two follows its square root upward at `attackMs()` and downward at
      `releaseMs()`. With the two set equal stage two is a plain one-pole and the reading is unbiased
      at any level; unequal constants leave a bias of the order of the ripple the inner stage leaves
      through, which is what `innerTimeConstantMs()` is there to be read against.
      On a stationary sine far enough above that inner corner the reading converges to A/sqrt(2),
      and after a step it crosses 63.2 % of the settled value at t0 + `attackMs()` and falls to
      36.8 % at t0 + `releaseMs()`.

    Every time constant is milliseconds realised in `prepare`; nothing in the loop knows a rate. The
    filtering runs per sample, so a stream gives a bit-identical reading series in chunks of 1 or of
    4096, and consuming samples and reading level are separate operations — `processChannel` advances
    the state, the getters are O(1) and valid between any two samples.

    Silence is exact rather than asymptotic, which is what §7's "no denormal CPU spikes" asks of this
    class: the RMS sum is forced to 0.0 the moment the window holds nothing but zeros (tracked by
    count, not by threshold), and both envelope stages flush at their own floor, so no state of this
    object ever sits at a subnormal value and an idle channel reads exactly `0.0f`.

    Threading: `prepare` allocates and belongs off the audio thread. `processChannel` touches only the
    state of the channel it names and allocates nothing, so channels cannot cross and one stream
    renders identically whatever chunk it arrives in. Reads belong on the thread that drives the
    channel; nothing here is safe to read concurrently from another thread. */
class LevelDetector final
{
public:
    /** The published envelope reading is either exactly `0.0f` or above this. At or below it the
        follower is flushed to zero — 240 dB below full scale, far under anything a consumer could act
        on, and the reason a long release tail cannot leave a subnormal state behind. */
    static constexpr double envelopeFloor = 1.0e-12;

    /** The inner mean-square smoother runs this many times faster than whichever envelope time
        constant is shorter, so its own lag is a fraction of a percent of that constant and it is the
        stage that decides how far below `envelope()` a stationary tone can be measured. */
    static constexpr double innerTimeConstantDivisor = 100.0;

    /** @param integrationMsIn window for both windowed readings, ms, > 0.
        @param attackMsIn    envelope attack time constant, ms, ≥ 0 (0 is instantaneous).
        @param releaseMsIn   envelope release time constant, ms, ≥ 0. */
    LevelDetector(double integrationMsIn, double attackMsIn, double releaseMsIn) noexcept;

    /** Sizes the per-channel state for the largest configuration the caller will drive: one window of
        samples (4 bytes) and one monotone queue of at most one entry per window slot (8 bytes) per
        channel, so the footprint is 12 bytes × `windowSamples()` × channels. */
    void prepare(double sampleRate, int channels);

    /** Back to exactly the state a fresh `prepare` leaves: same coefficients, empty windows. */
    void reset() noexcept;

    /** Consume count samples of one channel. Reads them; never writes them. Allocates nothing. */
    void processChannel(std::size_t channel, const float* samples, std::size_t count) noexcept;

    float windowedRms(std::size_t channel) const noexcept;

    /** `0.0f` until the first sample arrives. */
    float windowedPeak(std::size_t channel) const noexcept;

    float envelope(std::size_t channel) const noexcept;

    /** Samples in the integration window, from `integrationMs()` and the prepared rate. The windowed
        readings are exact — not merely close — `windowSamples()` samples after any input stops. */
    std::size_t windowSamples() const noexcept { return window; }

    /** Samples of continuous silence after which `envelope()` is exactly `0.0f`, given the envelope is
        at `fromReading` (full scale by default) when the silence starts: the release time constant
        times ln(from / floor), plus the inner stage's own area delay. */
    std::size_t silenceSamples(double fromReading = 1.0) const noexcept;

    double integrationMs() const noexcept { return integrationMsValue; }
    double attackMs() const noexcept { return attackMsValue; }
    double releaseMs() const noexcept { return releaseMsValue; }

    /** `min(attackMs(), releaseMs()) / innerTimeConstantDivisor`: the inner mean-square smoother's
        time constant, ms. Reported because it bounds what `envelope()` can be asked to resolve. */
    double innerTimeConstantMs() const noexcept { return innerMsValue; }

private:
    struct Channel
    {
        std::size_t slot = 0;
        std::uint32_t position = 0;
        std::size_t head = 0;
        std::size_t count = 0;
        std::size_t nonZero = 0;
        double sumOfSquares = 0.0;
        double smoothPower = 0.0;
        double level = 0.0;
    };

    struct Peak
    {
        float magnitude = 0.0f;
        std::uint32_t position = 0;
    };

    double integrationMsValue;
    double attackMsValue;
    double releaseMsValue;
    double innerMsValue = 0.0;
    double sampleRateValue = 0.0;
    double innerCoefficient = 0.0;
    double attackCoefficient = 0.0;
    double releaseCoefficient = 0.0;
    double flushPower = envelopeFloor * envelopeFloor;
    std::size_t window = 0;
    std::vector<Channel> channelState;
    std::vector<float> history;
    std::vector<Peak> peaks;
};

inline void LevelDetector::processChannel(std::size_t channel, const float* samples,
                                          std::size_t count) noexcept
{
    assert(channel < channelState.size() && window > 0);
    Channel& state = channelState[channel];
    float* const ring = history.data() + channel * window;
    Peak* const queue = peaks.data() + channel * window;

    for (std::size_t i = 0; i < count; ++i)
    {
        const float sample = samples[i];
        const double squared = static_cast<double>(sample) * static_cast<double>(sample);

        const float leaving = ring[state.slot];
        ring[state.slot] = sample;
        if (++state.slot == window)
            state.slot = 0;

        /* The ring is zero-initialised, so a slot that has never been written subtracts exactly
           nothing and the warm-up reads as silence before the start rather than as a partial mean. */
        const double sum = state.sumOfSquares - static_cast<double>(leaving)
                                                * static_cast<double>(leaving)
                           + squared;
        if (leaving != 0.0f)
            --state.nonZero;
        if (sample != 0.0f)
            ++state.nonZero;
        /* An all-zero window holds a sum of exactly zero: a running total is only as good as the
           subtractions that built it, and asserting the exact answer here is what lets an idle
           channel read 0.0f instead of a cancellation residue. The clamp is the same argument for a
           window that still holds a few tiny samples, where a negative residue would reach a sqrt. */
        state.sumOfSquares = state.nonZero == 0 ? 0.0 : (sum < 0.0 ? 0.0 : sum);

        const std::uint32_t position = state.position;
        ++state.position;

        while (state.count > 0
               && static_cast<std::uint32_t>(position - queue[state.head].position) >= window)
        {
            if (++state.head == window)
                state.head = 0;
            --state.count;
        }

        const float magnitude
            = sample == 0.0f ? 0.0f : (sample < 0.0f ? -sample : sample);
        while (state.count > 0)
        {
            std::size_t last = state.head + state.count - 1;
            if (last >= window)
                last -= window;
            if (queue[last].magnitude > magnitude)
                break;
            --state.count;
        }

        std::size_t tail = state.head + state.count;
        if (tail >= window)
            tail -= window;
        queue[tail] = Peak{magnitude, position};
        ++state.count;

        double& power = state.smoothPower;
        power += innerCoefficient * (squared - power);
        if (power <= flushPower)
            power = 0.0;

        const double target = std::sqrt(power);
        double& level = state.level;
        if (target > level)
            level += attackCoefficient * (target - level);
        else
            level -= releaseCoefficient * (level - target);
        if (level <= envelopeFloor)
            level = 0.0;
    }
}

inline float LevelDetector::windowedRms(std::size_t channel) const noexcept
{
    assert(channel < channelState.size() && window > 0);
    const double meanSquare = channelState[channel].sumOfSquares / static_cast<double>(window);
    return static_cast<float>(std::sqrt(meanSquare));
}

inline float LevelDetector::windowedPeak(std::size_t channel) const noexcept
{
    assert(channel < channelState.size() && window > 0);
    const Channel& state = channelState[channel];
    return state.count == 0 ? 0.0f : peaks[channel * window + state.head].magnitude;
}

inline float LevelDetector::envelope(std::size_t channel) const noexcept
{
    assert(channel < channelState.size() && window > 0);
    return static_cast<float>(channelState[channel].level);
}
}
