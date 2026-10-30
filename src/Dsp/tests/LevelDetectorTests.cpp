#include <LevelDetector.hpp>

#include "AllocationWatch.hpp"

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace
{
using Samples = std::vector<float>;

constexpr double kPi = 3.14159265358979323846;

/** 1/sqrt(2): what a stationary sine of amplitude A reads in RMS — criterion 1's number. */
constexpr double kInvRootTwo = 0.70710678118654752440;

/** 2/pi: what mean-rectifying the same sine reads instead. Printed beside the measured reading so the
    record shows the two are 10 % apart where the criterion is set at 0.5 %. */
constexpr double kMeanRectified = 0.63661977236758134308;

/** e^-1: 36.8 %, the fraction the release criterion is stated at; one minus it is the attack's. */
constexpr double kInverseE = 0.36787944117144233;

constexpr double kRates[] = {44100.0, 48000.0, 96000.0, 192000.0};

/** One reading triple per stop, plus the sample count each stop belongs to. */
struct Series
{
    std::vector<float> rms;
    std::vector<float> peak;
    std::vector<float> envelope;
    std::vector<std::size_t> at;
};

Samples sine(std::size_t count, double amplitude, double frequency, double sampleRate)
{
    Samples out(count);
    const double step = 2.0 * kPi * frequency / sampleRate;
    for (std::size_t i = 0; i < count; ++i)
        out[i] = static_cast<float>(amplitude * std::sin(step * static_cast<double>(i)));
    return out;
}

/** A stream that exercises everything the state machine has to survive: a tone, a level change, a
    silent stretch, a value near the bottom of the float range and one near the top. */
Samples mixedStream(std::size_t count, double sampleRate)
{
    Samples out(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double t = static_cast<double>(i);
        double value = 0.4 * std::sin(2.0 * kPi * 211.0 * t / sampleRate);
        if (i > count / 3 && i < 2 * count / 3)
            value *= 0.05;
        if (i % 5000 < 300)
            value = 0.0;
        if (i % 997 == 0)
            value = 1.0e-7;
        if (i % 1009 == 0)
            value = 0.98;
        out[i] = static_cast<float>(value);
    }
    return out;
}

/** Feed the whole stream in fixed chunks, taking all three readings at every chunk boundary. */
Series runInChunks(dsp::LevelDetector& detector, const Samples& stream, std::size_t chunk)
{
    Series out;
    for (std::size_t start = 0; start < stream.size(); start += chunk)
    {
        const std::size_t count = std::min(chunk, stream.size() - start);
        detector.processChannel(0, stream.data() + start, count);
        out.rms.push_back(detector.windowedRms(0));
        out.peak.push_back(detector.windowedPeak(0));
        out.envelope.push_back(detector.envelope(0));
        out.at.push_back(start + count);
    }
    return out;
}

/** The same with a caller-supplied sequence of chunk sizes. */
Series runWithChunkSizes(dsp::LevelDetector& detector, const Samples& stream,
                         const std::vector<std::size_t>& chunks)
{
    Series out;
    std::size_t start = 0;
    for (const std::size_t chunk : chunks)
    {
        if (start >= stream.size())
            break;
        const std::size_t count = std::min(chunk, stream.size() - start);
        detector.processChannel(0, stream.data() + start, count);
        out.rms.push_back(detector.windowedRms(0));
        out.peak.push_back(detector.windowedPeak(0));
        out.envelope.push_back(detector.envelope(0));
        out.at.push_back(start + count);
        start += count;
    }
    return out;
}

bool sameBits(const std::vector<float>& lhs, const std::vector<float>& rhs)
{
    return lhs.size() == rhs.size()
           && std::memcmp(lhs.data(), rhs.data(), lhs.size() * sizeof(float)) == 0;
}

/** How many of `other`'s stops disagree with the per-sample reference by any bit at all, and the
    largest absolute difference between them. The count is the criterion; the difference is for the log. */
void compareAgainstReference(const Series& reference, const Series& other, double& worstDifference,
                             std::size_t& differingStops)
{
    worstDifference = 0.0;
    differingStops = 0;
    for (std::size_t k = 0; k < other.at.size(); ++k)
    {
        const std::size_t position = other.at[k] - 1;
        const float want[3]
            = {reference.rms[position], reference.peak[position], reference.envelope[position]};
        const float got[3] = {other.rms[k], other.peak[k], other.envelope[k]};
        if (std::memcmp(want, got, sizeof(want)) != 0)
            ++differingStops;
        for (std::size_t j = 0; j < 3; ++j)
            worstDifference = std::max(worstDifference,
                                       std::abs(static_cast<double>(want[j]) - static_cast<double>(got[j])));
    }
}

/** Samples consumed since the step before a rising reading first reaches 63.2 % of its settled value.
    `stepAt` is the index of the first sample of the new level. */
double attackLengthSamples(const std::vector<float>& reading, std::size_t stepAt, double settled)
{
    const double want = (1.0 - kInverseE) * settled;
    for (std::size_t i = stepAt; i < reading.size(); ++i)
        if (static_cast<double>(reading[i]) >= want)
            return static_cast<double>(i + 1 - stepAt);
    return -1.0;
}

/** The same for a step to silence: samples of silence before the reading falls to 36.8 % of the level
    it was holding. `stepAt` is the index of the first silent sample. */
double releaseLengthSamples(const std::vector<float>& reading, std::size_t stepAt, double settled)
{
    const double want = kInverseE * settled;
    for (std::size_t i = stepAt; i < reading.size(); ++i)
        if (static_cast<double>(reading[i]) <= want)
            return static_cast<double>(i + 1 - stepAt);
    return -1.0;
}

bool monotoneNonDecreasing(const std::vector<float>& reading, std::size_t from, std::size_t to)
{
    for (std::size_t i = from + 1; i < to && i < reading.size(); ++i)
        if (reading[i] < reading[i - 1])
            return false;
    return true;
}

bool monotoneNonIncreasing(const std::vector<float>& reading, std::size_t from, std::size_t to)
{
    for (std::size_t i = from + 1; i < to && i < reading.size(); ++i)
        if (reading[i] > reading[i - 1])
            return false;
    return true;
}

double referenceRmsOver(const Samples& stream, std::size_t window)
{
    double sum = 0.0;
    for (std::size_t i = stream.size() - window; i < stream.size(); ++i)
        sum += static_cast<double>(stream[i]) * static_cast<double>(stream[i]);
    return std::sqrt(sum / static_cast<double>(window));
}

float referencePeakOver(const Samples& stream, std::size_t window)
{
    float peak = 0.0f;
    for (std::size_t i = stream.size() - window; i < stream.size(); ++i)
        peak = std::max(peak, std::abs(stream[i]));
    return peak;
}

std::ostream& log()
{
    return std::cout << "  ";
}
}

TEST_CASE("the level detector reads a stationary sine as RMS in both readings")
{
    /* Criterion 1. The envelope is measured with attack and release equal, which is the one setting
       where it is a linear filter and so has no crest-dependent bias to argue about; criterion 3
       measures the two constants separately. The tone sits far above the reciprocal of the 100 ms
       time constant, which is what "stationary" has to mean for an envelope: a follower that averaged
       a sine whose period is longer than its own time constant would not be reading level at all. */
    const double rate = 48000.0;
    constexpr double kTolerance = 0.005;

    for (const double amplitude : {0.25, 0.5, 1.0})
    {
        for (const double frequency : {997.0, 5000.0})
        {
            dsp::LevelDetector detector(100.0, 100.0, 100.0);
            detector.prepare(rate, 1);
            REQUIRE(detector.windowSamples() == 4800);

            const Samples stream
                = sine(5 * static_cast<std::size_t>(rate), amplitude, frequency, rate);
            const Series readings = runInChunks(detector, stream, 1);
            REQUIRE(readings.rms.size() == stream.size());

            const double want = amplitude * kInvRootTwo;

            double worstWindowed = 0.0;
            for (std::size_t i = detector.windowSamples() - 1; i < readings.rms.size(); ++i)
                worstWindowed = std::max(worstWindowed,
                                         std::abs(static_cast<double>(readings.rms[i]) - want) / want);

            const std::size_t settledFrom = readings.rms.size() - static_cast<std::size_t>(rate);
            double worstEnvelope = 0.0;
            for (std::size_t i = settledFrom; i < readings.envelope.size(); ++i)
                worstEnvelope = std::max(
                    worstEnvelope, std::abs(static_cast<double>(readings.envelope[i]) - want) / want);

            log() << "criterion 1  A=" << amplitude << " f=" << frequency << "Hz: windowed RMS worst "
                  << worstWindowed * 100.0 << "%, envelope worst " << worstEnvelope * 100.0
                  << "% of " << want << " (mean-rectifying would read " << amplitude * kMeanRectified
                  << ", " << (kMeanRectified - kInvRootTwo) / kInvRootTwo * 100.0 << "% off)\n";

            INFO("A=" << amplitude << " f=" << frequency);
            CHECK(worstWindowed <= kTolerance);
            CHECK(worstEnvelope <= kTolerance);
        }
    }
}

TEST_CASE("the level detector's windowed peak reads the sine's amplitude")
{
    /* Criterion 2. The impulse leg is what says "peak over the window" rather than "the newest
       sample": one non-zero sample in a window of silence has to be reported for exactly as long as
       it is inside the window, at exactly its own magnitude, and the RMS with it. */
    const double rate = 48000.0;
    constexpr double kTolerance = 0.005;

    for (const double amplitude : {0.25, 1.0})
    {
        dsp::LevelDetector detector(100.0, 20.0, 200.0);
        detector.prepare(rate, 1);

        const Samples stream = sine(2 * static_cast<std::size_t>(rate), amplitude, 997.0, rate);
        const Series readings = runInChunks(detector, stream, 1);

        const std::size_t from = stream.size() / 2;
        double worstExcess = 0.0;
        double worstDeficit = 0.0;
        for (std::size_t i = from; i < readings.peak.size(); ++i)
        {
            const double error = (static_cast<double>(readings.peak[i]) - amplitude) / amplitude;
            worstExcess = std::max(worstExcess, error);
            worstDeficit = std::max(worstDeficit, -error);
        }

        log() << "criterion 2  A=" << amplitude << ": windowed peak worst deficit "
              << worstDeficit * 100.0 << "%, worst excess " << worstExcess * 100.0 << "%\n";

        INFO("A=" << amplitude);
        CHECK(worstDeficit <= kTolerance);
        CHECK(worstExcess <= kTolerance);
    }

    dsp::LevelDetector detector(10.0, 5.0, 5.0);
    detector.prepare(rate, 1);
    const std::size_t window = detector.windowSamples();
    REQUIRE(window == 480);

    const float blip = 0.25f;
    const float blipRms = static_cast<float>(std::sqrt(static_cast<double>(blip) * blip
                                                       / static_cast<double>(window)));
    for (std::size_t i = 0; i < window * 3; ++i)
    {
        const float sample = i == 0 ? blip : 0.0f;
        detector.processChannel(0, &sample, 1);
        INFO("i=" << i);
        CHECK(detector.windowedPeak(0) == (i < window ? blip : 0.0f));
        CHECK(detector.windowedRms(0) == (i < window ? blipRms : 0.0f));
    }
}

TEST_CASE("the level detector's envelope lands on its attack and release milliseconds")
{
    /* Criterion 3. A step from silence to a steady level and back, measured on the published reading,
       with the two constants deliberately different so swapping them cannot pass — and then each one
       again with the other pinned to instantaneous. */
    const double rate = 48000.0;
    const double attackMs = 10.0;
    const double releaseMs = 100.0;
    const double level = 0.5;

    dsp::LevelDetector detector(5.0, attackMs, releaseMs);
    detector.prepare(rate, 1);

    const std::size_t lead = 64;
    const std::size_t hold = static_cast<std::size_t>(rate);
    const std::size_t tail = 2 * static_cast<std::size_t>(rate);
    Samples stream(lead + hold + tail, 0.0f);
    for (std::size_t i = lead; i < lead + hold; ++i)
        stream[i] = static_cast<float>(level);

    const Series readings = runInChunks(detector, stream, 1);
    const std::vector<float>& envelope = readings.envelope;

    const double settled = static_cast<double>(envelope[lead + hold - 1]);
    log() << "criterion 3  settled reading " << settled << " for a steady " << level << "\n";
    INFO("settled " << settled << " want " << level);
    CHECK(std::abs(settled - level) <= 0.001 * level);

    const double wantAttack = attackMs * rate / 1000.0;
    const double wantRelease = releaseMs * rate / 1000.0;
    const double attack = attackLengthSamples(envelope, lead, settled);
    const double release = releaseLengthSamples(envelope, lead + hold, settled);

    log() << "criterion 3  attack " << attack << " samples (" << attack * 1000.0 / rate
          << " ms) against " << wantAttack << " wanted; release " << release << " samples ("
          << release * 1000.0 / rate << " ms) against " << wantRelease << " wanted\n";

    INFO("attack " << attack << " want " << wantAttack);
    CHECK(std::abs(attack - wantAttack) <= 0.05 * wantAttack);
    INFO("release " << release << " want " << wantRelease);
    CHECK(std::abs(release - wantRelease) <= 0.05 * wantRelease);

    CHECK(monotoneNonDecreasing(envelope, lead, lead + hold));
    CHECK(monotoneNonIncreasing(envelope, lead + hold, envelope.size()));

    /* Each constant with the other held: an instantaneous attack reaches the level on the sample it
       arrives on while the release still takes its 100 ms, and a zero-length release is at exact zero
       on the first silent sample while a 20 ms attack still takes its 20 ms. */
    dsp::LevelDetector instantAttack(5.0, 0.0, releaseMs);
    instantAttack.prepare(rate, 1);
    const Series fastRise = runInChunks(instantAttack, stream, 1);
    CHECK(fastRise.envelope[lead] == static_cast<float>(level));
    const double fastRelease = releaseLengthSamples(
        fastRise.envelope, lead + hold, static_cast<double>(fastRise.envelope[lead + hold - 1]));
    INFO("instant-attack release " << fastRelease << " want " << wantRelease);
    CHECK(std::abs(fastRelease - wantRelease) <= 0.05 * wantRelease);

    dsp::LevelDetector instantRelease(5.0, 20.0, 0.0);
    instantRelease.prepare(rate, 1);
    const Series fastFall = runInChunks(instantRelease, stream, 1);
    CHECK(fastFall.envelope[lead + hold] == 0.0f);
    const double wantTwenty = 20.0 * rate / 1000.0;
    const double riseTwenty = attackLengthSamples(fastFall.envelope, lead, level);
    INFO("20 ms attack measured " << riseTwenty << " want " << wantTwenty);
    CHECK(std::abs(riseTwenty - wantTwenty) <= 0.05 * wantTwenty);
}

TEST_CASE("the level detector's readings never depend on how the stream is chunked")
{
    /* Criterion 4, and the one that says the filtering runs per sample. The reference is the stream
       delivered one sample at a time; every other chunking is compared against it, bit for bit, at the
       positions it stopped at. */
    const double rate = 48000.0;
    const Samples stream = mixedStream(30000, rate);

    dsp::LevelDetector reference(20.0, 5.0, 60.0);
    reference.prepare(rate, 1);
    const Series baseline = runInChunks(reference, stream, 1);
    REQUIRE(baseline.rms.size() == stream.size());

    for (const std::size_t chunk : {7u, 128u, 4096u})
    {
        dsp::LevelDetector detector(20.0, 5.0, 60.0);
        detector.prepare(rate, 1);
        const Series run = runInChunks(detector, stream, chunk);

        double worst = 0.0;
        std::size_t differing = 0;
        compareAgainstReference(baseline, run, worst, differing);

        log() << "criterion 4  chunk " << chunk << ": " << run.rms.size() << " stops, " << differing
              << " differ from the per-sample reference by any bit (worst " << worst << ")\n";
        CHECK(differing == 0);
        CHECK(worst == 0.0);
    }

    std::mt19937 generator(20261030U);
    std::uniform_int_distribution<int> spread(1, 900);
    std::vector<std::size_t> chunks;
    for (std::size_t planned = 0; planned < stream.size();)
    {
        const std::size_t chunk = static_cast<std::size_t>(spread(generator));
        chunks.push_back(chunk);
        planned += chunk;
    }

    dsp::LevelDetector detector(20.0, 5.0, 60.0);
    detector.prepare(rate, 1);
    const Series scrambled = runWithChunkSizes(detector, stream, chunks);
    REQUIRE(scrambled.rms.size() > stream.size() / 1000);

    double worst = 0.0;
    std::size_t differing = 0;
    compareAgainstReference(baseline, scrambled, worst, differing);
    log() << "criterion 4  seeded random chunk sizes: " << scrambled.rms.size() << " stops, "
          << differing << " differ by any bit (worst " << worst << ")\n";
    CHECK(differing == 0);
    CHECK(worst == 0.0);
}

TEST_CASE("the level detector's milliseconds mean the same thing at every sample rate")
{
    /* Criterion 5. The transient is measured in milliseconds at four rates, so a time constant left in
       samples — 1200 of them at 48 kHz, which is 10.9 ms at 44.1 and 6.25 ms at 192 — cannot pass, and
       neither can a window derived from a rate rather than from integrationMs. */
    const double attackMs = 25.0;
    const double releaseMs = 75.0;
    const double integrationMs = 12.0;
    const double level = 0.4;

    for (const double rate : kRates)
    {
        dsp::LevelDetector detector(integrationMs, attackMs, releaseMs);
        detector.prepare(rate, 1);

        const std::size_t lead = 8;
        const std::size_t hold = static_cast<std::size_t>(rate);
        const std::size_t tail = 2 * static_cast<std::size_t>(rate);
        Samples stream(lead + hold + tail, 0.0f);
        for (std::size_t i = lead; i < lead + hold; ++i)
            stream[i] = static_cast<float>(level);

        const Series readings = runInChunks(detector, stream, 1);
        const std::vector<float>& envelope = readings.envelope;
        const double settled = static_cast<double>(envelope[lead + hold - 1]);

        const double attack = attackLengthSamples(envelope, lead, settled) * 1000.0 / rate;
        const double release = releaseLengthSamples(envelope, lead + hold, settled) * 1000.0 / rate;
        const double windowMs = static_cast<double>(detector.windowSamples()) * 1000.0 / rate;

        log() << "criterion 5  " << rate << " Hz: attack " << attack << " ms, release " << release
              << " ms, window " << windowMs << " ms, settled " << settled << "\n";

        INFO("rate " << rate << " attack " << attack << " ms want " << attackMs);
        CHECK(std::abs(attack - attackMs) <= 0.01 * attackMs);
        INFO("rate " << rate << " release " << release << " ms want " << releaseMs);
        CHECK(std::abs(release - releaseMs) <= 0.01 * releaseMs);
        INFO("rate " << rate << " window " << windowMs << " ms want " << integrationMs);
        CHECK(std::abs(windowMs - integrationMs) <= 0.01 * integrationMs);
        INFO("rate " << rate << " settled " << settled << " want " << level);
        CHECK(std::abs(settled - level) <= 0.001 * level);
    }
}

TEST_CASE("the level detector reads silence as exact zero and leaves no state behind")
{
    /* Criterion 6. The windowed readings are exact on the sample the window drains — that count is
       windowSamples(), no margin — and the envelope on the count silenceSamples() publishes. Every
       reading on the way down is either exactly zero or a normal float: nothing here may sit in the
       subnormal range, which is this class's half of §7's "no denormal CPU spikes". */
    const double rate = 48000.0;
    const double level = 0.5;

    dsp::LevelDetector cold(50.0, 10.0, 100.0);
    cold.prepare(rate, 2);
    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        CHECK(cold.windowedRms(channel) == 0.0f);
        CHECK(cold.windowedPeak(channel) == 0.0f);
        CHECK(cold.envelope(channel) == 0.0f);
    }

    dsp::LevelDetector detector(50.0, 10.0, 100.0);
    detector.prepare(rate, 1);
    const std::size_t window = detector.windowSamples();
    const Samples drive(static_cast<std::size_t>(rate), static_cast<float>(level));
    detector.processChannel(0, drive.data(), drive.size());

    const std::size_t bound = detector.silenceSamples(level);
    log() << "criterion 6  window " << window << " samples, envelope flush bound " << bound
          << " samples (" << static_cast<double>(bound) / rate << " s) at release 100 ms\n";
    REQUIRE(bound > window);

    const std::size_t silentSamples = bound + window;
    bool monotone = true;
    bool clean = true;
    float previous[3] = {std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::infinity()};

    for (std::size_t i = 0; i < silentSamples; ++i)
    {
        const float silent = 0.0f;
        detector.processChannel(0, &silent, 1);
        const float reading[3]
            = {detector.windowedRms(0), detector.windowedPeak(0), detector.envelope(0)};

        for (std::size_t j = 0; j < 3; ++j)
        {
            const int category = std::fpclassify(reading[j]);
            if (category == FP_SUBNORMAL || category == FP_NAN || category == FP_INFINITE)
                clean = false;
            if (reading[j] > previous[j])
                monotone = false;
            previous[j] = reading[j];
        }

        INFO("i=" << i);
        if (i + 1 == window)
        {
            CHECK(reading[0] == 0.0f);
            CHECK(reading[1] == 0.0f);
        }
        if (i + 1 == bound)
            CHECK(reading[2] == 0.0f);
        if (i + 1 > window)
        {
            CHECK(reading[0] == 0.0f);
            CHECK(reading[1] == 0.0f);
        }
        if (i + 1 > bound)
            CHECK(reading[2] == 0.0f);
    }

    CHECK(monotone);
    CHECK(clean);
}

TEST_CASE("the level detector stays finite and inside the input's own peak on pathological input")
{
    /* Criterion 7. Each case carries its own peak and every reading at every sample has to sit inside
       it, which is also what rules a runaway out: the bound is the input's own peak, so a recursion
       that grew would leave it. The quarter-by-quarter comparison is the "no oscillation growth" half
       stated on its own. */
    const double rate = 48000.0;
    const std::size_t count = 40000;

    struct Case
    {
        const char* name;
        Samples stream;
        double peak;
    };

    std::vector<Case> cases;
    {
        Samples square(count);
        for (std::size_t i = 0; i < count; ++i)
            square[i] = (i / 37) % 2 == 0 ? 1.0f : -1.0f;
        cases.push_back(Case{"full-scale square", std::move(square), 1.0});
    }
    cases.push_back(Case{"full-scale DC", Samples(count, 1.0f), 1.0});
    {
        Samples noise(count);
        std::mt19937 generator(20261030U);
        std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
        float peak = 0.0f;
        for (auto& sample : noise)
        {
            sample = spread(generator);
            peak = std::max(peak, std::abs(sample));
        }
        cases.push_back(Case{"full-scale noise", std::move(noise), static_cast<double>(peak)});
    }
    {
        Samples impulses(count, 0.0f);
        for (std::size_t i = 13; i < count; i += 97)
            impulses[i] = i % 194 == 13 ? 1.0e30f : -1.0e30f;
        cases.push_back(Case{"1e30 impulse train", std::move(impulses), 1.0e30});
    }

    for (const Case& testCase : cases)
    {
        dsp::LevelDetector detector(25.0, 5.0, 50.0);
        detector.prepare(rate, 1);
        const Series readings = runInChunks(detector, testCase.stream, 1);

        double worstRms = 0.0;
        double worstPeak = 0.0;
        double worstEnvelope = 0.0;
        bool finite = true;
        for (std::size_t i = 0; i < readings.rms.size(); ++i)
        {
            const double rms = readings.rms[i];
            const double peak = readings.peak[i];
            const double envelope = readings.envelope[i];
            if (!std::isfinite(rms) || !std::isfinite(peak) || !std::isfinite(envelope))
            {
                finite = false;
                break;
            }
            worstRms = std::max(worstRms, rms);
            worstPeak = std::max(worstPeak, peak);
            worstEnvelope = std::max(worstEnvelope, envelope);
        }

        const std::size_t quarter = readings.rms.size() / 4;
        double middle = 0.0;
        double last = 0.0;
        for (std::size_t i = quarter; i < 2 * quarter; ++i)
            middle = std::max({middle, static_cast<double>(readings.rms[i]),
                               static_cast<double>(readings.envelope[i])});
        for (std::size_t i = 3 * quarter; i < readings.rms.size(); ++i)
            last = std::max({last, static_cast<double>(readings.rms[i]),
                             static_cast<double>(readings.envelope[i])});

        log() << "criterion 7  " << testCase.name << ": peak " << testCase.peak << ", worst rms "
              << worstRms << ", worst peak " << worstPeak << ", worst envelope " << worstEnvelope
              << "; second quarter " << middle << ", last quarter " << last << "\n";

        INFO(testCase.name);
        CHECK(finite);
        CHECK(worstRms <= testCase.peak * 1.000001);
        CHECK(worstPeak <= testCase.peak * 1.000001);
        CHECK(worstEnvelope <= testCase.peak * 1.000001);
        CHECK(last <= middle * 1.0001 + 1.0e-30);
    }
}

TEST_CASE("the level detector keeps one channel's state away from another's")
{
    /* Criterion 8. Channel 1 is fed silence and channel 2 is never driven at all, so a state shared
       with the driven channel would show up as a non-zero idle reading; and channel 0 has to read
       exactly what it reads on its own, in either channel order, which is where a shared accumulator
       would show up as a difference in the driven stream itself. */
    const double rate = 48000.0;
    const Samples stream = mixedStream(20000, rate);
    const Samples silence(stream.size(), 0.0f);

    dsp::LevelDetector alone(30.0, 8.0, 80.0);
    alone.prepare(rate, 1);
    const Series reference = runInChunks(alone, stream, 1);

    dsp::LevelDetector together(30.0, 8.0, 80.0);
    together.prepare(rate, 3);
    dsp::LevelDetector loudNeighbours(30.0, 8.0, 80.0);
    loudNeighbours.prepare(rate, 3);

    bool idleStaysZero = true;
    Series firstDriven;
    Series lastDriven;
    for (std::size_t i = 0; i < stream.size(); ++i)
    {
        together.processChannel(0, stream.data() + i, 1);
        together.processChannel(1, silence.data() + i, 1);

        loudNeighbours.processChannel(2, stream.data() + i, 1);
        loudNeighbours.processChannel(1, stream.data() + i, 1);
        loudNeighbours.processChannel(0, stream.data() + i, 1);

        for (const std::size_t idle : {1u, 2u})
        {
            if (together.windowedRms(idle) != 0.0f || together.windowedPeak(idle) != 0.0f
                || together.envelope(idle) != 0.0f)
                idleStaysZero = false;
        }

        firstDriven.rms.push_back(together.windowedRms(0));
        firstDriven.peak.push_back(together.windowedPeak(0));
        firstDriven.envelope.push_back(together.envelope(0));
        lastDriven.rms.push_back(loudNeighbours.windowedRms(0));
        lastDriven.peak.push_back(loudNeighbours.windowedPeak(0));
        lastDriven.envelope.push_back(loudNeighbours.envelope(0));
    }

    CHECK(idleStaysZero);
    CHECK(together.windowedRms(1) == 0.0f);
    CHECK(together.windowedPeak(1) == 0.0f);
    CHECK(together.envelope(1) == 0.0f);
    CHECK(together.windowedRms(2) == 0.0f);
    CHECK(together.windowedPeak(2) == 0.0f);
    CHECK(together.envelope(2) == 0.0f);

    CHECK(sameBits(firstDriven.rms, reference.rms));
    CHECK(sameBits(firstDriven.peak, reference.peak));
    CHECK(sameBits(firstDriven.envelope, reference.envelope));
    CHECK(sameBits(lastDriven.rms, reference.rms));
    CHECK(sameBits(lastDriven.peak, reference.peak));
    CHECK(sameBits(lastDriven.envelope, reference.envelope));

    log() << "criterion 8  idle channels read exactly 0.0f; the driven channel is bit-identical to a "
             "single-channel run whichever order the channels went in\n";
}

TEST_CASE("the level detector allocates nothing on the audio path and reset gives a fresh object")
{
    /* Criterion 9. The allocation leg shares this binary's single operator new tripwire; the reset leg
       is the operational definition of bit-identical-to-fresh — the same stream through an object that
       was driven and reset reads exactly as the same stream through one that never was. */
    const double rate = 48000.0;
    const Samples stream = mixedStream(20000, rate);

    dsp::LevelDetector detector(20.0, 5.0, 60.0);
    detector.prepare(rate, 2);

    long allocations = 0;
    {
        allocwatch::Watch watch;
        for (std::size_t start = 0; start < stream.size(); start += 96)
        {
            const std::size_t count = std::min<std::size_t>(96, stream.size() - start);
            detector.processChannel(0, stream.data() + start, count);
            detector.processChannel(1, stream.data() + start, count);
            static_cast<void>(detector.windowedRms(0));
            static_cast<void>(detector.windowedPeak(0));
            static_cast<void>(detector.envelope(0));
            static_cast<void>(detector.silenceSamples(1.0));
        }
        allocations = allocwatch::allocations;
    }
    CHECK(allocations == 0);

    dsp::LevelDetector fresh(20.0, 5.0, 60.0);
    fresh.prepare(rate, 1);
    const Series reference = runInChunks(fresh, stream, 1);

    dsp::LevelDetector reused(20.0, 5.0, 60.0);
    reused.prepare(rate, 1);
    reused.processChannel(0, stream.data(), 7000);
    reused.reset();
    const Series again = runInChunks(reused, stream, 1);

    CHECK(sameBits(again.rms, reference.rms));
    CHECK(sameBits(again.peak, reference.peak));
    CHECK(sameBits(again.envelope, reference.envelope));

    log() << "criterion 9  " << allocations << " allocations over " << stream.size()
          << " samples on 2 channels; reset() reproduced a fresh object bit for bit\n";
}

TEST_CASE("the level detector is deterministic and a 60 s run does not drift")
{
    /* Criterion 10, and the leg a cheap sliding RMS loses: a running sum that never subtracts the
       samples that left the window keeps reporting a window it no longer holds. The reading is
       therefore compared against the same window recomputed from the samples themselves, and against
       what the analytic answer was an hour of samples earlier. */
    const double rate = 48000.0;
    const double amplitude = 0.5;
    const double frequency = 997.0;
    const std::size_t count = static_cast<std::size_t>(60 * rate);

    const Samples stream = sine(count, amplitude, frequency, rate);
    const double want = amplitude * kInvRootTwo;

    const auto run = [rate, &stream] {
        dsp::LevelDetector detector(50.0, 50.0, 50.0);
        detector.prepare(rate, 1);
        Series out;
        for (std::size_t i = 0; i < stream.size(); ++i)
        {
            detector.processChannel(0, stream.data() + i, 1);
            if (i % 7 == 0 || i + 1 == stream.size())
            {
                out.rms.push_back(detector.windowedRms(0));
                out.peak.push_back(detector.windowedPeak(0));
                out.envelope.push_back(detector.envelope(0));
                out.at.push_back(i + 1);
            }
        }
        return out;
    };

    const Series first = run();
    const Series second = run();
    CHECK(sameBits(first.rms, second.rms));
    CHECK(sameBits(first.peak, second.peak));
    CHECK(sameBits(first.envelope, second.envelope));

    const std::size_t settledAt = static_cast<std::size_t>(rate) / 7 + 1;
    double worstRmsDrift = 0.0;
    double worstEnvelopeDrift = 0.0;
    for (std::size_t k = settledAt; k < first.rms.size(); ++k)
    {
        worstRmsDrift = std::max(worstRmsDrift,
                                 std::abs(static_cast<double>(first.rms[k]) - want) / want);
        worstEnvelopeDrift = std::max(worstEnvelopeDrift,
                                      std::abs(static_cast<double>(first.envelope[k]) - want) / want);
    }

    dsp::LevelDetector finished(50.0, 50.0, 50.0);
    finished.prepare(rate, 1);
    finished.processChannel(0, stream.data(), stream.size());
    const double exactRms = referenceRmsOver(stream, finished.windowSamples());
    const float exactPeak = referencePeakOver(stream, finished.windowSamples());
    const double reportedRms = finished.windowedRms(0);
    const double reportedPeak = finished.windowedPeak(0);

    log() << "criterion 10  60 s: worst RMS deviation " << worstRmsDrift * 100.0 << "%, worst envelope "
          << worstEnvelopeDrift * 100.0 << "%; the same window recomputed from the samples gives "
          << exactRms << " against " << reportedRms << " reported, peak " << exactPeak << " against "
          << reportedPeak << "\n";

    CHECK(worstRmsDrift <= 0.005);
    CHECK(worstEnvelopeDrift <= 0.005);
    INFO("window recomputed from the samples: " << exactRms << " against reported " << reportedRms);
    CHECK(std::abs(reportedRms - exactRms) <= 1.0e-6 * exactRms);
    CHECK(reportedPeak == exactPeak);
}
