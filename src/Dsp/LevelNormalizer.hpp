#pragma once

#include "LevelDetector.hpp"

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dsp
{
/** Holds the convolved stream at `√(envA · envB)`: the level stage D-015 defines and M3 item 04 ships.

    Per hop, per channel, three `LevelDetector`s read A, B and the wet hop, and the wet hop is scaled in
    place by

        gain = ( √(envA · envB) / envConv ) ^ amount,   clamped to ±`gainRangeDb`

    with conv's reading taken **upstream** of the gain it sets — the conv detector is fed before `wet` is
    written below, which is the only place that ordering is enforced. The identical formula fed from the
    scaled stream is an AGC loop wrapped around a follower, with windup and a pump period set by time
    constants nobody chose for that purpose (D-015).

    Half-slope is the behaviour, not a defect to correct: in the dB domain the target is the arithmetic
    mean of the two readings, so a change that hits one bus alone moves the held level by half of it.
    `amount` is an exponent rather than a blend between unity and the correction, so the dial meets the
    gate at unity instead of fighting it, and `amount` = 0 is bit-exact identity.

    Two independent rules make the stage total, and both are load-bearing:

    * **The gate.** Any of the three readings at or below the floor makes that hop's gain unity. Nothing
      divides by a reading the floor has caught, so a silent A or B leaves the wet path at its own level
      instead of running the correction to its range limit — §7's silent-bus criterion by construction
      rather than by tuning. The consequence to keep in view: a loud A over a floored B produces unity,
      not the `√(envA · floor)` boost a clamp-only design would apply.
    * **The finiteness rule.** A non-finite reading makes that hop's gain unity *and* counts the hop in
      `nonFiniteHops()`. The check sits on the three readings rather than on the computed gain: with
      readings finite and above the floor the computed gain is finite by construction, because the clamp
      bounds it, while exact silence — `log10(0)` = −inf in the dB domain — has to reach the gate without
      being counted as a fault. A poisoned detector keeps reading non-finite, so the degradation is
      permanent for that stream: unity and uncorrected, which beats silence or NaN, and is cheaper than a
      per-sample `isfinite` on three streams.

    The gain for hop *k* is computed from hop *k*'s own samples and nothing looks ahead, so the stage
    defers no sample and adds nothing to `latencySamples()` or `wetDelaySamples()`.

    `amount` and the floor are read once per hop and not ramped, because the applied gain is already
    continuous: every hop is scaled along a straight line from the previous hop's gain to this one, in the
    sample domain. That line is capped at `slewDbPerHop()` — the whole `2 · gainRangeDb` travel crossed in
    `fullTravelMs` — stated in milliseconds so the cap is the same speed at every hop the UI can express;
    a per-hop constant would be 8× slower in time at H = 512 than at H = 64, which is INDEX §8.5's unit
    error in different clothing.

    Threading: `prepare` allocates and belongs off the audio thread. `processChannel` touches only the
    state of the channel it names and allocates nothing, so channels cannot cross and one stream renders
    identically whatever chunk it arrives in. The counter is a relaxed atomic read as a diagnostic that
    nothing synchronises on, as with `SoftClipper`. */
class LevelNormalizer final
{
public:
    /** Attack and release of all three detectors, ms. 20 ms matches the blend and gain ramps D-011
        shipped; 100 ms release is M3 §6's anti-pumping lean. Revisit trigger: at the default L = 8192 the
        wet response itself spans 171 ms at 48 kHz, so a release shorter than L chases the smear. */
    static constexpr double attackMs = 20.0;
    static constexpr double releaseMs = 100.0;

    /** Both bounds of the applied gain, dB. D-008's measured wet response across the shipped L range
        spans 37.45 dB (0.798 at L = 1024 against 0.0107 at L = 65536), so ±36 dB covers it to within
        1.45 dB and leaves the §7 ±2 dB gate reachable with about 0.5 dB to spare. */
    static constexpr double gainRangeDb = 36.0;

    /** Time in which the slew cap lets the gain cross its whole 72 dB range — 360 dB/s, which at the
        default H = 128 / 48 kHz works out at 0.96 dB per hop. A zipper guard, not the musical filter;
        that is the envelope's job. */
    static constexpr double fullTravelMs = 200.0;

    /** Fully normalising: it is the feature. Off-by-default is this one line, and it changes what a
        fresh instance sounds like. */
    static constexpr float defaultAmount = 1.0f;

    /** Where the correction gives up and passes the wet path through unchanged. Sits 180 dB above
        `LevelDetector::envelopeFloor` = 1e-12, so the detector's own flush rule never interacts with it,
        and at the level item 03 measured as exact silence. The figure most likely to want moving —
        confirm by ear. */
    static constexpr float defaultFloorDb = -60.0f;

    LevelNormalizer() noexcept = default;

    /** Sizes three detectors and one gain state per channel for the largest configuration the caller will
        drive: 12 bytes × `hopSamples` × channels per detector — 36 bytes × H × channels in total, so
        9.2 kB at H = 128 stereo — plus 16 bytes per channel of gain state. The detector window is one
        hop; `envelope()` ignores it, so it buys a per-hop RMS reading for diagnostics and nothing else.

        Must be called before `processChannel`, and with the same `hopSamples` the caller will pass as
        `count`: the slew cap is per hop, so the class has to know its hop. */
    void prepare(double sampleRate, int channels, int hopSamples);

    /** Back to exactly the state a fresh `prepare` leaves: detectors empty, every channel at unity. */
    void reset() noexcept;

    /** Correction strength in [0, 1]: 0 is bit-exact identity, 1 is the full correction, and the form is
        `gain = ratio ^ amount`. Read at the next hop boundary; no ramp of its own, because the applied
        gain is ramped already. */
    void setAmount(float amountToUse) noexcept { amountValue = amountToUse; }

    /** Silence floor, dBFS. At or below it the gain is unity. Read at the next hop boundary. */
    void setFloorDb(float floorDbToUse) noexcept { floorDbValue = floorDbToUse; }

    float amount() const noexcept { return amountValue; }
    float floorDb() const noexcept { return floorDbValue; }

    /** Consumes one hop of A and B, one hop of the un-scaled convolved stream, and scales that convolved
        stream in place by the hop's gain ramp. Reads `a` and `b`; never writes them. Allocates nothing,
        takes no lock, throws nothing.

        `count` must be the `hopSamples` this object was prepared with. */
    void processChannel(std::size_t channel, const float* a, const float* b, float* wet,
                        std::size_t count) noexcept;

    /** The three readings the last hop was decided from, for diagnostics and for the test that pins where
        conv is measured (criterion A5). `envelopeConv()` is the level of the stream *before* this stage
        scaled it, whatever that stream now holds. */
    float envelopeA(std::size_t channel) const noexcept { return buses().a.envelope(channel); }
    float envelopeB(std::size_t channel) const noexcept { return buses().b.envelope(channel); }
    float envelopeConv(std::size_t channel) const noexcept { return buses().conv.envelope(channel); }

    /** The detector window the three buses share — one hop, from the prepare-time rate. */
    std::size_t windowSamples() const noexcept { return buses().conv.windowSamples(); }

    /** The gain the last hop settled on, dB: the ramp's endpoint, and 0.0 exactly whenever the gate holds.
        Diagnostic only — nothing in the signal path reads it back. */
    float gainDb(std::size_t channel) const noexcept
    {
        assert(channel < channelState.size());
        return static_cast<float>(channelState[channel].gainDb);
    }

    /** Hops whose decision was degraded to unity because a reading was not finite, since the last
        `resetNonFiniteCount()`: one per hop per affected channel, and permanent once a detector is
        poisoned. Nothing surfaces it in the UI; this is the only read-out. */
    std::uint64_t nonFiniteHops() const noexcept { return degraded.load(std::memory_order_relaxed); }

    void resetNonFiniteCount() noexcept { degraded.store(0, std::memory_order_relaxed); }

    std::size_t hopSamples() const noexcept { return hop; }

    /** The slew cap in dB per hop: `2 · gainRangeDb · 1000 / fullTravelMs` dB/s realised at the
        prepare-time rate — 0.96 dB at H = 128 / 48 kHz. */
    double slewDbPerHop() const noexcept { return slewPerHop; }

private:
    struct Buses
    {
        explicit Buses(double windowMs) noexcept
            : a(windowMs, attackMs, releaseMs),
              b(windowMs, attackMs, releaseMs),
              conv(windowMs, attackMs, releaseMs)
        {
        }

        LevelDetector a;
        LevelDetector b;
        LevelDetector conv;
    };

    /** The gain state is the ramp's start value and the slew anchor in the domain the cap is stated in.
        Both land exactly on their target at the last sample of every hop, so the hop-to-hop change the
        cap bounds is the change between two consecutive decided gains. */
    struct Channel
    {
        double gainDb = 0.0;
        float gain = 1.0f;
    };

    const Buses& buses() const noexcept
    {
        assert(detection.has_value());
        return *detection;
    }

    double slewPerHop = 0.0;
    std::size_t hop = 0;
    float hopInverse = 0.0f;
    float amountValue = defaultAmount;
    float floorDbValue = defaultFloorDb;
    std::optional<Buses> detection;
    std::vector<Channel> channelState;
    std::atomic<std::uint64_t> degraded{0};
};

inline void LevelNormalizer::processChannel(std::size_t channel, const float* a, const float* b,
                                            float* wet, std::size_t count) noexcept
{
    assert(detection.has_value() && count == hop && channel < channelState.size());
    Buses& buses = *detection;
    Channel& state = channelState[channel];

    /* Decision C made executable: the conv detector sees the engine's own output. Feeding it after the
       loop below is the one-line wiring slip that turns a feed-forward stage into an AGC loop while
       every other criterion still passes. */
    buses.conv.processChannel(channel, wet, count);
    buses.a.processChannel(channel, a, count);
    buses.b.processChannel(channel, b, count);

    const double envA = buses.a.envelope(channel);
    const double envB = buses.b.envelope(channel);
    const double envConv = buses.conv.envelope(channel);
    const double floorReading = std::pow(10.0, static_cast<double>(floorDbValue) * 0.05);

    double wantedDb = 0.0;
    if (!std::isfinite(envA) || !std::isfinite(envB) || !std::isfinite(envConv))
    {
        degraded.fetch_add(1, std::memory_order_relaxed);
    }
    else if (envA > floorReading && envB > floorReading && envConv > floorReading)
    {
        /* The dB domain, because that is where the geometric mean is a sum and where both limits are
           stated. 10·log10 of each reading of the pair is the mean of their dB values; conv's is the
           level the gain has to move, and the whole thing is the exponent `amount` scales. */
        wantedDb = static_cast<double>(amountValue)
                   * (10.0 * std::log10(envA) + 10.0 * std::log10(envB) - 20.0 * std::log10(envConv));
        if (wantedDb < -gainRangeDb)
            wantedDb = -gainRangeDb;
        else if (wantedDb > gainRangeDb)
            wantedDb = gainRangeDb;
    }

    const double lowest = state.gainDb - slewPerHop;
    const double highest = state.gainDb + slewPerHop;
    double decided = wantedDb;
    if (decided < lowest)
        decided = lowest;
    else if (decided > highest)
        decided = highest;

    /* 0 dB folds to exactly 1.0f, which is what makes the gated hop's identity bit-exact rather than
       merely equal, and keeps a stream that has settled back to unity sitting on the same bits the raw
       wet path carries. */
    const float from = state.gain;
    state.gainDb = decided;
    const float to = static_cast<float>(std::pow(10.0, decided * 0.05));
    state.gain = to;

    /* The straight line, with the last sample assigned rather than evaluated: `from + (to - from) * 1.0f`
       does not land on `to` in float (the same measurement MixEngine's Ramp records), and an endpoint
       criterion cannot be met on a rounding of it. */
    for (std::size_t i = 0; i < count; ++i)
    {
        const float gain = i + 1 == count
                               ? to
                               : from + (to - from)
                                             * (static_cast<float>(i + 1) * hopInverse);
        wet[i] *= gain;
    }
}
}
