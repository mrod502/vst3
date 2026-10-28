#include "AllocationWatch.hpp"

#include <Dft.hpp>
#include <GrainHistory.hpp>
#include <Window.hpp>

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

std::vector<float> ramp(int count, int offset)
{
    std::vector<float> out(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        out[static_cast<std::size_t>(i)] =
            static_cast<float>((offset + i) % 97) - 48.0f;
    return out;
}

std::vector<float> noise(int count, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
    std::vector<float> out(static_cast<std::size_t>(count));
    for (float& sample : out)
        sample = spread(rng);
    return out;
}

/** window[i] · x[t-L+i], with x below zero reading as silence. Accumulated from the same
    float table the extractor uses, so the comparison is exact rather than tolerant. */
std::vector<float> expectedGrain(const grain::GrainWindow& window,
                                 const std::vector<float>& stream,
                                 std::int64_t written)
{
    const int length = window.length();
    std::vector<float> expect(static_cast<std::size_t>(length));
    for (int i = 0; i < length; ++i)
    {
        const std::int64_t source = written - length + i;
        const float sample =
            source < 0 ? 0.0f : stream[static_cast<std::size_t>(source)];
        expect[static_cast<std::size_t>(i)] =
            window[i] * sample;
    }
    return expect;
}

void expectGrainMatches(const grain::GrainWindow& window,
                        const std::vector<float>& stream,
                        std::int64_t written,
                        const std::vector<float>& grain)
{
    const auto expect = expectedGrain(window, stream, written);
    for (int i = 0; i < window.length(); ++i)
    {
        INFO("t=" << written << " i=" << i << " got=" << grain[static_cast<std::size_t>(i)]
                  << " want=" << expect[static_cast<std::size_t>(i)]);
        CHECK(grain[static_cast<std::size_t>(i)] == expect[static_cast<std::size_t>(i)]);
    }
}
}

TEST_CASE("GrainHistory extraction equals window times the last L samples, ramp input",
          "[grainHistory]")
{
    const int length = 128;
    const int hop = 32;
    const int sizeF = 256;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);

    grain::GrainHistory history;
    history.prepare(length, hop, sizeF);

    std::vector<float> stream;
    std::vector<float> dst(static_cast<std::size_t>(sizeF), kPoison);

    for (int block = 0; block < 40; ++block)
    {
        const auto next = ramp(hop, block * hop);
        history.push(next.data(), hop);
        for (int i = 0; i < hop; ++i)
            stream.push_back(next[static_cast<std::size_t>(i)]);

        history.extractWindowed(window.data(), dst.data());
        expectGrainMatches(window, stream, history.written(), dst);
    }
}

TEST_CASE("GrainHistory extraction matches on random input through many ring wraps",
          "[grainHistory]")
{
    struct Case
    {
        int length;
        int hop;
    };

    for (const Case c : {Case{64, 32}, Case{64, 16}, Case{64, 8}, Case{250, 50},
                         Case{256, 64}, Case{1000, 250}, Case{2048, 512}})
    {
        int sizeFFft = 1;
        while (sizeFFft < 2 * c.length - 1)
            sizeFFft <<= 1;

        const grain::GrainWindow window(c.length, grain::GrainWindow::Kind::PeriodicHann);
        grain::GrainHistory history;
        history.prepare(c.length, c.hop, sizeFFft);

        const auto source = noise(c.hop * 300, 20261027U);
        std::vector<float> stream;
        std::vector<float> dst(static_cast<std::size_t>(sizeFFft), kPoison);

        for (int block = 0; block < 300; ++block)
        {
            history.push(source.data() + static_cast<std::size_t>(block) * c.hop, c.hop);
            for (int i = 0; i < c.hop; ++i)
                stream.push_back(source[static_cast<std::size_t>(block * c.hop + i)]);

            history.extractWindowed(window.data(), dst.data());
            INFO("L=" << c.length << " H=" << c.hop << " block=" << block);
            expectGrainMatches(window, stream, history.written(), dst);
        }
    }
}

TEST_CASE("GrainHistory startup grains read negative history as silence, unpoisoned",
          "[grainHistory]")
{
    const int length = 64;
    const int hop = 16;
    const int sizeF = 128;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);

    grain::GrainHistory history;
    history.prepare(length, hop, sizeF);

    std::vector<float> stream;
    std::vector<float> dst(static_cast<std::size_t>(sizeF), kPoison);

    ///< t = H gives grain start H-L < 0: the whole window must come out as the zero prefix.
    for (int block = 0; block * hop < length + hop; ++block)
    {
        const auto next = ramp(hop, block * hop);
        history.push(next.data(), hop);
        for (int i = 0; i < hop; ++i)
            stream.push_back(next[static_cast<std::size_t>(i)]);

        history.extractWindowed(window.data(), dst.data());
        INFO("t=" << history.written());
        expectGrainMatches(window, stream, history.written(), dst);
        for (int i = 0; i < length - static_cast<int>(history.written()); ++i)
            CHECK(dst[static_cast<std::size_t>(i)] == 0.0f);
    }
}

TEST_CASE("GrainHistory padding tail is exactly zero, including after a transform clobbers it",
          "[grainHistory]")
{
    const int length = 64;
    const int hop = 16;
    const int sizeF = 256;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);
    const fft::Dft dft(sizeF);

    grain::GrainHistory history;
    history.prepare(length, hop, sizeF);

    std::vector<float> dst(static_cast<std::size_t>(sizeF), kPoison);
    std::vector<float> bins(static_cast<std::size_t>(sizeF + 2), kPoison);
    const auto source = ramp(hop, 5);

    for (int block = 0; block < 30; ++block)
    {
        history.push(source.data(), hop);
        history.extractWindowed(window.data(), dst.data());

        for (int i = length; i < sizeF; ++i)
        {
            INFO("block=" << block << " pad i=" << i);
            CHECK(dst[static_cast<std::size_t>(i)] == 0.0f);
        }

        // An in-place backend overwrites the padding it is handed; the next extraction must
        // re-zero rather than inherit it.
        std::fill_n(dst.begin() + length, sizeF - length, kPoison);
        dft.forward(dst.data(), bins.data());
        history.extractWindowed(window.data(), dst.data());
        for (int i = length; i < sizeF; ++i)
        {
            INFO("after forward, block=" << block << " pad i=" << i);
            CHECK(dst[static_cast<std::size_t>(i)] == 0.0f);
        }
    }
}

TEST_CASE("GrainHistory reset restarts the grain exactly as a fresh instance",
          "[grainHistory]")
{
    const int length = 64;
    const int hop = 16;
    const int sizeF = 128;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);

    const auto source = noise(hop * 12, 7U);
    auto run = [&](bool reuse) {
        grain::GrainHistory history;
        history.prepare(length, hop, sizeF);
        if (reuse)
        {
            for (int block = 0; block < 9; ++block)
                history.push(source.data() + static_cast<std::size_t>(block) * hop, hop);
            history.reset();
        }
        std::vector<std::vector<float>> grains;
        for (int block = 0; block < 12; ++block)
        {
            history.push(source.data() + static_cast<std::size_t>(block) * hop, hop);
            std::vector<float> dst(static_cast<std::size_t>(sizeF), kPoison);
            history.extractWindowed(window.data(), dst.data());
            grains.push_back(std::move(dst));
        }
        return grains;
    };

    const auto fresh = run(false);
    const auto reused = run(true);
    REQUIRE(fresh.size() == reused.size());
    for (std::size_t g = 0; g < fresh.size(); ++g)
        for (std::size_t i = 0; i < fresh[g].size(); ++i)
        {
            INFO("grain=" << g << " i=" << i);
            CHECK(fresh[g][i] == reused[g][i]);
        }
}

TEST_CASE("GrainHistory push and extractWindowed allocate nothing", "[grainHistory]")
{
    const int length = 2048;
    const int hop = 512;
    const int sizeF = 4096;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);

    grain::GrainHistory history;
    history.prepare(length, hop, sizeF);

    std::vector<float> dst(static_cast<std::size_t>(sizeF), 0.0f);
    std::vector<float> source(static_cast<std::size_t>(hop), 0.25f);

    for (int warmUp = 0; warmUp < 4; ++warmUp)
    {
        history.push(source.data(), hop);
        history.extractWindowed(window.data(), dst.data());
    }

    long observed = 0;
    {
        AllocationWatch watch;
        for (int block = 0; block < 200; ++block)
        {
            history.push(source.data(), hop);
            history.extractWindowed(window.data(), dst.data());
        }
        observed = alloc::count();
    }
    CHECK(observed == 0);
}
