#include <LevelDetector.hpp>
#include <LevelNormalizer.hpp>

#include "AllocationWatch.hpp"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using Samples = std::vector<float>;

constexpr double kPi = 3.14159265358979323846;
constexpr double kRate = 48000.0;
constexpr std::size_t kHop = 128;

/** The window the stage sizes each of its three detectors to — one hop, in the millisecond unit the
    detector's constructor takes (item 04 decision E). */
constexpr double kWindowMs = 1000.0 * static_cast<double>(kHop) / kRate;

/** 1 kHz at 48 kHz is exactly 48 samples per period, so an RMS taken over a whole number of periods is
    the analytic A/sqrt(2) rather than an approximation of it. Every held level below is measured that way,
    which is what makes A1's 0.1 dB a statement about the stage and not about the measuring window. */
constexpr double kToneHz = 1000.0;

/** 1/sqrt(2): the RMS of a stationary sine of amplitude A, which is what the analytic predictions below
    are built from. */
constexpr double kInvRootTwo = 0.70710678118654752440;

/** The tail every held level is measured over: 100 periods of that tone. */
constexpr std::size_t kTailSamples = 4800;

constexpr std::size_t forHops(std::size_t hops)
{
    return hops * kHop;
}

Samples tone(std::size_t count, double amplitude)
{
    Samples out(count);
    const double step = 2.0 * kPi * kToneHz / kRate;
    for (std::size_t i = 0; i < count; ++i)
        out[i] = static_cast<float>(amplitude * std::sin(step * static_cast<double>(i)));
    return out;
}

/** Four equal stretches at four levels of the same tone, so the reading steps and the gain has to move. A
    stationary pair would never make the slew cap bite. */
Samples toneProgramme(std::size_t count)
{
    const double levels[] = {0.5, 0.02, 0.9, 0.05};
    Samples out(count);
    const double step = 2.0 * kPi * kToneHz / kRate;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double amplitude = levels[(i / (count / 4)) % 4];
        out[i] = static_cast<float>(amplitude * std::sin(step * static_cast<double>(i)));
    }
    return out;
}

Samples constant(std::size_t count, float value)
{
    return Samples(count, value);
}

Samples slice(const Samples& stream, std::size_t at, std::size_t count)
{
    return Samples(stream.begin() + static_cast<std::ptrdiff_t>(at),
                   stream.begin() + static_cast<std::ptrdiff_t>(at + count));
}

double rmsTail(const Samples& stream, std::size_t samples)
{
    REQUIRE(samples <= stream.size());
    double sum = 0.0;
    for (std::size_t i = stream.size() - samples; i < stream.size(); ++i)
        sum += static_cast<double>(stream[i]) * static_cast<double>(stream[i]);
    return std::sqrt(sum / static_cast<double>(samples));
}

double decibels(double ratio)
{
    return 20.0 * std::log10(ratio);
}

bool sameBits(const Samples& lhs, const Samples& rhs)
{
    return lhs.size() == rhs.size()
           && std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(float)) == 0;
}

/** One channel driven hop by hop: the scaled wet stream, and the gain the stage decided on at each hop. */
struct Driven
{
    Samples wet;
    std::vector<float> gainDb;
};

Driven driveOne(dsp::LevelNormalizer& stage, const Samples& a, const Samples& b, const Samples& wet)
{
    Driven out;
    out.wet = wet;
    for (std::size_t at = 0; at + kHop <= wet.size(); at += kHop)
    {
        stage.processChannel(0, a.data() + at, b.data() + at, out.wet.data() + at, kHop);
        out.gainDb.push_back(stage.gainDb(0));
    }
    return out;
}

/** A bare detector carrying the stage's own constants — the independent reading the target is measured
    from, so A1 is never the stage agreeing with itself. */
dsp::LevelDetector makeDetector()
{
    dsp::LevelDetector detector(kWindowMs, dsp::LevelNormalizer::attackMs,
                                dsp::LevelNormalizer::releaseMs);
    detector.prepare(kRate, 1);
    return detector;
}

double settledEnvelope(const Samples& stream)
{
    dsp::LevelDetector detector = makeDetector();
    for (std::size_t at = 0; at + kHop <= stream.size(); at += kHop)
        detector.processChannel(0, stream.data() + at, kHop);
    return detector.envelope(0);
}

std::ostream& log()
{
    return std::cout << "  ";
}
}

TEST_CASE("the level normalizer holds the wet path at the geometric mean of the two readings")
{
    /* Criteria A1 and A3. envA != envB is mandatory: equal readings cannot tell a geometric mean from an
       arithmetic one. A is 4x B, which is also the ratio A3's separation is stated at, and the wet path is
       deliberately quiet so the correction is a real move rather than a nudge. */
    const std::size_t count = forHops(560);
    const Samples a = tone(count, 0.5);
    const Samples b = tone(count, 0.125);
    const Samples wet = tone(count, 0.05);

    dsp::LevelNormalizer stage;
    stage.prepare(kRate, 1, kHop);
    const Driven drove = driveOne(stage, a, b, wet);

    const double envA = settledEnvelope(a);
    const double envB = settledEnvelope(b);
    const double geometric = std::sqrt(envA * envB);
    const double arithmetic = 0.5 * (envA + envB);
    const double held = rmsTail(drove.wet, kTailSamples);
    const double raw = rmsTail(wet, kTailSamples);

    /* A1 against the analytic answer, which the held level reproduces to the last digit. The target built
       from the readings sits 0.65 dB above it and that gap is the detector's, not the stage's: D-014's
       follower with attack 20 ms and release 100 ms reads a 1 kHz tone 7.8 % high, all three readings by
       the same factor, so the bias cancels in the ratio this stage applies and what is held is the analytic
       geometric mean. It will not cancel when the three buses have different ripple content — a smeared
       wet path against percussive dry ones — which is why the figure is recorded here rather than absorbed
       into a tolerance. */
    const double analyticGeometric = std::sqrt(0.5 * kInvRootTwo * 0.125 * kInvRootTwo);
    const double analyticArithmetic = 0.5 * (0.5 * kInvRootTwo + 0.125 * kInvRootTwo);

    const double ratio = envA / envB;
    log() << "criterion A1  held " << held << " against the analytic target " << analyticGeometric << " ("
          << decibels(held / analyticGeometric) << " dB) from a raw wet level of " << raw
          << "; the target built from the readings is " << geometric << ", "
          << decibels(geometric / analyticGeometric) << " dB above it\n";
    log() << "criterion A3  envA/envB " << ratio << ", readings' own arithmetic-minus-geometric "
          << decibels(arithmetic / geometric) << " dB; the held level sits "
          << decibels(analyticArithmetic / held) << " dB below where an arithmetic target would hold"
             " (1.938 expected)\n";

    INFO("envA " << envA << " envB " << envB << " target " << geometric << " held " << held);
    CHECK(envA > envB);
    CHECK(std::abs(decibels(held / analyticGeometric)) <= 0.1);
    CHECK(std::abs(decibels(analyticArithmetic / held) - 1.938) <= 0.1);
    CHECK(std::abs(ratio - 4.0) <= 0.04);
    CHECK(std::abs(decibels(arithmetic / geometric) - 1.938) <= 0.1);
    CHECK(std::abs(decibels(held / raw)) >= 6.0); ///< the correction is real, so nothing above is an accident
}

TEST_CASE("the level normalizer moves at half slope for one bus and full slope for both")
{
    /* Criterion A2, printed as a table because this is the figure a listener experiences as "the effect
       follows the mix". Each case settles from scratch. The change is +12 dB expressed as 10^(12/20) rather
       than as 4x, because 4x is 12.0412 dB and would move every row by 0.04 against its own expectation. */
    const double boost = std::pow(10.0, 12.0 / 20.0);
    const std::size_t count = forHops(400);
    const Samples wet = tone(count, 0.05);

    struct Case
    {
        double amplitudeA;
        double amplitudeB;
        const char* what;
        double wantDeltaDb;
    };

    const std::vector<Case> cases{
        {0.5, 0.125, "baseline A 0.5 / B 0.125", 0.0},
        {0.5 * boost, 0.125, "A alone +12 dB", 6.0},
        {0.5, 0.125 * boost, "B alone +12 dB", 6.0},
        {0.5 * boost, 0.125 * boost, "both buses +12 dB", 12.0},
    };

    double baseline = 0.0;
    for (const Case& c : cases)
    {
        dsp::LevelNormalizer stage;
        stage.prepare(kRate, 1, kHop);
        const Driven drove = driveOne(stage, tone(count, c.amplitudeA), tone(count, c.amplitudeB), wet);
        const double held = rmsTail(drove.wet, kTailSamples);
        if (c.wantDeltaDb == 0.0)
            baseline = held;

        const double delta = decibels(held / baseline);
        const double appliedGainDb = decibels(held / rmsTail(wet, kTailSamples));
        log() << "criterion A2  " << c.what << ": held " << held << ", " << delta
              << " dB from the baseline row (" << c.wantDeltaDb
              << " expected), applied gain " << appliedGainDb << " dB\n";
        INFO(c.what);
        CHECK(std::abs(delta - c.wantDeltaDb) <= 0.15);
        /* Every row has to sit inside the ±36 dB range, or a row could be reading the clamp rather than
           the slope. */
        CHECK(std::abs(appliedGainDb) <= dsp::LevelNormalizer::gainRangeDb - 6.0);
    }
}

TEST_CASE("the level normalizer's amount dial is an exponent and zero is bit-exact identity")
{
    /* Criterion A4. Zero is the identity on every sample of every hop — a bit claim, not a level one: with
       amount 0 the stage must not round the wet path at all. The mid-travel leg is what distinguishes the
       power form from a linear blend toward the correction, which is 0.508 dB apart at a 6 dB correction
       and 2.55 dB apart at this case's 14 dB one. */
    const std::size_t count = forHops(300);
    const Samples a = tone(count, 0.5);
    const Samples b = tone(count, 0.125);
    const Samples wet = tone(count, 0.05);
    const double raw = rmsTail(wet, kTailSamples);

    dsp::LevelNormalizer off;
    off.prepare(kRate, 1, kHop);
    off.setAmount(0.0f);
    const Driven identity = driveOne(off, a, b, wet);
    CHECK(sameBits(identity.wet, wet));
    CHECK(identity.gainDb.size() == count / kHop);
    for (const float hopGainDb : identity.gainDb)
        CHECK(hopGainDb == 0.0f);

    std::vector<double> heldAcrossDial;
    for (int step = 0; step <= 128; ++step)
    {
        dsp::LevelNormalizer dial;
        dial.prepare(kRate, 1, kHop);
        dial.setAmount(static_cast<float>(step) / 128.0f);
        heldAcrossDial.push_back(rmsTail(driveOne(dial, a, b, wet).wet, kTailSamples));
    }

    const double gainAtOne = heldAcrossDial.back() / raw;
    bool monotone = true;
    for (std::size_t i = 1; i < heldAcrossDial.size(); ++i)
        monotone = monotone && heldAcrossDial[i] >= heldAcrossDial[i - 1];

    const double mid = heldAcrossDial[64] / raw;
    const double powerForm = std::sqrt(gainAtOne);
    const double linearForm = 1.0 + 0.5 * (gainAtOne - 1.0);
    log() << "criterion A4  amount 1 is " << decibels(gainAtOne) << " dB of correction; amount 0.5 measured "
          << decibels(mid) << " dB against the power form " << decibels(powerForm) << " and the linear form "
          << decibels(linearForm) << " dB (the two predictions are "
          << std::abs(decibels(powerForm) - decibels(linearForm)) << " dB apart)\n";

    CHECK(monotone);
    CHECK(heldAcrossDial.front() == raw);
    CHECK(std::abs(decibels(mid / powerForm)) <= 0.01);
    CHECK(std::abs(decibels(powerForm) - decibels(linearForm)) >= 0.4);
}

TEST_CASE("the level normalizer reads the wet path upstream of the gain it applies")
{
    /* Criterion A5 — decision C made executable, and the only leg here that catches a one-line wiring slip
       with an otherwise green build. Feed the conv detector after the in-place scale and the stage becomes
       an AGC loop while every other criterion still passes, so the reading is compared against a bare
       detector on the un-scaled stream, bit for bit, at every hop boundary.
       The negative control is the same comparison against a detector fed the *scaled* stream: if the two
       readings were indistinguishable the check above would be vacuous, so it has to fail. */
    const std::size_t count = forHops(400);
    const Samples a = tone(count, 0.5);
    const Samples b = tone(count, 0.125);
    const Samples wet = tone(count, 0.05);

    dsp::LevelDetector upstream = makeDetector();
    dsp::LevelNormalizer stage;
    stage.prepare(kRate, 1, kHop);

    bool everyHopMatches = true;
    bool everCorrecting = false;
    Samples scaled = wet;

    for (std::size_t at = 0; at + kHop <= count; at += kHop)
    {
        upstream.processChannel(0, wet.data() + at, kHop);
        stage.processChannel(0, a.data() + at, b.data() + at, scaled.data() + at, kHop);

        if (stage.envelopeConv(0) != upstream.envelope(0) && everyHopMatches)
        {
            INFO("first mismatch at hop " << at / kHop << ": stage " << stage.envelopeConv(0)
                                          << " against the bare detector's " << upstream.envelope(0));
            everyHopMatches = false;
        }
        if (std::abs(stage.gainDb(0)) > 1.0f)
            everCorrecting = true;
    }

    dsp::LevelDetector downstream = makeDetector();
    bool everDistinguishable = false;
    for (std::size_t at = 0; at + kHop <= count; at += kHop)
    {
        downstream.processChannel(0, scaled.data() + at, kHop);
        if (downstream.envelope(0) != upstream.envelope(0))
            everDistinguishable = true;
    }

    log() << "criterion A5  the stage's conv reading equals a bare detector's on the un-scaled stream at "
          << count / kHop << " hop boundaries; the same detector fed the scaled stream reads differently: "
          << (everDistinguishable ? "yes" : "no") << "\n";
    CHECK(everyHopMatches);
    CHECK(everCorrecting);
    CHECK(everDistinguishable);
}

TEST_CASE("the level normalizer passes the wet path through when any one reading is at the floor")
{
    /* Criteria B1 and B2. The gate is on all three readings, so each is floored in turn with the other two
       loud — flooring B under a loud A is the case a clamp-only design would turn into a
       sqrt(envA · floor) boost, and unity is the answer. The control row shows what the gate is suppressing,
       or a stage that never corrected at all would pass every row above it. */
    const std::size_t count = forHops(400);
    const Samples loud = tone(count, 0.5);
    const Samples mid = tone(count, 0.125);
    const Samples wetLoud = tone(count, 0.2);
    const Samples underFloor = tone(count, 1.0e-4); ///< -80 dBFS, under the -60 dB default
    const Samples silence = constant(count, 0.0f);

    struct Case
    {
        const Samples* a;
        const Samples* b;
        const Samples* wet;
        const char* what;
    };

    const std::vector<Case> cases{
        {&underFloor, &mid, &wetLoud, "A at the floor, B and conv loud"},
        {&loud, &underFloor, &wetLoud, "B at the floor, A and conv loud"},
        {&loud, &mid, &silence, "conv at the floor, a silent wet path"},
        {&silence, &silence, &silence, "all three silent"},
    };

    for (const Case& c : cases)
    {
        dsp::LevelNormalizer stage;
        stage.prepare(kRate, 1, kHop);
        const Driven drove = driveOne(stage, *c.a, *c.b, *c.wet);

        INFO(c.what);
        CHECK(sameBits(drove.wet, *c.wet)); ///< unity means bit-identical, not merely equal
        for (const float hopGainDb : drove.gainDb)
            CHECK(hopGainDb == 0.0f);
        CHECK(stage.nonFiniteHops() == 0);
        log() << "criterion B1  " << c.what << ": every sample of every hop at exactly unity, "
              << stage.nonFiniteHops() << " hops counted non-finite\n";
    }

    dsp::LevelNormalizer control;
    control.prepare(kRate, 1, kHop);
    const Driven moved = driveOne(control, loud, mid, tone(count, 0.002));
    bool controlCorrected = false;
    for (const float hopGainDb : moved.gainDb)
        controlCorrected = controlCorrected || hopGainDb > 6.0f;

    log() << "criterion B2  the same buses with nothing at the floor drive the gain to "
          << moved.gainDb.back() << " dB, so the rows above are the gate and not a stage that never acts\n";
    CHECK(controlCorrected);
}

TEST_CASE("the level normalizer's gain stays inside the ±36 dB range and its clamp is monotone")
{
    /* Criterion B3. The wet level is walked in 1 dB steps across the range where the required correction
       crosses both clamps, so the settled gains are the correction function sampled finely: monotone, flat
       at exactly ±36 dB outside, and with no step at either boundary that a coarser grid would hide.
       The span is set by the clamps themselves: the target here is 0.1499, so the +36 dB rail is left at a
       wet amplitude of 0.0084 and the -36 dB rail is reached at 13.3 — 85 rungs from 2e-3 bracket both with
       room to spare. Anything narrower measures one rail and calls it the range.

       The rule column is the *analytic* one, `20 log10(target / wetRms)` with the target built from the
       amplitudes rather than the readings. That is the interesting half of the leg: D-014's follower reads a
       1 kHz tone 7.8 % high, all three buses alike, so the factor cancels in the ratio — including at the
       clamp, where the number compared against ±36 dB is the bias-free one. Building the rule from
       `envelope()` instead adds the follower's own 0.65 dB offset to every rung and leaves it looking like
       a gain-stage error. */
    const std::size_t count = forHops(120);
    const Samples a = tone(count, 0.5);
    const Samples b = tone(count, 0.125);
    const double target = std::sqrt(0.5 * kInvRootTwo * 0.125 * kInvRootTwo);

    bool inside = true;
    bool monotone = true;
    int clampedHigh = 0;
    int clampedLow = 0;
    double worstAgainstRule = 0.0;
    double previous = 1.0e300;

    for (int step = 0; step <= 84; ++step)
    {
        const double wetAmplitude = 2.0e-3 * std::pow(10.0, static_cast<double>(step) / 20.0);
        const Samples wet = tone(count, wetAmplitude);

        dsp::LevelNormalizer stage;
        stage.prepare(kRate, 1, kHop);
        const Driven drove = driveOne(stage, a, b, wet);
        const double settled = drove.gainDb.back();
        const double rawWanted = decibels(target / rmsTail(wet, kTailSamples));
        const double rule = std::min(dsp::LevelNormalizer::gainRangeDb,
                                     std::max(-dsp::LevelNormalizer::gainRangeDb, rawWanted));

        if (std::abs(settled) > dsp::LevelNormalizer::gainRangeDb + 1.0e-9)
            inside = false;
        if (settled > previous + 1.0e-9)
            monotone = false;
        if (settled == dsp::LevelNormalizer::gainRangeDb)
            ++clampedHigh;
        if (settled == -dsp::LevelNormalizer::gainRangeDb)
            ++clampedLow;
        previous = settled;
        worstAgainstRule = std::max(worstAgainstRule, std::abs(settled - rule));
        INFO("wet level " << wetAmplitude << ": settled " << settled << " dB, rule " << rule << " dB");
        CHECK(std::abs(settled - rule) <= 0.1);
    }

    log() << "criterion B3  85 wet levels a dB apart: worst deviation from clamp(wanted) "
          << worstAgainstRule << " dB, " << clampedHigh << " rungs held on the +36 dB clamp and "
          << clampedLow << " on the -36 dB one\n";
    CHECK(inside);
    CHECK(monotone);
    CHECK(clampedHigh >= 2);
    CHECK(clampedLow >= 2);
}

TEST_CASE("the level normalizer's slew cap is a time, not a per-hop figure")
{
    /* Criterion B4. The cap is 360 dB/s realised against the prepare-time rate, so it has to bite at every
       hop the UI can express: stated per hop it would be 8x slower in time at H = 512 than at H = 64, which
       is INDEX §8.5's unit error in different clothing.
       The wet bus is a constant 0.5 so the applied gain is recoverable sample by sample — 0.5 * g divides
       exactly — and the floor is pushed to -90 dB so the gate cannot mask what the limiter is doing. */
    for (const std::size_t hop : {std::size_t{64}, std::size_t{128}, std::size_t{256}, std::size_t{512}})
    {
        const double cap = 360.0 * static_cast<double>(hop) / kRate;

        dsp::LevelNormalizer stage;
        stage.prepare(kRate, 1, hop);
        stage.setFloorDb(-90.0f);
        CHECK(std::abs(stage.slewDbPerHop() - cap) <= 1.0e-12);

        const std::size_t count = forHops(400);
        const Samples a = toneProgramme(count);
        const Samples wet = constant(count, 0.5f);

        double worstHopStep = 0.0;
        double worstSampleStep = 0.0;
        double previousGainDb = 0.0;
        double previousSampleGain = 1.0;
        for (std::size_t at = 0; at + hop <= count; at += hop)
        {
            Samples piece = slice(wet, at, hop);
            stage.processChannel(0, a.data() + at, a.data() + at, piece.data(), hop);

            const double settled = stage.gainDb(0);
            worstHopStep = std::max(worstHopStep, std::abs(settled - previousGainDb));
            previousGainDb = settled;

            for (std::size_t i = 0; i < hop; ++i)
            {
                const double applied = static_cast<double>(piece[i]) / 0.5;
                worstSampleStep = std::max(worstSampleStep, std::abs(decibels(applied / previousSampleGain)));
                previousSampleGain = applied;
            }
        }

        /* Full travel: the wet bus steps from 0.01 to 100, which steps the wanted gain from the +36 dB rail to
           the -36 dB rail, and that is the only way to ask for the whole 72 dB at once. Stepping A instead
           leaves the wanted value drifting at the detector's 100 ms release — about 0.23 dB per hop, below the
           cap at every hop size the UI can select — so the gain tracks the follower and the leg measures
           D-014's release time (921 ms, measured) rather than the limiter. Both wet levels sit far above the
           -90 dB floor, so the gate keeps out of the way. */
        dsp::LevelNormalizer travel;
        travel.prepare(kRate, 1, hop);
        travel.setFloorDb(-90.0f);
        const Samples driveBus = constant(200000, 1.0f);
        const Samples quietBus = constant(hop, 0.01f);
        const Samples loudBus = constant(hop, 100.0f);
        Samples piece(hop, 0.0f);
        const std::size_t settleHops
            = static_cast<std::size_t>(2.0 * dsp::LevelNormalizer::gainRangeDb / cap) + 20;
        for (std::size_t n = 0; n < settleHops; ++n)
        {
            /* A fresh copy per hop, every time: `processChannel` scales what it is handed, so reusing one
               buffer feeds the stage its own output and closes the loop decision C exists to keep open. */
            std::copy_n(quietBus.begin(), hop, piece.begin());
            travel.processChannel(0, driveBus.data(), driveBus.data(), piece.data(), hop);
        }
        CHECK(travel.gainDb(0) == static_cast<float>(dsp::LevelNormalizer::gainRangeDb));

        int hopsToCross = 0;
        bool crossed = false;
        for (std::size_t n = 0; n < 8 * settleHops && !crossed; ++n)
        {
            const float* const drive = driveBus.data() + (settleHops + n) * hop;
            std::copy_n(loudBus.begin(), hop, piece.begin());
            travel.processChannel(0, drive, drive, piece.data(), hop);
            ++hopsToCross;
            if (travel.gainDb(0) == static_cast<float>(-dsp::LevelNormalizer::gainRangeDb))
                crossed = true;
        }

        const double travelMs
            = 1000.0 * static_cast<double>(hopsToCross) * static_cast<double>(hop) / kRate;
        log() << "criterion B4  H=" << hop << ": cap " << cap << " dB/hop, worst hop step " << worstHopStep
              << " dB, worst sample step " << worstSampleStep << " dB; 72 dB crossed in " << hopsToCross
              << " hops = " << travelMs << " ms (" << dsp::LevelNormalizer::fullTravelMs
              << " predicted)\n";
        CHECK(crossed);
        /* The slack is the getter's, not the limiter's: gainDb() is a float view of a double ramp, and two
           consecutive endpoints at 36 dB are 3.8e-6 apart in float, so a capped hop can read that much over
           the cap while the double underneath is exactly on it. */
        CHECK(worstHopStep <= cap + 1.0e-5);
        CHECK(worstSampleStep <= cap + 1.0e-6);
        CHECK(worstHopStep >= cap - 1.0e-6); ///< the cap has to actually bind, or the bound is vacuous
        CHECK(travelMs >= dsp::LevelNormalizer::fullTravelMs - 0.01);
        CHECK(travelMs <= dsp::LevelNormalizer::fullTravelMs + 2000.0 * static_cast<double>(hop) / kRate);
    }
}

TEST_CASE("the level normalizer returns to exactly unity once the inputs go silent")
{
    /* Criterion B5. The bound is the detector's published figure — silenceSamples() on a detector carrying
       the stage's own constants — consumed rather than re-derived, plus one hop for the decision already
       taken when the readings land. The second leg keeps the wet bus a constant 0.5 so that "exactly unity"
       is a claim about the samples rather than about a getter. */
    dsp::LevelDetector probe = makeDetector();
    const std::size_t silenceBound = probe.silenceSamples();

    const std::size_t settledLength = forHops(200);
    const Samples loud = tone(settledLength, 0.9);
    const Samples wetLoud = tone(settledLength, 0.05);

    dsp::LevelNormalizer stage;
    stage.prepare(kRate, 1, kHop);
    const Driven settledRun = driveOne(stage, loud, loud, wetLoud);
    const double settledGainDb = settledRun.gainDb.back();
    REQUIRE(std::abs(settledGainDb) > 1.0);

    const std::size_t idleHops = silenceBound / kHop + 8;
    const Samples oneHop = constant(kHop, 0.0f);

    std::size_t hopsToUnity = 0;
    bool stayedAtUnity = true;
    bool everyGainSane = true;
    for (std::size_t hop = 0; hop < idleHops; ++hop)
    {
        Samples piece = oneHop;
        stage.processChannel(0, oneHop.data(), oneHop.data(), piece.data(), kHop);
        const float decided = stage.gainDb(0);
        if (!std::isfinite(decided) || (decided != 0.0f && std::abs(decided) < 1.0e-25))
            everyGainSane = false;
        if (decided == 0.0f)
        {
            if (hopsToUnity == 0)
                hopsToUnity = hop + 1;
        }
        else if (hopsToUnity != 0)
        {
            stayedAtUnity = false;
        }
    }

    /* The sampled proof: A and B stop, the wet bus keeps a constant 0.5, and what comes back is the same
       bits that went in. */
    dsp::LevelNormalizer withTail;
    withTail.prepare(kRate, 1, kHop);
    driveOne(withTail, loud, loud, wetLoud);
    const Samples silence = constant(forHops(idleHops), 0.0f);
    const Samples tail = constant(forHops(idleHops), 0.5f);
    const Driven afterSilence = driveOne(withTail, silence, silence, tail);

    bool sameBitsAgain = true;
    for (std::size_t i = silenceBound / 2; i < tail.size(); ++i)
        sameBitsAgain = sameBitsAgain && afterSilence.wet[i] == tail[i];

    const double returnedMs = 1000.0 * static_cast<double>(hopsToUnity * kHop) / kRate;
    const double boundMs = 1000.0 * static_cast<double>(silenceBound + kHop) / kRate;
    log() << "criterion B5  settled gain " << settledGainDb << " dB; unity again after " << hopsToUnity
          << " hops = " << returnedMs << " ms, against the detector's published bound of " << silenceBound
          << " samples + 1 hop = " << boundMs << " ms\n";
    CHECK(hopsToUnity != 0);
    CHECK(returnedMs <= boundMs);
    CHECK(stayedAtUnity);
    CHECK(everyGainSane);
    CHECK(sameBitsAgain);
}

TEST_CASE("the level normalizer degrades a poisoned channel to unity and counts it")
{
    /* Criterion B6 and decision G. One NaN poisons that channel's detector for good, so from there on the
       answer is unity and uncorrected forever, counted once per hop per channel — never silence, never a
       NaN gain, and never a stage that goes quiet.
       What the stage does not do is take policy on a non-finite *sample*: at unity gain the multiply is
       exact, and SoftClipper is the stage that replaces and counts non-finite samples on the way out. A NaN
       on the wet bus therefore passes through at unity gain, which is recorded here as the contract. */
    const std::size_t hops = 40;
    const std::size_t count = forHops(hops);
    const Samples cleanA = tone(count, 0.5);
    const Samples cleanB = tone(count, 0.125);
    const Samples cleanWet = tone(count, 0.05);

    struct Case
    {
        int stream; ///< 0 = A, 1 = B, 2 = conv
        const char* what;
    };

    const std::vector<Case> cases{{0, "A"}, {1, "B"}, {2, "conv"}};

    for (const Case& c : cases)
    {
        Samples a = cleanA;
        Samples b = cleanB;
        Samples wet = cleanWet;
        Samples& victim = c.stream == 0 ? a : (c.stream == 1 ? b : wet);
        victim[kHop / 2] = std::numeric_limits<float>::quiet_NaN();

        dsp::LevelNormalizer stage;
        stage.prepare(kRate, 1, kHop);

        std::uint64_t countedSoFar = 0;
        bool unityAndCountedEveryHop = true;
        bool outputFinite = true;
        for (std::size_t hop = 0; hop < hops; ++hop)
        {
            const std::size_t at = hop * kHop;
            Samples piece = slice(wet, at, kHop);
            stage.processChannel(0, a.data() + at, b.data() + at, piece.data(), kHop);

            const std::uint64_t delta = stage.nonFiniteHops() - countedSoFar;
            countedSoFar = stage.nonFiniteHops();
            if (stage.gainDb(0) != 0.0f || delta != 1)
                unityAndCountedEveryHop = false;
            if (c.stream != 2)
                for (const float sample : piece)
                    outputFinite = outputFinite && std::isfinite(sample);
        }

        log() << "criterion B6  NaN on " << c.what << ": " << stage.nonFiniteHops() << " of " << hops
              << " hops counted, gain exactly unity on each of them,"
              << (c.stream == 2 ? " sample left to the clipper" : " output finite throughout") << "\n";
        INFO(c.what);
        CHECK(unityAndCountedEveryHop);
        CHECK(stage.nonFiniteHops() == hops);
        if (c.stream != 2)
            CHECK(outputFinite);
        else
            CHECK(std::isnan(stage.envelopeConv(0)));
    }

    /* Two poisoned channels are two counts per hop, which is what "once per hop per channel" means, and the
       poisoned channel's neighbour keeps correcting. */
    dsp::LevelNormalizer pair;
    pair.prepare(kRate, 2, kHop);
    Samples a = cleanA;
    a[0] = std::numeric_limits<float>::quiet_NaN();
    for (std::size_t hop = 0; hop < 10; ++hop)
    {
        const std::size_t at = hop * kHop;
        Samples wet0 = slice(cleanWet, at, kHop);
        Samples wet1 = wet0;
        pair.processChannel(0, a.data() + at, cleanB.data() + at, wet0.data(), kHop);
        pair.processChannel(1, cleanA.data() + at, cleanB.data() + at, wet1.data(), kHop);
    }
    log() << "criterion B6  10 hops with one of two channels poisoned: " << pair.nonFiniteHops()
          << " counts (10 expected), and the clean channel is still correcting at " << pair.gainDb(1)
          << " dB\n";
    CHECK(pair.nonFiniteHops() == 10);
    CHECK(std::abs(pair.gainDb(1)) > 1.0f);
}

TEST_CASE("the level normalizer allocates nothing on the hop path and reset gives a fresh stage")
{
    /* Criteria C6 and C9 at the stage level; the chain-level versions live with the mix stage and the
       shell. This shares DspTests' single operator new tripwire rather than defining a second one. */
    const std::size_t hops = 300;
    const std::size_t count = forHops(hops);
    const Samples a = toneProgramme(count);
    const Samples b = tone(count, 0.125);
    const Samples wet = tone(count, 0.05);

    dsp::LevelNormalizer stage;
    stage.prepare(kRate, 1, kHop);
    CHECK(stage.hopSamples() == kHop);
    CHECK(stage.windowSamples() == kHop); ///< decision E: one hop of window, from the prepare-time rate
    CHECK(stage.amount() == dsp::LevelNormalizer::defaultAmount);
    CHECK(stage.floorDb() == dsp::LevelNormalizer::defaultFloorDb);

    long allocations = 0;
    /* Hoisted deliberately: the leg counts allocations, so the hop-sized scratch the driver needs must not
       be one of them. A `slice` inside the watched scope reports 300 allocations for a stage that makes
       none, which is the difference between testing the stage and testing the test. */
    Samples piece(kHop);
    {
        allocwatch::Watch watch;
        for (std::size_t hop = 0; hop < hops; ++hop)
        {
            const std::size_t at = hop * kHop;
            stage.setAmount(static_cast<float>(hop % 5) / 4.0f);
            stage.setFloorDb(-60.0f + static_cast<float>(hop % 3));
            std::copy_n(wet.begin() + static_cast<std::ptrdiff_t>(at), kHop, piece.begin());
            stage.processChannel(0, a.data() + at, b.data() + at, piece.data(), kHop);
            static_cast<void>(stage.gainDb(0));
            static_cast<void>(stage.envelopeA(0));
            static_cast<void>(stage.envelopeB(0));
            static_cast<void>(stage.envelopeConv(0));
        }
        allocations = allocwatch::allocations;
    }

    /* reset() is the operational definition of bit-identical-to-fresh: the same stream through a stage that
       was driven and reset reads exactly as the same stream through one that never was. The two dials are
       the caller's and survive — the shell pushes its parameter targets before resetting the transport, so
       a reset that restored the defaults would drop them until the next callback. */
    const Samples probeA = tone(forHops(120), 0.5);
    const Samples probeB = tone(forHops(120), 0.125);
    const Samples probeWet = tone(forHops(120), 0.02);

    dsp::LevelNormalizer fresh;
    fresh.prepare(kRate, 1, kHop);
    const Driven reference = driveOne(fresh, probeA, probeB, probeWet);

    dsp::LevelNormalizer reused;
    reused.prepare(kRate, 1, kHop);
    driveOne(reused, a, b, wet);
    reused.reset();
    const Driven again = driveOne(reused, probeA, probeB, probeWet);

    dsp::LevelNormalizer withControls;
    withControls.prepare(kRate, 1, kHop);
    driveOne(withControls, a, b, wet);
    withControls.setAmount(0.25f);
    withControls.setFloorDb(-30.0f);
    withControls.reset();
    const bool controlsSurvived = withControls.amount() == 0.25f && withControls.floorDb() == -30.0f;
    withControls.setAmount(dsp::LevelNormalizer::defaultAmount);
    withControls.setFloorDb(dsp::LevelNormalizer::defaultFloorDb);
    const Driven afterControls = driveOne(withControls, probeA, probeB, probeWet);

    log() << "criterion C6/C9  " << allocations << " allocations over " << hops
          << " hops with both controls moving; reset() reproduced a fresh stage bit for bit: "
          << (sameBits(again.wet, reference.wet) ? "yes" : "no")
          << ", and the caller's amount and floor survived it: " << (controlsSurvived ? "yes" : "no")
          << "\n";
    CHECK(allocations == 0);
    CHECK(sameBits(again.wet, reference.wet));
    CHECK(sameBits(afterControls.wet, reference.wet));
    CHECK(controlsSurvived);
}
