#include <TraceRing.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

using container::TraceRing;

namespace
{
/** A payload whose three companion fields are a function of its sequence, so a record that arrives with
    fields from two different writes is caught rather than believed. */
struct Pattern
{
    std::uint64_t seq = 0;
    std::array<std::uint64_t, 3> body {};
};

[[nodiscard]] Pattern makePattern(std::uint64_t seq) noexcept
{
    return {seq, {seq * 3u + 1u, seq ^ 0x5a5a5a5a5a5a5a5aull, seq + 7u}};
}

[[nodiscard]] bool holdsPattern(const Pattern& pattern) noexcept
{
    return pattern.body == makePattern(pattern.seq).body;
}
}

TEST_CASE("trace ring: capacity is rounded up to a power of two", "[trace ring]")
{
    CHECK(TraceRing<Pattern>(0).capacity() == 1);
    CHECK(TraceRing<Pattern>(1).capacity() == 1);
    CHECK(TraceRing<Pattern>(5).capacity() == 8);
    CHECK(TraceRing<Pattern>(750).capacity() == 1024);
}

TEST_CASE("trace ring: the window comes out oldest first, ending on the newest write", "[trace ring]")
{
    TraceRing<Pattern> ring(8);

    for (std::uint64_t i = 0; i < 3; ++i)
        ring.write(makePattern(i));

    std::array<Pattern, 8> out {};
    const auto taken = ring.peekNewestInto({out.data(), out.size()});

    REQUIRE(taken == 3);
    CHECK(out[0].seq == 0);
    CHECK(out[1].seq == 1);
    CHECK(out[2].seq == 2);
}

TEST_CASE("trace ring: peeking consumes nothing, so two readers see the same window", "[trace ring]")
{
    TraceRing<Pattern> ring(8);

    for (std::uint64_t i = 0; i < 4; ++i)
        ring.write(makePattern(i));

    std::array<Pattern, 8> first {};
    std::array<Pattern, 8> second {};
    REQUIRE(ring.peekNewestInto({first.data(), first.size()}) == 4);
    REQUIRE(ring.peekNewestInto({second.data(), second.size()}) == 4);

    for (std::size_t i = 0; i < 4; ++i)
        CHECK(first[i].seq == second[i].seq);

    CHECK(ring.size() == 4);
}

TEST_CASE("trace ring: a drain consumes exactly what it delivered", "[trace ring]")
{
    TraceRing<Pattern> ring(8);

    for (std::uint64_t i = 0; i < 4; ++i)
        ring.write(makePattern(i));

    std::array<Pattern, 2> out {};
    REQUIRE(ring.drainNewestInto({out.data(), out.size()}) == 2);
    CHECK(out[0].seq == 2);
    CHECK(out[1].seq == 3);
    CHECK(ring.size() == 0);

    std::array<Pattern, 8> rest {};
    CHECK(ring.drainNewestInto({rest.data(), rest.size()}) == 0);
}

TEST_CASE("trace ring: an overflow drops the oldest and keeps the newest", "[trace ring]")
{
    TraceRing<Pattern> ring(4);

    for (std::uint64_t i = 0; i < 6; ++i)
        ring.write(makePattern(i));

    CHECK(ring.writes() == 6);
    CHECK(ring.dropped() == 2);
    CHECK(ring.size() == 4);

    std::array<Pattern, 8> out {};
    const auto taken = ring.peekNewestInto({out.data(), out.size()});

    /* The four newest, in order, with the newest of all present: a trace that dropped the latest reading
       would show a level stage that has stopped moving. */
    REQUIRE(taken == 4);
    CHECK(out[0].seq == 2);
    CHECK(out[3].seq == 5);
}

TEST_CASE("trace ring: newest reports the last publication and never a lap of it", "[trace ring]")
{
    TraceRing<Pattern> ring(1);

    Pattern read {};
    CHECK_FALSE(ring.newest(read));

    ring.write(makePattern(11));
    REQUIRE(ring.newest(read));
    CHECK(read.seq == 11);

    ring.write(makePattern(12));
    REQUIRE(ring.newest(read));
    CHECK(read.seq == 12);
    CHECK(holdsPattern(read));
}

TEST_CASE("trace ring: clear drops what is unread", "[trace ring]")
{
    TraceRing<Pattern> ring(8);

    for (std::uint64_t i = 0; i < 5; ++i)
        ring.write(makePattern(i));

    ring.clear();
    CHECK(ring.size() == 0);

    std::array<Pattern, 8> out {};
    CHECK(ring.drainNewestInto({out.data(), out.size()}) == 0);
}

TEST_CASE("trace ring: a racing writer cannot tear a record", "[trace ring]")
{
    /* Capacity 4 against a producer that outruns it on every iteration: the reader is always reading slots
       the writer is lapping, which is the only way to exercise the tag rather than describe it. Fixed
       iteration counts, so the case is deterministic in what it asserts even though the interleaving is
       not — no wall clock, no sleep. */
    constexpr std::uint64_t kWrites = 20000;

    TraceRing<Pattern> ring(4);
    std::atomic<bool> writerFinished {false};
    std::uint64_t validated = 0;
    std::uint64_t torn = 0;
    std::uint64_t seen = 0;
    bool ordered = true;

    std::thread producer(
        [&]
        {
            for (std::uint64_t i = 0; i < kWrites; ++i)
                ring.write(makePattern(i));
            writerFinished.store(true, std::memory_order_release);
        });

    /* Every record delivered must carry its own pattern and arrive after the one before it: the pair is
       criterion 4 and §5 arm 12 in one loop — a reader that walked the ring newest-first, or that copied a
       slot while the writer was inside it, fails here and nowhere else. */
    std::array<Pattern, 4> window {};
    std::uint64_t previous = 0;
    bool havePrevious = false;

    while (!writerFinished.load(std::memory_order_acquire) || ring.size() > 0)
    {
        const auto taken = ring.drainNewestInto({window.data(), window.size()});
        for (std::size_t i = 0; i < taken; ++i)
        {
            if (!holdsPattern(window[i]))
                ++torn;
            else
                ++validated;

            if (havePrevious && window[i].seq <= previous)
                ordered = false;
            previous = window[i].seq;
            havePrevious = true;
            seen = window[i].seq;
        }
    }

    producer.join();

    CHECK(torn == 0);
    CHECK(ordered);
    CHECK(seen == kWrites - 1);
    CHECK(validated > 0);
}

TEST_CASE("trace ring: a capacity-one ring under a producer that outruns it stays whole", "[trace ring]")
{
    /* §5 arm 6's shape: capacity 1 makes every read a race with the next write, so drop-oldest and
       no-torn-read are the only two properties left standing, and both are checkable. */
    constexpr std::uint64_t kWrites = 5000;

    TraceRing<Pattern> ring(1);
    std::atomic<bool> writerFinished {false};
    std::uint64_t torn = 0;
    std::uint64_t delivered = 0;

    std::thread producer(
        [&]
        {
            for (std::uint64_t i = 0; i < kWrites; ++i)
                ring.write(makePattern(i));
            writerFinished.store(true, std::memory_order_release);
        });

    Pattern read {};
    while (!writerFinished.load(std::memory_order_acquire))
    {
        if (ring.newest(read) && !holdsPattern(read))
            ++torn;
        if (ring.newest(read))
            ++delivered;
    }

    producer.join();

    CHECK(torn == 0);
    CHECK(delivered > 0);

    /* The last write is readable once the writer is done: the ring dropped history the reader never got to,
       and never the reading that is currently true. */
    REQUIRE(ring.newest(read));
    CHECK(read.seq == kWrites - 1);
    CHECK(ring.dropped() == kWrites - 1);
}
