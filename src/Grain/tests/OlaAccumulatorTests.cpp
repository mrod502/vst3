#include "AllocationWatch.hpp"

#include <OlaAccumulator.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <random>
#include <vector>

namespace
{
constexpr float kPoison = std::numeric_limits<float>::quiet_NaN();

int nextPow2(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

/** The accumulator's own arithmetic, written out with no ring: grain k (1-based) lands at
    absolute output position k*H, so out[q] = sum_k scale · y_k[q − k·H].

    Contributions are summed in hop order from 0.0f, the same sequence the ring produces, so
    the comparison below is bit-exact rather than tolerant. */
std::vector<float> naiveOverlapAdd(const std::vector<std::vector<float>>& grains, int hop,
                                   int length, int streamLength, float scale)
{
    const int live = 2 * length - 1;
    std::vector<float> out(static_cast<std::size_t>(streamLength), 0.0f);
    for (std::size_t k = 0; k < grains.size(); ++k)
    {
        const std::int64_t base = static_cast<std::int64_t>(k + 1) * hop;
        const float* y = grains[k].data();
        for (int j = 0; j < live; ++j)
        {
            const std::int64_t q = base + j;
            if (q >= streamLength)
                break;
            out[static_cast<std::size_t>(q)] += scale * y[static_cast<std::size_t>(j)];
        }
    }
    return out;
}

std::vector<float> noiseLength(int count, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
    std::vector<float> out(static_cast<std::size_t>(count));
    for (float& sample : out)
        sample = spread(rng);
    return out;
}

std::vector<float> poisonedGrain(int length, unsigned seed)
{
    const int sizeF = nextPow2(2 * length - 1);
    auto y = noiseLength(sizeF, seed);
    ///< Only y[0, 2L-1] is valid; the backend leaves junk past the linear convolution length.
    std::fill_n(y.begin() + (2 * length - 1), sizeF - (2 * length - 1), kPoison);
    return y;
}

struct Harness
{
    int length;
    int hop;
    int sizeF;
    grain::OlaAccumulator ola;
};
}

TEST_CASE("OlaAccumulator emits exactly a naive overlap-add, impulse and all-ones grains",
          "[ola]")
{
    struct Case
    {
        int length;
        int hop;
    };

    for (const Case c : {Case{64, 32}, Case{64, 16}, Case{64, 8}, Case{64, 64},
                         Case{256, 128}, Case{256, 64}, Case{256, 32}, Case{255, 85}})
        for (const int hopCount : {1, 3, 17, 200})
    {
        const int sizeF = nextPow2(2 * c.length - 1);
        Harness h{c.length, c.hop, sizeF, {}};
        h.ola.prepare(c.length, c.hop, sizeF);

        std::vector<std::vector<float>> grains;
        for (int k = 0; k < hopCount; ++k)
        {
            if (k % 3 == 0)
            {
                ///< all-ones over the valid span, junk after it
                std::vector<float> y(static_cast<std::size_t>(sizeF), kPoison);
                std::fill_n(y.begin(), 2 * c.length - 1, 1.0f);
                grains.push_back(std::move(y));
            }
            else if (k % 3 == 1)
            {
                std::vector<float> y(static_cast<std::size_t>(sizeF), kPoison);
                std::fill_n(y.begin(), 2 * c.length - 1, 0.0f);
                y[0] = 1.0f;
                y[c.length] = -0.5f;
                grains.push_back(std::move(y));
            }
            else
            {
                grains.push_back(poisonedGrain(c.length, 20261027U + static_cast<unsigned>(k)));
            }
        }

        const float scale = 0.125f;
        std::vector<float> emitted;
        std::vector<float> block(static_cast<std::size_t>(c.hop), kPoison);
        for (int k = 0; k < hopCount; ++k)
        {
            std::fill(block.begin(), block.end(), kPoison);
            h.ola.emitAndAdd(grains[static_cast<std::size_t>(k)].data(), scale, block.data());
            emitted.insert(emitted.end(), block.begin(), block.end());
        }

        const auto ref = naiveOverlapAdd(grains, c.hop, c.length,
                                         static_cast<int>(emitted.size()), scale);
        REQUIRE(emitted.size() == ref.size());
        INFO("L=" << c.length << " H=" << c.hop << " hops=" << hopCount);
        for (std::size_t q = 0; q < emitted.size(); ++q)
        {
            INFO("q=" << q << " got=" << emitted[q] << " want=" << ref[q]);
            CHECK(emitted[q] == ref[q]);
        }
    }
}

TEST_CASE("OlaAccumulator first output hop is the startup prefix and nothing before it leaks",
          "[ola]")
{
    const int length = 64;
    const int hop = 16;
    grain::OlaAccumulator ola;
    ola.prepare(length, hop, nextPow2(2 * length - 1));

    std::vector<float> y(static_cast<std::size_t>(nextPow2(2 * length - 1)), kPoison);
    std::fill_n(y.begin(), 2 * length - 1, 0.0f);
    y[0] = 2.0f;

    std::vector<float> block(static_cast<std::size_t>(hop), kPoison);
    ola.emitAndAdd(y.data(), 1.0f, block.data());
    for (float sample : block)
        CHECK(sample == 0.0f);

    ///< the grain lands at absolute position H, which is exactly the next block.
    ola.emitAndAdd(y.data(), 1.0f, block.data());
    CHECK(block[0] == 2.0f);
    for (int i = 1; i < hop; ++i)
        CHECK(block[static_cast<std::size_t>(i)] == 0.0f);
}

TEST_CASE("OlaAccumulator goes exactly silent ceil((2L-1)/H) hops after one loud grain",
          "[ola]")
{
    struct Case
    {
        int length;
        int hop;
    };

    for (const Case c : {Case{64, 32}, Case{64, 16}, Case{64, 8}, Case{64, 64},
                         Case{256, 64}, Case{255, 85}})
    {
        const int sizeF = nextPow2(2 * c.length - 1);
        grain::OlaAccumulator ola;
        ola.prepare(c.length, c.hop, sizeF);

        std::vector<float> loud(static_cast<std::size_t>(sizeF), kPoison);
        std::fill_n(loud.begin(), 2 * c.length - 1, 3.0f);
        std::vector<float> quiet(static_cast<std::size_t>(sizeF), kPoison);
        std::fill_n(quiet.begin(), 2 * c.length - 1, 0.0f);

        std::vector<float> block(static_cast<std::size_t>(c.hop), kPoison);
        std::vector<bool> blockHasEnergy;

        const int tailHops = (2 * c.length - 1) / c.hop + 4;
        for (int k = 0; k <= tailHops; ++k)
        {
            ola.emitAndAdd(k == 0 ? loud.data() : quiet.data(), 1.0f, block.data());
            bool any = false;
            for (float sample : block)
            {
                CHECK(sample == sample); ///< NaN poison never reaches the output
                any |= (sample != 0.0f);
            }
            blockHasEnergy.push_back(any);
        }

        ///< Call 1 emits the silent startup prefix; energy then spans ceil((2L-1)/H) calls.
        const int firstAllZeroCall = 1 + (2 * c.length - 1 + c.hop - 1) / c.hop;
        INFO("L=" << c.length << " H=" << c.hop << " firstAllZeroCall=" << firstAllZeroCall);
        for (std::size_t call = static_cast<std::size_t>(firstAllZeroCall);
             call < blockHasEnergy.size(); ++call)
            CHECK_FALSE(blockHasEnergy[call]);

        ///< ...and the boundary is tight: the call just before it still carries energy.
        CHECK(blockHasEnergy[static_cast<std::size_t>(firstAllZeroCall) - 1]);
    }
}

TEST_CASE("OlaAccumulator output is linear in scale and the scale is applied exactly once",
          "[ola]")
{
    const int length = 128;
    const int hop = 32;
    const int sizeF = nextPow2(2 * length - 1);

    auto runWith = [&](float scale) {
        grain::OlaAccumulator ola;
        ola.prepare(length, hop, sizeF);
        const auto grainY = poisonedGrain(length, 4242U);
        std::vector<float> out;
        std::vector<float> block(static_cast<std::size_t>(hop), kPoison);
        for (int k = 0; k < 24; ++k)
        {
            ola.emitAndAdd(grainY.data(), scale, block.data());
            out.insert(out.end(), block.begin(), block.end());
        }
        return out;
    };

    const auto unit = runWith(1.0f);
    const auto half = runWith(0.5f);
    const auto zero = runWith(0.0f);

    REQUIRE(unit.size() == half.size());
    for (std::size_t q = 0; q < unit.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(zero[q] == 0.0f);
        ///< scale 0.5 is a power of two, so halving is exact: a double-applied scale is 0.25.
        CHECK(half[q] == unit[q] * 0.5f);
    }
}

TEST_CASE("OlaAccumulator reset gives bit-identical output to a fresh instance", "[ola]")
{
    const int length = 64;
    const int hop = 16;
    const int sizeF = nextPow2(2 * length - 1);
    const auto grains = [&] {
        std::vector<std::vector<float>> g;
        for (int k = 0; k < 30; ++k)
            g.push_back(poisonedGrain(length, 991U + static_cast<unsigned>(k)));
        return g;
    }();

    auto run = [&](bool reuse) {
        grain::OlaAccumulator ola;
        ola.prepare(length, hop, sizeF);
        if (reuse)
        {
            std::vector<float> warm(static_cast<std::size_t>(hop), kPoison);
            for (int k = 0; k < 13; ++k)
                ola.emitAndAdd(grains[static_cast<std::size_t>(k)].data(), 0.75f, warm.data());
            ola.reset();
        }
        std::vector<float> out;
        std::vector<float> block(static_cast<std::size_t>(hop), kPoison);
        for (int k = 0; k < 30; ++k)
        {
            ola.emitAndAdd(grains[static_cast<std::size_t>(k)].data(), 0.75f, block.data());
            out.insert(out.end(), block.begin(), block.end());
        }
        return out;
    };

    const auto fresh = run(false);
    const auto reused = run(true);
    REQUIRE(fresh.size() == reused.size());
    for (std::size_t q = 0; q < fresh.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(fresh[q] == reused[q]);
    }
}

TEST_CASE("OlaAccumulator emits H samples per call and advances the head by exactly H",
          "[ola]")
{
    const int length = 512;
    const int hop = 64;
    grain::OlaAccumulator ola;
    ola.prepare(length, hop, nextPow2(2 * length - 1));

    std::vector<float> y(static_cast<std::size_t>(nextPow2(2 * length - 1)), kPoison);
    std::fill_n(y.begin(), 2 * length - 1, 0.25f);
    std::vector<float> block(static_cast<std::size_t>(hop), kPoison);

    for (int k = 1; k <= 300; ++k) ///< far more than one ring revolution at F = 1024
    {
        std::fill(block.begin(), block.end(), kPoison);
        ola.emitAndAdd(y.data(), 1.0f, block.data());
        INFO("call=" << k);
        for (float sample : block)
            CHECK(sample == sample);
        CHECK(ola.head() == static_cast<std::int64_t>(k) * hop);
    }
}

TEST_CASE("OlaAccumulator allocates nothing in emitAndAdd", "[ola]")
{
    const int length = 2048;
    const int hop = 512;
    grain::OlaAccumulator ola;
    ola.prepare(length, hop, nextPow2(2 * length - 1));

    std::vector<float> y(static_cast<std::size_t>(nextPow2(2 * length - 1)), 0.5f);
    std::vector<float> block(static_cast<std::size_t>(hop), 0.0f);

    for (int warmUp = 0; warmUp < 4; ++warmUp)
        ola.emitAndAdd(y.data(), 0.5f, block.data());

    long observed = 0;
    {
        AllocationWatch watch;
        for (int k = 0; k < 400; ++k)
            ola.emitAndAdd(y.data(), 0.5f, block.data());
        observed = alloc::count();
    }
    CHECK(observed == 0);
}
