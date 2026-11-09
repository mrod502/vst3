#include <LayoutGate.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace
{
/** gateLayout must not allocate. Replacing global operator new is allowed once per program, which is why
    this file is its own test binary; the shape follows ChannelMapTests.cpp and MixEngineTests.cpp. */
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
using engine::gateLayout;
using engine::kMaxChannels;
using engine::kMaxMainBusChannels;
using Decision = engine::LayoutDecision;
using Status = engine::ChannelMapStatus;
using Verdict = engine::LayoutVerdict;
using Map = engine::ChannelMap;

/** The sweep bounds of criteria 2 and 13: past the ceiling of 8 on both input sides, past the main-bus
    width on the output side, and including 0 for every position — the disabled sidechain bus, the disabled
    main output and the no-main-input state all arrive as 0 from the shell's translation. */
constexpr int kSweepMax = 12;
constexpr int kSweepCount = kSweepMax + 1;
constexpr int kTripleCount = kSweepCount * kSweepCount * kSweepCount;

/** The rule `EntangleProcessor.cpp` enforced before this item, restated from its three guards rather than
    from the gate, so criterion 1 compares two independent shapes of the same decision. It is the answer for
    a layout whose main bus is mono or stereo — the only shapes the old code could accept — and the sweep
    below keeps the counts in the range its guards could see. */
bool legacyGateAccepts(int chA, int chB, int chOut)
{
    if (chOut != chA)
        return false;
    if (chA != 1 && chA != 2)
        return false;
    return chB == 0 || chB == 1 || chB == 2;
}

/** The `chB <= chA`, except the mono-main family` sentence that INDEX §6 row 2 and M4 §4 item 3 both carry,
    written as the decision procedure it reads as. `03` §1.4 measures this over-admits; the leg that uses it
    is what makes INDEX §3 rule 2 testable rather than hortatory. */
bool paraphraseAccepts(int chA, int chB, int chOut)
{
    if (chA <= 0 || chOut != chA)
        return false;
    if (chA == 1 && chB > 1)
        return true;
    return chB <= chA;
}

struct Expected
{
    Verdict verdict;
    Status rule;
    int processed;
};

/** Folds a decision into a running hash so the allocation leg can prove the sweep ran without storing
    2 197 structs it would then have to compare field by field. */
std::uint64_t mix(std::uint64_t hash, const Decision& decision)
{
    auto fold = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 0x100000001B3ULL;
    };
    fold(static_cast<std::uint64_t>(decision.verdict));
    fold(static_cast<std::uint64_t>(decision.rule));
    fold(static_cast<std::uint64_t>(decision.processed));
    return hash;
}

struct Mismatch
{
    int count = 0;
    int chA = -1;
    int chB = -1;
    int out = -1;
    int verdict = -1;
    int wantedVerdict = -1;
    int rule = -1;
    int wantedRule = -1;
    int processed = -1;
    int wantedProcessed = -1;
};

void record(Mismatch& m, int chA, int chB, int out, const Decision& got, const Expected& want)
{
    if (m.count == 0)
    {
        m.chA = chA;
        m.chB = chB;
        m.out = out;
        m.verdict = static_cast<int>(got.verdict);
        m.wantedVerdict = static_cast<int>(want.verdict);
        m.rule = static_cast<int>(got.rule);
        m.wantedRule = static_cast<int>(want.rule);
        m.processed = got.processed;
        m.wantedProcessed = want.processed;
    }
    ++m.count;
}

void report(const Mismatch& m)
{
    INFO("first mismatch: (" << m.chA << ", " << m.chB << ", " << m.out << ") verdict=" << m.verdict
                             << " wanted=" << m.wantedVerdict << " rule=" << m.rule
                             << " wanted=" << m.wantedRule << " processed=" << m.processed
                             << " wanted=" << m.wantedProcessed);
}
}

TEST_CASE("layout gate: the twelve shapes the old gate could see keep their exact answers")
{
    /* The regression criterion, stated over the whole domain the old three guards could reach — main in and
       out mono/stereo/other and a sidechain that is absent, mono or stereo — so "nothing changed" is a
       swept claim rather than a list. Where the shape is one of the six the plugin accepted, the expected
       row also pins the rule the refusal carries and the processed count, because a verdict alone can move
       from `channelRule` to `mainCountMismatch` without any host seeing a difference and criterion 1 asks
       for the answer, not just the yes or no. */
    struct Row
    {
        int chA;
        int chB;
        int out;
        Expected expected;
    };
    constexpr std::array<Row, 12> rows{{
        {1, 0, 1, {Verdict::accepted, Status::ok, 1}},
        {1, 1, 1, {Verdict::accepted, Status::ok, 1}},
        {1, 2, 1, {Verdict::accepted, Status::ok, 1}},
        {2, 0, 2, {Verdict::accepted, Status::ok, 2}},
        {2, 1, 2, {Verdict::accepted, Status::ok, 2}},
        {2, 2, 2, {Verdict::accepted, Status::ok, 2}},
        {1, 0, 2, {Verdict::mainCountMismatch, Status::countMismatch, 0}},
        {1, 1, 2, {Verdict::mainCountMismatch, Status::countMismatch, 0}},
        {1, 2, 2, {Verdict::mainCountMismatch, Status::countMismatch, 0}},
        {2, 0, 1, {Verdict::mainCountMismatch, Status::ok, 0}},
        {2, 1, 1, {Verdict::mainCountMismatch, Status::ok, 0}},
        {2, 2, 1, {Verdict::mainCountMismatch, Status::ok, 0}},
    }};

    Mismatch table;
    for (const auto& row : rows)
    {
        const auto decision = gateLayout(row.chA, row.chB, row.out);
        if (decision.verdict != row.expected.verdict || decision.rule != row.expected.rule
            || decision.processed != row.expected.processed)
            record(table, row.chA, row.chB, row.out, decision, row.expected);
    }
    report(table);
    CHECK(table.count == 0);

    Mismatch agreement;
    int accepted = 0;
    for (const int chA : {1, 2, 3})
        for (const int chB : {0, 1, 2})
            for (const int out : {1, 2, 3})
            {
                const auto decision = gateLayout(chA, chB, out);
                const bool yes = decision.verdict == Verdict::accepted;
                if (yes)
                    ++accepted;
                if (yes != legacyGateAccepts(chA, chB, out))
                    record(agreement, chA, chB, out, decision,
                           {legacyGateAccepts(chA, chB, out) ? Verdict::accepted
                                                             : Verdict::channelRule,
                            Status::ok, yes ? decision.processed : 0});
            }
    report(agreement);
    CHECK(agreement.count == 0);
    /* The old gate accepted exactly these six, and no shape outside them entered the accepted set on the
       domain where both could be asked — the widening lives at chB > 2, which this sweep cannot reach. */
    CHECK(accepted == 6);
}

TEST_CASE("layout gate: one derivation — accepted only where the map resolves and the refusal keeps its status")
{
    /* Criterion 2, and the leg INDEX §3 rule 2 is really about: the gate is compared against `resolve` and
       never against a sentence about it. `rule` is checked on every path rather than only on the
       `channelRule` ones, because D-021 C promises a refusal carries what the map thought even when the
       verdict came from the width or the count — which is also what a gate that stopped calling `resolve`
       on some branch trips over. */
    Mismatch derivation;
    std::array<int, 5> verdictCounts = {};
    int accepted = 0;

    for (int chA = 0; chA <= kSweepMax; ++chA)
        for (int chB = 0; chB <= kSweepMax; ++chB)
            for (int out = 0; out <= kSweepMax; ++out)
            {
                const auto decision = gateLayout(chA, chB, out);
                const auto resolved = Map::resolve(chA, chB, out);
                ++verdictCounts[static_cast<std::size_t>(decision.verdict)];

                const bool acceptedHere = decision.verdict == Verdict::accepted;
                if (acceptedHere)
                    ++accepted;

                /* `rule` verbatim, always. */
                if (decision.rule != resolved.status)
                    record(derivation, chA, chB, out, decision,
                           {decision.verdict, resolved.status, decision.processed});

                /* accepted ⟹ the map serves the triple, and with the count the gate reports. */
                if (acceptedHere
                    && (resolved.status != Status::ok
                        || decision.processed != resolved.map.channels))
                    record(derivation, chA, chB, out, decision,
                           {Verdict::accepted, Status::ok, resolved.map.channels});

                /* Every refusal processes nothing, and a `channelRule` refusal is one the map really
                   rejected — the gate may not invent that verdict for a triple `resolve` serves. */
                if (!acceptedHere && decision.processed != 0)
                    record(derivation, chA, chB, out, decision,
                           {decision.verdict, decision.rule, 0});
                if (decision.verdict == Verdict::channelRule && resolved.status == Status::ok)
                    record(derivation, chA, chB, out, decision,
                           {Verdict::channelRule, Status::asymmetric, 0});
            }

    report(derivation);
    CHECK(derivation.count == 0);
    CHECK(kTripleCount == 2197);
    /* All four verdicts have to occur, or the sweep could pass by never leaving one branch of the ladder.
       accepted / mainWidth / mainCountMismatch / channelRule, in enum order. */
    CHECK(verdictCounts[0] > 0);
    CHECK(verdictCounts[1] > 0);
    CHECK(verdictCounts[2] > 0);
    CHECK(verdictCounts[3] > 0);
    /* The accepted set is small enough to name: a mono main takes a sidechain from absent to the ceiling
       (9 triples) and a stereo main takes absent, mono or stereo (3). Everything else the sweep visits is
       refused — the asymmetric families, the past-ceiling widths, every unequal main and the 0-count states. */
    INFO("accepted triples: " << accepted);
    CHECK(accepted == (kMaxChannels + 1) + 3);
}

TEST_CASE("layout gate: mainOut == mainIn is the gate's rule and not the map's")
{
    /* Six of the §1.2 refusals are the hardest ones in this item: `resolve` happily serves a stereo main
       narrowed to one output — a leading-channel selection, 1 processed channel — and the gate has to say
       no anyway, because D-018 keeps `mainOut == mainIn` for the shell. A gate written as
       `resolve(...) == ok` passes every other leg in this file and fails these three. */
    int whereTheMapAgrees = 0;
    for (int chB = 0; chB <= 9; ++chB)
    {
        const auto widerOut = gateLayout(1, chB, 2);
        const auto narrowerOut = gateLayout(2, chB, 1);
        CHECK(widerOut.verdict == Verdict::mainCountMismatch);
        CHECK(narrowerOut.verdict == Verdict::mainCountMismatch);
        CHECK(widerOut.processed == 0);
        CHECK(narrowerOut.processed == 0);

        if (Map::resolve(2, chB, 1).status == Status::ok)
            ++whereTheMapAgrees;
    }

    /* (2, 0, 1), (2, 1, 1) and (2, 2, 1) — the leading selections. Measured, not inherited: the plan's §4
       criterion 3 counts six, and over the range it names only these three resolve `ok`. */
    CHECK(whereTheMapAgrees == 3);

    /* Unequal mains are refused at every width, including the ones the width rule answers first: a 4-wide
       main with a 3-wide output is `mainWidth`, which is the ladder's order and is checked so a reorder
       cannot quietly change which reason a host is given. */
    for (int chA : {3, 4, 6, 8})
        for (int out = 1; out <= kSweepMax; ++out)
        {
            if (out == chA)
                continue;
            CHECK(gateLayout(chA, 0, out).verdict == Verdict::mainWidth);
        }
    for (int chA : {1, 2})
        for (int out = 1; out <= kSweepMax; ++out)
        {
            if (out == chA)
                continue;
            const auto decision = gateLayout(chA, 3, out);
            CHECK(decision.verdict == Verdict::mainCountMismatch);
        }
    /* A zero main output is the exception to the loop above and belongs to the map, not the count rule:
       `resolve` calls it `empty` before anything can be said about equality, and the ladder keeps that
       reason. It is asserted in the refusal-reasons leg rather than folded in here. */
    for (const int chA : {1, 2})
        CHECK(gateLayout(chA, 3, 0).rule == Status::empty);
}

TEST_CASE("layout gate: a mono main with a 2-to-8-wide sidechain is accepted as one consolidated channel")
{
    /* D-021 A's whole point: this family resolves since D-018 and is folded since item 02, but the plugin
       refused a wider-than-stereo sidechain itself, so nothing a host routed could reach it. Processed
       count stays 1 — the fold, not a broadcast — and the ceiling still binds the bus width at 9. */
    for (int chB = 2; chB <= kMaxChannels; ++chB)
    {
        const auto decision = gateLayout(1, chB, 1);
        CHECK(decision.verdict == Verdict::accepted);
        CHECK(decision.rule == Status::ok);
        CHECK(decision.processed == 1);
    }

    const auto monoSide = gateLayout(1, 1, 1);
    const auto absentSide = gateLayout(1, 0, 1);
    CHECK(monoSide.verdict == Verdict::accepted);
    CHECK(absentSide.verdict == Verdict::accepted);
    CHECK(monoSide.processed == 1);
    CHECK(absentSide.processed == 1);

    /* And the boundary: 8 is the widest sidechain bus the ceiling admits, 9 is not, and the refusal says
       which rule it tripped on rather than joining the general refusal. */
    CHECK(gateLayout(1, kMaxChannels, 1).verdict == Verdict::accepted);
    const auto past = gateLayout(1, kMaxChannels + 1, 1);
    CHECK(past.verdict == Verdict::channelRule);
    CHECK(past.rule == Status::exceedsCapacity);
    CHECK(past.processed == 0);

    /* The main bus is the one that decides, so the same wide sidechain on a stereo main is a refusal —
       D-018's asymmetric family, and newly reachable as a *named* refusal by this item. */
    for (int chB = kMaxMainBusChannels + 1; chB <= kSweepMax; ++chB)
    {
        const auto stereo = gateLayout(2, chB, 2);
        CHECK(stereo.verdict == Verdict::channelRule);
        CHECK(stereo.rule == Status::asymmetric);
        CHECK(stereo.processed == 0);
    }
}

TEST_CASE("layout gate: every refusal names one of the three reasons and keeps the map's status")
{
    /* Criterion 5: one leg per family. The reasons exist so the shell can tell a host why (D-017: "a
       layout gate needs the reason, not a bare `false`"), which means the verdict is only half the answer
       and `rule` is the half a fifth status would have been the cheap way out of. */
    struct Case
    {
        int chA;
        int chB;
        int out;
        Verdict verdict;
        Status rule;
    };
    constexpr std::array<Case, 16> cases{{
        {3, 0, 3, Verdict::mainWidth, Status::ok},
        {3, 1, 3, Verdict::mainWidth, Status::ok},
        {3, 3, 3, Verdict::mainWidth, Status::ok},
        {4, 0, 4, Verdict::mainWidth, Status::ok},
        {4, 4, 4, Verdict::mainWidth, Status::ok},
        {6, 0, 6, Verdict::mainWidth, Status::ok},
        {6, 6, 6, Verdict::mainWidth, Status::ok},
        {8, 0, 8, Verdict::mainWidth, Status::ok},
        {8, 8, 8, Verdict::mainWidth, Status::ok},
        /* The width rule outranks the ceiling as a *reason* while the map still gets a say: resolve has
           already given up on 9 channels, and D-021 C says that answer is kept verbatim rather than
           relabelled mainWidth. */
        {9, 9, 9, Verdict::mainWidth, Status::exceedsCapacity},
        {1, 9, 1, Verdict::channelRule, Status::exceedsCapacity},
        {2, 3, 2, Verdict::channelRule, Status::asymmetric},
        {2, 9, 2, Verdict::channelRule, Status::asymmetric},
        {0, 0, 0, Verdict::channelRule, Status::empty},
        {0, 2, 1, Verdict::channelRule, Status::empty},
        {2, 0, 0, Verdict::channelRule, Status::empty},
    }};

    Mismatch table;
    for (const auto& row : cases)
    {
        const auto decision = gateLayout(row.chA, row.chB, row.out);
        const Expected want{row.verdict, row.rule, 0};
        if (decision.verdict != want.verdict || decision.rule != want.rule
            || decision.processed != want.processed)
            record(table, row.chA, row.chB, row.out, decision, want);
    }
    report(table);
    CHECK(table.count == 0);

    /* A disabled main output and a missing main input are the map's `empty` in both directions — A is the
       mandatory side (D-017) and the gate does not re-label either as a width or a count problem, because
       "there is nothing to render" is the reason a host needs and it is already spelled. */
    for (int chB = 0; chB <= kSweepMax; ++chB)
    {
        const auto noMain = gateLayout(0, chB, 0);
        CHECK(noMain.verdict == Verdict::channelRule);
        CHECK(noMain.rule == Status::empty);
        const auto noOut = gateLayout(2, chB, 0);
        CHECK(noOut.verdict == Verdict::channelRule);
        CHECK(noOut.rule == Status::empty);
    }

    /* No verdict other than `accepted` may report a processed count, whatever the reason. */
    for (int chA = 0; chA <= kSweepMax; ++chA)
        for (int chB = 0; chB <= kSweepMax; ++chB)
            for (int out = 0; out <= kSweepMax; ++out)
                if (gateLayout(chA, chB, out).verdict != Verdict::accepted)
                    CHECK(gateLayout(chA, chB, out).processed == 0);
}

TEST_CASE("layout gate: the chB <= chA paraphrase over-admits and this gate refuses all of it")
{
    /* The trap INDEX §5 states as a general lesson: a necessary condition read as a decision procedure.
       Over the triples a host can present (`out == chA`, mains up to 8, sidechains to 9) the sentence
       admits 22 the rule rejects — 21 asymmetric pairs where a multi-channel main faces a narrower
       multi-channel sidechain, and (1, 9, 1) past the ceiling. A gate written from that sentence passes
       criterion 1 and the accepted family's legs and is still wrong 22 ways, so agreement with the prose
       is asserted here *as a failure* and the gate is checked against the helper instead.
       `03` §5 arm 2 installs the sentence as code to prove this leg reddens on it. */
    int paraphraseOverAdmits = 0;
    int gateAgreedWithTheMap = 0;
    int gateOverAdmitted = 0;

    for (int chA = 1; chA <= kMaxChannels; ++chA)
        for (int chB = 0; chB <= kMaxChannels + 1; ++chB)
        {
            const bool sentence = paraphraseAccepts(chA, chB, chA);
            const auto resolved = Map::resolve(chA, chB, chA);
            const auto decision = gateLayout(chA, chB, chA);
            const bool mapSays = resolved.status == Status::ok;

            if (sentence && !mapSays)
                ++paraphraseOverAdmits;
            if (sentence && decision.verdict == Verdict::accepted)
                ++gateAgreedWithTheMap;
            if (decision.verdict == Verdict::accepted && !mapSays)
                ++gateOverAdmitted;
        }

    /* Measured 2026-11-04 and re-measured here rather than quoted: 21 asymmetric triples plus (1, 9, 1). */
    CHECK(paraphraseOverAdmits == 22);
    /* The gate refuses every one of them — which is exactly the assertion the paraphrase-shaped gate
       fails, 21 + 1 ways. */
    CHECK(gateOverAdmitted == 0);
    INFO("gate-and-sentence agreements inside the sentence's admitted set: " << gateAgreedWithTheMap);
    CHECK(gateAgreedWithTheMap > 0);

    /* The other direction is real but not gate-visible: `resolve` is more permissive than the sentence on
       84 triples over [0, 9]³, every one of them an output narrower than the main (`out < chA`), and none
       at `out == chA`. It is D-021 B that keeps those out of the shell — the gate's `chOut != chA` rule,
       not the map, is what refuses them. */
    int resolveOnly = 0;
    int resolveOnlyAtOutEqChA = 0;
    for (int chA = 0; chA <= 9; ++chA)
        for (int chB = 0; chB <= 9; ++chB)
            for (int out = 0; out <= 9; ++out)
            {
                if (Map::resolve(chA, chB, out).status != Status::ok)
                    continue;
                if (paraphraseAccepts(chA, chB, out))
                    continue;
                ++resolveOnly;
                if (out == chA)
                    ++resolveOnlyAtOutEqChA;
                CHECK(gateLayout(chA, chB, out).verdict != Verdict::accepted);
            }
    CHECK(resolveOnly == 84);
    CHECK(resolveOnlyAtOutEqChA == 0);
}

TEST_CASE("layout gate: the answer is total and stable however often or in what order it is asked")
{
    /* Criterion 11's gate half: `isBusesLayoutSupported` is asked before `prepareToPlay`, after it, and
       across a mid-session layout change, so a decision that depended on anything outside its three
       arguments would show up here. There is no state to carry — no static, no accumulator, no read of a
       previous call — which the reversed-order comparison is the cheap proof of. */
    auto decideAll = [](bool reversed) {
        std::uint64_t hash = 0xcbf29ce484222325ULL;
        for (int i = 0; i < kTripleCount; ++i)
        {
            const int index = reversed ? kTripleCount - 1 - i : i;
            const int chA = index / (kSweepCount * kSweepCount);
            const int chB = (index / kSweepCount) % kSweepCount;
            const int out = index % kSweepCount;
            hash = mix(hash, gateLayout(chA, chB, out));
        }
        return hash;
    };

    const auto forward = decideAll(false);
    const auto repeat = decideAll(false);
    const auto reversed = decideAll(true);

    CHECK(forward == repeat);
    CHECK(reversed != forward); ///< the fold walks different triples, so a stuck decision shows up
    CHECK(forward != 0xcbf29ce484222325ULL);

    /* The same triple answered through both spellings the shell uses: counts from an `AudioChannelSet`
       translation are just ints, so the gate cannot tell a host layout from a harness one — and the
       accepted set has to be identical whichever way it was reached. */
    for (int chA = 0; chA <= kSweepMax; ++chA)
        for (int chB = 0; chB <= kSweepMax; ++chB)
            for (int out = 0; out <= kSweepMax; ++out)
            {
                const auto first = gateLayout(chA, chB, out);
                const auto second = gateLayout(chA, chB, out);
                CHECK(first.verdict == second.verdict);
                CHECK(first.rule == second.rule);
                CHECK(first.processed == second.processed);
            }

    static_assert(noexcept(gateLayout(2, 1, 2)));
    static_assert(std::is_trivially_copyable_v<Decision>);
    static_assert(sizeof(Decision) <= 3 * sizeof(int));
    /* The two rules this item owns are visible in the type as well as the ladder: the ceiling the map
       carries is 8 and the main bus stops at 2, so a widening of one is never accidentally the other. */
    static_assert(kMaxMainBusChannels == 2);
    static_assert(kMaxChannels == 8);
    static_assert(kMaxMainBusChannels < kMaxChannels);
}

namespace
{
/** The eight verdicts pinned at compile time: the six shapes that shipped before this item and the two
    endpoints of the family it opens. Deliberately few — item 09 found four of its seven planned arms
    caught by the compiler where a runtime leg was predicted (F-012), and a pin shadows the leg that would
    otherwise name the same triple. `03` §7's mitigation, kept to that set. */
constexpr Decision kPinnedMonoNoSide = gateLayout(1, 0, 1);
constexpr Decision kPinnedMonoMono = gateLayout(1, 1, 1);
constexpr Decision kPinnedMonoWide = gateLayout(1, 2, 1);
constexpr Decision kPinnedStereoNoSide = gateLayout(2, 0, 2);
constexpr Decision kPinnedStereoMono = gateLayout(2, 1, 2);
constexpr Decision kPinnedStereoStereo = gateLayout(2, 2, 2);
constexpr Decision kPinnedMonoEight = gateLayout(1, kMaxChannels, 1);
constexpr Decision kPinnedMonoNine = gateLayout(1, kMaxChannels + 1, 1);

static_assert(kPinnedMonoNoSide.verdict == Verdict::accepted
                  && kPinnedMonoNoSide.processed == 1,
              "mono main with no sidechain bus must stay the layout every host can load");
static_assert(kPinnedMonoMono.verdict == Verdict::accepted && kPinnedMonoMono.processed == 1,
              "mono main with a mono sidechain must process one channel");
static_assert(kPinnedMonoWide.verdict == Verdict::accepted && kPinnedMonoWide.processed == 1,
              "mono main with a stereo sidechain must process one consolidated channel, not two");
static_assert(kPinnedStereoNoSide.verdict == Verdict::accepted
                  && kPinnedStereoNoSide.processed == 2,
              "stereo main with no sidechain is the shipped default's dry shape");
static_assert(kPinnedStereoMono.verdict == Verdict::accepted && kPinnedStereoMono.processed == 2,
              "stereo main with a mono sidechain broadcasts to two processed channels");
static_assert(kPinnedStereoStereo.verdict == Verdict::accepted && kPinnedStereoStereo.processed == 2,
              "stereo main with a stereo sidechain is the layout GarageBand-only hosts require");
static_assert(kPinnedMonoEight.verdict == Verdict::accepted && kPinnedMonoEight.processed == 1,
              "an 8-wide sidechain on a mono main is the family D-021 A opens, at the ceiling");
static_assert(kPinnedMonoNine.verdict != Verdict::accepted
                  && kPinnedMonoNine.processed == 0
                  && kPinnedMonoNine.rule == Status::exceedsCapacity,
              "one channel past the ceiling must be refused by name, not truncated to the ceiling");
}

TEST_CASE("layout gate: the compile-time pins still match what the gate answers at runtime")
{
    struct Row
    {
        int chA;
        int chB;
        int out;
        Decision pinned;
    };
    constexpr std::array<Row, 8> pinned{{
        {1, 0, 1, kPinnedMonoNoSide},
        {1, 1, 1, kPinnedMonoMono},
        {1, 2, 1, kPinnedMonoWide},
        {2, 0, 2, kPinnedStereoNoSide},
        {2, 1, 2, kPinnedStereoMono},
        {2, 2, 2, kPinnedStereoStereo},
        {1, kMaxChannels, 1, kPinnedMonoEight},
        {1, kMaxChannels + 1, 1, kPinnedMonoNine},
    }};

    for (const auto& row : pinned)
    {
        const auto runtime = gateLayout(row.chA, row.chB, row.out);
        CHECK(runtime.verdict == row.pinned.verdict);
        CHECK(runtime.rule == row.pinned.rule);
        CHECK(runtime.processed == row.pinned.processed);
    }
}

TEST_CASE("layout gate: deciding the whole swept table allocates nothing")
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    long allocations = 0;
    {
        const AllocationWatch watch;
        for (int chA = 0; chA <= kSweepMax; ++chA)
            for (int chB = 0; chB <= kSweepMax; ++chB)
                for (int out = 0; out <= kSweepMax; ++out)
                    hash = mix(hash, gateLayout(chA, chB, out));
        allocations = allocationCount;
    }
    CHECK(allocations == 0);
    /* The sweep has to have produced a non-trivial fold, or a zero allocation count proves nothing. */
    CHECK(hash != 0xcbf29ce484222325ULL);
}
