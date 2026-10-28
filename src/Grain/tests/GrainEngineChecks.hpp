#pragma once

/** The §6 integration checks, written against the seam rather than a backend.

    Included by two test programs: one driving fft::Dft (the oracle, JUCE-light) and one driving
    fft::JuceFft (the platform transform). Same assertions, both engines, which is what §6.1 asks
    for; a failure names the backend by the test binary it appeared in. */

#include "AllocationWatch.hpp"
#include "GrainEngine.hpp"

#include <HopScheduler.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace graincheck
{
constexpr double kPi = 3.14159265358979323846;

inline int nextPow2(int value) noexcept
{
    int size = 1;
    while (size < value)
        size <<= 1;
    return size;
}

inline engine::EngineConfig configFor(int length, int hop, int latency, int channels = 1)
{
    engine::EngineConfig config;
    config.sampleRate = 48000.0;
    config.hop = hop;
    config.maxChannels = channels;
    config.latencySamples = latency;
    config.grainLength = length;
    return config;
}

inline std::vector<float> noise(std::size_t count, unsigned seed, float amplitude = 0.25f)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-amplitude, amplitude);
    std::vector<float> out(count);
    for (float& sample : out)
        sample = spread(rng);
    return out;
}

inline double hannAt(int i, int length) noexcept
{
    return 0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(i)
                                 / static_cast<double>(length)));
}

inline double sampleAt(const std::vector<float>& x, std::int64_t index) noexcept
{
    if (index < 0 || index >= static_cast<std::int64_t>(x.size()))
        return 0.0;
    return static_cast<double>(x[static_cast<std::size_t>(index)]);
}

inline std::vector<float> runEngine(grain::GrainConvolutionEngine& engine, int hop,
                                    const std::vector<float>& a, const std::vector<float>& b)
{
    const int hops = static_cast<int>(a.size()) / hop;
    std::vector<float> out(static_cast<std::size_t>(hops) * static_cast<std::size_t>(hop), 0.0f);

    const float* aPtr = a.data();
    const float* bPtr = b.data();
    float* outPtr = out.data();
    for (int k = 0; k < hops; ++k)
    {
        engine.processHop(&aPtr, &bPtr, &outPtr, 1);
        aPtr += hop;
        bPtr += hop;
        outPtr += hop;
    }
    return out;
}

/** The chain written out from the plan's definition, in double, with no transform and no ring.

    Grain k completes at T_k = k·H having started at s_k = T_k − L, and its result sample j is
    placed at absolute output T_k + j — so an event at n0 first sounds at n0 + L. Negative starts
    read as silence, which is the entire startup behaviour. Deliberately structured differently
    from the engine, so agreement is evidence rather than tautology. */
inline std::vector<double> naiveWetStream(const std::vector<float>& a, const std::vector<float>& b,
                                          int length, int hop, double scale)
{
    const int total = static_cast<int>(a.size());
    std::vector<double> out(static_cast<std::size_t>(total), 0.0);

    std::vector<double> window(static_cast<std::size_t>(length));
    for (int i = 0; i < length; ++i)
        window[static_cast<std::size_t>(i)] = hannAt(i, length);

    std::vector<double> ga(static_cast<std::size_t>(length));
    std::vector<double> gb(static_cast<std::size_t>(length));

    for (int complete = hop; complete <= total; complete += hop)
    {
        const std::int64_t start = static_cast<std::int64_t>(complete) - length;
        for (int i = 0; i < length; ++i)
        {
            ga[static_cast<std::size_t>(i)] =
                window[static_cast<std::size_t>(i)] * sampleAt(a, start + i);
            gb[static_cast<std::size_t>(i)] =
                window[static_cast<std::size_t>(i)] * sampleAt(b, start + i);
        }

        for (int j = 0; j <= 2 * length - 2; ++j)
        {
            double acc = 0.0;
            const int first = std::max(0, j - length + 1);
            const int last = std::min(length - 1, j);
            for (int i = first; i <= last; ++i)
                acc += ga[static_cast<std::size_t>(i)] * gb[static_cast<std::size_t>(j - i)];

            const std::int64_t at = static_cast<std::int64_t>(complete) + j;
            if (at < total)
                out[static_cast<std::size_t>(at)] += scale * acc;
        }
    }
    return out;
}

inline double peakAbs(const std::vector<double>& v)
{
    double peak = 0.0;
    for (double s : v)
        peak = std::max(peak, std::abs(s));
    return peak;
}

inline double maxAbsDiff(const std::vector<float>& got, const std::vector<double>& want)
{
    double worst = 0.0;
    for (std::size_t q = 0; q < got.size(); ++q)
        worst = std::max(worst, std::abs(static_cast<double>(got[q]) - want[q]));
    return worst;
}

inline double wetScale(int length, int hop) noexcept
{
    return 2.0 * static_cast<double>(hop) / static_cast<double>(length);
}

struct Case
{
    int length;
    int hop;
};

/** Enough hops to cover the negative-start grains, steady state and several ring revolutions. */
inline int hopsFor(const Case& c)
{
    return 2 * c.length / c.hop + 4;
}

/** Delta-probe geometry. n0 on the hop grid and at least one grain into the stream, with the whole
    response plus a hop of silence after it inside the buffer. */
inline int probeStart(const Case& c) noexcept
{
    return 2 * c.length;
}

inline int probeTotal(const Case& c) noexcept
{
    return probeStart(c) + 2 * c.length + 2 * c.hop;
}

/** An IFFT of a spectral product leaves ~1e-8 where the exact result is zero, and the overlap-add
    ring carries that residue onward, so the silence around a delta takes a tolerance. The response
    peak is 1.0, which makes this absolute rather than relative. */
constexpr double kDeltaTolerance = 1e-5;

inline void expectMatchesReference(const grain::TransformFactory& factory, const Case& c,
                                   unsigned seed)
{
    const int total = hopsFor(c) * c.hop;
    const auto a = noise(static_cast<std::size_t>(total), seed);
    const auto b = noise(static_cast<std::size_t>(total), seed + 1U);

    grain::GrainConvolutionEngine engine(factory);
    engine.prepare(configFor(c.length, c.hop, 0));
    CHECK(engine.transformSize() == nextPow2(2 * c.length - 1));

    const auto got = runEngine(engine, c.hop, a, b);
    const auto want = naiveWetStream(a, b, c.length, c.hop, wetScale(c.length, c.hop));

    REQUIRE(got.size() == want.size());
    const double peak = peakAbs(want);
    REQUIRE(peak > 0.0);
    INFO("L=" << c.length << " H=" << c.hop << " peak=" << peak);
    CHECK(maxAbsDiff(got, want) <= 1e-5 * peak);
}

/** The wet stream for A = delta at n0, B = constant 1. */
inline std::vector<float> runDeltaProbe(const grain::TransformFactory& factory, const Case& c,
                                        int n0, int total)
{
    std::vector<float> a(static_cast<std::size_t>(total), 0.0f);
    a[static_cast<std::size_t>(n0)] = 1.0f;
    std::vector<float> b(static_cast<std::size_t>(total), 1.0f);

    grain::GrainConvolutionEngine engine(factory);
    engine.prepare(configFor(c.length, c.hop, 0));
    return runEngine(engine, c.hop, a, b);
}

/** A = delta at n0, B = 1 turns the wet stream into the window itself.

    Sum over the hops whose window covers n0: the weights are w[n0 − s_k] at exactly the hop
    phases, so their sum is the COLA constant C and the 1/C engine gain cancels it, leaving
    out[n0 + L + m] = w(m) — window shape and scale in one assertion. */
inline void expectDeltaIsWindow(const grain::TransformFactory& factory, const Case& c)
{
    const int n0 = probeStart(c);
    const int total = probeTotal(c);
    const auto got = runDeltaProbe(factory, c, n0, total);

    INFO("L=" << c.length << " H=" << c.hop << " n0=" << n0);

    for (int q = 0; q < n0 + c.length; ++q)
        CHECK(std::abs(static_cast<double>(got[static_cast<std::size_t>(q)])) <= kDeltaTolerance);

    double worst = 0.0;
    for (int m = 0; m < c.length; ++m)
    {
        const double have = static_cast<double>(
            got[static_cast<std::size_t>(n0 + c.length + m)]);
        worst = std::max(worst, std::abs(have - hannAt(m, c.length)));
    }
    INFO("worst deviation from the window shape=" << worst);
    CHECK(worst <= kDeltaTolerance);

    for (int m = c.length; m < c.length + c.hop; ++m)
    {
        const double tail = static_cast<double>(got[static_cast<std::size_t>(n0 + c.length + m)]);
        CHECK(std::abs(tail) <= kDeltaTolerance);
    }
}

inline void expectDeltaCentroid(const grain::TransformFactory& factory, const Case& c)
{
    const int n0 = probeStart(c);
    const auto got = runDeltaProbe(factory, c, n0, probeTotal(c));

    double weighted = 0.0;
    double energy = 0.0;
    for (std::size_t q = 0; q < got.size(); ++q)
    {
        const double e = static_cast<double>(got[q]) * static_cast<double>(got[q]);
        weighted += static_cast<double>(q) * e;
        energy += e;
    }
    REQUIRE(energy > 0.0);

    ///< w(m) over m in [0, L) pairs m with L-m, so its centroid is exactly L/2.
    const double want = static_cast<double>(n0) + 1.5 * static_cast<double>(c.length);
    INFO("centroid=" << weighted / energy << " want=" << want);
    CHECK(std::abs(weighted / energy - want) < 0.5);
}

inline void expectLatencyContract(const grain::TransformFactory& factory)
{
    for (const Case c : {Case{256, 64}, Case{1024, 256}})
        for (const int latency : {0, 1, c.hop, c.length / 2, c.length})
        {
            grain::GrainConvolutionEngine engine(factory);
            engine.prepare(configFor(c.length, c.hop, latency));
            INFO("L=" << c.length << " H=" << c.hop << " R=" << latency);
            CHECK(engine.latencySamples() == latency);
            CHECK(engine.wetDelaySamples() == c.length);
        }

    ///< L moves, the declaration does not.
    grain::GrainConvolutionEngine engine(factory);
    for (const int length : {256, 1024, 2048})
    {
        engine.prepare(configFor(length, length / 4, 96));
        INFO("L=" << length);
        CHECK(engine.latencySamples() == 96);
        CHECK(engine.wetDelaySamples() == length);
    }
}

inline void expectPrepareRejectsUnusableConfig(const grain::TransformFactory& factory)
{
    const int length = 512;

    for (const int badLatency : {-1, length + 1, 4 * length})
    {
        grain::GrainConvolutionEngine engine(factory);
        INFO("R=" << badLatency);
        CHECK_THROWS_AS(engine.prepare(configFor(length, length / 4, badLatency)),
                        std::invalid_argument);
    }

    for (const int good : {0, 1, length})
    {
        grain::GrainConvolutionEngine engine(factory);
        INFO("R=" << good);
        CHECK_NOTHROW(engine.prepare(configFor(length, length / 4, good)));
    }

    for (const auto pair : {std::pair<int, int>{512, 100}, {511, 64}, {0, 64}, {64, 0}})
    {
        grain::GrainConvolutionEngine engine(factory);
        INFO("L=" << pair.first << " H=" << pair.second);
        CHECK_THROWS_AS(engine.prepare(configFor(pair.first, pair.second, 0)),
                        std::invalid_argument);
    }
}

/** Output sample q is untouched when every input sample at and after q is changed.

    This is the honest check that a reported latency of 0 is not hiding L samples. The tight bound
    is better than the plan's q < probe: a result sample at q is written by the grain whose hop
    boundary T*(q) = ⌊q/H⌋·H is the last one at or before q, and that grain reads inputs below
    T*(q) only — so the region that must stay identical is q < probe + H for a probe on the hop
    grid, which also proves the delay comes from the hop structure and not from a hidden buffer. */
inline void expectCausalAtZeroLatency(const grain::TransformFactory& factory, const Case& c)
{
    const int total = 4 * c.length;
    const auto a = noise(static_cast<std::size_t>(total), 31337U);
    const auto b = noise(static_cast<std::size_t>(total), 8675309U);

    grain::GrainConvolutionEngine engine(factory);
    engine.prepare(configFor(c.length, c.hop, 0));
    const auto baseline = runEngine(engine, c.hop, a, b);

    ///< Probes on the hop grid, leaving a hop of room for the response to show.
    const int lastProbe = total - c.length - c.hop;
    for (int probe = c.hop; probe <= lastProbe; probe += c.hop)
    {
        auto perturbedA = a;
        auto perturbedB = b;
        for (int q = probe; q < total; ++q)
        {
            perturbedA[static_cast<std::size_t>(q)] = 0.75f;
            perturbedB[static_cast<std::size_t>(q)] = -0.75f;
        }

        grain::GrainConvolutionEngine other(factory);
        other.prepare(configFor(c.length, c.hop, 0));
        const auto otherOut = runEngine(other, c.hop, perturbedA, perturbedB);

        INFO("probe=" << probe);
        REQUIRE(otherOut.size() == baseline.size());
        for (int q = 0; q < probe + c.hop; ++q)
        {
            INFO("q=" << q);
            CHECK(otherOut[static_cast<std::size_t>(q)] == baseline[static_cast<std::size_t>(q)]);
        }

        ///< Not vacuous: somewhere past the protected region the two streams must diverge.
        bool diverged = false;
        for (int q = probe + c.hop; q < total && !diverged; ++q)
        {
            diverged = otherOut[static_cast<std::size_t>(q)]
                       != baseline[static_cast<std::size_t>(q)];
        }
        INFO("diverged after the protected region=" << diverged);
        CHECK(diverged);
    }
}

inline void expectCommutative(const grain::TransformFactory& factory, const Case& c)
{
    const int total = hopsFor(c) * c.hop;
    const auto a = noise(static_cast<std::size_t>(total), 5150U);
    const auto b = noise(static_cast<std::size_t>(total), 5151U);

    grain::GrainConvolutionEngine forward(factory);
    forward.prepare(configFor(c.length, c.hop, 0));
    const auto ab = runEngine(forward, c.hop, a, b);

    grain::GrainConvolutionEngine swapped(factory);
    swapped.prepare(configFor(c.length, c.hop, 0));
    const auto ba = runEngine(swapped, c.hop, b, a);

    REQUIRE(ab.size() == ba.size());
    double peak = 0.0;
    double worst = 0.0;
    for (std::size_t q = 0; q < ab.size(); ++q)
    {
        peak = std::max(peak, std::abs(static_cast<double>(ab[q])));
        worst = std::max(worst, std::abs(static_cast<double>(ab[q]) - static_cast<double>(ba[q])));
    }
    INFO("L=" << c.length << " peak=" << peak << " worst=" << worst);
    CHECK(worst <= 1e-6 * peak);
}

inline void expectResetIsFresh(const grain::TransformFactory& factory, const Case& c)
{
    const int total = hopsFor(c) * c.hop;
    const auto a = noise(static_cast<std::size_t>(total), 6060U);
    const auto b = noise(static_cast<std::size_t>(total), 6061U);

    grain::GrainConvolutionEngine baselineEngine(factory);
    baselineEngine.prepare(configFor(c.length, c.hop, 0));
    const auto baseline = runEngine(baselineEngine, c.hop, a, b);

    grain::GrainConvolutionEngine engine(factory);
    engine.prepare(configFor(c.length, c.hop, 0));
    runEngine(engine, c.hop, noise(static_cast<std::size_t>(total), 999U),
              noise(static_cast<std::size_t>(total), 998U));
    engine.reset();
    const auto afterReset = runEngine(engine, c.hop, a, b);

    REQUIRE(afterReset.size() == baseline.size());
    for (std::size_t q = 0; q < baseline.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(afterReset[q] == baseline[q]);
    }

    ///< A re-prepare is the other way to come back from a different take.
    engine.prepare(configFor(c.length, c.hop, 0));
    const auto afterReprepare = runEngine(engine, c.hop, a, b);
    for (std::size_t q = 0; q < baseline.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(afterReprepare[q] == baseline[q]);
    }
}

inline void expectChannelsIndependent(const grain::TransformFactory& factory, const Case& c)
{
    const int total = hopsFor(c) * c.hop;
    const auto a0 = noise(static_cast<std::size_t>(total), 111U);
    const auto b0 = noise(static_cast<std::size_t>(total), 222U);
    const auto a1 = noise(static_cast<std::size_t>(total), 333U);
    const auto b1 = noise(static_cast<std::size_t>(total), 444U);

    grain::GrainConvolutionEngine stereo(factory);
    stereo.prepare(configFor(c.length, c.hop, 0, 2));

    const int hops = total / c.hop;
    std::vector<float> out0(static_cast<std::size_t>(total), 0.0f);
    std::vector<float> out1(static_cast<std::size_t>(total), 0.0f);
    const float* inA[2];
    const float* inB[2];
    float* outPtr[2];
    for (int k = 0; k < hops; ++k)
    {
        const auto offset = static_cast<std::ptrdiff_t>(k) * c.hop;
        inA[0] = a0.data() + offset;
        inA[1] = a1.data() + offset;
        inB[0] = b0.data() + offset;
        inB[1] = b1.data() + offset;
        outPtr[0] = out0.data() + offset;
        outPtr[1] = out1.data() + offset;
        stereo.processHop(inA, inB, outPtr, 2);
    }

    ///< Each signal alone in its own engine: reusing one would feed channel 1 the history of 0.
    const auto alone0 = [&] {
        grain::GrainConvolutionEngine solo(factory);
        solo.prepare(configFor(c.length, c.hop, 0, 1));
        return runEngine(solo, c.hop, a0, b0);
    }();
    const auto alone1 = [&] {
        grain::GrainConvolutionEngine solo(factory);
        solo.prepare(configFor(c.length, c.hop, 0, 1));
        return runEngine(solo, c.hop, a1, b1);
    }();

    for (std::size_t q = 0; q < out0.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(out0[q] == alone0[q]);
        CHECK(out1[q] == alone1[q]);
    }
}

inline void expectNoAllocationInProcessHop(const grain::TransformFactory& factory, const Case& c)
{
    const int total = hopsFor(c) * c.hop;
    const auto a = noise(static_cast<std::size_t>(total), 1717U);
    const auto b = noise(static_cast<std::size_t>(total), 1718U);

    grain::GrainConvolutionEngine engine(factory);
    engine.prepare(configFor(c.length, c.hop, c.length / 2));

    std::vector<float> out(static_cast<std::size_t>(c.hop), 0.0f);
    const float* aPtr = a.data();
    const float* bPtr = b.data();
    float* outPtr = out.data();
    for (int k = 0; k < 4; ++k)
    {
        engine.processHop(&aPtr, &bPtr, &outPtr, 1);
        aPtr += c.hop;
        bPtr += c.hop;
    }

    long observed = -1;
    {
        AllocationWatch watch;
        for (int k = 4; k < 60; ++k)
        {
            engine.processHop(&aPtr, &bPtr, &outPtr, 1);
            aPtr += c.hop;
            bPtr += c.hop;
        }
        observed = alloc::count();
    }
    CHECK(observed == 0);
}

/** Host callback size must not reach the DSP.

    Driven at R = H rather than 0 because HopScheduler pre-fills the output ring with exactly
    latencySamples and then requires every callback to drain what it asked for (T2): with R = 0
    and hostBlock < H the ring is empty on the first callback. The wet stream is the same shape
    either way; R only shifts where the host cuts it. */
inline void expectBlockSizeInvariance(const grain::TransformFactory& factory, const Case& c)
{
    const int total = 48 * c.hop;
    const auto a = noise(static_cast<std::size_t>(total), 555U);
    const auto b = noise(static_cast<std::size_t>(total), 556U);

    auto runWithBlocks = [&](const std::vector<int>& blocks) {
        int sum = 0;
        int largest = 0;
        for (int block : blocks)
        {
            sum += block;
            largest = std::max(largest, block);
        }
        REQUIRE(sum == total);

        grain::GrainConvolutionEngine engine(factory);
        engine::SchedulerConfig schedulerConfig;
        schedulerConfig.engine = configFor(c.length, c.hop, c.hop);
        schedulerConfig.channels = 1;
        schedulerConfig.sidechainChannels = 1;
        schedulerConfig.maxHostBlock = largest;

        engine::HopScheduler scheduler(engine);
        scheduler.prepare(schedulerConfig);

        std::vector<float> out(static_cast<std::size_t>(total), 0.0f);
        int position = 0;
        for (int block : blocks)
        {
            const float* aIn = a.data() + position;
            const float* bIn = b.data() + position;
            float* outPtr = out.data() + position;
            scheduler.process(&aIn, &bIn, &outPtr, 1, block);
            position += block;
        }
        return out;
    };

    std::vector<float> baseline;
    for (const int hostBlock : {32, 128, 512, 2048})
    {
        std::vector<int> fixedBlocks(static_cast<std::size_t>(total / hostBlock), hostBlock);
        const auto out = runWithBlocks(fixedBlocks);
        if (baseline.empty())
        {
            baseline = out;
            continue;
        }
        INFO("hostBlock=" << hostBlock);
        REQUIRE(out.size() == baseline.size());
        for (std::size_t q = 0; q < out.size(); ++q)
        {
            INFO("q=" << q);
            CHECK(out[q] == baseline[q]);
        }
    }

    std::vector<int> randomBlocks;
    std::mt19937 rng(1234U);
    std::uniform_int_distribution<int> pick(1, 300);
    for (int remaining = total; remaining > 0;)
    {
        const int block = std::min(remaining, std::max(1, pick(rng)));
        randomBlocks.push_back(block);
        remaining -= block;
    }
    const auto out = runWithBlocks(randomBlocks);
    REQUIRE(out.size() == baseline.size());
    for (std::size_t q = 0; q < out.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(out[q] == baseline[q]);
    }
}
}
