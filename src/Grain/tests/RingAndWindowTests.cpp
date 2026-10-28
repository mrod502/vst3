#include <RingSpan.hpp>
#include <Window.hpp>

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace
{
/** The same span computed the slow way, slot by slot, with no wrap logic to trust. */
std::vector<std::size_t> naiveSlots(std::size_t capacity, std::int64_t first, int count)
{
    std::vector<std::size_t> slots;
    const std::int64_t mask = static_cast<std::int64_t>(capacity - 1);
    for (int i = 0; i < count; ++i)
        slots.push_back(
            static_cast<std::size_t>((first + i) & mask));
    return slots;
}

void expectSegmentsMatchNaive(std::size_t capacity, std::int64_t first, int count)
{
    const grain::RingSegments span = grain::segmentsFor(capacity, first, count);

    REQUIRE(span.firstCount + span.secondCount == count);
    REQUIRE(span.firstOffset < capacity);
    REQUIRE(span.firstCount + span.secondCount
            <= static_cast<int>(capacity)); ///< no slot visited twice

    const auto expected = naiveSlots(capacity, first, count);
    std::vector<std::size_t> actual;
    for (int i = 0; i < span.firstCount; ++i)
        actual.push_back(span.firstOffset + static_cast<std::size_t>(i));
    for (int i = 0; i < span.secondCount; ++i)
        actual.push_back(static_cast<std::size_t>(i));

    INFO("capacity=" << capacity << " first=" << first << " count=" << count);
    CHECK(actual == expected);
}
}

TEST_CASE("RingSpan covers every index the slow way, across many wraps", "[ring]")
{
    std::mt19937 rng(20261027U);
    for (const std::size_t capacity : {2u, 8u, 64u, 1024u})
    {
        for (int trial = 0; trial < 400; ++trial)
        {
            std::uniform_int_distribution<std::int64_t> start(
                0, static_cast<std::int64_t>(capacity) * 7);
            std::uniform_int_distribution<int> count(
                0, static_cast<int>(capacity));
            expectSegmentsMatchNaive(capacity, start(rng), count(rng));
        }
    }
}

TEST_CASE("RingSpan is correct at the 2^31 and 2^32 boundaries a 32-bit index would break on",
          "[ring]")
{
    const std::int64_t bases[] = {
        (static_cast<std::int64_t>(1) << 31) - 3,
        static_cast<std::int64_t>(1) << 31,
        (static_cast<std::int64_t>(1) << 32) - 3,
        static_cast<std::int64_t>(1) << 32,
        (static_cast<std::int64_t>(1) << 62) - 3,
    };

    for (const std::int64_t base : bases)
        for (const std::size_t capacity : {8u, 256u, 4096u})
            for (int count = 0; count <= static_cast<int>(capacity); count += 7)
                expectSegmentsMatchNaive(capacity, base, count);
}

TEST_CASE("RingSpan never revisits a slot at the full-capacity span", "[ring]")
{
    for (const std::size_t capacity : {2u, 16u, 512u})
        for (std::int64_t first = 0; first < static_cast<std::int64_t>(capacity) * 2; ++first)
            expectSegmentsMatchNaive(capacity, first, static_cast<int>(capacity));
}

TEST_CASE("Window COLA sum is L/(2H) at every hop phase", "[window]")
{
    constexpr double kTolerance = 1e-6;
    const int length = 512;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);

    for (const int hop : {length / 2, length / 4, length / 8})
    {
        const double ideal = grain::idealColaSum(length, hop);
        for (int phase = 0; phase < hop; ++phase)
        {
            const double sum = grain::colaSumAtPhase(window, hop, phase);
            INFO("hop=" << hop << " phase=" << phase << " sum=" << sum << " ideal=" << ideal);
            CHECK(std::abs(sum - ideal) < kTolerance);
        }
    }
}

TEST_CASE("Window COLA holds at the power-of-two sizes the engine prepares with", "[window]")
{
    constexpr double kTolerance = 1e-6;

    for (const int length : {64, 256, 2048, 4096})
        for (const int divisor : {2, 4, 8})
        {
            const int hop = length / divisor;
            const grain::GrainWindow window(length,
                                            grain::GrainWindow::Kind::PeriodicHann);
            const double ideal = grain::idealColaSum(length, hop);
            for (int phase = 0; phase < hop; ++phase)
            {
                const double sum = grain::colaSumAtPhase(window, hop, phase);
                INFO("L=" << length << " hop=" << hop << " phase=" << phase);
                CHECK(std::abs(sum - ideal) < kTolerance);
            }
        }
}

TEST_CASE("A symmetric Hann is not COLA: the same check must reject it", "[window]")
{
    // Negative control. Denominator L-1 instead of L leaves the hop-phase sum off by roughly
    // 1/(2H), which is what makes event gain depend on grid phase -- the reason M2 switched
    // windows. If this test ever goes green, the COLA check above is measuring nothing.
    const int length = 512;
    std::vector<float> symmetric(static_cast<std::size_t>(length));
    for (int i = 0; i < length; ++i)
        symmetric[static_cast<std::size_t>(i)] = static_cast<float>(
            0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i)
                                  / static_cast<double>(length - 1))));

    for (const int hop : {length / 2, length / 4, length / 8})
    {
        const double ideal = grain::idealColaSum(length, hop);
        double worst = 0.0;
        for (int phase = 0; phase < hop; ++phase)
        {
            double sum = 0.0;
            for (int i = phase; i < length; i += hop)
                sum += static_cast<double>(symmetric[static_cast<std::size_t>(i)]);
            worst = std::max(worst, std::abs(sum - ideal));
        }
        INFO("hop=" << hop << " worst deviation=" << worst);
        CHECK(worst > 1e-3);
    }
}

TEST_CASE("Window is periodic Hann, not a shifted or symmetric variant", "[window]")
{
    const int length = 256;
    const grain::GrainWindow window(length, grain::GrainWindow::Kind::PeriodicHann);

    CHECK(window[0] == 0.0f);
    CHECK(std::abs(window[length / 4] - 0.5f) < 1e-6f);
    CHECK(std::abs(window[length / 2] - 1.0f) < 1e-6f);
    for (int i = 0; i < length; ++i)
    {
        const double ideal = 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846
                                                   * static_cast<double>(i)
                                                   / static_cast<double>(length)));
        INFO("i=" << i);
        CHECK(std::abs(static_cast<double>(window[i]) - ideal) < 1e-7);
    }
}
