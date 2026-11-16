/* M5 item 02: the (chA, chB) × hostBlock cross product — every layout the gate accepts against every callback
   plan, at the matrix grain length and at the shipped one.

   §5.2 already proves invariance across two callback plans at one layout and §5.20's M3 across three plans for
   ten cells; what neither does is vary the *pair*. This binary does, over the twelve triples `engine::gateLayout`
   accepts — including (1,4,1), (1,6,1) and (1,7,1), which no leg had a layout for before this item — and over
   eight callback plans, including a uniform plan at each of the milestone's sizes `{1, 7, 64, 128, 4096}` so a
   steady 64-sample callback is separated from the DSP's own hop rather than visited once inside a ragged list.

   The claim is bit-identity **within one (triple, shape)** and never across `L`, across triples or across
   backends (`INDEX.md` §6 row 5). Its weakness is stated in the item's decision D and in F-017: both sides of
   every delta here come out of the same `HopScheduler`, so a *resolution* error inside it moves both sides
   together and the cross product cannot see it. The twelve anchor cases are the witness against that — one
   absolute expectation per triple, an unmapped output slot at exactly `0.0f` and a mapped one non-zero — and
   §5 arm 3 is the run that says whether they bite.

   Two run sets, hence two prefixes: `cross product: ` is quick-set material, `cross product long: ` is not
   (`INDEX.md` §3 rule 3's amendment). `ctest -R "cross product"` still selects the item whole because ctest
   matches a substring. Case names spell a triple `1x5x1` rather than `(1,5,1)` because Catch2 splits a
   command-line test spec on commas and every name here has to survive being typed at a shell; the printed lines
   use the `(1,5,1)` notation.

   The models are not restated here: `compareStreams`, the cell table and both slot models are the harness's,
   lifted out of §5.20 by this item so the two binaries cannot disagree about what "identical" or "channel 0"
   means (decision B, F-020). */

#include <HarnessShell.hpp>
#include <LayoutGate.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <random>
#include <vector>

namespace
{
using harness::acceptedLayout;
using harness::acceptedLayouts;
using harness::blocksSummingTo;
using harness::channelIsExactSilence;
using harness::channelPeak;
using harness::compareChannel;
using harness::compareStreams;
using harness::configure;
using harness::constantBlocks;
using harness::delayedBy;
using harness::kDeltaTolerance;
using harness::kDefault;
using harness::kMaxBlock;
using harness::kMatrixAmplitude;
using harness::kSampleRate;
using harness::kTightest;
using harness::makeProcessor;
using harness::matrixCellOf;
using harness::MatrixCell;
using harness::monoPairModel;
using harness::noiseStream;
using harness::parkLevelStage;
using harness::render;
using harness::Shape;
using harness::singleImpulse;
using harness::slotModel;
using harness::slotModelName;
using harness::Stream;
using harness::totalOf;

/* The two shapes the item runs on. `kShortShape` is the pair §5.20 calls kMatrix — the largest hop the UI can
   pick with the shortest grain expressible at it, so a render costs microseconds — and `kShippedShape` is both
   UI defaults, the configuration the plugin ships on, where the same render costs about 80× that (§1.2). */
constexpr Shape kShortShape = kTightest;
constexpr Shape kShippedShape = kDefault;

/** The run length of each half, as a multiple of `L`. The short half keeps §5.20's `8L`. The long half runs
    `4L`, which is what `OfflineRender`'s non-matrix sections use, still clears `2L + R`, and is the shortening
    decision E offers on one condition: that the bound is printed per cell before any sample is rendered. It is
    (`preflight:` lines), and §10 of the item file carries the `8L` figure that decision was made against. */
constexpr int kShortRunLengths = 8;
constexpr int kLongRunLengths = 4;

/** The milestone's callback sizes, each of which has to appear as a *uniform* plan. */
constexpr std::array<int, 5> kMilestoneSizes{{1, 7, 64, 128, 4096}};

/** One line per (cell, leg) in §5.20's shape, so the two binaries' output reads as one suite's. */
struct Report
{
    int decided = 0;
    int failed = 0;

    bool line(bool condition, const char *cell, const juce::String &leg, const juce::String &figure)
    {
        ++decided;
        if (!condition)
            ++failed;
        std::cout << (condition ? "  ok   " : "  FAIL ") << cell << "  " << leg << ": " << figure << "\n";
        return condition;
    }
};

juce::String runContext()
{
#ifdef NDEBUG
    const char *const buildType = "Release, NDEBUG defined";
#else
    const char *const buildType = "asserts on, NDEBUG not defined";
#endif
    return juce::String("host ") + juce::SystemStats::getOperatingSystemName().toRawUTF8()
           + (juce::SystemStats::isOperatingSystem64Bit() ? " 64-bit" : " 32-bit") + ", build " + buildType
           + ", " + juce::String(juce::SystemStats::getNumCpus()) + " cpus";
}

juce::String clockText(double seconds) { return juce::String::formatted("%.2f s", seconds); }

/* ------------------------------------------------------------------ plans --- */

/** A callback plan, carrying the seed that produced it when it has one to pin (decision C). */
struct BlockPlan
{
    const char *name;
    std::vector<int> (*build)(int total);
    const char *seed; ///< "-" when the plan has no randomness in it
};

/** The seed of the plan this binary owns. The other random plan's fill is seeded inside `blocksSummingTo`
    (20261022), and that figure is printed as part of that plan rather than re-derived here. */
constexpr unsigned kPlanSeed = 20261111U;

/** A callback list whose every size is random: `[1, kMaxBlock]`, with a zero-length callback every fifth entry.

    `blocksSummingTo`'s seeded fill only starts after its fixed ramp has run the total out, so over an 8 192-sample
    run it never runs at all — the plan it seeds is ragged-headed, not random. This is the plan that is random end
    to end, and the one whose seed this binary picks and prints. */
std::vector<int> randomBlocks(int total)
{
    std::mt19937 rng(kPlanSeed);
    std::uniform_int_distribution<int> spread(1, kMaxBlock);
    std::vector<int> blocks;
    int remaining = total;
    for (int entry = 0; remaining > 0; ++entry)
    {
        if (entry % 5 == 4)
            blocks.push_back(0);
        const int taken = std::min(spread(rng), remaining);
        blocks.push_back(taken);
        remaining -= taken;
    }
    blocks.push_back(0);
    return blocks;
}

/** The plan set: both ragged patterns the harness already had, a uniform plan at each of the milestone's five
    sizes, and one random plan. The five uniform plans are the reason the list exists — §1.6 entry 14 records
    that the milestone's `64` was already *reachable* in the harness and never *uniform*, and there is all the
    difference in the world between a callback size that appears once inside a ramp and one that a whole render
    is delivered in. */
const std::vector<BlockPlan> &blockPlans()
{
    static const std::vector<BlockPlan> plans{
        { "ragged {1..4096} + seeded fill", [](int total) { return blocksSummingTo(total, 0); },
          "20261022 (the harness's own fill)" },
        { "ragged {1,4,4096}", [](int total) { return blocksSummingTo(total, 1); }, "-" },
        { "constant 1", [](int total) { return constantBlocks(total, 1); }, "-" },
        { "constant 7", [](int total) { return constantBlocks(total, 7); }, "-" },
        { "constant 64", [](int total) { return constantBlocks(total, 64); }, "-" },
        { "constant 128", [](int total) { return constantBlocks(total, 128); }, "-" },
        { "constant 4096", [](int total) { return constantBlocks(total, 4096); }, "-" },
        { "seeded random [1,4096] + a zero every 5th", &randomBlocks, "20261111 (this binary's)" },
    };
    return plans;
}

/** The plan the anchor renders on: the same one the invariance legs use as their reference, so the absolute
    claim and the relative one are made about one transport, not two. */
std::vector<int> canonicalPlan(int total) { return blocksSummingTo(total, 0); }

/** The plans, spelled out with their callback counts, for the line that has to name what it compared. */
juce::String planListText(int total)
{
    juce::String text;
    for (const auto &plan : blockPlans())
    {
        if (!text.isEmpty())
            text += ", ";
        text += plan.name;
        text += "(" + juce::String(static_cast<int>(plan.build(total).size())) + ")";
    }
    return text;
}

/* ---------------------------------------------------------------- bounds --- */

/** What a cell's assertions land after, and the run length that has to clear them — decision C's third
    condition, checked and printed per cell before a single sample is rendered. §5 arm 6 is the run that proves
    this is wired rather than decorative. */
struct RunBounds
{
    int length = 0;   ///< the running L, after the shell's clamp
    int total = 0;    ///< the run
    int response = 0; ///< 2L: an impulse at n0 = 0 answers across [L, 2L-1]
    int latency = 0;  ///< R, which is the hop at every layout this binary runs
    int stage = 0;    ///< the level stage's window + release, asked of the class even though the stage is parked
    bool cleared = false;
};

RunBounds runBounds(const Shape &shape, int lengthsOfL)
{
    dsp::LevelNormalizer stage;
    stage.prepare(kSampleRate, 1, shape.hop);
    RunBounds bounds;
    bounds.length = EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop);
    bounds.total = lengthsOfL * bounds.length;
    bounds.response = 2 * bounds.length;
    bounds.latency = shape.hop;
    bounds.stage = static_cast<int>(stage.windowSamples())
                   + static_cast<int>(kSampleRate * dsp::LevelNormalizer::releaseMs * 0.001);
    bounds.cleared = bounds.total > bounds.response + bounds.latency && bounds.total > bounds.stage;
    return bounds;
}

juce::String boundsText(const RunBounds &bounds)
{
    return juce::String::formatted("H=%d L=%d, %d samples (%d hops) vs 2L+R=%d and the stage's %d",
                                   bounds.latency, bounds.length, bounds.total,
                                   bounds.total / bounds.latency, bounds.response + bounds.latency,
                                   bounds.stage);
}

/* ----------------------------------------------------------------- table --- */

/** Every triple the harness claims, against `resolve` and against the gate, before anything renders. A rule
    that moved reddens here rather than turning every leg under it into a green restatement of an obsolete
    mapping — §5.20's M0, over twelve triples instead of ten rows, and the leg that decides the geometry half of
    criterion 3 for `(1,4,1)`, `(1,6,1)` and `(1,7,1)`. */
bool checkTable(Report &report)
{
    bool passed = report.line(acceptedLayouts().size() == 12, "table",
                              "the accepted set is twelve triples",
                              "the harness lists "
                                  + juce::String(static_cast<int>(acceptedLayouts().size()))
                                  + "; a triple gained or lost at the gate is a decision, not a drift");
    for (const auto &layout : acceptedLayouts())
    {
        const auto resolved = engine::ChannelMap::resolve(layout.chA, layout.chB, layout.chOut);
        const auto decision = engine::gateLayout(layout.chA, layout.chB, layout.chOut);

        bool same = resolved.status == engine::ChannelMapStatus::ok
                    && decision.verdict == engine::LayoutVerdict::accepted
                    && decision.processed == layout.processed
                    && decision.rule == resolved.status
                    && resolved.map.channels == layout.processed;
        for (int slot = 0; slot < layout.processed; ++slot)
        {
            const auto index = static_cast<std::size_t>(slot);
            same = same && resolved.map.a[index] == layout.a[index]
                   && resolved.map.b[index] == layout.b[index];
        }

        juce::String table;
        for (int slot = 0; slot < layout.processed; ++slot)
        {
            if (slot != 0)
                table += ", ";
            table += slotModelName(matrixCellOf(layout, kShortShape), static_cast<std::size_t>(slot));
        }
        passed = report.line(same, layout.name, "the literal mapping is resolve's",
                             juce::String(layout.family) + "; gate accepted, processed "
                                 + juce::String(layout.processed) + "; " + table)
                 && passed;
    }
    return passed;
}

/** The plan set is a set, it delivers each milestone size uniformly, and it contains the two shapes the claim
    needs: a callback size that is not a multiple of the hop at either hop value, and a zero-length callback.

    Run on the short half's total, because a plan is a shape and the short total is where the whole set fits; the
    long half's cells print their own counts in their own lines. */
bool checkPlanSet(Report &report)
{
    const int total = runBounds(kShortShape, kShortRunLengths).total;
    std::vector<std::vector<int>> built;
    bool passed = true;
    for (const auto &plan : blockPlans())
    {
        const auto blocks = plan.build(total);
        passed = report.line(totalOf(blocks) == total, "plans", plan.name,
                             juce::String::formatted("%d callbacks, %d samples, seed %s",
                                                     static_cast<int>(blocks.size()), totalOf(blocks),
                                                     plan.seed))
                 && passed;
        built.push_back(blocks);
    }

    int distinct = 0;
    for (std::size_t first = 0; first < built.size(); ++first)
        for (std::size_t second = first + 1; second < built.size(); ++second)
            if (built[first] != built[second])
                ++distinct;
    const int pairs = static_cast<int>(built.size() * (built.size() - 1) / 2);
    passed = report.line(distinct == pairs, "plans", "every plan differs from every other",
                         juce::String::formatted("%d of %d pairs differ", distinct, pairs)
                             + " — a repeated plan would make part of the cross product compare a render "
                               "with itself")
             && passed;

    for (const int size : kMilestoneSizes)
    {
        const auto uniform = constantBlocks(total, size);
        bool steady = !uniform.empty();
        for (std::size_t index = 0; index + 1 < uniform.size(); ++index)
            steady = steady && uniform[index] == size;
        const bool inTheSet = std::any_of(blockPlans().begin(), blockPlans().end(),
                                          [size, total](const BlockPlan &plan)
                                          { return plan.build(total) == constantBlocks(total, size); });
        passed = report.line(steady && inTheSet, "plans",
                             "the milestone's size " + juce::String(size) + " is delivered uniformly",
                             juce::String::formatted("%d callbacks, %d each but the last (%d)",
                                                     static_cast<int>(uniform.size()), size,
                                                     uniform.empty() ? -1 : uniform.back())
                             + " — pattern 0 visits this size once, which cannot separate it from the hop")
                 && passed;
    }

    for (const int hop : {kShortShape.hop, kShippedShape.hop})
    {
        int offGrid = 0;
        int withZero = 0;
        for (const auto &plan : blockPlans())
        {
            bool holdsAnOffGridSize = false;
            bool holdsAZero = false;
            for (const int size : plan.build(total))
            {
                if (size % hop != 0)
                    holdsAnOffGridSize = true;
                if (size == 0)
                    holdsAZero = true;
            }
            if (holdsAnOffGridSize)
                ++offGrid;
            if (holdsAZero)
                ++withZero;
        }
        passed = report.line(offGrid > 0 && withZero > 0, "plans",
                             "off-grid callbacks and a zero-length callback exist at H="
                                 + juce::String(hop),
                             juce::String::formatted("%d of %d plans hold a size that is not a multiple of %d, "
                                                     "%d carry a zero-length callback",
                                                     offGrid, static_cast<int>(blockPlans().size()), hop,
                                                     withZero))
                 && passed;
    }
    return passed;
}

/* ------------------------------------------------------------- invariance --- */

/** One triple: the same material, every plan, bit-identical — or the delta says where it is not.

    The material is built once, outside the plan loop (decision C's first condition): a generator called inside
    it would make every plan differ for the right-looking reason and no delta in the file would mean anything.
    §5 arm 5 is that mutation, and this function's shape is the thing it has to break. */
bool checkInvariance(Report &report, const MatrixCell &cell, const RunBounds &bounds, unsigned seedOffset)
{
    const auto started = juce::Time::getCurrentTime();
    if (!bounds.cleared)
    {
        report.line(false, cell.name, "run length",
                    boundsText(bounds) + " — the run does not clear what its assertions land after, so "
                                        "nothing compared below it would be evidence");
        return false;
    }

    auto processor = makeProcessor();
    if (!configure(*processor, *cell.bus, cell.shape) || !parkLevelStage(*processor))
    {
        report.line(false, cell.name, "configure", "the shell refused this layout or the level-stage park");
        return false;
    }

    const unsigned seedA = 5150U + seedOffset;
    const unsigned seedB = 5151U + seedOffset;
    const auto a = noiseStream(cell.chA, bounds.total, seedA, kMatrixAmplitude);
    const Stream side = cell.chB > 0 ? noiseStream(cell.chB, bounds.total, seedB, kMatrixAmplitude)
                                     : Stream{};
    const auto &plans = blockPlans();
    const auto first = render(*processor, a, cell.chB > 0 ? &side : nullptr, plans.front().build(bounds.total));

    double worst = 0.0;
    juce::String diverged;
    int callbacks = 0;
    for (std::size_t index = 1; index < plans.size(); ++index)
    {
        const auto blocks = plans[index].build(bounds.total);
        callbacks += static_cast<int>(blocks.size());
        const auto delta = compareStreams(
            render(*processor, a, cell.chB > 0 ? &side : nullptr, blocks), first);
        worst = std::max(worst, delta.worst);
        if ((delta.shapeMismatch || !delta.bitIdentical) && diverged.isEmpty())
            diverged = plans[index].name;
    }
    callbacks += static_cast<int>(plans.front().build(bounds.total).size());

    const double costSeconds = (juce::Time::getCurrentTime() - started).inSeconds();
    const bool bitExact = !(worst > 0.0) && diverged.isEmpty();
    return report.line(bitExact, cell.name, "every callback plan, one render",
                       juce::String::formatted("%d plans, %d callbacks, seeds %u/%u, max %.3e%s%s, cost %s",
                                               static_cast<int>(plans.size()), callbacks, seedA, seedB, worst,
                                               bitExact ? ", bit-identical" : ", DIVERGED at ",
                                               diverged.isEmpty() ? "" : diverged.toRawUTF8(),
                                               clockText(costSeconds).toRawUTF8()))
           && bitExact;
}

/* ----------------------------------------------------------------- anchor --- */

/** One triple's absolute expectations, on the canonical plan (decision D).

    F-017's shape, stated for this item: a block-plan delta is computed between two renders of the same
    `HopScheduler`, so a resolution error inside it moves both sides and the cross product stays green. What
    cannot move with it is an absolute claim about a slot — this output carries the source its table entry names
    and nothing else, which means every other slot of the same render is exactly `0.0f` while this one is not.

    Two loops, one per side, taken from §5.20's M4 and M5 with their model comparisons kept: an impulse in one A
    source with the rest of A exactly zero, and an impulse in one B source against broadband A. A slot the map
    says does not read the fed source must be exactly zero — an FFT of an all-zero hop is exactly zero, so
    anything else is a leak rather than rounding. */
bool checkAnchor(Report &report, const MatrixCell &cell, const RunBounds &bounds)
{
    if (!bounds.cleared)
    {
        report.line(false, cell.name, "run length", boundsText(bounds));
        return false;
    }
    auto processor = makeProcessor();
    if (!configure(*processor, *cell.bus, cell.shape) || !parkLevelStage(*processor))
    {
        report.line(false, cell.name, "configure", "the shell refused this layout or the level-stage park");
        return false;
    }
    const auto blocks = canonicalPlan(bounds.total);
    const int latency = processor->getLatencySamples();
    bool passed = report.line(latency == cell.shape.hop, cell.name, "R is the hop",
                              juce::String::formatted("declared R=%d, H=%d, canonical plan %d callbacks",
                                                      latency, cell.shape.hop,
                                                      static_cast<int>(blocks.size())));

    const auto broadbandA = noiseStream(cell.chA, bounds.total, 5150U, kMatrixAmplitude);

    for (int source = 0; source < cell.chA; ++source)
    {
        const auto a = singleImpulse(cell.chA, bounds.total, source, 0);
        const Stream side = cell.chB > 0
                                ? noiseStream(cell.chB, bounds.total,
                                              60601U + static_cast<unsigned>(source), 0.3f)
                                : Stream{};
        const auto out = render(*processor, a, cell.chB > 0 ? &side : nullptr, blocks);

        int silentWrong = 0;
        int deadMapped = 0;
        double worst = 0.0;
        double responsePeak = 0.0;
        for (int slot = 0; slot < cell.chOut; ++slot)
        {
            const auto index = static_cast<std::size_t>(slot);
            if (cell.a[index] != static_cast<std::uint16_t>(source))
            {
                if (!channelIsExactSilence(out, index))
                    ++silentWrong;
                continue;
            }
            const auto slotPeak = channelPeak(out, index);
            responsePeak = std::max(responsePeak, slotPeak);
            if (!(slotPeak > 0.0))
                ++deadMapped;
            /* With no sidechain the shell pins p to -1, so the response *is* the dry path and the model is A
               delayed by R; with one it is the mapped pair rendered alone. Either way the model is not the
               scheduler under test, which is what makes the non-zero half of this line the anchor. */
            const auto delta = cell.chB == 0
                                   ? compareStreams(out, delayedBy(a, latency))
                                   : compareChannel(out, index, slotModel(cell, a, index, side, blocks), 0);
            worst = std::max(worst, delta.worst);
        }
        passed = report.line(silentWrong == 0 && deadMapped == 0, cell.name,
                             "impulse in A" + juce::String(source)
                                 + (cell.chB == 0 ? " (no sidechain: the dry path)" : ""),
                             juce::String::formatted("%d slot(s) unmapped and not exactly zero, %d mapped and "
                                                     "dead, model max %.3e, response peak %.4f",
                                                     silentWrong, deadMapped, worst, responsePeak))
                 && passed;
    }

    for (int source = 0; source < cell.chB; ++source)
    {
        const auto side = singleImpulse(cell.chB, bounds.total, source, 0);
        const auto out = render(*processor, broadbandA, &side, blocks);
        const float scale = cell.b[0] == engine::kConsolidatedSource
                                ? 1.0f / static_cast<float>(cell.chB)
                                : 1.0f;

        int silentWrong = 0;
        int deadMapped = 0;
        int answered = 0;
        double worst = 0.0;
        for (int slot = 0; slot < cell.chOut; ++slot)
        {
            const auto index = static_cast<std::size_t>(slot);
            const bool readsThis = cell.b[index] == engine::kConsolidatedSource
                                   || cell.b[index] == static_cast<std::uint16_t>(source);
            if (!readsThis)
            {
                if (!channelIsExactSilence(out, index))
                    ++silentWrong;
                continue;
            }
            ++answered;
            if (!(channelPeak(out, index) > 0.0))
                ++deadMapped;
            const auto model = monoPairModel(cell, broadbandA, index, side,
                                             static_cast<std::size_t>(source), scale, blocks);
            worst = std::max(worst, compareChannel(out, index, model, 0).worst);
        }
        passed = report.line(silentWrong == 0 && deadMapped == 0 && answered > 0
                                 && !(worst > kDeltaTolerance),
                             cell.name, "impulse in B" + juce::String(source),
                             juce::String::formatted("%d slot(s) answered, %d mapped and dead, %d unmapped and "
                                                     "not exactly zero, model max %.3e%s",
                                                     answered, deadMapped, silentWrong, worst,
                                                     scale > 1.0f - 1.0e-6f
                                                         ? ""
                                                         : juce::String::formatted(
                                                               " (fold, against 1/%d of the single-channel "
                                                               "model)",
                                                               cell.chB)
                                                               .toRawUTF8()))
                 && passed;
    }
    return passed;
}

/* ------------------------------------------------------------------ cases --- */

/** The lines the anchor writes for the whole accepted set: R once per triple and one line per fed source. */
int expectedAnchorLines()
{
    int lines = 0;
    for (const auto &layout : acceptedLayouts())
        lines += 1 + layout.chA + layout.chB;
    return lines;
}

struct HalfSummary
{
    int decided = 0;
    int failed = 0;
    double seconds = 0.0;
};

/** The twelve triples of one half, with its own clock on its own line.

    Both halves call this: same legs, same plans, same comparison, at a window 8× longer and the shipped hop
    instead of the matrix one. The only difference the code carries is the shape, the run multiple and the
    prefix — which is the point of the pair, and the reason `cross product long` has to be its own ctest case
    rather than another loop inside the quick one. */
HalfSummary runHalf(const char *halfName, const Shape &shape, int lengthsOfL, bool withAnchor)
{
    Report report;
    const auto started = juce::Time::getCurrentTime();
    std::cout << "\n" << halfName << " — " << runContext().toRawUTF8() << "\n"
              << "       plans compared: " << planListText(runBounds(shape, lengthsOfL).total).toRawUTF8()
              << "\n";

    for (std::size_t index = 0; index < acceptedLayouts().size(); ++index)
    {
        const MatrixCell cell = matrixCellOf(acceptedLayouts()[index], shape);
        const RunBounds bounds = runBounds(shape, lengthsOfL);
        std::cout << "       " << cell.name << " preflight: " << boundsText(bounds).toRawUTF8()
                  << (bounds.cleared ? " — cleared" : " — NOT CLEARED") << "\n";
        checkInvariance(report, cell, bounds, static_cast<unsigned>(index));
        if (withAnchor)
            checkAnchor(report, cell, bounds);
    }

    const double seconds = (juce::Time::getCurrentTime() - started).inSeconds();
    std::cout << halfName << ": " << static_cast<int>(acceptedLayouts().size()) << " triples x "
              << static_cast<int>(blockPlans().size()) << " plans, " << report.decided
              << " lines decided, " << report.failed << " FAIL, wall clock "
              << clockText(seconds).toRawUTF8() << ", " << runContext().toRawUTF8() << "\n";
    return HalfSummary{ report.decided, report.failed, seconds };
}
}

TEST_CASE("cross product: the twelve accepted triples are the twelve the gate takes")
{
    Report report;
    const bool passed = checkTable(report);
    INFO(runContext().toRawUTF8());
    CHECK(passed);
    CHECK(report.failed == 0);
}

TEST_CASE("cross product: the callback plan set differs and covers the milestone sizes uniformly")
{
    Report report;
    const bool passed = checkPlanSet(report);
    INFO(runContext().toRawUTF8());
    CHECK(passed);
    CHECK(report.failed == 0);
}

/* One case per triple, so a triple that stops existing reddens the case named for it instead of quietly
   lowering a count nobody reads (01 arm 5's finding about the monolith). The `1x5x1` spelling is a filter
   affordance, not notation: the printed lines use (1,5,1). */
/** The cell an anchor case runs on, with the table's existence checked rather than assumed: §5 arm 4 deletes a
    triple from the shared table, and a null dereference there would end the run instead of naming the triple
    that went missing. */
MatrixCell anchorCell(int chA, int chB)
{
    const auto *layout = acceptedLayout(chA, chB);
    INFO("the harness table has no triple for (" << chA << "," << chB << "," << chA << ")");
    REQUIRE(layout != nullptr);
    return matrixCellOf(*layout, kShortShape);
}

TEST_CASE("cross product: 1x0x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 0),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x1x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 1),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x2x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 2),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x3x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 3),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x4x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 4),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x5x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 5),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x6x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 6),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x7x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 7),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 1x8x1 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(1, 8),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 2x0x2 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(2, 0),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 2x1x2 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(2, 1),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: 2x2x2 answers in absolute terms on its canonical plan")
{
    Report report;
    CHECK(checkAnchor(report, anchorCell(2, 2),
                      runBounds(kShortShape, kShortRunLengths)));
}

TEST_CASE("cross product: every callback plan gives the same samples at the matrix length")
{
    const auto summary = runHalf("cross product", kShortShape, kShortRunLengths, false);
    CHECK(summary.decided == 12);
    CHECK(summary.failed == 0);
    /* Decision E's bound on the quick half, and the figure that decides it is the wall clock printed above with
       its host and build type — rule 7. Over bound is a finding for §11, never a number to re-measure. */
    CHECK(summary.seconds < 1.0);
}

/* The long half runs the anchor as well as the plan set, because decision D asks for an absolute expectation per
   triple and a delta-only long half would be the weakest half of the pair: a block-plan delta compares two renders
   of one engine fed one total, and the hop grid is a function of that total rather than of where the callback
   boundaries fell, so anything keyed to absolute position moves both sides equally.

   Stating that is not a claim that lengthening the run buys DSP coverage — it does not, and arm 2 is the run that
   says so: an accumulator misaligned whenever its ring is larger than the short run's capacity reddens
   `OfflineRender` §5.3 and leaves both halves of this file green, because the dry path cannot be reached from the
   wet and the fold's ratio is shared by the model render (F-021). What this half holds is the geometry claims at
   the shape the plugin ships on — mapping, gate verdict, declared `R`, the fold reciprocal, the whole callback set —
   and arm 3 reddens them at both lengths while the block-plan deltas stay green. */
TEST_CASE("cross product long: every callback plan and the absolute anchor at the shipped length")
{
    const auto summary = runHalf("cross product long", kShippedShape, kLongRunLengths, true);
    CHECK(summary.decided == 12 + expectedAnchorLines());
    CHECK(summary.failed == 0);
    CHECK(summary.seconds < 20.0);
}

int main(int argc, char **argv)
{
    /* The startup ShellTests needs, for the reasons it records there: the APVTS flush timer wants a message
       manager, constructing one claims this thread as the message thread (F-018) so every prepare in every leg
       below writes a layout report line, and `Logger::~Logger` asserts that it is not destroying the logger it
       finds installed. No `--dump-wavs` here: nothing in this binary calls an expectation that takes a
       FailDump. */
    juce::ScopedJuceInitialiser_GUI gui;
    harness::ReportLogFilter logFilter;
    juce::Logger::setCurrentLogger(&logFilter);

    Catch::Session session;
    const int result = session.run(argc, argv);

    juce::Logger::setCurrentLogger(nullptr);
    return result;
}
