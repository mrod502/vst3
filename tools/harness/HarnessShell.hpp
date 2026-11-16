#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <ChannelMap.hpp>
#include <LevelNormalizer.hpp>

#include "EngineChain.h"
#include "EntangleProcessor.h"
#include "HarnessStream.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

/** The JUCE-linked half of the shared plugin-seam harness: the bus layouts a leg can be configured onto, the
    drivers that poke the shell's APVTS and negotiate a layout, the four render routes (through the shell,
    through the same builder with no shell in it, through the bare wet engine, and the two safety stages applied
    by hand), the expectations that print `ok`/`FAIL` and optionally dump the offending audio, and the log filter
    that keeps the shell's layout report out of a run's output.

    Extracted verbatim from tools/OfflineRender.cpp by M5 item 01: what these helpers print is part of that
    program's byte-identical baseline, so a moved body says exactly what it said before. The only added behaviour
    is the `FailDump` hook on the three expectations, which is inert unless a caller hands one over.

    M5 item 02 lifted the channel-matrix models into here the same way — `describe`, the parked-dial state, the
    accepted-triple table, the cell type and the two slot models — because its cross-product binary is their
    second consumer and restating the expected routing in a second place is the F-020 failure mode. They print
    nothing on their own, so §5.20's baseline is unaffected by the move.

    This target *links* `vstplug` and never adds a shell `.cpp` to its own sources, so the shell a leg renders and
    the plugin the VST3 bundle loads are the same object code — the property §5.1's shell-identity legs rest on
    and `EngineChain.h`'s "one builder, two callers" warning is about. */

/* No `createPluginFilter()` declaration here: juce_audio_processors.h declares it at global scope to spare plugin
   code a missing-prototype warning, and the definition comes from the linked vstplug. Re-declaring it in a header
   every consumer includes only trips -Wredundant-decls in each of them. */

namespace harness
{

struct BusSetup
{
    const char *name;
    juce::AudioChannelSet mainSet;
    bool withSidechain;
    /** The sidechain bus's shape when `withSidechain` is set. Stereo for every setup that predates
        M4 item 03 — which is what keeps `applyLayout` meaning the same thing to those legs as it did
        before it had a field to look at. Refusal probes do not come through here: `applyLayout` fails
        the harness on a rejected layout, which is right for a render leg and wrong for a refusal. */
    juce::AudioChannelSet sideSet;
};

extern const BusSetup kStereoWithB;
extern const BusSetup kStereoNoB;
extern const BusSetup kMonoNoB;
extern const BusSetup kMonoWithB;
extern const BusSetup kMonoWithEightB;
extern const BusSetup kMonoWithMonoB;
extern const BusSetup kStereoWithMonoB;
extern const BusSetup kMonoWithThreeB;
extern const BusSetup kMonoWithFiveB;
/* The three widths that take the accepted set to twelve. They are consolidated like the 3, 5 and 8
   above, and 6 and 7 are the second pair of non-dyadic reciprocals in the accepted set. */
extern const BusSetup kMonoWithFourB;
extern const BusSetup kMonoWithSixB;
extern const BusSetup kMonoWithSevenB;

std::unique_ptr<juce::AudioProcessor> makeProcessor();
EntangleAudioProcessor &shellOf(juce::AudioProcessor &processor);
int indexOfHop(int hop);
int indexOfLength(int length);
bool setChoice(juce::AudioProcessor &processor, const juce::String &parameterId, int index);
bool setFloat(juce::AudioProcessor &processor, const juce::String &parameterId, float value);
bool setBool(juce::AudioProcessor &processor, const juce::String &parameterId, bool value);
/** The parameter's denormalised value straight out of the APVTS, for a leg that has to compare what
    a state load produced rather than what a render sounded like. */
float parameterValue(juce::AudioProcessor &processor, const juce::String &parameterId);
bool applyLayout(juce::AudioProcessor &processor, const BusSetup &bus);
bool configure(juce::AudioProcessor &processor, const BusSetup &bus, const Shape &shape);

template <typename Parameter, typename Value>
bool setParameter(juce::AudioProcessor &processor, const juce::String &parameterId, Value value,
                  const char *kind)
{
    auto *parameter = dynamic_cast<Parameter *>(shellOf(processor).apvts.getParameter(parameterId));
    if (parameter == nullptr)
    {
        std::cout << "  FAIL no " << kind << " parameter named " << parameterId << "\n";
        return false;
    }
    *parameter = value; ///< notifies the host, exactly as a host poke would
    return true;
}

/** The three parameters the shell reads on every callback, as a reference render should run them. */
struct MixSetting
{
    float dial = 0.0f;
    float gainDb = 0.0f;
    bool bypass = false;
    /** The level stage's two controls, at the shell's own defaults so a leg that says nothing about
        them runs what ships. Both setters no-op on a variant with no stage installed, so every
        variant takes the same pushes and a leg cannot compare two chains by construction. */
    float normalisationAmount = dsp::LevelNormalizer::defaultAmount;
    float floorDb = dsp::LevelNormalizer::defaultFloorDb;
};

Stream render(juce::AudioProcessor &processor, const Stream &in, const Stream *sideIn,
              const std::vector<int> &blocks);

/** One render with the three mix parameters parked at a setting. */
Stream renderAt(juce::AudioProcessor &processor, const Stream &a, const Stream *b, float dial,
                float gainDb, bool bypassed, const std::vector<int> &blocks);

Stream renderReference(const Shape &shape, const Stream &a, const Stream &b, int channels,
                       int sidechainChannels, const std::vector<int> &blocks,
                       const MixSetting &mix = {},
                       entangle::ChainVariant variant = entangle::ChainVariant::Shipped);

Stream renderPureWet(const Shape &shape, const Stream &a, const Stream &b, int channels,
                     int sidechainChannels, const std::vector<int> &blocks);

Stream shapedBySafetyStages(const Stream &raw, float gainDb, bool blockDc, bool clip);

/** The dial state every routing leg runs: p = 0, 0 dB, bypass off, the level stage parked at amount 0.

    Lifted out of §5.20 by M5 item 02 along with the cell table below: a leg that says nothing about the level
    stage runs what ships, and what ships is not linear in B. Parking it makes the wet path linear, which is
    what lets a fold model and a broadcast model mean anything (§5.20's dial discipline). */
extern const MixSetting kMatrixMix;

/** Parks the level stage on the shell side of a leg, so it matches the `kMatrixMix` the reference renders
    run: amount 0 is bit-exact identity (§5.12's leg for that is `checkLevelStageMovesNothing`), which makes
    the wet path linear in B and keeps a non-linear, history-carrying stage out of the comparisons below. */
bool parkLevelStage(juce::AudioProcessor &processor);

/** The broadband level the routing legs run their material at, and why it is not 0.4.

    D-008's wet gain at `L` = 1024 is 0.798 — eight times the default grain length's — so two uncorrelated
    streams at 0.3 convolve to a peak of 1.26, which is above `SoftClipper::knee` = 1.0. Nothing in a routing
    leg asserts anything about that curve, and the stage parked at amount 0 removes only the normalizer, not
    the clipper: the fold and mean models are linear-in-B arguments, so they are wrong the moment the clipper
    bends the stream. At 0.1 the peak lands near 0.14 and every comparison is against a path that is linear by
    construction. The impulse material is quieter still (response peaks near 0.3) and needs no such care. */
constexpr float kMatrixAmplitude = 0.1f;

/** The human-readable form of a `StreamDelta`, for a section that tallies its own lines. */
juce::String describe(const StreamDelta &delta);

/** One gate-accepted (chA, chB, chOut) triple: the layout a leg can be configured onto and the mapping the
    harness expects to hear out of it. No shape and no run length — those belong to the cell that runs it, and
    the same triple is legitimately run at two shapes.

    The `a`/`b` arrays are hand-written on purpose, not read back from `resolve`: expectations derived from the
    rule would make every consumer self-fulfilling, and a wrong rule would render wrongly while agreeing with
    itself. §5.20's first leg asserts the table against `resolve` for every triple before a single sample is
    rendered, and the cross product's table leg does the same. The table is evidence, not a second decider —
    nothing in the product or this harness routes anything from it, and `ChannelMap.hpp`'s own
    `static_assert`s remain the pins on the rule.

    One definition shared by both binaries (item 02 decision B): two copies of the expected mapping is the bug
    the lift exists to prevent, not the duplication it looks like. */
struct AcceptedLayout
{
    const char *name; ///< "(1,5,1)" — the triple, as every line prints it
    const BusSetup *bus;
    int chA;
    int chB;
    int chOut;
    int processed;
    const char *family;
    std::array<std::uint16_t, engine::kMaxChannels> a{};
    std::array<std::uint16_t, engine::kMaxChannels> b{};
};

/** The whole set `engine::gateLayout` accepts — measured: a mono main with chB in [0, 8] and a
    stereo main with chB in [0, 2]. Nothing refused is in here (D-021 B: no cell for a layout the gate
    declines), and `OfflineRender` §5.21 is the leg that keeps this list equal to the gate's. */
const std::array<AcceptedLayout, 12> &acceptedLayouts();
const AcceptedLayout *acceptedLayout(int chA, int chB) noexcept;

/** One row of a matrix: an accepted layout run at one shape, under the name its section prints. */
struct MatrixCell
{
    const char *name;
    const BusSetup *bus;
    Shape shape;
    int chA;
    int chB;
    int chOut;
    int processed;
    const char *family;
    std::array<std::uint16_t, engine::kMaxChannels> a{};
    std::array<std::uint16_t, engine::kMaxChannels> b{};

    /** 8L — 16 hops at the matrix shape, leaving 2L of room after an impulse at n0 = 0, whose response
        occupies [n0 + L, n0 + 2L − 1]. A run has to clear the bounds its assertions land after, so a driver
        checks this per cell before any leg runs: an assertion mid-transient is not evidence, and shortening a
        section is the likeliest way to break it silently. */
    int runningLengthSamples() const noexcept
    {
        return 8 * EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop);
    }
};

/** An accepted layout at a shape, under its own name unless the section wants a suffix on it. The only way to
    make a `MatrixCell`: the mapping cannot be spelled at a call site. `displayName` and `family` override what
    the triple says, for a row that exists to witness something other than the routing — §5.20's second (1,5,1)
    row is the shape assumption, not a second look at 1/5. */
MatrixCell matrixCellOf(const AcceptedLayout &layout, const Shape &shape,
                        const char *displayName = nullptr, const char *family = nullptr) noexcept;

/** A cell rendered through a fresh shell at its own layout and shape with the stage parked. A leg that needs
    two cells at a time cannot share another's rig; a failed configure prints its own FAIL inside `configure`
    and renders nothing, which the caller's comparison then reports as a shape mismatch. */
Stream renderOf(const MatrixCell &cell, const Stream &a, const Stream *side,
                const std::vector<int> &blocks);

/** What slot `slot` reads, spelled out for the printout: the A source and the B side the table names. */
juce::String slotModelName(const MatrixCell &cell, std::size_t slot);

/** The reference for one slot, rendered alone: one A channel and, on the B side, exactly what the map says
    that slot reads — channel 0 for a broadcast slot, channel `c` for a diagonal one, the whole side for a
    consolidated slot (whose fold the reference computes itself). Nothing else is in the layout, which is the
    point: the claim is that the slot hears no other channel. */
Stream slotModel(const MatrixCell &cell, const Stream &a, std::size_t slot, const Stream &side,
                 const std::vector<int> &blocks);

/** conv(A[a[c]], B[j]) scaled by `scale`, rendered alone on one A channel and one B channel. With scale 1
    this is the broadcast and diagonal model; with 1/chB it is the model for a consolidated slot fed a single
    sidechain channel, because the fold is linear and the mean of one channel is that channel over chB. The
    scale lands on the *output* here and on the *input* in the scheduler, and the rounding difference between
    those two is exactly what kDeltaTolerance is for. */
Stream monoPairModel(const MatrixCell &cell, const Stream &a, std::size_t slot, const Stream &side,
                     std::size_t sideChannel, float scale, const std::vector<int> &blocks);

/** Where a failing expectation writes the stream it was shown.

    Off by default, and staying off is the point (`01` §0 E): a leg that dumps on every failure turns one red
    run into two hundred files, and a default `OfflineRender` run already writes ~20 MB of delta anchors into a
    gitignored directory. A binary opts in by parsing `--dump-wavs` out of its own argv and passing the directory
    it made. Nothing in the harness reads a dumped file back as input — signals stay generated. */
struct FailDump
{
    const juce::File *directory = nullptr;
};

bool expectIdentical(const Stream &got, const Stream &want, const char *what, FailDump dump = {});
bool expectDelayedBitExact(const Stream &out, const Stream &in, int latency, const char *what,
                           FailDump dump = {});
bool expectFinite(const Stream &out, const char *what, FailDump dump = {});

bool writeWav(const juce::File &file, const Stream &data);

/** Feeds a whole stream through the processor as host callbacks, with a gesture applied to the
    shell's parameters before every callback.

    Channels are laid out the way AudioProcessor::getChannelIndexInProcessBlockBuffer wants them:
    main input at 0, sidechain after it, main output aliasing the main input — so this also
    exercises the ingest-then-drain order the scheduler depends on.

    One reset(), before the first block: a sweep is one continuous transport, and re-priming the
    rings each callback would measure the latency pre-fill instead of the ramps a gesture drives.
    §5.19 passes `resetFirst = false`: a leg that measures what a mid-session re-prepare leaves
    behind cannot start by re-priming the rings itself.

    The bus counts are read inside the loop, not once, because a §5.19 gesture changes the layout
    mid-transport: a host hands every callback a buffer sized to the layout in force at that
    callback, and under-provisioning one would have the plugin read past the end of the harness's
    own buffer. `in` and `sideIn` must therefore cover the *widest* phase of the render — the plugin
    reads only what its current layout asks for, so an extra row is ignored rather than a hole.
    `outputRows` grows the captured stream past the count at entry, which is what a render that ends
    wider than it started needs; rows the plugin did not write are not compared by any caller. */
template <class Gesture>
Stream renderSwept(juce::AudioProcessor &processor, const Stream &in, const Stream *sideIn,
                   const std::vector<int> &blocks, Gesture gesture, bool resetFirst = true,
                   int outputRows = 0)
{
    const int rows = std::max(processor.getChannelCountOfBus(true, 0), outputRows);

    Stream out(static_cast<std::size_t>(rows),
               std::vector<float>(static_cast<std::size_t>(totalOf(blocks)),
                                  std::numeric_limits<float>::quiet_NaN()));

    juce::MidiBuffer midi;
    int position = 0;
    int callback = 0;
    if (resetFirst)
        processor.reset(); ///< each pass is a fresh transport, so the rings must start pre-filled

    for (const int samples : blocks)
    {
        /* The gesture runs before the counts are read, so a callback that changes the layout is filled and
           measured as the layout in force for it. Sized first, the switching callback hands the new
           configuration the old layout's buffers and segfaults inside HopScheduler::process. */
        gesture(callback);
        ++callback;

        const int mainChannels = processor.getChannelCountOfBus(true, 0);
        const int sideChannels = processor.getChannelCountOfBus(true, 1);
        const int bufferChannels = std::max(processor.getTotalNumInputChannels(),
                                            processor.getTotalNumOutputChannels());

        juce::AudioBuffer<float> buffer(bufferChannels, samples);
        for (int channel = 0; channel < mainChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(channel, i,
                                 in[static_cast<std::size_t>(channel)]
                                    [static_cast<std::size_t>(position + i)]);
        // Always written, so a connected-but-unfed sidechain reaches the plugin as silence rather
        // than as uninitialised memory.
        for (int channel = 0; channel < sideChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(mainChannels + channel, i,
                                 sideIn != nullptr
                                     ? (*sideIn)[static_cast<std::size_t>(channel)]
                                                [static_cast<std::size_t>(position + i)]
                                     : 0.0f);

        processor.processBlock(buffer, midi);

        const int captured = std::min(rows, processor.getTotalNumOutputChannels());
        for (int channel = 0; channel < captured; ++channel)
            for (int i = 0; i < samples; ++i)
                out[static_cast<std::size_t>(channel)][static_cast<std::size_t>(position + i)] =
                    buffer.getSample(channel, i);
        position += samples;
    }

    return out;
}

/** Swallows the plugin's report lines except where a section is testing that delivery, forwarding
    anything else to stdout the way JUCE's default logger does.

    Needed because the guard in `refreshLayoutReport` passes on this thread: `MessageManager`'s constructor
    records the thread that built it (juce_MessageManager.cpp:38) and `ScopedJuceInitialiser_GUI` builds one on
    `main()`, so every prepare in §5.1…§5.20 writes a line — which is F-018, and without this filter C7 has no
    byte comparison to make against the pre-report baseline. §5.21 replaces it with a counting logger to test
    the delivery itself. */
struct ReportLogFilter : juce::Logger
{
    void logMessage(const juce::String &message) override
    {
        if (message.contains("entangle layout"))
            return;
        std::cout << message << "\n";
    }
};

}
