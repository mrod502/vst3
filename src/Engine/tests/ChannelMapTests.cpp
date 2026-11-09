#include <ChannelMap.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <random>
#include <span>
#include <vector>

namespace
{
/** resolve must not allocate. Replacing global operator new is allowed once per program, which is why
    this file is its own test binary; the shape follows MixEngineTests.cpp and HopSchedulerTests.cpp. */
bool armAllocations = false;
long allocationCount = 0;
}

void* operator new(std::size_t size)
{
    if (armAllocations)
        ++allocationCount;
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new[](std::size_t size)
{
    if (armAllocations)
        ++allocationCount;
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
struct AllocationWatch
{
    AllocationWatch()
    {
        allocationCount = 0;
        armAllocations = true;
    }
    ~AllocationWatch() { armAllocations = false; }
};
}

namespace
{
using engine::ChannelMap;
using engine::kConsolidatedSource;
using engine::kMaxChannels;
using engine::kSilentSource;
using Result = engine::ResolvedChannelMap;
using Status = engine::ChannelMapStatus;

/** The swept channel counts: 0 (the sidechain absent), the mono and stereo cases the shell ships,
    3/4/6 (multi-channel, including the unequal pairs), and 8/16/17 which straddle the ceiling — 17 in
    particular, because a bound checked with the wrong comparison shows up only past 16. */
constexpr std::array<int, 9> kSweep = {0, 1, 2, 3, 4, 6, 8, 16, 17};
constexpr int kSweepCount = static_cast<int>(kSweep.size());
constexpr int kTripleCount = kSweepCount * kSweepCount * kSweepCount;

/** The documented table, restated row by row from ChannelMap.hpp rather than from resolve's guard chain,
    so a mistake has to be made twice in two different shapes to pass criterion 1. */
struct Expectation
{
    Status status = Status::empty;
    int channels = 0;
    std::array<std::uint16_t, kMaxChannels> a{};
    std::array<std::uint16_t, kMaxChannels> b{};
};

std::uint16_t tableIndex(int sideChannels, int c)
{
    if (sideChannels == 0)
        return kSilentSource;
    if (sideChannels == 1)
        return 0;
    return static_cast<std::uint16_t>(c);
}

/** The table's B column. Its own function rather than a shared one with the A column because the sides
    stopped being interchangeable on D-018: only B folds, and only when the main bus is mono. */
std::uint16_t bTableIndex(int chA, int chB, int c)
{
    if (chA == 1 && chB > 1)
        return kConsolidatedSource;
    return tableIndex(chB, c);
}

/** The largest `channelsOut` the table accepts for a pair — `max(chA, chB)` everywhere except the
    consolidated family, whose single output is what D-018 decides. */
int maximumOutputs(int chA, int chB)
{
    return chA == 1 && chB > 1 ? 1 : std::max(chA, chB);
}

Expectation expect(int chA, int chB, int channelsOut, int capacity = kMaxChannels)
{
    const int sources = std::max(chA, chB);

    if (chA == 0 || channelsOut <= 0)
        return {Status::empty, 0, {}, {}};
    if (chA > 1 && chB > 1 && chA != chB)
        return {Status::asymmetric, 0, {}, {}};
    if (capacity < kMaxChannels ? sources > capacity : sources > kMaxChannels)
        return {Status::exceedsCapacity, 0, {}, {}};
    if (channelsOut > maximumOutputs(chA, chB))
        return {Status::countMismatch, 0, {}, {}};

    Expectation expected{Status::ok, channelsOut, {}, {}};
    for (int c = 0; c < channelsOut; ++c)
    {
        expected.a[static_cast<std::size_t>(c)] = tableIndex(chA, c);
        expected.b[static_cast<std::size_t>(c)] = bTableIndex(chA, chB, c);
    }
    return expected;
}

/** A rejected resolve must leave the map exactly as a default-constructed one: `channels == 0` and every
    index 0. That is what "nothing half-written" means for a value-returning API — a fill that ran before
    the rejection would show either a channel count or an index here. */
bool isPristine(const ChannelMap& map)
{
    if (map.channels != 0)
        return false;
    for (const auto index : map.a)
        if (index != 0)
            return false;
    for (const auto index : map.b)
        if (index != 0)
            return false;
    return true;
}

bool sameIndices(const ChannelMap& actual, const Expectation& expected)
{
    if (actual.channels != expected.channels)
        return false;
    for (int c = 0; c < kMaxChannels; ++c)
    {
        const auto slot = static_cast<std::size_t>(c);
        const auto wantedA = c < expected.channels ? expected.a[slot] : std::uint16_t{0};
        const auto wantedB = c < expected.channels ? expected.b[slot] : std::uint16_t{0};
        if (actual.a[slot] != wantedA || actual.b[slot] != wantedB)
            return false;
    }
    return true;
}

/** Folds a result into a running hash, so the allocation leg can prove the sweep ran without storing
    729 results — and without comparing padding bytes. */
std::uint64_t mix(std::uint64_t hash, const Result& result)
{
    auto fold = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 0x100000001B3ULL;
    };
    fold(static_cast<std::uint64_t>(result.status));
    fold(static_cast<std::uint64_t>(result.map.channels));
    for (const auto index : result.map.a)
        fold(index);
    for (const auto index : result.map.b)
        fold(index);
    return hash;
}

/** Field-by-field rather than memcmp, so a difference is about the rule and not about padding. */
bool sameResult(const Result& left, const Result& right)
{
    if (left.status != right.status || left.map.channels != right.map.channels)
        return false;
    for (int c = 0; c < kMaxChannels; ++c)
    {
        const auto slot = static_cast<std::size_t>(c);
        if (left.map.a[slot] != right.map.a[slot] || left.map.b[slot] != right.map.b[slot])
            return false;
    }
    return true;
}

struct TripleMismatch
{
    int count = 0;
    int chA = -1;
    int chB = -1;
    int out = -1;
    int status = -1;
    int wanted = -1;
};
}

TEST_CASE("channel map: the whole swept triple table resolves to its documented rule")
{
    TripleMismatch mismatch;
    std::array<int, 5> statusCounts = {};

    for (const int chA : kSweep)
        for (const int chB : kSweep)
            for (const int out : kSweep)
            {
                const auto actual = ChannelMap::resolve(chA, chB, out);
                const auto wanted = expect(chA, chB, out);
                ++statusCounts[static_cast<std::size_t>(actual.status)];

                const bool statusOk = actual.status == wanted.status;
                const bool mapOk = statusOk && actual.status == Status::ok
                                       ? sameIndices(actual.map, wanted)
                                       : isPristine(actual.map);
                if (!statusOk || !mapOk)
                {
                    if (mismatch.count == 0)
                    {
                        mismatch.chA = chA;
                        mismatch.chB = chB;
                        mismatch.out = out;
                        mismatch.status = static_cast<int>(actual.status);
                        mismatch.wanted = static_cast<int>(wanted.status);
                    }
                    ++mismatch.count;
                }
            }

    INFO("first mismatch: chA=" << mismatch.chA << " chB=" << mismatch.chB
                                << " out=" << mismatch.out << " status=" << mismatch.status
                                << " wanted=" << mismatch.wanted);
    CHECK(mismatch.count == 0);
    CHECK(kTripleCount == 729);
    /* The sweep has to actually contain each outcome, or the table above could pass by never leaving
       one branch. ok / asymmetric / countMismatch / exceedsCapacity, in that enum order. */
    CHECK(statusCounts[0] > 0);
    CHECK(statusCounts[2] > 0);
    CHECK(statusCounts[3] > 0);
    CHECK(statusCounts[4] > 0);
}

TEST_CASE("channel map: a mono side broadcasts channel 0 and a multi side is the identity")
{
    for (const int chA : kSweep)
        for (const int chB : kSweep)
            for (int out = 1; out <= std::min(std::max(chA, chB), kMaxChannels); ++out)
            {
                const auto resolved = ChannelMap::resolve(chA, chB, out);
                if (resolved.status != Status::ok)
                    continue;
                /* The consolidated family is not this rule: a mono main neither broadcasts nor indexes its
                   sidechain, it folds it, and the marker that says so is the next case's subject. */
                if (chA == 1 && chB > 1)
                    continue;

                for (int c = 0; c < out; ++c)
                {
                    const auto slot = static_cast<std::size_t>(c);
                    if (chA == 1)
                        CHECK(resolved.map.a[slot] == 0);
                    else
                        CHECK(resolved.map.a[slot] == static_cast<std::uint16_t>(c));

                    if (chB == 0)
                        CHECK(resolved.map.b[slot] == kSilentSource);
                    else if (chB == 1)
                        CHECK(resolved.map.b[slot] == 0);
                    else
                        CHECK(resolved.map.b[slot] == static_cast<std::uint16_t>(c));
                }
            }
}

TEST_CASE("channel map: a mono main with a wider sidechain resolves to one consolidated channel")
{
    for (int chB = 2; chB <= kMaxChannels; ++chB)
    {
        const auto resolved = ChannelMap::resolve(1, chB, 1);
        REQUIRE(resolved.status == Status::ok);
        CHECK(resolved.map.channels == 1);
        CHECK(resolved.map.a[0] == 0);
        CHECK(resolved.map.b[0] == kConsolidatedSource);
        /* And not the silent sentinel: a marker that aliases "no channel here" would turn every folded
           sidechain into a missing bus downstream. */
        CHECK(resolved.map.b[0] != kSilentSource);
        CHECK(resolved.map.b[0] != 0);
        /* One output means one slot. Everything above it stays at its value-initialised 0, so no consumer
           can read a fold the map does not claim. */
        for (int slot = 1; slot < kMaxChannels; ++slot)
        {
            CHECK(resolved.map.a[static_cast<std::size_t>(slot)] == 0);
            CHECK(resolved.map.b[static_cast<std::size_t>(slot)] == 0);
        }
    }
}

TEST_CASE("channel map: more than one output in the consolidated family is a count mismatch")
{
    for (int chB = 2; chB <= kMaxChannels; ++chB)
        for (int out = 2; out <= kMaxChannels + 2; ++out)
        {
            const auto resolved = ChannelMap::resolve(1, chB, out);
            CHECK(resolved.status == Status::countMismatch);
            CHECK(isPristine(resolved.map));
        }

    /* The general rule keeps its teeth outside the family, and the ceiling still outranks the count: a bus
       that cannot be represented reports that instead. */
    CHECK(ChannelMap::resolve(1, 1, 2).status == Status::countMismatch);
    CHECK(ChannelMap::resolve(2, 1, 3).status == Status::countMismatch);
    CHECK(ChannelMap::resolve(1, kMaxChannels + 1, 3).status == Status::exceedsCapacity);
}

TEST_CASE("channel map: a mono sidechain or a missing one is never consolidated")
{
    /* The predicate is `chA == 1 && chB > 1`, not `chA == 1`. Written short it folds a mono sidechain — a
       mean of one channel is that channel, so the marker would be a lie — and erases D-002's silence one
       character further along. These are the two neighbours that catch it. */
    const auto mono = ChannelMap::resolve(1, 1, 1);
    REQUIRE(mono.status == Status::ok);
    CHECK(mono.map.channels == 1);
    CHECK(mono.map.a[0] == 0);
    CHECK(mono.map.b[0] == 0);

    const auto absent = ChannelMap::resolve(1, 0, 1);
    REQUIRE(absent.status == Status::ok);
    CHECK(absent.map.b[0] == kSilentSource);

    for (const auto& map : {mono.map, absent.map})
        for (const auto index : map.b)
            CHECK(index != kConsolidatedSource);

    /* The mirror family keeps broadcasting: a wide main with a mono side is `chA` outputs of channel 0. */
    const auto broadcast = ChannelMap::resolve(3, 1, 3);
    REQUIRE(broadcast.status == Status::ok);
    CHECK(broadcast.map.b[0] == 0);
    CHECK(broadcast.map.b[1] == 0);
    CHECK(broadcast.map.b[2] == 0);
}

TEST_CASE("channel map: the fold is a marker, not an index that happens to be 0")
{
    /* `resolve(1, 2, 1)` leaves the same bytes in the index arrays under D-017's rule (B's channel 0,
       channel 1 dropped) and D-018's (the mean of both) — which is D-018's note that the contract cannot
       ride on the index arrays alone. So the map *states* the fold and this leg asserts the statement; a
       leg that read only `b[0] == 0` could not tell the two contracts apart and would pass either way. */
    const auto folded = ChannelMap::resolve(1, 2, 1);
    REQUIRE(folded.status == Status::ok);
    CHECK(folded.map.b[0] == kConsolidatedSource);
    CHECK(folded.map.b[0] != 0);
    CHECK(folded.map.b[0] != kSilentSource);
    CHECK(kConsolidatedSource != kSilentSource);
    /* Outside every real index range, so a consumer that treats it as one fails out of range rather than
       quietly reading the dry bus. */
    CHECK(kConsolidatedSource > static_cast<std::uint16_t>(kMaxChannels));

    /* Three reads of the sidechain, three distinct values: absent, mono, folded. */
    const auto silent = ChannelMap::resolve(1, 0, 1);
    const auto mono = ChannelMap::resolve(1, 1, 1);
    CHECK(silent.map.b[0] == kSilentSource);
    CHECK(mono.map.b[0] == 0);
    CHECK(folded.map.b[0] != silent.map.b[0]);
    CHECK(folded.map.b[0] != mono.map.b[0]);
}

TEST_CASE("channel map: the ceiling binds the sidechain bus width, not the consolidated channel count")
{
    const auto atTheCeiling = ChannelMap::resolve(1, kMaxChannels, 1);
    REQUIRE(atTheCeiling.status == Status::ok);
    CHECK(atTheCeiling.map.b[0] == kConsolidatedSource);

    const auto pastIt = ChannelMap::resolve(1, kMaxChannels + 1, 1);
    CHECK(pastIt.status == Status::exceedsCapacity);
    CHECK(isPristine(pastIt.map));

    /* The same boundary at every capacity `resolve` accepts, which is also the proof that the ceiling is
       checked before the family's count rule — one output is not a licence for a 9-channel bus. */
    for (const int capacity : {1, 2, 3, kMaxChannels})
    {
        const auto atBoundary = ChannelMap::resolve(1, capacity, 1, capacity);
        REQUIRE(atBoundary.status == Status::ok);
        CHECK(atBoundary.map.b[0] == (capacity > 1 ? kConsolidatedSource : std::uint16_t{0}));

        const auto pastBoundary = ChannelMap::resolve(1, capacity + 1, 1, capacity);
        CHECK(pastBoundary.status == Status::exceedsCapacity);
        CHECK(isPristine(pastBoundary.map));
    }
}

TEST_CASE("channel map: commutativity survives only on the equal-count diagonal, where it is trivial (F-013)")
{
    /* Restated rather than quietly narrowed, which is what criterion 9 of item 09 forbids. Under D-017 this
       leg swapped every `ok` triple that has a sidechain and compared the two index arrays bit-exactly — 601
       assertions, `01-channel-map-helper.md` §10 row 3 — because every rule was stated over the pair without
       saying which side is the plugin's own signal. D-018 keys the rule to the **main** bus, so the
       non-diagonal `ok` families are (`chA > 1`, `chB == 1`) and (`chA == 1`, `chB > 1`), and each one's swap
       partner is a different channel count: a broadcast map against a consolidated one. There is nothing to
       compare, and F-013 works the four families through. M4 §7's commutativity row is struck on the same
       grounds and item 06 does not inherit a tolerance from it.

       What is left is asserted as what it is — the diagonal, where both sides read index `c`, so the
       exchange below holds by construction. The per-slot identity checks are here precisely to show that it
       is a tautology on this domain, not evidence that the mapping is symmetric in its arguments. */
    int diagonalTriples = 0;
    for (const int chA : kSweep)
        for (const int chB : kSweep)
        {
            if (chA != chB || chB == 0 || chA > kMaxChannels)
                continue;
            for (int out = 1; out <= chA; ++out)
            {
                const auto forward = ChannelMap::resolve(chA, chB, out);
                const auto swapped = ChannelMap::resolve(chB, chA, out);
                REQUIRE(forward.status == Status::ok);
                REQUIRE(swapped.status == Status::ok);
                CHECK(swapped.map.channels == forward.map.channels);
                for (int c = 0; c < forward.map.channels; ++c)
                {
                    const auto slot = static_cast<std::size_t>(c);
                    CHECK(forward.map.a[slot] == static_cast<std::uint16_t>(c));
                    CHECK(forward.map.b[slot] == static_cast<std::uint16_t>(c));
                    CHECK(swapped.map.a[slot] == forward.map.b[slot]);
                    CHECK(swapped.map.b[slot] == forward.map.a[slot]);
                }
                ++diagonalTriples;
            }
        }

    /* The families that lost their swap partner, asserted as the reason the domain shrank rather than left
       to a reader who might re-widen this leg by mistake. */
    const auto broadcast = ChannelMap::resolve(4, 1, 4);
    const auto folded = ChannelMap::resolve(1, 4, 1);
    REQUIRE(broadcast.status == Status::ok);
    REQUIRE(folded.status == Status::ok);
    CHECK(broadcast.map.channels == 4);
    CHECK(folded.map.channels == 1);
    CHECK(broadcast.map.b[0] == 0);
    CHECK(folded.map.b[0] == kConsolidatedSource);
    /* The wide output counts this leg used to iterate over no longer resolve for a mono main. */
    CHECK(ChannelMap::resolve(1, 4, 4).status == Status::countMismatch);

    INFO("diagonal triples checked: " << diagonalTriples);
    /* 1 + 2 + 3 + 4 + 6 + 8 over the swept counts that fit under the ceiling. */
    CHECK(diagonalTriples == 24);
}

TEST_CASE("channel map: an asymmetric pair is rejected and nothing is half-written")
{
    int checked = 0;
    int written = 0;
    int wrongStatus = 0;
    for (const int chA : kSweep)
        for (const int chB : kSweep)
        {
            if (chA <= 1 || chB <= 1 || chA == chB)
                continue;
            for (int out = 1; out <= kMaxChannels + 2; ++out)
            {
                const auto resolved = ChannelMap::resolve(chA, chB, out);
                if (resolved.status != Status::asymmetric)
                    ++wrongStatus;
                if (!isPristine(resolved.map))
                    ++written;
                ++checked;
            }
        }

    INFO("asymmetric triples checked: " << checked);
    CHECK(checked > 0);
    CHECK(wrongStatus == 0);
    CHECK(written == 0);
}

TEST_CASE("channel map: a missing sidechain is silence and never channel 0")
{
    const auto absent = ChannelMap::resolve(2, 0, 2);
    const auto mono = ChannelMap::resolve(2, 1, 2);

    REQUIRE(absent.status == Status::ok);
    REQUIRE(mono.status == Status::ok);
    CHECK(absent.map.b[0] == kSilentSource);
    CHECK(absent.map.b[1] == kSilentSource);
    CHECK(mono.map.b[0] == 0);
    CHECK(mono.map.b[1] == 0);
    /* The distinguishing pair: if silence were mapped onto 0 the two maps would agree in every B slot,
       which is the one-character change that turns "no sidechain" into "convolve against the dry bus". */
    CHECK(absent.map.b[0] != mono.map.b[0]);
    CHECK(absent.map.b[1] != mono.map.b[1]);
    /* and A follows the A rule in both. */
    CHECK(absent.map.a[0] == 0);
    CHECK(absent.map.a[1] == 1);
    CHECK(mono.map.a[0] == 0);
    CHECK(mono.map.a[1] == 1);

    CHECK(ChannelMap::resolve(1, 0, 1).map.b[0] == kSilentSource);
    CHECK(ChannelMap::resolve(8, 0, 8).map.b[7] == kSilentSource);
    CHECK(ChannelMap::resolve(0, 0, 0).status == Status::empty);
    CHECK(ChannelMap::resolve(0, 2, 2).status == Status::empty);
    CHECK(ChannelMap::resolve(0, 2, 0).status == Status::empty);
    CHECK(kSilentSource > static_cast<std::uint16_t>(kMaxChannels));
}

TEST_CASE("channel map: more outputs than sources is a count mismatch, fewer selects the leading ones")
{
    /* The boundary in both directions around the most outputs the table allows for each pair: one below it
       and at it select, one above and two above reject. `maxOut` is written per pair rather than derived
       from `std::max(chA, chB)`, because for the consolidated pair that identity no longer holds (D-018)
       and this leg states its own rule rather than borrowing the oracle's. */
    struct Pair
    {
        int chA;
        int chB;
        int maxOut;
    };
    constexpr std::array<Pair, 6> pairs
        = {Pair{1, 0, 1}, Pair{2, 0, 2}, Pair{1, 2, 1}, Pair{2, 1, 2}, Pair{2, 2, 2}, Pair{4, 4, 4}};

    for (const auto& pair : pairs)
    {
        if (pair.maxOut > 1)
        {
            const auto fewer = ChannelMap::resolve(pair.chA, pair.chB, pair.maxOut - 1);
            CHECK(fewer.status == Status::ok);
            CHECK(fewer.map.channels == pair.maxOut - 1);
        }
        const auto exact = ChannelMap::resolve(pair.chA, pair.chB, pair.maxOut);
        CHECK(exact.status == Status::ok);
        CHECK(exact.map.channels == pair.maxOut);
        CHECK(ChannelMap::resolve(pair.chA, pair.chB, pair.maxOut + 1).status
              == Status::countMismatch);
        CHECK(ChannelMap::resolve(pair.chA, pair.chB, pair.maxOut + 2).status
              == Status::countMismatch);
    }

    /* A mono main over a stereo sidechain is D-018's consolidated case: one output, and the sidechain read
       as the mean of both channels. Until item 09 this leg asserted the opposite shape — channel 0 of each
       side with B's channel 1 dropped — which is the row D-018 retires. */
    const auto monoOut = ChannelMap::resolve(1, 2, 1);
    REQUIRE(monoOut.status == Status::ok);
    CHECK(monoOut.map.a[0] == 0);
    CHECK(monoOut.map.b[0] == kConsolidatedSource);
    CHECK(isPristine(ChannelMap::resolve(2, 2, 3).map));
}

TEST_CASE("channel map: the capacity boundary is checked before any index is written")
{
    for (const int capacity : {1, 2, 3, kMaxChannels})
    {
        for (const int side : {capacity, capacity + 1})
        {
            const auto resolved = ChannelMap::resolve(side, side, side, capacity);
            if (side == capacity)
            {
                CHECK(resolved.status == Status::ok);
                CHECK(resolved.map.channels == side);
            }
            else
            {
                CHECK(resolved.status == Status::exceedsCapacity);
                CHECK(isPristine(resolved.map));
            }
        }
        /* The over-capacity side on one bus only, which is the case a fill-before-check would write
           indices for: the mono partner keeps the pair out of the asymmetric rule. */
        const auto oneSide = ChannelMap::resolve(capacity + 1, 1, capacity + 1, capacity);
        CHECK(oneSide.status == Status::exceedsCapacity);
        CHECK(isPristine(oneSide.map));
        const auto truncated = ChannelMap::resolve(capacity + 1, capacity + 1, 1, capacity);
        CHECK(truncated.status == Status::exceedsCapacity);
        CHECK(isPristine(truncated.map));
    }

    CHECK(ChannelMap::resolve(kMaxChannels, kMaxChannels, kMaxChannels).status == Status::ok);
    CHECK(ChannelMap::resolve(kMaxChannels + 1, kMaxChannels + 1, kMaxChannels + 1).status
          == Status::exceedsCapacity);
    /* A capacity above the array bound is clamped to it, never trusted. */
    CHECK(ChannelMap::resolve(16, 16, 16, 64).status == Status::exceedsCapacity);
    CHECK(ChannelMap::resolve(4096, 4096, 4096).status == Status::exceedsCapacity);
    CHECK(ChannelMap::resolve(4096, 1, 4096).status == Status::exceedsCapacity);
    CHECK(ChannelMap::resolve(1, 4096, 4096).status == Status::exceedsCapacity);
    CHECK(ChannelMap::resolve(4096, 0, 4096).status == Status::exceedsCapacity);
}

TEST_CASE("channel map: a count that cannot describe a layout resolves to empty")
{
    CHECK(ChannelMap::resolve(-1, 2, 2).status == Status::empty);
    CHECK(ChannelMap::resolve(2, -1, 2).status == Status::empty);
    CHECK(ChannelMap::resolve(2, 2, -1).status == Status::empty);
    CHECK(ChannelMap::resolve(2, 2, 0).status == Status::empty);
    CHECK(isPristine(ChannelMap::resolve(-1, 2, 2).map));
    CHECK(isPristine(ChannelMap::resolve(2, -1, 2).map));
    CHECK(isPristine(ChannelMap::resolve(2, 2, 0).map));
}

namespace
{
/** The mappings the plugin will actually ship, pinned where a rule edit breaks the build instead of
    a test run. Kept few and named so a failure points at the rule, not at the template. */
constexpr auto kShippedMonoOut = ChannelMap::resolve(1, 0, 1);
constexpr auto kShippedStereoOut = ChannelMap::resolve(2, 0, 2);
constexpr auto kShippedMonoBoth = ChannelMap::resolve(1, 1, 1);
constexpr auto kShippedStereoMonoSide = ChannelMap::resolve(2, 1, 2);
constexpr auto kShippedStereoBoth = ChannelMap::resolve(2, 2, 2);
constexpr auto kShippedMonoWideSide = ChannelMap::resolve(1, 2, 1);
constexpr auto kShippedMonoEightSide = ChannelMap::resolve(1, kMaxChannels, 1);
constexpr auto kShippedEightOut = ChannelMap::resolve(8, 0, 8);
constexpr auto kShippedEightBoth = ChannelMap::resolve(8, 8, 8);
constexpr auto kPastTheCeiling = ChannelMap::resolve(16, 16, 16);

static_assert(kShippedMonoOut.status == Status::ok && kShippedMonoOut.map.channels == 1
                  && kShippedMonoOut.map.a[0] == 0 && kShippedMonoOut.map.b[0] == kSilentSource,
              "(1, 0) must resolve to a mono A against a silent B");
static_assert(kShippedStereoOut.status == Status::ok && kShippedStereoOut.map.channels == 2
                  && kShippedStereoOut.map.a[0] == 0 && kShippedStereoOut.map.a[1] == 1
                  && kShippedStereoOut.map.b[0] == kSilentSource
                  && kShippedStereoOut.map.b[1] == kSilentSource,
              "(2, 0) must be the identity on A with a silent B");
static_assert(kShippedMonoBoth.status == Status::ok && kShippedMonoBoth.map.a[0] == 0
                  && kShippedMonoBoth.map.b[0] == 0,
              "(1, 1) must pair channel 0 with channel 0");
static_assert(kShippedStereoMonoSide.status == Status::ok && kShippedStereoMonoSide.map.channels == 2
                  && kShippedStereoMonoSide.map.a[1] == 1
                  && kShippedStereoMonoSide.map.b[0] == 0
                  && kShippedStereoMonoSide.map.b[1] == 0,
              "(2, 1) must broadcast the sidechain to both outputs");
static_assert(kShippedStereoBoth.status == Status::ok && kShippedStereoBoth.map.a[1] == 1
                  && kShippedStereoBoth.map.b[1] == 1,
              "(2, 2) must be the identity on both sides");
/* The consolidated pair, pinned in the shape D-018 gives it. Its *rejection* at out > 1 is deliberately
   left to the runtime legs: pinning it here would move that rule behind a compile error, which proves
   less than a red test about the code a caller actually executes (F-012). */
static_assert(kShippedMonoWideSide.status == Status::ok && kShippedMonoWideSide.map.channels == 1
                  && kShippedMonoWideSide.map.a[0] == 0
                  && kShippedMonoWideSide.map.b[0] == kConsolidatedSource,
              "(1, 2) must be one channel whose sidechain is folded, not A broadcast over two outputs");
static_assert(kShippedMonoEightSide.status == Status::ok
                  && kShippedMonoEightSide.map.channels == 1
                  && kShippedMonoEightSide.map.b[0] == kConsolidatedSource,
              "an 8-wide sidechain on a mono main must fold to one channel at the ceiling");
static_assert(kShippedEightOut.status == Status::ok && kShippedEightOut.map.channels == kMaxChannels
                  && kShippedEightOut.map.a[kMaxChannels - 1] == kMaxChannels - 1
                  && kShippedEightOut.map.b[kMaxChannels - 1] == kSilentSource,
              "the ceiling itself must resolve, at the top index");
static_assert(kShippedEightBoth.status == Status::ok && kShippedEightBoth.map.a[7] == 7
                  && kShippedEightBoth.map.b[7] == 7,
              "(8, 8) must stay the identity at the ceiling");
static_assert(kPastTheCeiling.status == Status::exceedsCapacity,
              "one channel past the ceiling must be rejected, not truncated");

static_assert(noexcept(ChannelMap::resolve(2, 1, 2)));
static_assert(std::is_trivially_copyable_v<ChannelMap>);
static_assert(std::is_trivially_copyable_v<Result>);
static_assert(sizeof(ChannelMap) <= sizeof(int) + 2 * kMaxChannels * sizeof(std::uint16_t));
}

TEST_CASE("channel map: the compile-time pins still match what resolve does at runtime")
{
    struct Shipped
    {
        int chA;
        int chB;
        int out;
        const Result* pinned;
    };
    constexpr std::array<Shipped, 10> shipped
        = {{Shipped{1, 0, 1, &kShippedMonoOut},
            Shipped{2, 0, 2, &kShippedStereoOut},
            Shipped{1, 1, 1, &kShippedMonoBoth},
            Shipped{2, 1, 2, &kShippedStereoMonoSide},
            Shipped{2, 2, 2, &kShippedStereoBoth},
            Shipped{1, 2, 1, &kShippedMonoWideSide},
            Shipped{1, kMaxChannels, 1, &kShippedMonoEightSide},
            Shipped{8, 0, 8, &kShippedEightOut},
            Shipped{8, 8, 8, &kShippedEightBoth},
            Shipped{16, 16, 16, &kPastTheCeiling}}};

    for (const auto& pair : shipped)
    {
        const auto runtime = ChannelMap::resolve(pair.chA, pair.chB, pair.out);
        CHECK(runtime.status == pair.pinned->status);
        CHECK(runtime.map.channels == pair.pinned->map.channels);
        for (int c = 0; c < kMaxChannels; ++c)
        {
            const auto slot = static_cast<std::size_t>(c);
            CHECK(runtime.map.a[slot] == pair.pinned->map.a[slot]);
            CHECK(runtime.map.b[slot] == pair.pinned->map.b[slot]);
        }
    }
}

TEST_CASE("channel map: resolving the swept table allocates nothing")
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    long allocations = 0;
    {
        const AllocationWatch watch;
        for (const int chA : kSweep)
            for (const int chB : kSweep)
                for (const int out : kSweep)
                    hash = mix(hash, ChannelMap::resolve(chA, chB, out));
        allocations = allocationCount;
    }
    CHECK(allocations == 0);
    /* The sweep has to have produced a non-trivial fold, or a zero allocation count proves nothing. */
    CHECK(hash != 0xcbf29ce484222325ULL);
}

TEST_CASE("channel map: the table is the same resolved twice, forwards or backwards")
{
    /* The whole table resolved, resolved again, and resolved with the sweep in the opposite direction,
       compared field by field rather than through a hash so a difference is about the rule. This is the
       guard against a hidden static or an accumulator carried from one resolve into the next. */
    auto triples = std::vector<std::array<int, 3>>{};
    triples.reserve(kTripleCount);
    for (const int chA : kSweep)
        for (const int chB : kSweep)
            for (const int out : kSweep)
                triples.push_back({chA, chB, out});

    auto resolveAll = [](std::span<const std::array<int, 3>> list) {
        auto results = std::vector<Result>{};
        results.reserve(list.size());
        for (const auto& triple : list)
            results.push_back(ChannelMap::resolve(triple[0], triple[1], triple[2]));
        return results;
    };

    const auto forward = resolveAll(triples);
    const auto repeat = resolveAll(triples);
    const auto backwards = std::vector<std::array<int, 3>>(triples.rbegin(), triples.rend());
    const auto reverseOrder = resolveAll(backwards);

    REQUIRE(forward.size() == static_cast<std::size_t>(kTripleCount));
    CHECK(reverseOrder.size() == forward.size());

    int repeatDifferences = 0;
    for (std::size_t i = 0; i < forward.size(); ++i)
        if (!sameResult(forward[i], repeat[i]))
            ++repeatDifferences;
    CHECK(repeatDifferences == 0);

    /* Slot i of the reversed sweep holds triples[size-1-i], so it pairs with the same slot counted
       from the other end of the forward run. */
    int orderDifferences = 0;
    for (std::size_t i = 0; i < forward.size(); ++i)
        if (!sameResult(reverseOrder[i], forward[forward.size() - 1 - i]))
            ++orderDifferences;
    CHECK(orderDifferences == 0);
}

TEST_CASE("channel map: every index is in range and every source that fits is reachable")
{
    std::mt19937 rng(20261101U);
    std::uniform_int_distribution<int> sources(0, 24);
    std::uniform_int_distribution<int> outputs(0, 24);
    std::uniform_int_distribution<int> capacityPick(1, kMaxChannels + 2);

    int checked = 0;
    int violations = 0;

    auto sideRange = [&violations](int sideChannels,
                                   const std::array<std::uint16_t, kMaxChannels>& side, int channels,
                                   bool foldedToMean) {
        /* A folded side names no channel at all: the marker is its whole claim, in every slot the map
           filled. An index there would mean the map promises a mean and points at one channel. */
        if (foldedToMean)
        {
            for (int c = 0; c < std::min(channels, kMaxChannels); ++c)
                if (side[static_cast<std::size_t>(c)] != kConsolidatedSource)
                    ++violations;
            return;
        }
        std::uint32_t seen = 0;
        /* Clamped to the array the map actually has, so a resolve that reports more channels than it
           filled is a violation recorded below rather than a read past the end here. */
        for (int c = 0; c < std::min(channels, kMaxChannels); ++c)
        {
            const auto index = side[static_cast<std::size_t>(c)];
            if (sideChannels == 0)
            {
                if (index != kSilentSource)
                    ++violations;
                continue;
            }
            if (index == kSilentSource || index == kConsolidatedSource || index >= sideChannels
                || index >= 32)
            {
                ++violations;
                continue;
            }
            seen |= 1u << index;
        }
        /* A multi-channel side whose channels fit must appear in full: this is the half that catches a
           rule which skips or wraps a source instead of reaching it. */
        const int reachable = std::min({sideChannels, channels, kMaxChannels});
        const std::uint32_t want = reachable >= 32 ? 0xFFFFFFFFu : (1u << reachable) - 1u;
        if (seen != want)
            ++violations;
    };

    for (int trial = 0; trial < 4000; ++trial)
    {
        const int chA = sources(rng);
        const int chB = sources(rng);
        const int out = outputs(rng);
        const int capacity = capacityPick(rng);
        const auto resolved = ChannelMap::resolve(chA, chB, out, capacity);
        if (resolved.status != Status::ok)
            continue;
        ++checked;

        const bool folds = chA == 1 && chB > 1;
        if (resolved.map.channels != out || out < 1
            || out > std::min(maximumOutputs(chA, chB), capacity)
            || resolved.map.channels > kMaxChannels)
            ++violations;
        sideRange(chA, resolved.map.a, resolved.map.channels, false);
        sideRange(chB, resolved.map.b, resolved.map.channels, folds);
    }

    INFO("ok triples swept: " << checked);
    CHECK(checked > 0);
    CHECK(violations == 0);
}
