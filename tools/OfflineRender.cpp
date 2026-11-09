/** Plugin-seam harness: the Mode 1 engine inside EntangleAudioProcessor.

    The only place shell, scheduler and engine meet, so it is where the item-6 acceptance criteria
    live. Every check is a bit-exact or tolerance-bound measurement of the shell rather than an
    assumption about it, and the program is plain console PASS/FAIL with a nonzero exit.

    The checks map to plan/M2-item-6-impl-plan.md §5, with M3 item 1 re-pointing 3 and 5 at the mix
    law and adding 10, M3 item 2 splitting 3 by chain variant and adding 11, and M3 item 4 adding
    12 to 16 for the level stage:
      1  the shell adds nothing       — render vs the same engine+scheduler chain built directly
      2  block-size invariance        — two block sequences summing to the same total
      3  delta anchor                 — the wet window at n0+R+L, then silence; at the dial's centre
                                        the law gives the dry path weight 0, so n0+R is silent too.
                                        The physics run on the raw-wet variant; the shipped chain is
                                        held against itself and its reshaping reported (C3, A9, B6)
      4  declared latency             — R == hop for every pair the UI can express, and L moves it
                                        never
      5  dry path                     — no sidechain is A delayed by R, bit-exact; a connected but
                                        silent bus is exact zeros; the render at the dial's centre is
                                        the wet stream through the two safety stages, with the level
                                        stage parked at amount 0 so that by-hand copy is the model
      6  numeric hygiene              — no NaN or inf, silence in is silence out
      7  headroom                     — peak per configuration and peak versus dial position: the
                                        level is reported for the blocked normalizer's target, the
                                        post-clip ceiling is normative, and B7's engagement and click
                                        figures ride along
      8  lifecycle                    — reset, hop round-trip, layout change mid-session
      9  no throw                     — every (hop, L) pair survives prepareToPlay
      10 mix law                      — the dial endpoints bit-exact at the D-009 levels, the pin
                                        over the whole dial travel, bypass and output gain, and a
                                        parameter state written before M3 still loading
      11 wet DC                       — a 1 % offset on both buses and the DC the convolved path
                                        carries with the blocker out and in (A10)
      12 level stage moves nothing     — R is still one hop with the stage installed, and the delta
                                        anchor arrives at the same sample with the same value
                                        whether or not a correction is behind it (C1, C2)
      13 the L-spread anchored        — settled RMS across the whole shipped L range against the
                                        target, with the same chain minus the stage beside it (C3)
      14 headroom with the stage on   — peak at amount 0 and 1 against the ceiling, including the
                                        shape whose wet path arrives 36 dB under target (C5)
      15 the dial travel, stage in    — nine positions: peak, click ratio against the parked render,
                                        and the centre held to the target (C4)
      16 automating the two controls  — amount and floor swept every callback, plus a stream parked
                                        on the floor: click metric and finiteness (C8)
      17 the fold's level consequence  — a consolidated stereo sidechain against the same material as
                                        a mono sidechain, on the shipped chain, amount 1 against
                                        amount 0: the figure D-018 owed and M9 row 2 owns (M4 item 02)
      18 layout negotiation           — the gate's answer and the host's answer agree for every bus
                                        shape JUCE can hand over; a refusal applies nothing while the
                                        per-bus probes substitute onto layouts the gate accepts; the
                                        sidechain maximum a host is offered follows the gate; and the
                                        mono-main family with a wide sidechain — unreachable from a
                                        host until M4 item 03 — renders as the fold, not as one of its
                                        channels
      19 allocation and re-prepare     — the sizing floor of D-022 A asked of the shell's own
                                        maxHostBlockFor, the reported latency's one input across every
                                        layout and hop, and four things a running transport has to
                                        survive: a mid-session layout change (which lands bit-exactly on
                                        an instance prepared at the destination layout), a layout
                                        re-applied unchanged, a second prepareToPlay with the numbers it
                                        just had, and a grain-dial move the clamp swallows (which must
                                        not re-prepare at H = 256 and must at H = 512). Plus the 28
                                        (hop, grain) pairs driven through a callback, and a refused
                                        layout survived. M4 item 04; the key's equality and the
                                        scheduler's byte figures are ReconfigureTests', and the
                                        whole-plugin footprint prints from tools/PrepareFootprint.
     20 the channel matrix            — nine of the twelve gate-accepted triples, each through the shell at
                                        its own layout: the hand-written mapping table checked against
                                        resolve, the geometry and the first callback's written count,
                                        shell identity, three block plans, A-side and B-side independence
                                        per mapped source with every unmapped output exactly 0.0f, and the
                                        dry path. Plus the two asymmetric resolutions each matching their
                                        own model (no commutativity — F-013), the level stage confined to
                                        one channel with the stage live, and the fold's reciprocal at
                                        chB ∈ {2,3,5,8}. M4 item 06; the section keeps its own wall clock
                                        because criterion 13's bound is on that, not on the suite's.
     21 the layout report            — every field of the line a human in a host reads (D-023), for all
                                        twelve gate-accepted triples, against an authority outside the
                                        report: accessors for the counts, the applied layout for the names,
                                        gateLayout for the verdict pair, clampGrainLength for the running
                                        length. Plus JUCE's own word for each bus shape, surround and
                                        ambisonic included (D-021 B's third trigger, the half answerable
                                        without a host); the prepares=/skipped= pair against what
                                        configureRings was asked to do (D-022 B from outside the class); a
                                        refused layout moving neither the report's configuration nor a
                                        running render; and the log guard on both of its branches. M4 item
                                        08; its C6 is tools/PrepareFootprint's before/after table and its
                                        C7 the sections above, unchanged.

    Inputs are seeded noise in a playing range, not the raw integer ramp this harness used to feed:
    §5.7 asks for a peak-level number, which is meaningless on a stream that reaches 1e6. */

#include <juce_audio_utils/juce_audio_utils.h>

#include <MixEngine.hpp>
#include <GrainEngine.hpp>
#include <HopScheduler.hpp>
#include <DcBlocker.hpp>
#include <LayoutGate.hpp>
#include <LevelNormalizer.hpp>
#include <SoftClipper.hpp>

#include "EngineChain.h"
#include "EntangleProcessor.h"
#include "JuceTransformFactory.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <thread>
#include <vector>

/** Defined by the plugin's shared code, which this harness links rather than recompiling. */
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kMaxBlock = 4096;

/** The library's tolerance, unchanged: an IFFT of a spectral product leaves ~1e-8 where the exact
    result is zero and the OLA ring carries that residue onward. Against a response peak of 1.0 it
    reads as absolute. */
constexpr double kDeltaTolerance = 1e-5;

/** How far the wet-minus-dry reconstruction of §5.5 has to be trusted: the shell sums wet into the
    dry stream in float, so recovering it costs ~2 ulp of the dry sample it was added to. */
constexpr double kWetRecoveryTolerance = 1e-6;

using Stream = std::vector<std::vector<float>>;

struct Shape
{
    int hop;
    int chosen; ///< the dial position; the running L is the shell's clamp of it
};

constexpr Shape kDefault{128, 8192};    ///< both UI defaults
constexpr Shape kTightest{512, 1024};   ///< L = 2H, the least overlap the UI can express
constexpr Shape kLongest{512, 65536};   ///< top of the set, where §4.6's tail number lives

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

/* juce::AudioChannelSet is not a literal type, so these are const rather than constexpr. */
const BusSetup kStereoWithB{"stereo-with-b", juce::AudioChannelSet::stereo(), true,
                            juce::AudioChannelSet::stereo()};
const BusSetup kStereoNoB{"stereo-no-b", juce::AudioChannelSet::stereo(), false,
                          juce::AudioChannelSet::stereo()};
const BusSetup kMonoNoB{"mono-no-b", juce::AudioChannelSet::mono(), false,
                        juce::AudioChannelSet::stereo()};
/* The family D-021 A makes host-selectable: a mono main whose sidechain is wider than the main, which
   resolves to one consolidated channel. Stereo here reproduces the shape §5.17 measures through the
   builder; the 8-wide variant is the ceiling, and the only one no leg could reach before this item. */
const BusSetup kMonoWithB{"mono-with-b", juce::AudioChannelSet::mono(), true,
                          juce::AudioChannelSet::stereo()};
const BusSetup kMonoWithEightB{"mono-with-8-b", juce::AudioChannelSet::mono(), true,
                               juce::AudioChannelSet::discreteChannels(8)};
/* The four shapes §5.20's matrix needs and no leg above used: the whole broadcast family — the mono main
   with one sidechain channel and the stereo main with one, which had never reached a sample anywhere —
   and two of the consolidated widths the matrix samples (odd non-dyadic 1/3 and 1/5; 2 and 8 are already
   above). §5.20 reads them through its own cell table, which carries the expected mapping next to each. */
const BusSetup kMonoWithMonoB{"mono-with-1-b", juce::AudioChannelSet::mono(), true,
                              juce::AudioChannelSet::mono()};
const BusSetup kStereoWithMonoB{"stereo-with-1-b", juce::AudioChannelSet::stereo(), true,
                                juce::AudioChannelSet::mono()};
const BusSetup kMonoWithThreeB{"mono-with-3-b", juce::AudioChannelSet::mono(), true,
                               juce::AudioChannelSet::discreteChannels(3)};
const BusSetup kMonoWithFiveB{"mono-with-5-b", juce::AudioChannelSet::mono(), true,
                              juce::AudioChannelSet::discreteChannels(5)};

std::unique_ptr<juce::AudioProcessor> makeProcessor()
{
    return std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
}

EntangleAudioProcessor &shellOf(juce::AudioProcessor &processor)
{
    return static_cast<EntangleAudioProcessor &>(processor);
}

int indexOfHop(int hop)
{
    for (int index = 0; index < static_cast<int>(EntangleAudioProcessor::hopValues.size()); ++index)
        if (EntangleAudioProcessor::hopValues[static_cast<std::size_t>(index)] == hop)
            return index;
    return -1;
}

int indexOfLength(int length)
{
    for (int index = 0; index < static_cast<int>(EntangleAudioProcessor::grainLengthValues.size());
         ++index)
        if (EntangleAudioProcessor::grainLengthValues[static_cast<std::size_t>(index)] == length)
            return index;
    return -1;
}

bool setChoice(juce::AudioProcessor &processor, const juce::String &parameterId, int index)
{
    auto *choice = dynamic_cast<juce::AudioParameterChoice *>(
        shellOf(processor).apvts.getParameter(parameterId));
    if (choice == nullptr)
    {
        std::cout << "  FAIL no choice parameter named " << parameterId << "\n";
        return false;
    }
    *choice = index; ///< notifies the host, exactly as a host poke would
    return true;
}

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

bool setFloat(juce::AudioProcessor &processor, const juce::String &parameterId, float value)
{
    return setParameter<juce::AudioParameterFloat>(processor, parameterId, value, "float");
}

bool setBool(juce::AudioProcessor &processor, const juce::String &parameterId, bool value)
{
    return setParameter<juce::AudioParameterBool>(processor, parameterId, value, "bool");
}

/** The parameter's denormalised value straight out of the APVTS, for a leg that has to compare what
    a state load produced rather than what a render sounded like. */
float parameterValue(juce::AudioProcessor &processor, const juce::String &parameterId)
{
    auto *raw = shellOf(processor).apvts.getRawParameterValue(parameterId);
    return raw != nullptr ? raw->load() : std::numeric_limits<float>::quiet_NaN();
}

bool applyLayout(juce::AudioProcessor &processor, const BusSetup &bus)
{
    auto layout = processor.getBusesLayout();
    layout.getChannelSet(true, 0) = bus.mainSet;
    layout.getChannelSet(false, 0) = bus.mainSet;
    layout.getChannelSet(true, 1) =
        bus.withSidechain ? bus.sideSet : juce::AudioChannelSet::disabled();
    if (!processor.setBusesLayout(layout))
    {
        std::cout << "FAIL " << bus.name << ": layout rejected\n";
        return false;
    }
    return true;
}

bool configure(juce::AudioProcessor &processor, const BusSetup &bus, const Shape &shape)
{
    if (!applyLayout(processor, bus))
        return false;
    if (!setChoice(processor, EntangleAudioProcessor::hopParameterId, indexOfHop(shape.hop)))
        return false;
    if (!setChoice(processor, EntangleAudioProcessor::grainLengthParameterId,
                   indexOfLength(shape.chosen)))
        return false;

    processor.setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
    processor.prepareToPlay(kSampleRate, kMaxBlock);
    return true;
}

Stream noiseStream(int channels, int samples, unsigned seed, float amplitude)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-amplitude, amplitude);
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] = spread(rng);
    return data;
}

Stream constantStream(int channels, int samples, float value)
{
    return Stream(static_cast<std::size_t>(channels),
                  std::vector<float>(static_cast<std::size_t>(samples), value));
}

Stream impulseStream(int channels, int samples, int at)
{
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples), 0.0f));
    for (int channel = 0; channel < channels; ++channel)
        data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(at)] = 1.0f;
    return data;
}

/** The same material with one channel lit and the rest exactly zero.

    `impulseStream` writes an impulse into every channel at once, which is why no leg above it can
    localise a response to one source channel — the whole content of M4 §7's independence row. §5.20's
    M4 and M5 legs feed one channel at a time from here. */
Stream singleImpulse(int channels, int samples, int channel, int at)
{
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples), 0.0f));
    data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(at)] = 1.0f;
    return data;
}

/** Block sequences summing to exactly `total`, so two renders of one stream line up sample for
    sample instead of by luck — the old harness rendered whatever its block list happened to sum
    to, which silently capped how large an L the delta test could address.

    Zero-length callbacks stay in: they are what exercise the drain accounting. Pattern 0 is the
    M1 list plus a seeded random run, pattern 1 is {1, 4, 4096}. */
std::vector<int> blocksSummingTo(int total, int pattern)
{
    std::vector<int> blocks;
    int remaining = total;
    auto push = [&blocks, &remaining](int wanted) {
        if (remaining <= 0)
            return;
        const int taken = std::min(wanted, remaining);
        blocks.push_back(taken);
        remaining -= taken;
    };

    if (pattern == 0)
    {
        for (const int size : {1, 7, 32, 64, 128, 512, 1000, 2048, 4096})
        {
            blocks.push_back(0);
            push(size);
        }
        std::mt19937 rng(20261022U);
        std::uniform_int_distribution<int> random(kMaxBlock / 4, kMaxBlock);
        while (remaining > 0)
        {
            blocks.push_back(0);
            push(random(rng));
        }
    }
    else
    {
        while (remaining > 0)
            for (const int size : {1, 4, 4096})
            {
                blocks.push_back(0);
                push(size);
            }
    }

    blocks.push_back(0);
    blocks.push_back(0);
    return blocks;
}

/** A uniform callback list: `total` in `size`-sample callbacks, remainder last, no zero-length entries.

    §5.19's local `blocksOf` is `blocksSummingTo(total, 0)` under another name, so the file had no uniform
    plan until §5.20's third block plan asked for one: a steady 512-sample callback is the case the two
    ragged lists cannot separate from the DSP's own hop, and it is the shape a host actually runs. */
std::vector<int> constantBlocks(int total, int size)
{
    std::vector<int> blocks;
    for (int remaining = total; remaining > 0; remaining -= size)
        blocks.push_back(std::min(size, remaining));
    return blocks;
}

int totalOf(const std::vector<int> &blocks)
{
    int total = 0;
    for (int block : blocks)
        total += block;
    return total;
}

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
           measured as the layout that is in force for it. Sized first, the switching callback hands the new
           configuration the old layout's buffers — a 2-processed scheduler draining into a 1-channel output —
           and segfaults inside HopScheduler::process. Every other leg's gesture leaves the counts alone, so
           this ordering only matters to §5.19. */
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

Stream render(juce::AudioProcessor &processor, const Stream &in, const Stream *sideIn,
              const std::vector<int> &blocks)
{
    return renderSwept(processor, in, sideIn, blocks, [](int) {});
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

/** The shipped chain with no shell in it, built by the same builder the shell calls
    (entangle::makeChain) at the configuration the shell is configured to. §5.1 — anything the
    shell converts, delays or gains shows up here and nowhere else. Composing the chain a second way
    would stop this comparing against what ships, which is why the builder is shared.

    The variant is part of the leg's identity, not a switch on convenience: M3 item 2 criterion C1
    requires every leg to say which chain it runs on, because an anchor measured through a chain that
    has quietly gained a high-pass no longer pins the engine's gain. */
Stream renderReference(const Shape &shape, const Stream &a, const Stream &b, int channels,
                       int sidechainChannels, const std::vector<int> &blocks,
                       const MixSetting &mix = {},
                       entangle::ChainVariant variant = entangle::ChainVariant::Shipped)
{
    static const JuceTransformFactory factory; ///< outlives every engine built below it
    auto wet = entangle::makeChain(factory, variant);

    entangle::ChainConfig chain;
    chain.sampleRate = kSampleRate;
    chain.hop = shape.hop;
    chain.grainLength = EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop);
    chain.channels = channels;
    chain.sidechainChannels = sidechainChannels;
    chain.outputChannels = channels;
    chain.maxHostBlock = EntangleAudioProcessor::maxHostBlockFor(kMaxBlock);

    const engine::SchedulerConfig config = entangle::makeSchedulerConfig(chain);
    /* The resolved count, not the bus width: for every configuration the harness drives today the two
        agree, and writing the pointer arrays from the map is what keeps this harness resolving the way
        the product does rather than by index (EngineChain.h's "one builder, two callers"). */
    const int processed = config.map.channels;

    /* Targets before prepare so the coefficients snap onto them, then again on every callback — the
        order EntangleAudioProcessor uses in configureRings and processBlock. A static setting makes
        the two identical; the point of mirroring it is that a moving one stays comparable. */
    const float effectiveDial = entangle::effectiveBlend(mix.dial, sidechainChannels > 0);
    wet->setBlendTarget(effectiveDial);
    wet->setOutputGainTarget(mix.gainDb);
    wet->setBypassTarget(mix.bypass);
    wet->setNormalisationAmountTarget(mix.normalisationAmount);
    wet->setSilenceFloorTarget(mix.floorDb);

    engine::HopScheduler scheduler(*wet);
    scheduler.prepare(config);
    scheduler.reset();

    const int total = totalOf(blocks);
    Stream out(static_cast<std::size_t>(processed),
               std::vector<float>(static_cast<std::size_t>(total),
                                  std::numeric_limits<float>::quiet_NaN()));

    int position = 0;
    for (const int samples : blocks)
    {
        wet->setBlendTarget(effectiveDial);
        wet->setOutputGainTarget(mix.gainDb);
        wet->setBypassTarget(mix.bypass);
        wet->setNormalisationAmountTarget(mix.normalisationAmount);
        wet->setSilenceFloorTarget(mix.floorDb);

        /* Source pointers in, processed pointers out, exactly as the shell hands over its bus arrays:
           the scheduler is what turns the map into the repeated pointer (broadcast) or the folded mean,
           so a harness that pre-resolved by index would be comparing something the product never runs. */
        std::vector<const float *> aPtrs(static_cast<std::size_t>(channels));
        std::vector<const float *> bPtrs(static_cast<std::size_t>(sidechainChannels));
        std::vector<float *> outPtrs(static_cast<std::size_t>(processed));
        for (int channel = 0; channel < channels; ++channel)
            aPtrs[static_cast<std::size_t>(channel)] =
                a[static_cast<std::size_t>(channel)].data() + position;
        for (int channel = 0; channel < sidechainChannels; ++channel)
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data() + position;
        for (int channel = 0; channel < processed; ++channel)
            outPtrs[static_cast<std::size_t>(channel)] =
                out[static_cast<std::size_t>(channel)].data() + position;

        scheduler.process(aPtrs.data(),
                          sidechainChannels > 0 ? bPtrs.data() : nullptr, outPtrs.data(),
                          processed, samples);
        position += samples;
    }
    return out;
}

/** The wet stream alone — the grain engine with no law in front of it — which is what the shell
    renders at the dial's centre, where the dry weight is exactly 0 and the wet weight exactly 1. */
Stream renderPureWet(const Shape &shape, const Stream &a, const Stream &b, int channels,
                     int sidechainChannels, const std::vector<int> &blocks)
{
    static const JuceTransformFactory factory;
    grain::GrainConvolutionEngine wet(factory);

    entangle::ChainConfig chain;
    chain.sampleRate = kSampleRate;
    chain.hop = shape.hop;
    chain.grainLength = EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop);
    chain.channels = channels;
    chain.sidechainChannels = sidechainChannels;
    chain.outputChannels = channels;
    chain.maxHostBlock =
        std::max(kMaxBlock, EntangleAudioProcessor::maxHostBlockSamples);

    const engine::SchedulerConfig config = entangle::makeSchedulerConfig(chain);
    const int processed = config.map.channels;

    engine::HopScheduler scheduler(wet);
    scheduler.prepare(config);
    scheduler.reset();

    const int total = totalOf(blocks);
    Stream out(static_cast<std::size_t>(processed),
               std::vector<float>(static_cast<std::size_t>(total), 0.0f));

    int position = 0;
    for (const int samples : blocks)
    {
        std::vector<const float *> aPtrs(static_cast<std::size_t>(channels));
        std::vector<const float *> bPtrs(static_cast<std::size_t>(sidechainChannels));
        std::vector<float *> outPtrs(static_cast<std::size_t>(processed));
        for (int channel = 0; channel < channels; ++channel)
            aPtrs[static_cast<std::size_t>(channel)] =
                a[static_cast<std::size_t>(channel)].data() + position;
        for (int channel = 0; channel < sidechainChannels; ++channel)
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data() + position;
        for (int channel = 0; channel < processed; ++channel)
            outPtrs[static_cast<std::size_t>(channel)] =
                out[static_cast<std::size_t>(channel)].data() + position;

        scheduler.process(aPtrs.data(),
                          sidechainChannels > 0 ? bPtrs.data() : nullptr, outPtrs.data(),
                          processed, samples);
        position += samples;
    }
    return out;
}

/* The two safety stages re-applied by hand to a raw wet stream, in decision C's order: block the
   convolved samples, fold the law's centre coefficient in, clip last.

   Duplicated from MixEngine::processHop on purpose. Criterion C3 endorses a by-hand reference so the
   shipped chain can still be asserted against something independent, and names the risk that comes
   with it: if the engine's stage order changes and this copy does not, §5.1 — which runs the shell
   against the real builder — is the leg that says so. The blocker is fed a whole channel in one call;
   criterion A6 proves its state advances one sample at a time, so a hop-by-hop feed yields the same
   samples. */
Stream shapedBySafetyStages(const Stream &raw, float gainDb, bool blockDc, bool clip)
{
    Stream out = raw;
    dsp::DcBlocker blocker;
    if (blockDc)
        blocker.prepare(kSampleRate, static_cast<int>(out.size()));
    dsp::SoftClipper clipper;
    const float gain = static_cast<float>(std::pow(10.0, static_cast<double>(gainDb) / 20.0));

    for (std::size_t channel = 0; channel < out.size(); ++channel)
    {
        auto &samples = out[channel];
        if (blockDc)
            blocker.processChannel(channel, samples.data(), samples.size());
        if (gain != 1.0f)
            for (auto &sample : samples)
                sample = gain * sample;
        if (clip)
            clipper.processChannel(samples.data(), samples.size());
    }
    return out;
}

/** The DC component of a render: the largest |mean| over the last `window` samples of any channel,
    which is the spectral DC bin the wet path would carry. Reported over a settled tail rather than
    the whole render, because the head of every pass here is silence. */
double dcComponentOf(const Stream &stream, std::size_t window)
{
    double worst = 0.0;
    for (const auto &channel : stream)
    {
        if (channel.empty())
            continue;
        const std::size_t from = window < channel.size() ? channel.size() - window : 0;
        double sum = 0.0;
        for (std::size_t i = from; i < channel.size(); ++i)
            sum += static_cast<double>(channel[i]);
        worst = std::max(worst, std::abs(sum / static_cast<double>(channel.size() - from)));
    }
    return worst;
}

/** The level a settled stream is actually held at: RMS over the last `window` samples of each
    channel, in the same units as the samples. The head of every pass here is silence and the level
    stage spends its first 200 ms travelling, which is why this is a tail window and not the render. */
std::vector<double> settledRms(const Stream &stream, std::size_t window)
{
    std::vector<double> rms;
    rms.reserve(stream.size());
    for (const auto &channel : stream)
    {
        const std::size_t from = window < channel.size() ? channel.size() - window : 0;
        double sum = 0.0;
        for (std::size_t i = from; i < channel.size(); ++i)
            sum += static_cast<double>(channel[i]) * static_cast<double>(channel[i]);
        rms.push_back(channel.size() > from
                          ? std::sqrt(sum / static_cast<double>(channel.size() - from))
                          : 0.0);
    }
    return rms;
}

/** How much of a render the clipper is actually shaping — the fraction B7 asks for, where "engaged at
    default settings" is either a measured fact or a guess. */
double fractionAboveKnee(const Stream &stream)
{
    std::size_t above = 0;
    std::size_t total = 0;
    for (const auto &channel : stream)
        for (const float sample : channel)
        {
            above += std::abs(sample) > dsp::SoftClipper::knee ? 1U : 0U;
            ++total;
        }
    return total != 0 ? static_cast<double>(above) / static_cast<double>(total) : 0.0;
}

/** 01 criterion 6's click prototype: max |x[n] - 2x[n-1] + x[n-2]| over every channel. */
double secondDifferencePeak(const Stream &stream)
{
    double worst = 0.0;
    for (const auto &channel : stream)
        for (std::size_t i = 2; i < channel.size(); ++i)
        {
            const double d2 = std::abs(static_cast<double>(channel[i])
                                       - 2.0 * static_cast<double>(channel[i - 1])
                                       + static_cast<double>(channel[i - 2]));
            worst = std::max(worst, d2);
        }
    return worst;
}

/** Where a render first clears `threshold` — the index criterion B6 requires not to move when a stage
    is installed, since a stage that delays would move it by exactly its delay. */
std::size_t arrivalIndex(const Stream &stream, double threshold)
{
    const std::size_t length = stream.empty() ? 0 : stream[0].size();
    for (std::size_t i = 0; i < length; ++i)
        for (const auto &channel : stream)
            if (std::abs(static_cast<double>(channel[i])) > threshold)
                return i;
    return std::numeric_limits<std::size_t>::max();
}

bool expectIdentical(const Stream &got, const Stream &want, const char *what)
{
    if (got.size() != want.size())
    {
        std::cout << "  FAIL " << what << ": " << got.size() << " channels vs " << want.size()
                  << "\n";
        return false;
    }
    for (std::size_t channel = 0; channel < got.size(); ++channel)
    {
        if (got[channel].size() != want[channel].size())
        {
            std::cout << "  FAIL " << what << ": ch" << channel << " length "
                      << got[channel].size() << " vs " << want[channel].size() << "\n";
            return false;
        }
        for (std::size_t i = 0; i < got[channel].size(); ++i)
        {
            if (got[channel][i] != want[channel][i])
            {
                std::cout << "  FAIL " << what << ": ch" << channel << " n" << i << " got "
                          << got[channel][i] << " want " << want[channel][i] << "\n";
                return false;
            }
        }
    }
    std::cout << "  ok   " << what << "\n";
    return true;
}

bool expectDelayedBitExact(const Stream &out, const Stream &in, int latency, const char *what)
{
    for (std::size_t channel = 0; channel < out.size(); ++channel)
    {
        for (std::size_t i = 0; i < out[channel].size(); ++i)
        {
            const float expected = i < static_cast<std::size_t>(latency)
                                       ? 0.0f
                                       : in[channel][i - static_cast<std::size_t>(latency)];
            if (out[channel][i] != expected)
            {
                std::cout << "  FAIL " << what << ": ch" << channel << " n" << i << " got "
                          << out[channel][i] << " want " << expected << "\n";
                return false;
            }
        }
    }
    std::cout << "  ok   " << what << ": bit-exact, delayed by " << latency << "\n";
    return true;
}

bool expectFinite(const Stream &out, const char *what)
{
    for (std::size_t channel = 0; channel < out.size(); ++channel)
        for (std::size_t i = 0; i < out[channel].size(); ++i)
            if (!std::isfinite(out[channel][i]))
            {
                std::cout << "  FAIL " << what << ": non-finite at ch" << channel << " n" << i
                          << " = " << out[channel][i] << "\n";
                return false;
            }
    std::cout << "  ok   " << what << ": no NaN, no inf\n";
    return true;
}

double peakOf(const Stream &stream)
{
    double peak = 0.0;
    for (const auto &channel : stream)
        for (const float sample : channel)
            peak = std::max(peak, static_cast<double>(std::abs(sample)));
    return peak;
}

/** Level of a ratio, for the legs that have to quote a reshaping in the unit the decision log uses. */
double decibels(double ratio)
{
    return 20.0 * std::log10(ratio);
}

double hannAt(int i, int length) noexcept
{
    return 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i)
                                 / static_cast<double>(length)));
}

bool writeWav(const juce::File &file, const Stream &data)
{
    juce::WavAudioFormat format;
    auto fileStream = std::make_unique<juce::FileOutputStream>(file);
    if (!fileStream->openedOk())
    {
        std::cout << "  FAIL cannot open " << file.getFullPathName().toRawUTF8() << "\n";
        return false;
    }

    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate(kSampleRate)
                             .withChannelLayout(data.size() > 1 ? juce::AudioChannelSet::stereo()
                                                                : juce::AudioChannelSet::mono())
                             .withBitsPerSample(32)
                             .withSampleFormat(
                                 juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    std::unique_ptr<juce::OutputStream> stream = std::move(fileStream);
    auto writer = format.createWriterFor(stream, options);
    if (writer == nullptr)
    {
        std::cout << "  FAIL writer for " << file.getFileName().toRawUTF8() << "\n";
        return false;
    }

    std::vector<const float *> channels(data.size());
    for (std::size_t channel = 0; channel < data.size(); ++channel)
        channels[channel] = data[channel].data();

    const bool written = writer->writeFromFloatArrays(
        channels.data(), static_cast<int>(data.size()),
        static_cast<int>(data.empty() ? 0 : data[0].size()));
    writer.reset();

    if (!written)
        std::cout << "  FAIL write " << file.getFileName().toRawUTF8() << "\n";
    return written;
}

//==============================================================================
// §5.1 — the shell adds nothing.
bool checkShellAddsNothing(int total)
{
    const auto blocks = blocksSummingTo(total, 0);
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const auto a = noiseStream(channels, total, 20261028U, 0.4f);
    const auto b = noiseStream(sideChannels, total, 90210U, 0.4f);

    const auto throughShell = render(*processor, a, &b, blocks);
    const auto direct = renderReference(kDefault, a, b, channels, sideChannels, blocks);

    std::cout << "[5.1] shell vs the shipped chain (MixEngine over GrainConvolutionEngine) and"
              << " HopScheduler, built by the same builder, H="
              << kDefault.hop << " L=" << kDefault.chosen << "\n";
    return expectIdentical(throughShell, direct, "shell adds nothing, bit-identical");
}

//==============================================================================
// §5.2 — host callback size must not reach the DSP.
bool checkBlockInvariance(int total)
{
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const auto a = noiseStream(channels, total, 5150U, 0.4f);
    const auto b = noiseStream(sideChannels, total, 5151U, 0.4f);

    const auto blocks0 = blocksSummingTo(total, 0);
    const auto blocks1 = blocksSummingTo(total, 1);

    std::cout << "[5.2] block-size invariance over " << blocks0.size() << " and "
              << blocks1.size() << " callbacks, both summing to " << total << "\n";
    const auto first = render(*processor, a, &b, blocks0);
    const auto second = render(*processor, a, &b, blocks1);
    return expectIdentical(first, second, "two block sequences render identically");
}

//==============================================================================
// §5.3 — delta anchor, and the number behind §4.6's tail.
/*  Split in two by M3 item 2 criterion C3. The physics legs — nothing before n0+R+L, a Hann of peak
    1.0 inside the wet window, nothing after n0+R+2L-1 — run on the raw-wet variant: that is the
    assertion that pins the engine's gain, and the shipped chain deliberately reshapes the response
    (F-011), so a high-pass left in front of these legs would stop them meaning anything. The shipped
    chain is then held against the same chain built without the shell, which is where a stage that
    moves the arrival index or the latency is caught (B6), and the reshaping is measured rather than
    assumed (A9). */
bool checkDeltaAnchor(const Shape &shape, const juce::File &outputDirectory, bool keepAudio)
{
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, shape))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const int hop = shape.hop;
    /* n0 sits on the hop grid and above every L in the set the engine can be configured to for
       this shape, so the impulse arrives once the history ring is full rather than while it is
       still filling — which is what makes the response the clean window the library measures. */
    const int n0 = 2 * shape.chosen;
    const int total = 4 * shape.chosen + 2 * hop;
    const auto blocks = blocksSummingTo(total, 0);

    const auto a = impulseStream(channels, total, n0);
    const auto b = constantStream(sideChannels, total, 1.0f);

    /* The four renders this leg needs, each a named variant. All of them come from the one builder,
       so "raw wet" cannot quietly mean a different chain from the shipped one minus two stages. */
    const auto raw = renderReference(shape, a, b, channels, sideChannels, blocks, MixSetting{},
                                     entangle::ChainVariant::RawWet);
    const auto shipped = render(*processor, a, &b, blocks);
    const auto shippedReference = renderReference(shape, a, b, channels, sideChannels, blocks);
    /* The clipper with nothing in front of it. Criterion B6 asks that installing the stage does not
       move the anchor's arrival, and the blocker reshapes the envelope on purpose, so an arrival
       index taken across Shipped and RawWet measures the filter, not a delay. */
    const auto clipperOnly = renderReference(shape, a, b, channels, sideChannels, blocks,
                                             MixSetting{}, entangle::ChainVariant::ClipperOnly);

    const int latency = processor->getLatencySamples();
    const int length = EntangleAudioProcessor::clampGrainLength(shape.chosen, hop);
    std::cout << "[5.3] delta anchor H=" << hop << " L=" << length << " R=" << latency << " n0="
              << n0 << " — physics legs on raw wet, shipped chain held against it below\n";

    bool passed = true;
    double worstWindow = 0.0;
    double worstSilence = 0.0;

    for (std::size_t channel = 0; channel < raw.size() && passed; ++channel)
    {
        const auto &stream = raw[channel];

        /* (a) nothing at all before the wet window opens — including n0 + R, where this harness used
           to find the dry spike. At the dial's centre the law gives the dry path weight exactly 0, so
           the spike is the thing M3 removes rather than the thing that proves the seam works; the dry
           path itself is proven bit-exact at the dial endpoints in §5.10. */
        for (int i = 0; i < n0 + latency + length; ++i)
        {
            const double magnitude = std::abs(static_cast<double>(stream[static_cast<std::size_t>(i)]));
            if (magnitude > kDeltaTolerance)
            {
                std::cout << "  FAIL ch" << channel << " n" << i << " = " << magnitude
                          << " before the wet window\n";
                passed = false;
                break;
            }
            worstSilence = std::max(worstSilence, magnitude);
        }

        ///< (b) the wet window itself, at n0 + R + L + m.
        for (int m = 0; m < length; ++m)
        {
            const double have = static_cast<double>(
                stream[static_cast<std::size_t>(n0 + latency + length + m)]);
            const double deviation = std::abs(have - hannAt(m, length));
            worstWindow = std::max(worstWindow, deviation);
            if (deviation > kDeltaTolerance)
            {
                std::cout << "  FAIL ch" << channel << " wet m=" << m << " = " << have << " want "
                          << hannAt(m, length) << "\n";
                passed = false;
                break;
            }
        }

        ///< (c) nothing after the window clears the tolerance — this is what pins the support.
        for (int i = n0 + latency + 2 * length; i < total; ++i)
        {
            const double magnitude = std::abs(static_cast<double>(stream[static_cast<std::size_t>(i)]));
            if (magnitude > kDeltaTolerance)
            {
                std::cout << "  FAIL ch" << channel << " n" << i << " = " << magnitude
                          << " after the wet window\n";
                passed = false;
                break;
            }
        }
    }

    if (passed)
        std::cout << "  ok   delta: silence up to n" << (n0 + latency + length) << " (no dry spike at n"
                  << (n0 + latency) << "), window over [" << (n0 + latency + length)
                  << ", " << (n0 + latency + 2 * length - 1) << "], worst window deviation "
                  << worstWindow << ", worst silence " << worstSilence << "\n";

    /* §4.6: the tail the host is not told about. The response's support is
       [n0 + R + L, n0 + R + 2L - 1], which loops (a) and (c) just proved by showing nothing outside
       it clears the tolerance — so the support is pinned even though Hann's own taper puts the
       outer samples below any absolute threshold. Measured here instead of quoted: the last sample
       that is still 1e-3 of the response peak, against the analytic end. */
    const int analyticFirst = n0 + latency + length;
    const int analyticLast = n0 + latency + 2 * length - 1;
    int lastAudible = -1;
    for (const auto &channel : raw)
        for (int i = total - 1; i >= 0; --i)
        {
            const double magnitude = std::abs(static_cast<double>(channel[static_cast<std::size_t>(i)]));
            if (magnitude < 1e-3)
                continue;
            lastAudible = std::max(lastAudible, i);
            break;
        }

    const double tailSeconds = static_cast<double>(2 * length - 1 - latency) / kSampleRate;
    std::cout << "       tail: support [" << analyticFirst << ", " << analyticLast
              << "], last sample above 1e-3 at n=" << lastAudible
              << "; beyond the declared R the response runs " << (2 * length - 1 - latency)
              << " samples = " << tailSeconds << " s, and getTailLengthSeconds() declares 0\n";

    if (lastAudible < analyticFirst || lastAudible > analyticLast)
    {
        std::cout << "  FAIL the audible response lies outside its analytic support\n";
        passed = false;
    }

    /* ---- the shipped chain (criterion C3). ----
       The shell's render is held against the same builder's chain, which is §5.1's identity again but
       at this shape; what it buys here is that everything below speaks about what ships without the
       shell's own conversions in the way. */
    passed = expectIdentical(shipped, shippedReference,
                             "shipped chain through the shell == the same chain without it")
             && passed;

    /*  B6: the clipper adds no delay, measured on the response itself. The blocker is deliberately
       out of this comparison — it reshapes the envelope by design, so the only thing that could move
       the arrival index between these two variants is a delay in the clipper, which is what the
       §5 discriminator "insert a one-sample delay in the clipper" is looking for. */
    const std::size_t rawArrival = arrivalIndex(raw, kDeltaTolerance);
    const std::size_t clipperArrival = arrivalIndex(clipperOnly, kDeltaTolerance);
    if (clipperArrival != rawArrival)
    {
        std::cout << "  FAIL the clipper moved the wet arrival: raw wet at n" << rawArrival
                  << ", clipper-only at n" << clipperArrival << "\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   clipper-only arrival at n" << clipperArrival << " == raw wet's ("
                  << "B6: the stage delays nothing; declared latency is §5.4)\n";
    }

    /*  A9: the change the blocker makes to this anchor, at the shipped corner — the measurement the
       cutoff decision is taken from, since the anchor's own DC component is half its peak (a periodic
       Hann's mean is 0.5) and its main lobe reaches only to its first null at 2*sr/L. */
    {
        const auto windowMean = [&](const Stream &stream) {
            double worst = 0.0;
            for (const auto &channel : stream)
            {
                double sum = 0.0;
                for (int m = 0; m < length; ++m)
                    sum += static_cast<double>(
                        channel[static_cast<std::size_t>(analyticFirst + m)]);
                worst = std::max(worst, std::abs(sum / static_cast<double>(length)));
            }
            return worst;
        };

        double worstDifference = 0.0;
        for (std::size_t channel = 0; channel < raw.size(); ++channel)
            for (std::size_t i = 0; i < raw[channel].size(); ++i)
                worstDifference = std::max(
                    worstDifference,
                    std::abs(static_cast<double>(shipped[channel][i])
                             - static_cast<double>(raw[channel][i])));

        std::cout << "       A9 shipped vs raw: peak " << peakOf(raw) << " -> " << peakOf(shipped)
                  << ", DC over the wet window " << windowMean(raw) << " -> " << windowMean(shipped)
                  << ", max |difference| " << worstDifference << " = "
                  << decibels(worstDifference / std::max(peakOf(raw), 1e-30))
                  << " dB below the response peak\n";

        /*  The sweep the corner is chosen from: the same class at every corner the milestone's "a few
           Hz" could mean, run straight over the raw anchor as a physics probe — no shell, no law. The
           window column is the deviation from the analytic Hann; "after support" is the largest sample
           past n0+R+2L-1, where the anchor is exactly zero and anything left is the filter's own
           transient, which is the figure that grows with L. */
        std::cout << "       A9 corner probe on the raw anchor — corner Hz, worst |Hann - filtered|\n"
                     "       over the wet window, filtered peak, max |y| past the analytic support:\n";
        for (const double corner : {0.5, 1.0, 2.0, 5.0, 10.0, 20.0})
        {
            dsp::DcBlocker probe(corner);
            probe.prepare(kSampleRate, static_cast<int>(raw.size()));
            Stream filtered = raw;
            double worstInWindow = 0.0;
            for (std::size_t channel = 0; channel < filtered.size(); ++channel)
            {
                probe.processChannel(channel, filtered[channel].data(), filtered[channel].size());
                for (int m = 0; m < length; ++m)
                    worstInWindow = std::max(
                        worstInWindow,
                        std::abs(static_cast<double>(
                                     filtered[channel][static_cast<std::size_t>(analyticFirst + m)])
                                 - hannAt(m, length)));
            }
            double afterSupport = 0.0;
            for (const auto &channel : filtered)
                for (std::size_t i = static_cast<std::size_t>(analyticLast) + 1; i < channel.size(); ++i)
                    afterSupport = std::max(afterSupport,
                                            std::abs(static_cast<double>(channel[i])));

            std::cout << "         " << corner << " Hz: window " << worstInWindow << ", peak "
                      << peakOf(filtered) << ", past support " << afterSupport;
            if (corner == dsp::DcBlocker::cornerHz)
                std::cout << "   <- shipped";
            std::cout << "\n";
        }
    }

    if (keepAudio && !writeWav(
            outputDirectory.getChildFile(
                juce::String("delta-L") + juce::String(length) + ".wav"),
            shipped))
        passed = false;

    return passed;
}

//==============================================================================
// §5.4 and §5.9 — declared latency over the whole UI space, and no throw anywhere in it.
bool checkLatencyAndPrepareSpace()
{
    std::cout << "[5.4][5.9] every hop x grain-length pair: declared latency and prepareToPlay\n";
    bool passed = true;
    int prepared = 0;

    try
    {
        for (const int hop : EntangleAudioProcessor::hopValues)
        {
            auto processor = makeProcessor();
            if (!applyLayout(*processor, kStereoWithB))
                return false;

            if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId, indexOfHop(hop)))
                return false;

            int latencyAtThisHop = -1;
            for (const int chosen : EntangleAudioProcessor::grainLengthValues)
            {
                if (!setChoice(*processor, EntangleAudioProcessor::grainLengthParameterId,
                               indexOfLength(chosen)))
                    return false;

                processor->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
                processor->prepareToPlay(kSampleRate, kMaxBlock);
                ++prepared;

                const int declared = processor->getLatencySamples();
                const int running = shellOf(*processor).grainLengthSamples();
                const int expected =
                    EntangleAudioProcessor::clampGrainLength(chosen, hop);

                if (declared != hop)
                {
                    std::cout << "  FAIL H=" << hop << " L=" << chosen << ": declared latency "
                              << declared << " != " << hop << "\n";
                    passed = false;
                }
                if (running != expected)
                {
                    std::cout << "  FAIL H=" << hop << " L=" << chosen << ": running L " << running
                              << " != clamped " << expected << "\n";
                    passed = false;
                }
                if (running < 2 * hop || running % hop != 0)
                {
                    std::cout << "  FAIL H=" << hop << " L=" << running
                              << ": below the 2H floor or not a multiple of the hop\n";
                    passed = false;
                }
                if (running > EntangleAudioProcessor::maxGrainToHopRatio * hop)
                {
                    std::cout << "  FAIL H=" << hop << " L=" << running
                              << ": prepared a pair over the measured CPU gate\n";
                    passed = false;
                }
                if (latencyAtThisHop < 0)
                    latencyAtThisHop = declared;
                else if (declared != latencyAtThisHop)
                {
                    std::cout << "  FAIL H=" << hop << ": latency moved with L (" << declared
                              << " vs " << latencyAtThisHop << ")\n";
                    passed = false;
                }
            }
        }
    }
    catch (const std::exception &error)
    {
        std::cout << "  FAIL prepareToPlay let an exception escape: " << error.what() << "\n";
        return false;
    }

    if (passed)
        std::cout << "  ok   " << prepared
                  << " pairs prepared: latency == hop and unchanged as L moves\n";
    return passed;
}

/** The clamp rule itself: every pair the two dials can express must land inside the engine's
    admissible band — a multiple of H, at least 2H for the COLA sum, and at most 128H, the measured
    CPU gate of D-008. Clamping a value that is already admissible must be the identity, or the
    dial's own presets would not survive the rule that guards them. */
bool checkGrainLengthClamp()
{
    std::cout << "[5.9] clampGrainLength\n";
    bool passed = true;
    for (const int hop : EntangleAudioProcessor::hopValues)
        for (const int chosen : EntangleAudioProcessor::grainLengthValues)
        {
            const int clamped = EntangleAudioProcessor::clampGrainLength(chosen, hop);
            const auto complain = [&](const char *what) {
                std::cout << "  FAIL H=" << hop << " L=" << chosen << " -> " << clamped << ": "
                          << what << "\n";
                passed = false;
            };
            if (clamped % hop != 0)
                complain("not a multiple of the hop, so F would not be 2L");
            if (clamped < 2 * hop)
                complain("below the 2H COLA floor");
            if (clamped > EntangleAudioProcessor::maxGrainToHopRatio * hop)
                complain("over the measured CPU gate");
            if (clamped > EntangleAudioProcessor::maxGrainLengthSamples)
                complain("over the cap");
            const bool alreadyAdmissible = chosen >= 2 * hop
                && chosen <= EntangleAudioProcessor::maxGrainToHopRatio * hop
                && chosen % hop == 0;
            if (alreadyAdmissible && clamped != chosen)
                complain("moved a value that was already admissible");
        }

    // The rule must bind inside the shipped set, or the gate is decoration. At H = 128 the ceiling
    // is 16384, so the two longest presets are the ones the measurement excludes; at H = 512 all
    // seven were measured under budget and none may move.
    if (EntangleAudioProcessor::clampGrainLength(32768, 128) != 16384
        || EntangleAudioProcessor::clampGrainLength(65536, 128) != 16384
        || EntangleAudioProcessor::clampGrainLength(65536, 256) != 32768)
    {
        std::cout << "  FAIL the CPU ceiling does not bind where it was measured to\n";
        passed = false;
    }
    for (const int chosen : EntangleAudioProcessor::grainLengthValues)
        if (EntangleAudioProcessor::clampGrainLength(chosen, 512) != chosen)
        {
            std::cout << "  FAIL H=512 moved " << chosen << ", a pair measured under budget\n";
            passed = false;
        }

    if (EntangleAudioProcessor::clampGrainLength(1024, 1024) != 2048)
    {
        std::cout << "  FAIL L = H is not raised to the 2H floor\n";
        passed = false;
    }
    if (EntangleAudioProcessor::clampGrainLength(4096, 65536) != EntangleAudioProcessor::maxGrainLengthSamples)
    {
        std::cout << "  FAIL the cap does not bind when 2H exceeds it\n";
        passed = false;
    }
    // The shipped hop set must never put the two bounds in a fight: the floor has to stay under
    // the ceiling or the clamp returns a value it just declared inadmissible.
    for (const int hop : EntangleAudioProcessor::hopValues)
        if (2 * hop > std::min(EntangleAudioProcessor::maxGrainLengthSamples,
                               EntangleAudioProcessor::maxGrainToHopRatio * hop))
        {
            std::cout << "  FAIL H=" << hop << ": floor and ceiling cross\n";
            passed = false;
        }
    if (passed)
        std::cout << "  ok   " << EntangleAudioProcessor::hopValues.size() * EntangleAudioProcessor::grainLengthValues.size()
                  << " pairs land in [2H, " << EntangleAudioProcessor::maxGrainToHopRatio
                  << "H], admissible ones unchanged\n";
    return passed;
}

//==============================================================================
// §5.5 — the dry path, with B absent, silent, and present.
bool checkDryPath(const Shape &shape, int total)
{
    const auto blocks = blocksSummingTo(total, 0);
    bool passed = true;

    auto processorNoB = makeProcessor();
    if (!configure(*processorNoB, kStereoNoB, shape))
        return false;
    const int channels = processorNoB->getChannelCountOfBus(true, 0);
    const int latency = processorNoB->getLatencySamples();
    const auto a = noiseStream(channels, total, 6060U, 0.4f);

    std::cout << "[5.5] dry path H=" << shape.hop
              << " L=" << EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop) << "\n";

    const auto firstDry = render(*processorNoB, a, nullptr, blocks);
    passed = expectDelayedBitExact(firstDry, a, latency,
                                   "sidechain disabled: A delayed by R, nothing else") &&
             passed;

    auto processorWithB = makeProcessor();
    if (!configure(*processorWithB, kStereoWithB, shape))
        return false;

    /* A bus that is connected but never fed is silence on B, and at the dial's centre the law is
       pure convolution, so the render is exact zeros — not A. The pin of D-011 guards a *disabled*
       bus, which is the case above; a connected bus means the host asked for the effect and gets the
       effect of silence. This is also §2's "silence stays silence": no noise floor, exactly zero. */
    const auto silentBus = render(*processorWithB, a, nullptr, blocks);
    bool silentIsExactZero = true;
    for (const auto &channel : silentBus)
        for (const float sample : channel)
            silentIsExactZero = silentIsExactZero && sample == 0.0f;
    if (!silentIsExactZero)
    {
        std::cout << "  FAIL sidechain connected and silent is not exact zeros\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   sidechain connected and silent at the dial's centre: exact zeros\n";
    }

    const int sideChannels = processorWithB->getChannelCountOfBus(true, 1);
    const auto b = noiseStream(sideChannels, total, 6061U, 0.4f);
    /* The level stage is parked at amount 0 for the renders below. At 1 it is the thing under test,
       and the by-hand two-stage reference further down cannot model a correction it does not
       contain; criterion A4 proves amount 0 is bit-exact unity, so with the dial there the shipped
       chain reduces to exactly blocker → law → clip and the copy is the right reference again. The
       correction itself is §5.13's subject. */
    if (!setFloat(*processorWithB, EntangleAudioProcessor::normalisationAmountParameterId, 0.0f))
        return false;
    const auto withB = render(*processorWithB, a, &b, blocks);
    const auto withoutB = render(*processorWithB, a, nullptr, blocks);

    bool differs = false;
    for (std::size_t channel = 0; channel < withB.size() && !differs; ++channel)
        for (std::size_t i = 0; i < withB[channel].size() && !differs; ++i)
            differs = withB[channel][i] != withoutB[channel][i];

    if (!differs)
    {
        std::cout << "  FAIL sidechain content changed nothing — T3 inverted and did not fire\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   sidechain content changes the render (the old T3 inverts, as it must)\n";
    }

    /* Criterion 3, re-pointed by M3 item 2 criterion C3: at the dial's centre the shell's render is
       the wet stream through the two safety stages and nothing else — no gain, no delay, no
       contribution from the law's dry weights. The reference is the pure-wet engine (no law, no
       stages) with the stages applied by hand in decision C's order, so the leg still pins the
       engine's gain against something that contains no mix stage at all; the duplication is C3's and
       §5.1 is what keeps the copy honest. The tolerance is item 6's, written down rather than assumed. */
    const auto wetOnly = renderPureWet(shape, a, b, channels, sideChannels, blocks);
    const auto centreWet = shapedBySafetyStages(wetOnly, 0.0f, true, true);
    double worst = 0.0;
    bool bitIdentical = true;
    for (std::size_t channel = 0; channel < withB.size(); ++channel)
        for (std::size_t i = 0; i < withB[channel].size(); ++i)
        {
            worst = std::max(worst, std::abs(static_cast<double>(withB[channel][i])
                                             - static_cast<double>(centreWet[channel][i])));
            bitIdentical = bitIdentical && withB[channel][i] == centreWet[channel][i];
        }

    if (worst > kWetRecoveryTolerance)
    {
        std::cout << "  FAIL dial at centre is not the wet stream through the stages: worst "
                  << worst << "\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   dial at centre == raw wet + DC block + soft clip, worst " << worst
                  << (bitIdentical ? " (bit-identical)" : "") << " (tol "
                  << kWetRecoveryTolerance << ")\n";
    }

    /*  The same pair against the unfiltered wet path, for the record: the size of the difference the
       shipped chain makes on program material rather than on a delta, which is the number that made
       the leg above necessary. */
    double worstStaged = 0.0;
    for (std::size_t channel = 0; channel < withB.size(); ++channel)
        for (std::size_t i = 0; i < withB[channel].size(); ++i)
            worstStaged = std::max(worstStaged,
                                   std::abs(static_cast<double>(withB[channel][i])
                                            - static_cast<double>(wetOnly[channel][i])));
    std::cout << "       shipped chain over the same wet path unfiltered: peak " << peakOf(wetOnly)
              << " -> " << peakOf(withB) << ", max |difference| " << worstStaged << "\n";

    return passed;
}

//==============================================================================
// §5.6 — numeric hygiene.
bool checkNumericHygiene(const Shape &shape, int total)
{
    const auto blocks = blocksSummingTo(total, 0);
    bool passed = true;

    struct Case
    {
        const char *what;
        float amplitude;
        bool impulse;
    };

    for (const BusSetup &bus : {kStereoWithB, kMonoNoB})
    {
        auto processor = makeProcessor();
        if (!configure(*processor, bus, shape))
            return false;

        const int channels = processor->getChannelCountOfBus(true, 0);
        const int sideChannels = processor->getChannelCountOfBus(true, 1);

        for (const Case c : {Case{"pattern", 0.4f, false}, Case{"impulse", 0.0f, true},
                             Case{"full-scale noise on both inputs", 1.0f, false}})
        {
            const auto a = c.impulse ? impulseStream(channels, total, total / 4)
                                     : noiseStream(channels, total, 31337U, c.amplitude);
            const auto b = noiseStream(std::max(sideChannels, 1), total, 8675309U, c.amplitude);
            const auto out = render(*processor, a, sideChannels > 0 ? &b : nullptr, blocks);
            passed = expectFinite(out, c.what) && passed;
            std::cout << "       peak " << peakOf(out) << "\n";
        }

        const auto silence = constantStream(channels, total, 0.0f);
        const auto silentOut = render(*processor, silence, nullptr, blocks);
        bool allZero = true;
        for (const auto &channel : silentOut)
            for (const float sample : channel)
                allZero = allZero && sample == 0.0f;
        if (!allZero)
        {
            std::cout << "  FAIL silence in is not silence out\n";
            passed = false;
        }
        else
        {
            std::cout << "  ok   silence in, silence out\n";
        }
    }
    return passed;
}

//==============================================================================
// §5.7 — headroom. M3 item 2 makes one line of this normative — nothing leaves the shipped chain
// above the clipper's ceiling — and keeps the rest a report: the level curve is the blocked
// normalizer's decision input (M3 item 1 §7), not this item's pass condition.
bool reportHeadroom(const Shape &shape, int total)
{
    const auto blocks = blocksSummingTo(total, 0);
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, shape))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    bool passed = true;

    std::cout << "[5.7] headroom at the shipped chain's output. The level curve is reported for the\n"
                 "       blocked normalizer's target decision (M3 item 1 §7); the post-clip peak\n"
                 "       against the ceiling is normative (B7), the engagement and click figures not.\n";

    /*  The four operating points the deferral chain has quoted since D-009, now in before/after
       pairs: the same input through the same law with neither stage, then with both. "Engaged at
       default settings" stops being an assertion this way — the percentage is the fraction of the
       render the clipper had to shape, and the d2 ratio is 01 criterion 6's click metric against that
       unshaped baseline, which is what a memoryless C1 stage is expected to lose rather than gain. */
    for (const float amplitude : {0.4f, 1.0f})
    {
        const auto a = noiseStream(channels, total, 111U, amplitude);
        const auto independent = noiseStream(sideChannels, total, 222U, amplitude);
        const auto correlated = a;

        for (const bool correlatedPair : {false, true})
        {
            const Stream &side = correlatedPair ? correlated : independent;
            const char *pairing = correlatedPair ? "A == B" : "independent";

            const auto out = render(*processor, a, &side, blocks);
            const auto unshaped = renderReference(shape, a, side, channels, sideChannels, blocks,
                                                  MixSetting{}, entangle::ChainVariant::RawWet);
            const double peak = peakOf(out);
            if (peak > dsp::SoftClipper::ceiling + 1e-6)
            {
                std::cout << "  FAIL B7: amplitude " << amplitude << ", " << pairing << ": post-clip\n"
                          << "  FAIL B7: peak " << peak << " exceeds the ceiling "
                          << dsp::SoftClipper::ceiling << "\n";
                passed = false;
            }

            const double shapedClick = secondDifferencePeak(out);
            const double baselineClick = secondDifferencePeak(unshaped);
            std::cout << "       amplitude " << amplitude << ", " << pairing << ": peak "
                      << peakOf(unshaped) << " with neither stage -> " << peak << " shipped"
                      << (peak > 1.0 ? "  (over full scale)" : "") << "; "
                      << (100.0 * fractionAboveKnee(out)) << " % of samples above the knee; d2 "
                      << shapedClick << " vs " << baselineClick << " = "
                      << (baselineClick > 0.0 ? shapedClick / baselineClick : 1.0) << "\n";
        }
    }

    /* Peak versus dial position. The dial is the only gain the stage has under a unity law, so this
       is the sweep the normalizer's target gets chosen from — printed whether or not anything gates
       on it. The endpoints are the two bit-exact dry positions and 0 is pure convolution, which is
       where a correlated pair can sum above full scale. Each position carries three figures: the
       post-clip peak, the fraction of the render above the knee, and 01 criterion 6's click metric
       against the same input at the same static dial with neither stage. A memoryless C1 curve should
       hold the first and lose on the third; a knee that had moved would show up as a jump in the
       second, and a stage that clicked as it shaped would show up in the third. */
    std::cout << "       peak / above-knee % / d2 over the unshaped render, versus dial (criterion:\n"
                 "       no discontinuity across the travel; the level itself is the blocked\n"
                 "       normalizer's question):\n";
    for (const float amplitude : {0.4f, 1.0f})
    {
        const auto a = noiseStream(channels, total, 111U, amplitude);
        const auto independent = noiseStream(sideChannels, total, 222U, amplitude);
        const auto correlated = a;

        for (const bool correlatedPair : {false, true})
        {
            const Stream &side = correlatedPair ? correlated : independent;
            std::cout << "       " << amplitude << (correlatedPair ? " A == B:" : " independent:");
            for (const float dial : {-1.0f, -0.75f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                if (!setFloat(*processor, EntangleAudioProcessor::blendParameterId, dial))
                    return false;
                const auto out = render(*processor, a, &side, blocks);
                const double peak = peakOf(out);
                if (peak > dsp::SoftClipper::ceiling + 1e-6)
                {
                    std::cout << "\n  FAIL B7: amplitude " << amplitude
                              << (correlatedPair ? " A == B" : " independent") << " at dial " << dial
                              << " leaves the chain at " << peak << "\n";
                    passed = false;
                }
                const auto unshaped = renderReference(shape, a, side, channels, sideChannels, blocks,
                                                      MixSetting{dial},
                                                      entangle::ChainVariant::RawWet);
                const double shapedClick = secondDifferencePeak(out);
                const double baselineClick = secondDifferencePeak(unshaped);
                std::cout << " " << dial << "=" << peak << "/"
                          << (100.0 * fractionAboveKnee(out)) << "%/"
                          << (baselineClick > 0.0 ? shapedClick / baselineClick : 1.0);
            }
            std::cout << "\n";
        }
    }

    if (!setFloat(*processor, EntangleAudioProcessor::blendParameterId, 0.0f))
        return false;
    return passed;
}

//==============================================================================
// §5.10 — the mix law through the shell: the dial endpoints, the pin, bypass, output gain, and a
// parameter state that predates the two new parameters.
/** One render with the three mix parameters parked at a setting.

    Every pass starts from reset(), which snaps the mix stage's coefficients onto their targets, so
    what these legs measure is a settled state. The ramps themselves are the Engine console tests'
    subject — they need a stub wet source to be bit-exact about, which the shell cannot offer. */
Stream renderAt(juce::AudioProcessor &processor, const Stream &a, const Stream *b, float dial,
                float gainDb, bool bypassed, const std::vector<int> &blocks)
{
    if (!setFloat(processor, EntangleAudioProcessor::blendParameterId, dial)
        || !setFloat(processor, EntangleAudioProcessor::outputGainParameterId, gainDb)
        || !setBool(processor, EntangleAudioProcessor::bypassParameterId, bypassed))
        return {};
    return render(processor, a, b, blocks);
}

bool checkMixLaw()
{
    const int total = 4 * kDefault.chosen + 2 * kDefault.hop;
    const auto blocks = blocksSummingTo(total, 0);

    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const int latency = processor->getLatencySamples();

    std::cout << "[5.10] mix law through the shell, H=" << kDefault.hop << " L="
              << shellOf(*processor).grainLengthSamples() << ", R=" << latency << "\n";
    bool passed = true;

    /* Criteria 1 and 2, at the D-009 levels rather than a quiet test signal. The correlated pair is
       the one that matters: with A == B the wet stream is at its loudest relative to the dry, so any
       residue the weights left behind (INDEX §8.4 measures cosf(pi/2f) at -4.37e-08) shows up here as
       a difference from A instead of hiding under a rounding. The independent pair is what proves the
       dial picks B at +1 rather than whatever A happens to be. */
    for (const float amplitude : {0.4f, 1.0f})
    {
        const auto a = noiseStream(channels, total, 555U, amplitude);
        const auto independent = noiseStream(sideChannels, total, 666U, amplitude);
        const auto correlated = a;

        for (const bool correlatedPair : {false, true})
        {
            const Stream &side = correlatedPair ? correlated : independent;
            const char *pairing = correlatedPair ? "A == B" : "independent";
            const juce::String at = "amplitude " + juce::String(amplitude) + ", " + pairing;

            const auto minusOne = renderAt(*processor, a, &side, -1.0f, 0.0f, false, blocks);
            passed = expectDelayedBitExact(minusOne, a, latency,
                                    ("dial -1, " + at + ": A by R, bit-exact").toRawUTF8())
                     && passed;
            const auto plusOne = renderAt(*processor, a, &side, 1.0f, 0.0f, false, blocks);
            passed = expectDelayedBitExact(plusOne, side, latency,
                                    ("dial +1, " + at + ": B by R, bit-exact").toRawUTF8())
                     && passed;
        }
    }

    // Criterion 9: with no sidechain bus, the dial runs the effect off at every position it can hold.
    auto pinned = makeProcessor();
    if (!configure(*pinned, kStereoNoB, kDefault))
        return false;
    const int pinnedChannels = pinned->getChannelCountOfBus(true, 0);
    const int pinnedLatency = pinned->getLatencySamples();
    const auto pinInput = noiseStream(pinnedChannels, total, 4711U, 0.4f);

    bool pinnedEverywhere = true;
    /* Every position the dial can hold, on the parameter's own quantum rather than a decimal list
       that would snap onto it anyway. */
    constexpr int kDialPositions =
        static_cast<int>(2.0f / EntangleAudioProcessor::blendStep) + 1;
    for (int step = 0; step < kDialPositions; ++step)
    {
        const float dial = -1.0f + EntangleAudioProcessor::blendStep * static_cast<float>(step);
        if (!setFloat(*pinned, EntangleAudioProcessor::blendParameterId, dial))
            return false;
        if (shellOf(*pinned).effectiveBlendPosition() != -1.0f)
        {
            std::cout << "  FAIL dial " << dial << " reports effective position "
                      << shellOf(*pinned).effectiveBlendPosition() << ", not pure A\n";
            pinnedEverywhere = false;
        }
        if (step % 20 != 0)
            continue; ///< a render at this L costs 258 hops of a 16384-point FFT; the position is the same proof every step

        const auto out = render(*pinned, pinInput, nullptr, blocks);
        for (std::size_t channel = 0; channel < out.size() && pinnedEverywhere; ++channel)
            for (std::size_t i = 0; i < out[channel].size() && pinnedEverywhere; ++i)
            {
                const float want = i < static_cast<std::size_t>(pinnedLatency)
                                       ? 0.0f
                                       : pinInput[channel][i - static_cast<std::size_t>(pinnedLatency)];
                if (out[channel][i] != want)
                {
                    std::cout << "  FAIL dial " << dial << " pinned render differs at ch" << channel
                              << " n" << i << ": " << out[channel][i] << " != " << want << "\n";
                    pinnedEverywhere = false;
                }
            }
    }
    if (pinnedEverywhere)
        std::cout << "  ok   no sidechain: " << kDialPositions
                  << " dial positions report pure A, and every 20th renders A delayed by R,"
                  << " bit-exact\n";
    passed = pinnedEverywhere && passed;

    /* And the pin releases: the same instance with the bus enabled and re-prepared runs the dial it
       was left holding, which is what makes the retained value worth retaining. */
    if (!applyLayout(*pinned, kStereoWithB))
        return false;
    pinned->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
    pinned->prepareToPlay(kSampleRate, kMaxBlock);
    if (!setFloat(*pinned, EntangleAudioProcessor::blendParameterId, 0.25f))
        return false;
    /* Compared against the value the parameter holds rather than the literal written into it: the
       claim is that the dial's own position runs again, not that a decimal survives the round trip
       through JUCE's normalised domain. */
    const float pokedDial = parameterValue(*pinned, EntangleAudioProcessor::blendParameterId);
    const bool restored = shellOf(*pinned).effectiveBlendPosition() == pokedDial;
    if (!restored)
        std::cout << "  FAIL the dial did not come back after the bus appeared: "
                  << shellOf(*pinned).effectiveBlendPosition() << " != " << pokedDial << "\n";
    else
    {
        /* A real B on the bus that just appeared: at a positive dial the dry bus is B, so rendering
           this with silence on the sidechain would prove nothing about the dial and would report
           silence for the honest reason. */
        const auto pinSide = noiseStream(pinnedChannels, total, 4712U, 0.4f);
        const auto live = render(*pinned, pinInput, &pinSide, blocks);
        bool audible = false;
        for (std::size_t channel = 0; channel < live.size() && !audible; ++channel)
            for (std::size_t i = 0; i < live[channel].size() && !audible; ++i)
                audible = live[channel][i] != 0.0f;
        if (!audible)
            std::cout << "  FAIL the dial came back but renders silence\n";
        else
            std::cout << "  ok   sidechain appears, re-prepare: the dial's own 0.25 runs again\n";
        passed = audible && passed;
    }

    // Criterion 10: bypass, on a dial position where the effect is audible.
    const auto a = noiseStream(channels, total, 8181U, 0.4f);
    const auto b = noiseStream(sideChannels, total, 8182U, 0.4f);
    const auto inEffect = renderAt(*processor, a, &b, -0.4f, 0.0f, false, blocks);
    const int latencyBeforeBypass = processor->getLatencySamples();
    const auto bypassed = renderAt(*processor, a, &b, -0.4f, 0.0f, true, blocks);
    const int latencyAfterBypass = processor->getLatencySamples();

    passed = expectDelayedBitExact(bypassed, a, latency,
                            "bypassed: A by R, wet and dry-B contributions exactly zero")
             && passed;
    if (latencyBeforeBypass != latencyAfterBypass)
    {
        std::cout << "  FAIL bypass moved the declared latency: " << latencyBeforeBypass << " then "
                  << latencyAfterBypass << "\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   bypass left the declared latency at " << latencyAfterBypass << "\n";
    }

    /* The dial survives the cycle: unbypass and re-render, and the settled numbers are the ones from
        before it — bit for bit, because nothing in a bypass touches the blend target. */
    const auto afterCycle = renderAt(*processor, a, &b, -0.4f, 0.0f, false, blocks);
    passed = expectIdentical(inEffect, afterCycle, "the dial survives a bypass cycle") && passed;

    /*  Criterion 11, re-pointed at the shipped chain: the output gain, against the same wet stream
        DC-blocked, scaled by the parameter's own figure and clipped last. That is what the law
        produces at the dial's centre, where the dry weights are exactly 0 and the coefficient
        arithmetic has nothing left to round — and the order is the point: the clip bounds the *gained*
        sum, so 6 dB and 12 dB are expected to sit on the ceiling rather than 6 and 12 dB above a
        clipped signal. A reference scaled after the clip would pass the bound and miss the order. */
    const auto wet = renderPureWet(kDefault, a, b, channels, sideChannels, blocks);
    /* Parked at amount 0 for the same reason §5.5 parks it: the reference is the wet stream through
       the two stages by hand, and a third stage that is doing something would move every rung of the
       gain ladder for a reason this leg says nothing about. The gain order is the claim here, and it
       is the same claim at amount 0 or 1. */
    if (!setFloat(*processor, EntangleAudioProcessor::normalisationAmountParameterId, 0.0f))
        return false;
    for (const float gainDb : {-60.0f, -12.0f, 0.0f, 6.0f, 12.0f})
    {
        const auto gained = renderAt(*processor, a, &b, 0.0f, gainDb, false, blocks);
        const auto want = shapedBySafetyStages(wet, gainDb, true, true);
        double worst = 0.0;
        bool bitIdentical = true;
        for (std::size_t channel = 0; channel < gained.size(); ++channel)
            for (std::size_t i = 0; i < gained[channel].size(); ++i)
            {
                worst = std::max(worst, std::abs(static_cast<double>(gained[channel][i])
                                                 - static_cast<double>(want[channel][i])));
                bitIdentical = bitIdentical && gained[channel][i] == want[channel][i];
            }
        const double relative = worst / std::max(peakOf(want), 1e-30);
        const juce::String label =
            juce::String(gainDb, 0) + " dB: measured gain matches 10^(dB/20), then the clip";
        if (!(relative <= 1e-6))
        {
            std::cout << "  FAIL " << label << ": peak-relative deviation " << relative << "\n";
            passed = false;
        }
        else
        {
            std::cout << "  ok   " << label << " (peak-relative " << relative
                      << ", tol 1e-6" << (bitIdentical ? ", bit-identical" : "") << ")\n";
        }
    }

    /* Finite and monotone across the whole range. At the tightest shape rather than the default one
       because the property is about the parameter's arithmetic, not the wet path's length, and 73
       renders of 5120 samples is the difference between a leg that runs and one that does not. */
    {
        const int sweepTotal = 4 * kTightest.chosen + 2 * kTightest.hop;
        const auto sweepBlocks = blocksSummingTo(sweepTotal, 0);
        auto sweeper = makeProcessor();
        if (!configure(*sweeper, kStereoWithB, kTightest))
            return false;
        const int sweepChannels = sweeper->getChannelCountOfBus(true, 0);
        const int sweepSide = sweeper->getChannelCountOfBus(true, 1);
        const auto sweepA = noiseStream(sweepChannels, sweepTotal, 909U, 0.4f);
        const auto sweepB = noiseStream(sweepSide, sweepTotal, 910U, 0.4f);

        double previous = -1.0;
        bool monotone = true;
        bool finite = true;
        for (int decibels = -60; decibels <= 12 && monotone; ++decibels)
        {
            const auto out = renderAt(*sweeper, sweepA, &sweepB, 0.0f,
                                      static_cast<float>(decibels), false, sweepBlocks);
            const double peak = peakOf(out);
            finite = finite && std::isfinite(peak);
            if (peak < previous)
            {
                std::cout << "  FAIL output gain is not monotone: " << decibels << " dB peaks "
                          << peak << " after " << previous << "\n";
                monotone = false;
            }
            previous = peak;
        }
        if (monotone && finite)
            std::cout << "  ok   output gain finite and monotone over all 73 dB values in range\n";
        passed = monotone && finite && passed;
    }

    if (!setFloat(*processor, EntangleAudioProcessor::outputGainParameterId, 0.0f)
        || !setBool(*processor, EntangleAudioProcessor::bypassParameterId, false)
        || !setFloat(*processor, EntangleAudioProcessor::blendParameterId, 0.0f))
        return false;
    return passed;
}

/** Criterion 14: a parameter state written before this item exists must load with its own three
    values exactly where they were and the new two at their defaults.

    The state is produced rather than hand-written, so it is the current format minus the two new
    children — which is precisely what the previous format was, without this harness having to know
    how JUCE happens to serialise a parameter. */
bool checkParameterStateCompatibility()
{
    std::cout << "[5.10] a parameter state from before the new parameters\n";
    bool passed = true;

    auto writer = makeProcessor();
    if (!configure(*writer, kStereoWithB, Shape{128, 4096}))
        return false;
    if (!setFloat(*writer, EntangleAudioProcessor::blendParameterId, 0.37f))
        return false;
    /* The writer's own held value, not the 0.37 that was asked for: the blend's 0.01 quantum is not
       dyadic, so 0.37 comes back a few ULP off its decimal. The claim under test is that the number
       in the file is the number that runs after the load, which is a statement about the round trip,
       not about decimal literals surviving it. */
    const float writtenBlend = parameterValue(*writer, EntangleAudioProcessor::blendParameterId);

    /* The state this build writes, minus the two parameters that did not exist before this item —
        which is what the previous format was, without the harness having to know how JUCE happens to
        serialise a parameter. The removals have to actually remove something, or the "legacy" state
        below would be today's state and the leg would prove nothing. */
    auto legacyState = shellOf(*writer).apvts.copyState();
    const auto indexOfParameter = [&legacyState](const juce::String &parameterId) {
        /* JUCE writes every parameter as <PARAM id="..." value="..."/>, so the parameter's name is a
           property of the child, not the child's type. */
        const auto child = legacyState.getChildWithProperty("id", parameterId);
        return child.isValid() ? legacyState.indexOf(child) : -1;
    };
    const int gainIndex = indexOfParameter(EntangleAudioProcessor::outputGainParameterId);
    const int bypassIndex = indexOfParameter(EntangleAudioProcessor::bypassParameterId);
    const int amountIndex = indexOfParameter(EntangleAudioProcessor::normalisationAmountParameterId);
    const int floorIndex = indexOfParameter(EntangleAudioProcessor::silenceFloorParameterId);
    if (gainIndex < 0 || bypassIndex < 0 || amountIndex < 0 || floorIndex < 0)
    {
        std::cout << "  FAIL the state has no outputGain/bypass/normalisationAmount/silenceFloor\n"
                  << "  FAIL child to strip (" << gainIndex << ", " << bypassIndex << ", "
                  << amountIndex << ", " << floorIndex << ")\n";
        return false;
    }
    /* Highest index first, or an earlier removal invalidates the index of a later one. */
    const std::array<int, 4> indices{{gainIndex, bypassIndex, amountIndex, floorIndex}};
    std::vector<int> descending(indices.begin(), indices.end());
    std::sort(descending.begin(), descending.end(), std::greater<int>());
    for (const int index : descending)
        legacyState.removeChild(index, nullptr);
    const juce::String legacy = legacyState.toXmlString();

    /* Read into an instance left at a *different* hop and grain length, so a load that ignored the
       file would be visible rather than accidentally correct. Fresh instance, fresh parameters: that
       is the state a host is in when it opens a project, and the new parameters' defaults are what
       must survive a file that says nothing about them. */
    auto reader = makeProcessor();
    if (!configure(*reader, kStereoWithB, kTightest))
        return false;
    reader->setStateInformation(legacy.toRawUTF8(), legacy.getNumBytesAsUTF8());

    struct Expectation
    {
        juce::String parameterId;
        float want;
        const char *what;
    };

    const Expectation expectations[] = {
        { EntangleAudioProcessor::blendParameterId, writtenBlend, "blend from the file" },
        { EntangleAudioProcessor::hopParameterId, 1.0f, "hop index for 128 from the file" },
        { EntangleAudioProcessor::grainLengthParameterId, 2.0f, "grain index for 4096 from the file" },
        { EntangleAudioProcessor::outputGainParameterId, 0.0f, "output gain at its default" },
        { EntangleAudioProcessor::bypassParameterId, 0.0f, "bypass at its default" },
        { EntangleAudioProcessor::normalisationAmountParameterId,
          EntangleAudioProcessor::normalisationAmountMax, "normalisation amount at its default" },
        { EntangleAudioProcessor::silenceFloorParameterId,
          EntangleAudioProcessor::silenceFloorDefaultDb, "silence floor at its default" },
    };

    for (const Expectation &e : expectations)
    {
        const float got = parameterValue(*reader, e.parameterId);
        if (got != e.want)
        {
            std::cout << "  FAIL " << e.what << ": " << got << ", want " << e.want << "\n";
            passed = false;
        }
        else
        {
            std::cout << "  ok   " << e.what << ": " << got << "\n";
        }
    }

    /* And the file's own blend still runs after the load: the pin is not involved on a bus that is
       present, so the effective position is the loaded one. */
    if (shellOf(*reader).effectiveBlendPosition() != writtenBlend)
    {
        std::cout << "  FAIL the loaded dial does not run: "
                  << shellOf(*reader).effectiveBlendPosition() << " != " << writtenBlend << "\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   the loaded dial is what runs\n";
    }

    return passed;
}

//==============================================================================
// §5.8 — lifecycle.
bool checkLifecycle(int total)
{
    const auto blocks = blocksSummingTo(total, 0);
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const auto a = noiseStream(channels, total, 4242U, 0.4f);
    const auto b = noiseStream(sideChannels, total, 4243U, 0.4f);

    std::cout << "[5.8] lifecycle on one processor instance\n";
    bool passed = true;

    const auto first = render(*processor, a, &b, blocks);
    const auto again = render(*processor, a, &b, blocks);
    passed = expectIdentical(first, again, "reset() between renders is bit-identical") && passed;

    ///< Hop out and back: the render must come home to the same numbers.
    if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId,
                   indexOfHop(EntangleAudioProcessor::hopValues[2]))
        || !setChoice(*processor, EntangleAudioProcessor::hopParameterId,
                      indexOfHop(kDefault.hop)))
        return false;
    processor->prepareToPlay(kSampleRate, kMaxBlock);
    const auto returned = render(*processor, a, &b, blocks);
    passed = expectIdentical(first, returned, "hop 128 -> 256 -> 128 returns the original render") &&
             passed;

    /* Layout change mid-session. Re-prepare clears the rings and re-applies the pre-fill, so the
       interrupted stream is legitimately not the uninterrupted one — what must hold is that it is
       finite and that the processor still renders the original stream afterwards. */
    auto interrupted = makeProcessor();
    if (!configure(*interrupted, kStereoWithB, kDefault))
        return false;
    interrupted->reset();

    int position = 0;
    juce::MidiBuffer midi;
    for (const int samples : blocks)
    {
        juce::AudioBuffer<float> buffer(4, samples);
        for (int channel = 0; channel < channels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(channel, i,
                                 a[static_cast<std::size_t>(channel)]
                                    [static_cast<std::size_t>(position + i)]);
        for (int channel = 0; channel < sideChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(channels + channel, i,
                                 b[static_cast<std::size_t>(channel)]
                                    [static_cast<std::size_t>(position + i)]);
        interrupted->processBlock(buffer, midi);
        position += samples;

        if (position == total / 2)
        {
            if (sideChannels > 0)
                interrupted->setBusesLayout(
                    [&] {
                        auto layout = interrupted->getBusesLayout();
                        layout.getChannelSet(true, 1) = juce::AudioChannelSet::mono();
                        return layout;
                    }());
            interrupted->processorLayoutsChanged();
            break;
        }
    }

    const auto afterLayoutChange = render(*interrupted, a, &b, blocks);
    passed = expectFinite(afterLayoutChange, "render after processorLayoutsChanged") && passed;

    ///< Mono and stereo both render, and neither is silent or NaN.
    for (const BusSetup &bus : {kStereoWithB, kMonoNoB})
    {
        auto mono = makeProcessor();
        if (!configure(*mono, bus, kDefault))
            return false;
        const int busChannels = mono->getChannelCountOfBus(true, 0);
        const auto busIn = noiseStream(busChannels, total, 999U, 0.4f);
        const auto busOut = render(*mono, busIn, nullptr, blocks);
        if (!expectFinite(busOut, bus.name))
            passed = false;
        std::cout << "       " << bus.name << " peak " << peakOf(busOut) << "\n";
    }

    return passed;
}

//==============================================================================
// §5.11 — the DC the convolved path carries when both inputs are offset (criterion A10).
/*  The transform primitive's own note is that the DC bin of the spectral product is (Σa)·(Σb): an
    offset on either bus reaches the output multiplied through the other, which is the whole reason a
    blocker exists. Measured here rather than left as that prose, at the default H and L, with the
    stage out and in over the same rendered stream so the two readings cannot be two runs. */
bool checkWetDcComponent()
{
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const int length = EntangleAudioProcessor::clampGrainLength(kDefault.chosen, kDefault.hop);
    constexpr float kOffset = 0.01f; ///< the 1 % of full scale the criterion names

    /*  Long enough for both readings to be steady, and keyed to the wet path rather than to the
       corner. The DC term in the spectral product builds up over the transform's own history, which
       is a few L on this shape, and the blocker then needs its documented bound from there. Sizing
       the render from the bound alone was tried and is wrong: mutation 2 (corner ×100) shrinks the
       bound to a few hundred samples, the wet baseline never reaches its plateau inside the render,
       and the leg reports a ratio between two things that have not settled. The anchor-length renders
       earlier in this file are shorter than either allowance, so their residue is still in flight —
       which is why that reading is reported beside the settled one rather than passed over: how long
       the decay takes is a property of the corner, and the corner is what this leg is evidence for. */
    dsp::DcBlocker probe;
    probe.prepare(kSampleRate, channels);
    const int settling = probe.settlingSamples();
    const int total = 4 * kDefault.chosen + settling + length;
    const auto blocks = blocksSummingTo(total, 0);

    const auto a = constantStream(channels, total, kOffset);
    const auto b = constantStream(sideChannels, total, kOffset);

    const auto raw = renderReference(kDefault, a, b, channels, sideChannels, blocks, MixSetting{},
                                     entangle::ChainVariant::RawWet);
    const auto shipped = render(*processor, a, &b, blocks);

    /* The plateau reading, and the same reading taken where the anchor-length renders stop — that
       one is mid-transient, and it is reported rather than skipped because how long the residue
       takes is a property of the corner this leg is evidence for. */
    const std::size_t window = static_cast<std::size_t>(length);
    const int anchorTotal = 4 * kDefault.chosen + 2 * kDefault.hop;
    Stream early = shipped;
    for (auto &channel : early)
        channel.resize(static_cast<std::size_t>(anchorTotal));

    const double rawDc = dcComponentOf(raw, window);
    const double shippedDc = dcComponentOf(shipped, window);
    const double earlyDc = dcComponentOf(early, static_cast<std::size_t>(anchorTotal) / 4);

    std::cout << "[5.11] DC on both buses, default H and L (criterion A10 — the reason the stage is\n"
                 "       in the chain, measured instead of quoted from src/Fft/README.md)\n";
    std::cout << "       " << (100.0 * kOffset) << " % offset on A and B: the wet path carries DC "
              << rawDc << " with the blocker out and " << shippedDc << " once the stage has settled\n"
              << "       (" << decibels(shippedDc / std::max(rawDc, 1e-300)) << " dB); " << earlyDc
              << " = " << decibels(earlyDc / std::max(rawDc, 1e-300))
              << " dB is still in flight where a\n";
    std::cout << "       " << anchorTotal << "-sample anchor render ends. Documented settling bound "
              << settling << " samples = " << (static_cast<double>(settling) / kSampleRate) << " s\n";

    if (!(shippedDc <= 1e-6 * rawDc))
    {
        std::cout << "  FAIL A10: the settled wet path still carries " << shippedDc << " of DC,\n"
                  << "  FAIL A10: above 1e-6 of the " << rawDc << " it carries without the stage\n";
        return false;
    }
    std::cout << "  ok   A10: the wet path's DC is inside 1e-6 of itself once the stage has settled\n";
    return true;
}

//==============================================================================
// §5.12 — the level stage moves neither the delay figures nor the delta anchor's onset (C1, C2).
/*  The stage reads the hop it is scaling, so it cannot defer anything: criterion C1 is that claim on
    the shipped chain rather than on a stub. C2's onset half is the same fact from the other end — the
    gate is closed while the wet path is silence, so a delta anchor must arrive at the same sample,
    with the same value, whether or not a correction is installed behind it. Where the two renders
    part company later is the correction working, and that is §5.13's subject, not a failure here. */
bool checkLevelStageMovesNothing(const Shape &shape)
{
    const int n0 = 2 * shape.chosen;
    const int total = 4 * shape.chosen + 2 * shape.hop;
    const auto blocks = blocksSummingTo(total, 0);

    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, shape))
        return false;
    const int channels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const int latency = processor->getLatencySamples();

    const auto a = impulseStream(channels, total, n0);
    const auto b = constantStream(sideChannels, total, 1.0f);

    const auto corrected = renderReference(shape, a, b, channels, sideChannels, blocks);
    const auto uncorrected = renderReference(shape, a, b, channels, sideChannels, blocks,
                                             MixSetting{}, entangle::ChainVariant::NoNormalizer);
    const auto throughShell = render(*processor, a, &b, blocks);

    std::cout << "[5.12] level stage moves nothing, H=" << shape.hop << " L="
              << EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop)
              << " R=" << latency << " (Shipped vs NoNormalizer)\n";
    bool passed = true;

    if (latency != shape.hop)
    {
        std::cout << "  FAIL C1: the stage moved the declared latency: " << latency << ", want "
                  << shape.hop << "\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   C1: declared latency is still one hop (" << latency
                  << ") with the stage installed\n";
    }

    passed = expectIdentical(throughShell, corrected,
                             "C1: the shipped chain with the stage, with and without the shell")
             && passed;

    const std::size_t correctedArrival = arrivalIndex(corrected, kDeltaTolerance);
    const std::size_t uncorrectedArrival = arrivalIndex(uncorrected, kDeltaTolerance);
    if (correctedArrival != uncorrectedArrival)
    {
        std::cout << "  FAIL C2: the stage delayed the anchor: arrival " << uncorrectedArrival
                  << " without it, " << correctedArrival << " with\n";
        passed = false;
    }
    else
    {
        /* How far the two chains stay bit-identical, and the size of the first gap once they part, are
           printed rather than gated. The gate is on the three *readings*, and this anchor leaves them
           mismatched from its first sample — an impulse on A against a constant 1.0 on B — so with a
           hop long enough for the response to arrive while A's reading is still up, the stage engages
           on the response's own first hop. That is the correction doing what it is told, not a delay,
           and the delay claim is the arrival index above. */
        std::size_t divergence = 0;
        while (divergence < static_cast<std::size_t>(total)
               && corrected[0][divergence] == uncorrected[0][divergence])
            ++divergence;
        const double gap = divergence < static_cast<std::size_t>(total)
                             ? decibels(static_cast<double>(corrected[0][divergence])
                                        / static_cast<double>(uncorrected[0][divergence]))
                             : 0.0;
        std::cout << "  ok   C2: arrival index unchanged at n=" << correctedArrival
                  << " with the same sample value; bit-identical to n=" << divergence
                  << (divergence < static_cast<std::size_t>(total)
                          ? (juce::String(" (") + juce::String(gap, 3) + " dB)").toRawUTF8()
                          : "")
                  << "; peak " << peakOf(uncorrected) << " -> " << peakOf(corrected) << "\n";
    }
    return passed;
}

//==============================================================================
// §5.13 — the L-spread is anchored (C3). The flatness criterion the milestone states and item 01
// reported without gating.
/*  The whole wet gain table is 1/C with C = L/(2H), so the wet level tracks L; D-008 measured the
    response-peak version of that as a 37.45 dB span. This is the same table on program material —
    the settled RMS, with the correction on and the same chain minus the stage printed beside it — and
    the leg that shows the feature does something rather than merely existing. */
bool checkLevelAnchoring()
{
    /* H = 512 is the only shipped hop that admits the whole grain-length set: the CPU gate caps L at
       128H and 128 x 512 is the top of the list. At any other hop the top rows clamp, and a spread
       measured over a range the UI cannot reach is not the spread under test. The second table fixes L
       and moves the hop because that is the other half of the (L, H) pair the wet gain table is built
       from, and the two axes turn out not to carry the level the same way. */
    constexpr int hop = 512;
    constexpr double kSpreadToleranceDb = 2.0;
    const int total = 3 * static_cast<int>(kSampleRate);
    const std::size_t tail = static_cast<std::size_t>(kSampleRate);
    const auto blocks = blocksSummingTo(total, 0);
    /* Quiet enough that the clipper is idle once the level is held near the input's own: a render the
       clipper is shaping is not a measurement of the level stage. The engagement % is printed with
       every row so that claim is visible rather than assumed. */
    const float amplitude = 0.1f;

    std::cout << "[5.13] the L-spread anchored to the target (C3). Independent A and B at amplitude "
              << amplitude << ",\n       dial at the centre, RMS over the last second; target ="
                 " sqrt(rmsA . rmsB) over the same window\n";
    bool passed = true;
    double worstAnchored = 0.0;
    std::vector<double> bareDeviations;

    const auto row = [&](int rowHop, int chosen) {
        const Shape shape{rowHop, chosen};
        const int length = EntangleAudioProcessor::clampGrainLength(chosen, rowHop);
        if (length != chosen)
        {
            std::cout << "  FAIL the shape clamped L=" << chosen << " to " << length
                      << ", so this row is not the shipped value\n";
            return false;
        }

        const auto a = noiseStream(2, total, 31337U, amplitude);
        const auto b = noiseStream(2, total, 73331U, amplitude);
        const auto anchored = renderReference(shape, a, b, 2, 2, blocks);
        const auto bare = renderReference(shape, a, b, 2, 2, blocks, MixSetting{},
                                          entangle::ChainVariant::NoNormalizer);
        const auto aRms = settledRms(a, tail);
        const auto bRms = settledRms(b, tail);
        const auto anchoredRms = settledRms(anchored, tail);
        const auto bareRms = settledRms(bare, tail);

        double rowWorst = 0.0;
        for (std::size_t channel = 0; channel < anchoredRms.size(); ++channel)
        {
            const double target = std::sqrt(aRms[channel] * bRms[channel]);
            if (anchoredRms[channel] <= 0.0 || target <= 0.0 || bareRms[channel] <= 0.0)
            {
                std::cout << "  FAIL L=" << chosen << " ch" << channel
                          << " has no level to compare (0 or NaN)\n";
                return false;
            }
            const double deviation = decibels(anchoredRms[channel]) - decibels(target);
            bareDeviations.push_back(decibels(bareRms[channel]) - decibels(target));
            rowWorst = std::max(rowWorst, std::abs(deviation));
        }
        worstAnchored = std::max(worstAnchored, rowWorst);

        std::cout << "       H=" << rowHop << " L=" << chosen << " (C="
                  << length / (2 * rowHop) << "): anchored " << decibels(anchoredRms[0])
                  << " dBFS, worst-channel deviation " << rowWorst << " dB; bare "
                  << decibels(bareRms[0]) << " (" << bareDeviations.back() << " dB); peak "
                  << peakOf(anchored) << ", " << (100.0 * fractionAboveKnee(anchored))
                  << " % above the knee\n";

        if (rowWorst > kSpreadToleranceDb)
        {
            std::cout << "  FAIL C3: H=" << rowHop << " L=" << chosen << " is " << rowWorst
                      << " dB from the target, over the " << kSpreadToleranceDb << " dB gate\n";
            return false;
        }
        return true;
    };

    std::cout << "       grain length at the widest hop the UI allows:\n";
    for (const int chosen : EntangleAudioProcessor::grainLengthValues)
        passed = row(hop, chosen) && passed;

    std::cout << "       hop at the default grain length:\n";
    for (const int rowHop : EntangleAudioProcessor::hopValues)
        passed = row(rowHop, kDefault.chosen) && passed;

    const double spread = *std::max_element(bareDeviations.begin(), bareDeviations.end())
                        - *std::min_element(bareDeviations.begin(), bareDeviations.end());
    std::cout << "       no-stage baseline spans " << spread << " dB over both tables; D-008's\n"
              << "       response-peak figure for the L axis alone is 37.45 dB, so that figure is a\n"
              << "       property of the delta response, not of program level. Worst corrected\n"
              << "       deviation " << worstAnchored << " dB against the " << kSpreadToleranceDb
              << " dB gate\n";
    return passed;
}

//==============================================================================
// §5.14 — headroom with the correction on (C5).
/*  Normalising a quiet wet path *raises* it, so D-009's peaks are the without-the-stage figures and
    this is the gate the milestone's own risk list asks for. The forced case is H = 64 with L = 8192:
    C = 64, so the wet path arrives 36 dB under the target and the stage asks for its whole range,
    which is where a ceiling that is not really a ceiling would show up. */
bool checkLevelHeadroom()
{
    struct Case
    {
        const char *what;
        Shape shape;
        float amplitude;
        bool correlated;
        float gainDb;
        float floorDb;
    };

    const Case cases[] = {
        { "default shape, A == B, full scale, 0 dB out", kDefault, 1.0f, true, 0.0f,
          EntangleAudioProcessor::silenceFloorDefaultDb },
        { "default shape, A == B, full scale, +12 dB out", kDefault, 1.0f, true, 12.0f,
          EntangleAudioProcessor::silenceFloorDefaultDb },
        /* The forced boost, in three rows. At H = 64 and L = 8192 the wet path arrives with C = 64, so a
           -40 dBFS input pair leaves it well under the shipped -60 dBFS floor: the first row is decision
           D's promise that nothing is amplified down there (amount 0 and amount 1 render identically).
           The floor is what decides where "down there" is, so the next two rows move it to -90 dB and the
           stage gets its full +36 dB to ask for — a ceiling that is only nominal shows up there, and only
           approached from below. */
        { "H=64 L=8192 (-40 dBFS in), floor -60: gate holds, no boost", Shape{64, 8192}, 0.01f, true,
          0.0f, EntangleAudioProcessor::silenceFloorDefaultDb },
        { "H=64 L=8192 (-40 dBFS in), floor -90, 0 dB out", Shape{64, 8192}, 0.01f, true, 0.0f, -90.0f },
        { "H=64 L=8192 (-40 dBFS in), floor -90, +12 dB out", Shape{64, 8192}, 0.01f, true, 12.0f,
          -90.0f },
    };

    std::cout << "[5.14] headroom with the correction on (C5): the peak through the shipped chain at\n"
                 "       amount 0 and amount 1, against the clipper's ceiling of "
              << dsp::SoftClipper::ceiling << "\n";
    bool passed = true;

    for (const Case &c : cases)
    {
        const int total = 4 * c.shape.chosen + 2 * c.shape.hop;
        const auto blocks = blocksSummingTo(total, 0);
        auto processor = makeProcessor();
        if (!configure(*processor, kStereoWithB, c.shape))
            return false;
        const int channels = processor->getChannelCountOfBus(true, 0);
        const int sideChannels = processor->getChannelCountOfBus(true, 1);
        const auto a = noiseStream(channels, total, 4242U, c.amplitude);
        const auto independent = noiseStream(sideChannels, total, 8484U, c.amplitude);
        const Stream &side = c.correlated ? a : independent;
        if (!setFloat(*processor, EntangleAudioProcessor::silenceFloorParameterId, c.floorDb))
            return false;

        double offPeak = 0.0;
        double onPeak = 0.0;
        for (const float amount : {0.0f, 1.0f})
        {
            if (!setFloat(*processor, EntangleAudioProcessor::normalisationAmountParameterId, amount)
                || !setFloat(*processor, EntangleAudioProcessor::outputGainParameterId, c.gainDb))
                return false;
            const auto out = render(*processor, a, &side, blocks);
            const double peak = peakOf(out);
            if (!std::isfinite(peak))
            {
                std::cout << "  FAIL C5: " << c.what << " at amount " << amount << " is not finite\n";
                passed = false;
            }
            if (peak > dsp::SoftClipper::ceiling + 1e-6)
            {
                std::cout << "  FAIL C5: " << c.what << " at amount " << amount
                          << " leaves the chain at " << peak << ", over the ceiling\n";
                passed = false;
            }
            if (amount == 0.0f)
                offPeak = peak;
            else
                onPeak = peak;
        }
        std::cout << "       " << c.what << ": peak " << offPeak << " with the stage parked at 0 -> "
                  << onPeak << " at 1 ("
                  << decibels(onPeak / std::max(offPeak, 1e-30)) << " dB of move)\n";
    }
    return passed;
}

//==============================================================================
// §5.15 — the nine-position dial travel with the stage installed (C4).
/*  The law's shape across the travel is D-010's contribution and the correction does not touch it, so
    the level curve is reported rather than gated. What gates is that putting a slow gain ramp in front
    of the law adds no step: the click metric is held against the same render with the stage parked at
    amount 0, where the only ramps left are the shell's own — which is exactly the baseline the
    criterion names ("no click above the step the shipped ramps already produce"). */
bool checkLevelAcrossTravel()
{
    constexpr double kClickRatioGate = 2.0;
    constexpr double kCentreToleranceDb = 2.0;
    const int total = 2 * static_cast<int>(kSampleRate);
    const std::size_t tail = static_cast<std::size_t>(kSampleRate) / 2;
    const auto blocks = blocksSummingTo(total, 0);

    std::cout << "[5.15] the dial travel with the stage in (C4): peak / click ratio against the same\n"
                 "       render parked at amount 0 / centre RMS against the target; the level curve\n"
                 "       itself is D-010's and is reported, not gated.\n";
    bool passed = true;

    for (const float amplitude : {0.4f, 1.0f})
    {
        const auto a = noiseStream(2, total, 555U, amplitude);
        const auto independent = noiseStream(2, total, 666U, amplitude);

        for (const bool correlatedPair : {false, true})
        {
            const Stream &side = correlatedPair ? a : independent;
            auto processor = makeProcessor();
            if (!configure(*processor, kStereoWithB, kDefault))
                return false;

            std::cout << "       " << amplitude << (correlatedPair ? " A == B:" : " independent:");
            for (const float dial : {-1.0f, -0.75f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                double peak[2] = {0.0, 0.0};
                double click[2] = {0.0, 0.0};
                int index = 0;
                for (const float amount : {0.0f, 1.0f})
                {
                    if (!setFloat(*processor, EntangleAudioProcessor::blendParameterId, dial)
                        || !setFloat(*processor,
                                     EntangleAudioProcessor::normalisationAmountParameterId, amount))
                        return false;
                    const auto out = render(*processor, a, &side, blocks);
                    peak[index] = peakOf(out);
                    click[index] = secondDifferencePeak(out);
                    if (!(peak[index] <= dsp::SoftClipper::ceiling + 1e-6))
                    {
                        std::cout << "\n  FAIL C4: dial " << dial << " at amount " << amount
                                  << " leaves the chain at " << peak[index] << "\n";
                        passed = false;
                    }
                    ++index;
                }
                const double ratio = click[0] > 0.0 ? click[1] / click[0] : 1.0;
                if (ratio > kClickRatioGate)
                {
                    std::cout << "\n  FAIL C4: dial " << dial << ": the stage multiplies the click by "
                              << ratio << ", over the " << kClickRatioGate << " gate\n";
                    passed = false;
                }
                std::cout << " " << dial << "=" << peak[1] << "/" << ratio;
            }
            std::cout << "\n";

            /* The centre is the one position where the output *is* the corrected wet path, so it is
               the position that can be held against the target rather than against another render. */
            if (!setFloat(*processor, EntangleAudioProcessor::blendParameterId, 0.0f)
                || !setFloat(*processor, EntangleAudioProcessor::normalisationAmountParameterId, 1.0f))
                return false;
            const auto centre = render(*processor, a, &side, blocks);
            const auto centreRms = settledRms(centre, tail);
            const auto aRms = settledRms(a, tail);
            const auto bRms = settledRms(side, tail);
            for (std::size_t channel = 0; channel < centreRms.size(); ++channel)
            {
                const double target = std::sqrt(aRms[channel] * bRms[channel]);
                const double deviation = decibels(centreRms[channel]) - decibels(target);
                std::cout << "       ch" << channel << " at the centre: " << decibels(centreRms[channel])
                          << " dBFS against the target " << decibels(target) << " = " << deviation
                          << " dB\n";
                if (std::abs(deviation) > kCentreToleranceDb)
                {
                    std::cout << "  FAIL C4: the centre is " << deviation
                              << " dB from the target, over the " << kCentreToleranceDb << " dB gate\n";
                    passed = false;
                }
            }
        }
    }
    return passed;
}

//==============================================================================
// §5.16 — automating the two new controls at audio rate (C8).
/*  Decision F is that the two controls need no sample-domain ramp of their own because the stage
    already ramps what they set. Automating a dial every callback is how that gets tested rather than
    asserted; the same pass with the stream parked exactly on the floor covers the risk the item's §7
    names — unity and correction alternating one hop each. */
bool checkLevelZipper()
{
    constexpr double kClickRatioGate = 2.0;
    const int total = 2 * static_cast<int>(kSampleRate);
    const auto blocks = blocksSummingTo(total, 0);
    const auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const auto a = noiseStream(2, total, 20261031U, 0.4f);
    const auto b = noiseStream(2, total, 31031999U, 0.4f);

    /* A full trip up the dial and back over the pass, so every position is crossed at audio rate. */
    const auto triangle = [&blocks](int callback) {
        const double phase = static_cast<double>(callback)
                           / static_cast<double>(std::max<std::size_t>(blocks.size(), 2));
        return phase < 0.5 ? phase * 2.0 : 2.0 - phase * 2.0;
    };

    std::cout << "[5.16] automating amount and floor every callback (C8): click metric against the\n"
                 "       static render, and a stream deliberately parked on the floor\n";
    bool passed = true;

    struct Sweep
    {
        const char *what;
        const juce::String &parameterId;
        float from;
        float to;
        float restore; ///< where the leg leaves the control: its shipped default
    };
    const Sweep sweeps[] = {
        { "amount 0 -> 1 -> 0", EntangleAudioProcessor::normalisationAmountParameterId,
          EntangleAudioProcessor::normalisationAmountMin, EntangleAudioProcessor::normalisationAmountMax,
          EntangleAudioProcessor::normalisationAmountMax },
        { "floor -90 -> -20 -> -90 dB", EntangleAudioProcessor::silenceFloorParameterId,
          EntangleAudioProcessor::silenceFloorMinDb, EntangleAudioProcessor::silenceFloorMaxDb,
          EntangleAudioProcessor::silenceFloorDefaultDb },
    };

    for (const Sweep &s : sweeps)
    {
        if (!setFloat(*processor, s.parameterId, s.from))
            return false;
        const auto staticRender = render(*processor, a, &b, blocks);
        const auto swept = renderSwept(*processor, a, &b, blocks, [&](int callback) {
            const double value = static_cast<double>(s.from)
                               + static_cast<double>(s.to - s.from) * triangle(callback);
            setFloat(*processor, s.parameterId, static_cast<float>(value));
        });
        if (!expectFinite(swept, s.what))
            passed = false;

        const double sweptClick = secondDifferencePeak(swept);
        const double staticClick = secondDifferencePeak(staticRender);
        const double ratio = staticClick > 0.0 ? sweptClick / staticClick : 1.0;
        std::cout << "       " << s.what << ": d2 " << sweptClick << " swept vs " << staticClick
                  << " static = " << ratio << " (peak " << peakOf(swept) << ")\n";
        if (ratio > kClickRatioGate)
        {
            std::cout << "  FAIL C8: " << s.what << " multiplies the click by " << ratio
                      << ", over the " << kClickRatioGate << " gate\n";
            passed = false;
        }
        if (!setFloat(*processor, s.parameterId, s.restore))
            return false;
    }

    /* Parked on the floor: the readings of a noise pair sit within a hair of the gate, so unity and
       correction alternate hop by hop. The gain ramp is what keeps that a swell rather than a click,
       and this is the leg that says whether it does. */
    const float floorDb = -20.0f;
    const float amplitudeOnFloor = 0.1f * static_cast<float>(std::sqrt(3.0));
    const auto parkedA = noiseStream(2, total, 777U, amplitudeOnFloor);
    const auto parkedB = noiseStream(2, total, 778U, amplitudeOnFloor);
    if (!setFloat(*processor, EntangleAudioProcessor::silenceFloorParameterId, floorDb))
        return false;
    const auto parked = render(*processor, parkedA, &parkedB, blocks);
    const auto uncorrected = renderReference(kDefault, parkedA, parkedB, 2, 2, blocks,
                                             MixSetting{0.0f, 0.0f, false, 0.0f, floorDb},
                                             entangle::ChainVariant::Shipped);
    if (!expectFinite(parked, "a stream parked on the floor"))
        passed = false;
    const double parkedClick = secondDifferencePeak(parked);
    const double uncorrectedClick = secondDifferencePeak(uncorrected);
    const double parkedRatio = uncorrectedClick > 0.0 ? parkedClick / uncorrectedClick : 1.0;
    std::cout << "       parked at the " << floorDb
              << " dB floor: d2 " << parkedClick << " vs " << uncorrectedClick
              << " uncorrected = " << parkedRatio << ", peak " << peakOf(parked) << "\n";
    if (parkedRatio > kClickRatioGate)
    {
        std::cout << "  FAIL C8: gate chatter at the floor multiplies the click by " << parkedRatio
                  << ", over the " << kClickRatioGate << " gate\n";
        passed = false;
    }
    return passed;
}

/* D-018 records the fold's level consequence as arithmetic — "a −3 dB fold drops envB, so the level
   stage's target moves −3 dB at amount = 1" — and INDEX §7 says no figure exists for it. This is the
   figure, measured through the shared builder rather than derived: one mono main, a consolidated stereo
   sidechain against the same material arriving as a mono sidechain, dial at the centre so the wet path
   carries the render, and both against the stage parked at amount 0 so the correction is separable from
   the fold. Reported, not tuned; whether any of it is audible is M9 row 2's (item 02 criterion 9). */
bool checkConsolidatedSidechainLevel()
{
    std::cout << "[5.17] the fold's level consequence (M4 item 02). Mono main, decorrelated stereo\n"
                 "       sidechain against channel 0 of that pair as a mono sidechain; dial at the\n"
                 "       centre, RMS over the last second of a three-second render.\n";

    const Shape shape = kTightest;
    const int total = 3 * static_cast<int>(kSampleRate);
    const std::size_t tail = static_cast<std::size_t>(kSampleRate);
    const auto blocks = blocksSummingTo(total, 0);
    const float amplitude = 0.1f;

    const auto a = noiseStream(1, total, 20261103U, amplitude);
    const auto wide = noiseStream(2, total, 4242424U, amplitude);
    const Stream narrow{wide[0]};

    const MixSetting corrected{};
    const MixSetting parked{0.0f, 0.0f, false, 0.0f, dsp::LevelNormalizer::defaultFloorDb};

    const auto foldOn = renderReference(shape, a, wide, 1, 2, blocks, corrected);
    const auto foldOff = renderReference(shape, a, wide, 1, 2, blocks, parked);
    const auto monoOn = renderReference(shape, a, narrow, 1, 1, blocks, corrected);
    const auto monoOff = renderReference(shape, a, narrow, 1, 1, blocks, parked);

    if (!expectFinite(foldOn, "the consolidated render")
        || !expectFinite(monoOn, "the mono-sidechain render"))
        return false;

    const double foldLevel = settledRms(foldOn, tail)[0];
    const double monoLevel = settledRms(monoOn, tail)[0];
    const double foldRaw = settledRms(foldOff, tail)[0];
    const double monoRaw = settledRms(monoOff, tail)[0];

    if (foldLevel <= 0.0 || monoLevel <= 0.0 || foldRaw <= 0.0 || monoRaw <= 0.0)
    {
        std::cout << "  FAIL the fold comparison has no level to compare (0 or NaN)\n";
        return false;
    }

    /* The parked pair isolates the mean itself: the wet path is linear in B for a fixed A — the only
       gain in it is the analytic 1/sum of the OLA windows, D-008 — so the mean of two decorrelated
       channels lands 3.01 dB under one of them whatever the level stage then decides. That much is
       gated; the two figures below it are the ones D-018 owes. */
    const double rawDelta = decibels(foldRaw) - decibels(monoRaw);
    const double correctedDelta = decibels(foldLevel) - decibels(monoLevel);
    const double appliedOnFold = decibels(foldLevel) - decibels(foldRaw);
    const double appliedOnMono = decibels(monoLevel) - decibels(monoRaw);

    std::cout << "       stage parked at amount 0: mono side " << decibels(monoRaw)
              << " dBFS, folded " << decibels(foldRaw) << " → fold delta " << rawDelta << " dB\n";
    std::cout << "       correction applied: " << appliedOnMono << " dB on a mono side, "
              << appliedOnFold << " dB folded → the fold moves the correction "
              << (appliedOnFold - appliedOnMono) << " dB\n";
    std::cout << "       corrected output: " << decibels(monoLevel) << " vs " << decibels(foldLevel)
              << " dBFS → fold delta " << correctedDelta << " dB\n";

    if (std::abs(rawDelta + 3.0103) > 0.5)
    {
        std::cout << "  FAIL the fold sits " << rawDelta
                  << " dB under a single sidechain channel; two decorrelated channels averaged\n"
                       "       are -3.01 dB, and this is the render-level proof of that arithmetic\n";
        return false;
    }
    std::cout << "  ok   the fold measures within 0.5 dB of -3.01 dB on the uncorrected wet path\n";
    return true;
}

/* ------------------------------------------------------------------- §5.18 ---
   Layout negotiation from the host's side (M4 item 03).

   `LayoutGateTests` sweeps the pure function; this is the half that needs a real `AudioProcessor`: the
   shell's `AudioChannelSet` → counts translation, what JUCE does with a refusal on each of its two paths,
   what the plugin advertises once the gate widens the sidechain, and whether the family the gate newly
   admits renders as the fold. Every probe compares the *host's* answer against `engine::gateLayout` for
   the same triple rather than against a sentence about it (INDEX §3 rule 2). */
struct LayoutRow
{
    juce::AudioChannelSet mainIn;
    juce::AudioChannelSet side;
    juce::AudioChannelSet mainOut;
};

int sideCountOf(const juce::AudioChannelSet &side) noexcept
{
    return side.isDisabled() ? 0 : side.size();
}

juce::AudioProcessor::BusesLayout layoutOf(juce::AudioProcessor &processor, const LayoutRow &row)
{
    auto layout = processor.getBusesLayout();
    layout.getChannelSet(true, 0) = row.mainIn;
    layout.getChannelSet(true, 1) = row.side;
    layout.getChannelSet(false, 0) = row.mainOut;
    return layout;
}

bool gateAccepts(const LayoutRow &row) noexcept
{
    return engine::gateLayout(row.mainIn.size(), sideCountOf(row.side), row.mainOut.size()).verdict
           == engine::LayoutVerdict::accepted;
}

std::ostream &operator<<(std::ostream &out, const LayoutRow &row)
{
    return out << "(" << row.mainIn.size() << "," << sideCountOf(row.side) << ","
               << row.mainOut.size() << ")";
}

const char *verdictName(engine::LayoutVerdict verdict) noexcept
{
    switch (verdict)
    {
        case engine::LayoutVerdict::accepted:
            return "accepted";
        case engine::LayoutVerdict::mainWidth:
            return "mainWidth";
        case engine::LayoutVerdict::mainCountMismatch:
            return "mainCountMismatch";
        case engine::LayoutVerdict::channelRule:
            return "channelRule";
    }
    return "?";
}

const char *ruleName(engine::ChannelMapStatus status) noexcept
{
    switch (status)
    {
        case engine::ChannelMapStatus::ok:
            return "ok";
        case engine::ChannelMapStatus::empty:
            return "empty";
        case engine::ChannelMapStatus::asymmetric:
            return "asymmetric";
        case engine::ChannelMapStatus::countMismatch:
            return "countMismatch";
        case engine::ChannelMapStatus::exceedsCapacity:
            return "exceedsCapacity";
    }
    return "?";
}

/** The rule `isBusesLayoutSupported` enforced before this item, restated so the leg can name which
    accepted rows a host could not reach until now. Deliberately not `LayoutGateTests`' copy — that is a
    different binary, and this one is measuring what the shipped shell used to answer. */
bool legacyGateAccepts(int chA, int chB, int chOut)
{
    if (chOut != chA)
        return false;
    if (chA != 1 && chA != 2)
        return false;
    return chB == 0 || chB == 1 || chB == 2;
}

/** The counts `configureRings` reads off the live buses, built the way it builds them. */
entangle::ChainConfig liveChainConfig(juce::AudioProcessor &processor)
{
    auto *sidechainBus = processor.getBus(true, 1);
    entangle::ChainConfig chain;
    chain.sampleRate = kSampleRate;
    chain.hop = kDefault.hop;
    chain.grainLength = EntangleAudioProcessor::clampGrainLength(kDefault.chosen, kDefault.hop);
    chain.channels = processor.getChannelCountOfBus(true, 0);
    chain.sidechainChannels =
        sidechainBus != nullptr && sidechainBus->isEnabled() ? processor.getChannelCountOfBus(true, 1) : 0;
    chain.outputChannels = processor.getChannelCountOfBus(false, 0);
    chain.maxHostBlock = EntangleAudioProcessor::maxHostBlockSamples;
    return chain;
}

bool checkLayoutNegotiation()
{
    std::cout << "[5.18] layout negotiation at the host seam (M4 item 03). Every probe goes through the\n"
                 "       public checkBusesLayoutSupported on a fresh instance and is compared against\n"
                 "       engine::gateLayout for the same triple, so the shell's channel-set translation is\n"
                 "       measured rather than assumed.\n";
    bool passed = true;

    /* Eight shapes per bus: the two the shell shipped, the two disabled states, quad and 5.1 (both of
       which `resolve` takes and the gate refuses), the ceiling at 8, an ambisonic first-order set — 4
       channels under a different name, which is criterion 8's naming test arriving in the main sweep —
       and 9, one past the ceiling. */
    const std::vector<juce::AudioChannelSet> busShapes = {
        juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(),
        juce::AudioChannelSet::stereo(), juce::AudioChannelSet::quadraphonic(),
        juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::octagonal(),
        juce::AudioChannelSet::ambisonic(1), juce::AudioChannelSet::discreteChannels(9)};

    int probes = 0;
    int disagreements = 0;
    int acceptedByGate = 0;
    int newlyReachable = 0;
    for (const auto &mainIn : busShapes)
        for (const auto &side : busShapes)
            for (const auto &mainOut : {mainIn, juce::AudioChannelSet::mono(),
                                        juce::AudioChannelSet::stereo()})
            {
                const auto processor = makeProcessor();
                const LayoutRow row{mainIn, side, mainOut};
                const auto decision = engine::gateLayout(row.mainIn.size(), sideCountOf(row.side),
                                                         row.mainOut.size());
                const bool host = processor->checkBusesLayoutSupported(layoutOf(*processor, row));
                const bool gate = decision.verdict == engine::LayoutVerdict::accepted;
                ++probes;
                if (gate)
                {
                    ++acceptedByGate;
                    if (!legacyGateAccepts(row.mainIn.size(), sideCountOf(row.side), row.mainOut.size()))
                        ++newlyReachable;
                }
                if (host != gate)
                {
                    ++disagreements;
                    if (disagreements <= 5)
                        std::cout << "  FAIL host=" << (host ? "yes" : "no") << " gate="
                                  << (gate ? "yes" : "no") << " for " << row << " ["
                                  << verdictName(decision.verdict) << "/" << ruleName(decision.rule)
                                  << "]\n";
                }
            }
    std::cout << "       " << probes << " layouts probed, " << acceptedByGate << " accepted ("
              << newlyReachable << " of them new to this item), " << disagreements
              << " host/gate disagreements\n";
    if (disagreements != 0)
        passed = false;
    if (newlyReachable == 0)
    {
        std::cout << "  FAIL nothing became reachable: D-021 A's widening is not in the gate\n";
        passed = false;
    }

    /* A refusal has to be clean: `setBusesLayout` asks `canApplyBusesLayout`, whose default is the gate,
       and applies nothing when it says no (juce_AudioProcessor.cpp:118-131). */
    {
        struct Refusal
        {
            const char *what;
            LayoutRow row;
        };
        const std::vector<Refusal> refusals = {
            {"stereo main, quad side",
             {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::quadraphonic(),
              juce::AudioChannelSet::stereo()}},
            {"stereo main, 5.1 side",
             {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::create5point1(),
              juce::AudioChannelSet::stereo()}},
            {"stereo main, 8 discrete side",
             {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::octagonal(),
              juce::AudioChannelSet::stereo()}},
            {"stereo main, 9 discrete side",
             {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::discreteChannels(9),
              juce::AudioChannelSet::stereo()}},
            {"mono main, 9 discrete side",
             {juce::AudioChannelSet::mono(), juce::AudioChannelSet::discreteChannels(9),
              juce::AudioChannelSet::mono()}},
            {"quad main, no side",
             {juce::AudioChannelSet::quadraphonic(), juce::AudioChannelSet::disabled(),
              juce::AudioChannelSet::quadraphonic()}},
            {"5.1 main over 5.1 side",
             {juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1(),
              juce::AudioChannelSet::create5point1()}},
            {"mono in, stereo out",
             {juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled(),
              juce::AudioChannelSet::stereo()}},
            {"stereo in, mono out",
             {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled(),
              juce::AudioChannelSet::mono()}},
            {"stereo in, ambisonic out",
             {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo(),
              juce::AudioChannelSet::ambisonic(1)}},
        };

        auto processor = makeProcessor();
        if (!applyLayout(*processor, kStereoWithB))
            return false;
        const auto before = processor->getBusesLayout();
        int applied = 0;
        int moved = 0;
        for (const auto &refusal : refusals)
        {
            if (gateAccepts(refusal.row))
            {
                std::cout << "  FAIL the refusal table holds an accepted row " << refusal.row << "\n";
                passed = false;
            }
            if (processor->setBusesLayout(layoutOf(*processor, refusal.row)))
                ++applied;
            if (processor->getBusesLayout() != before)
                ++moved;
        }
        std::cout << "       " << refusals.size() << " refusals through setBusesLayout: " << applied
                  << " wrongly applied, " << moved << " left with a moved layout\n";
        if (applied != 0 || moved != 0)
        {
            std::cout << "  FAIL a refusal was not clean — the whole-layout setter must apply nothing\n";
            passed = false;
        }
    }

    /* The other path is JUCE's, and it substitutes rather than refusing: the per-bus probes hand the
       request to getNextBestLayout (juce_AudioProcessor.cpp:231-325), which tries the request, then the
       opposite bus set to it, then that bus's default, then every bus set to the request. The leg cannot
       stop that; what it asserts is that whatever lands is a layout the gate accepts (F-015). */
    {
        auto processor = makeProcessor();
        if (!applyLayout(*processor, kStereoWithB))
            return false;
        std::cout << "       per-bus probes from (2,2,2) — the substitution table. Every row must land on\n"
                     "       a layout the gate accepts, whether or not it kept the request.\n";

        int landedRejected = 0;
        int honoured = 0;
        int probeCount = 0;
        auto probeBus = [&](juce::AudioProcessor::Bus *bus, const juce::AudioChannelSet &wanted,
                            const char *which) {
            const auto landed = bus->getBusesLayoutForLayoutChangeOfBus(wanted);
            const bool exact
                = landed.getChannelSet(bus->isInput(), bus->getBusIndex()) == wanted;
            const LayoutRow landedRow{landed.getMainInputChannelSet(), landed.getChannelSet(true, 1),
                                      landed.getMainOutputChannelSet()};
            if (!gateAccepts(landedRow))
                ++landedRejected;
            if (exact)
                ++honoured;
            ++probeCount;
            std::cout << "         " << which << " <- " << wanted.getDescription() << " ["
                      << wanted.size() << "ch] " << (exact ? "honoured" : "substituted") << " to "
                      << landedRow << " " << (gateAccepts(landedRow) ? "accepted" : "REFUSED") << "\n";
        };

        auto *sideBus = processor->getBus(true, 1);
        auto *mainBus = processor->getBus(true, 0);
        if (sideBus == nullptr || mainBus == nullptr)
        {
            std::cout << "  FAIL the processor has no sidechain or no main input bus\n";
            return false;
        }
        for (const int channels : {3, 4, 6, 8, 9})
            probeBus(sideBus, juce::AudioChannelSet::discreteChannels(channels), "sidechain");
        probeBus(sideBus, juce::AudioChannelSet::quadraphonic(), "sidechain");
        probeBus(mainBus, juce::AudioChannelSet::quadraphonic(), "main input");
        probeBus(mainBus, juce::AudioChannelSet::create5point1(), "main input");
        probeBus(mainBus, juce::AudioChannelSet::discreteChannels(8), "main input");

        /* The rungs that do move a bus, which is the part of F-015 a refusal table alone cannot show.
           From a mono layout, asking one main bus for stereo: the gate refuses the unequal pair, the
           "opposite bus to the requested set" rung (juce_AudioProcessor.cpp:290-300) then succeeds, and
           the bus the host never touched follows it to stereo. The gate cannot stop that — what it buys
           is that the layout it lands on is one it serves, on one channel pair rather than a split. */
        auto monoProcessor = makeProcessor();
        if (!applyLayout(*monoProcessor, kMonoNoB))
            return false;
        probeBus(monoProcessor->getBus(true, 0), juce::AudioChannelSet::stereo(),
                 "main input from mono");
        probeBus(monoProcessor->getBus(false, 0), juce::AudioChannelSet::stereo(),
                 "main output from mono");
        probeBus(monoProcessor->getBus(true, 1), juce::AudioChannelSet::stereo(),
                 "sidechain from mono");

        std::cout << "       " << honoured << " of " << probeCount
                  << " probes honoured the request; " << landedRejected
                  << " landed on a layout the gate refuses\n";
        if (landedRejected != 0)
        {
            std::cout << "  FAIL getNextBestLayout produced a layout the gate will not serve\n";
            passed = false;
        }
        if (processor->getBusesLayout() != (layoutOf(*processor, LayoutRow{juce::AudioChannelSet::stereo(),
                                                                          juce::AudioChannelSet::stereo(),
                                                                          juce::AudioChannelSet::stereo()})))
        {
            std::cout << "  FAIL a probe moved the applied layout — these are read-only probes\n";
            passed = false;
        }
        if (monoProcessor->getBusesLayout()
            != layoutOf(*monoProcessor, LayoutRow{juce::AudioChannelSet::mono(),
                                                  juce::AudioChannelSet::disabled(),
                                                  juce::AudioChannelSet::mono()}))
        {
            std::cout << "  FAIL a probe moved the mono layout — these are read-only probes\n";
            passed = false;
        }
    }

    /* What a host is told it can have is computed from the gate, so widening the sidechain needs no
       `BusesProperties` edit. The main bus keeps its 2; the sidechain's maximum follows the *main* width,
       because the probe is asked with the current main layout in place. */
    {
        std::cout << "       advertised maxima (getMaxSupportedChannels, limit 12) — the gate's answer is\n"
                     "       the advertisement, so the sidechain's maximum moves with the main bus it is\n"
                     "       asked beside:\n";
        for (const BusSetup &bus : {kStereoNoB, kMonoNoB, kMonoWithB})
        {
            auto processor = makeProcessor();
            if (!applyLayout(*processor, bus))
                return false;
            const int mainMax = processor->getBus(true, 0)->getMaxSupportedChannels(12);
            const int sideMax = processor->getBus(true, 1)->getMaxSupportedChannels(12);
            const int outMax = processor->getBus(false, 0)->getMaxSupportedChannels(12);
            const int wantedSide = bus.mainSet == juce::AudioChannelSet::mono()
                                       ? engine::kMaxChannels
                                       : engine::kMaxMainBusChannels;
            std::cout << "       " << bus.name << ": main in " << mainMax << ", sidechain " << sideMax
                      << " (gate's ceiling is " << engine::kMaxChannels << " wide only for a mono main), main out "
                      << outMax << "\n";
            if (mainMax != engine::kMaxMainBusChannels || outMax != engine::kMaxMainBusChannels
                || sideMax != wantedSide)
            {
                std::cout << "  FAIL the advertisement moved somewhere D-021 A did not say it should\n";
                passed = false;
            }
        }
    }

    /* Criterion 8: the rule is count-blind. Every set with the same channel count gets the same answer,
       named surround and ambisonic included — which is M4 §6's ambisonic question answered at the gate,
       and an answer about *verdicts*, not a claim of surround support. */
    {
        int setsProbed = 0;
        int gateMismatch = 0;
        int spread = 0;
        for (const int mainChannels : {1, 2})
        {
            const auto mainSet = mainChannels == 1 ? juce::AudioChannelSet::mono()
                                                   : juce::AudioChannelSet::stereo();
            const auto processor = makeProcessor();
            for (int channels = 1; channels <= engine::kMaxChannels; ++channels)
            {
                auto sets = juce::AudioChannelSet::channelSetsWithNumberOfChannels(channels);
                sets.add(juce::AudioChannelSet::discreteChannels(channels));
                sets.add(juce::AudioChannelSet::namedChannelSet(channels));
                if (channels == 4)
                    sets.add(juce::AudioChannelSet::ambisonic(1));
                if (channels == 9)
                    sets.add(juce::AudioChannelSet::ambisonic(2));

                int yes = 0;
                int no = 0;
                for (const auto &set : sets)
                {
                    if (set.isDisabled() || set.size() != channels)
                        continue;
                    const LayoutRow row{mainSet, set, mainSet};
                    const bool gate = gateAccepts(row);
                    if (processor->checkBusesLayoutSupported(layoutOf(*processor, row)) != gate)
                        ++gateMismatch;
                    gate ? ++yes : ++no;
                    ++setsProbed;
                }
                if (yes != 0 && no != 0)
                    ++spread;
                std::cout << "       main " << mainChannels << " / side " << channels << ": " << yes
                          << " sets accepted, " << no << " refused\n";
            }
        }
        std::cout << "       " << setsProbed << " named sets probed; " << gateMismatch
                  << " disagreed with the gate, " << spread << " channel counts split by naming\n";
        if (gateMismatch != 0 || spread != 0)
        {
            std::cout << "  FAIL the verdict depends on the set's name, not its channel count\n";
            passed = false;
        }
    }

    /* The bus inventory is JUCE's, not the gate's: `canAddBus`/`canRemoveBus` are not overridden, so a
       third bus never reaches the gate to be argued about. */
    {
        auto processor = makeProcessor();
        const bool inventory =
            processor->getBusCount(true) == 2 && processor->getBusCount(false) == 1;
        auto three = processor->getBusesLayout();
        three.inputBuses.add(juce::AudioChannelSet::mono());
        const bool threeRefused = !processor->checkBusesLayoutSupported(three);
        std::cout << "       inventory: " << processor->getBusCount(true) << " in / "
                  << processor->getBusCount(false) << " out; a layout with a third input bus is "
                  << (threeRefused ? "refused before the gate" : "NOT refused") << "\n";
        if (!inventory || !threeRefused)
        {
            std::cout << "  FAIL the bus inventory needs a rule the shell does not have\n";
            passed = false;
        }
    }

    /* Criterion 11: the answer must not depend on when it is asked, and the two readers of the live
       triple must agree about it. */
    {
        const std::vector<LayoutRow> probe = {
            {juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled(),
             juce::AudioChannelSet::mono()},
            {juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo(),
             juce::AudioChannelSet::mono()},
            {juce::AudioChannelSet::mono(), juce::AudioChannelSet::octagonal(),
             juce::AudioChannelSet::mono()},
            {juce::AudioChannelSet::mono(), juce::AudioChannelSet::discreteChannels(9),
             juce::AudioChannelSet::mono()},
            {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled(),
             juce::AudioChannelSet::stereo()},
            {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono(),
             juce::AudioChannelSet::stereo()},
            {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::quadraphonic(),
             juce::AudioChannelSet::stereo()},
            {juce::AudioChannelSet::quadraphonic(), juce::AudioChannelSet::disabled(),
             juce::AudioChannelSet::quadraphonic()}};
        const auto answers = [&probe](juce::AudioProcessor &processor) {
            std::size_t bits = 0;
            for (const auto &row : probe)
                bits = bits * 2U + (processor.checkBusesLayoutSupported(layoutOf(processor, row)) ? 1U : 0U);
            return bits;
        };

        auto processor = makeProcessor();
        const auto beforePrepare = answers(*processor);
        processor->prepareToPlay(kSampleRate, kMaxBlock);
        const auto afterPrepare = answers(*processor);
        if (!applyLayout(*processor, kMonoWithEightB))
            return false;
        const auto afterLayoutChange = answers(*processor);
        std::cout << "       the same probe table before prepare, after prepareToPlay and after a\n"
                     "       mid-session layout change: "
                  << (beforePrepare == afterPrepare && beforePrepare == afterLayoutChange
                          ? "identical"
                          : "MOVED")
                  << "; processed for the live layout "
                  << engine::gateLayout(
                         processor->getChannelCountOfBus(true, 0),
                         sideCountOf(processor->getBusesLayout().getChannelSet(true, 1)),
                         processor->getChannelCountOfBus(false, 0))
                         .processed
                  << "\n";
        if (beforePrepare != afterPrepare || beforePrepare != afterLayoutChange)
        {
            std::cout << "  FAIL isBusesLayoutSupported is not stable across prepare or a layout change\n";
            passed = false;
        }

        const entangle::ChainConfig chain = liveChainConfig(*processor);
        const engine::LayoutDecision decision
            = engine::gateLayout(chain.channels, chain.sidechainChannels, chain.outputChannels);
        const engine::SchedulerConfig schedulerConfig = entangle::makeSchedulerConfig(chain);
        std::cout << "       live triple " << chain.channels << "," << chain.sidechainChannels << ","
                  << chain.outputChannels << ": gate's processed " << decision.processed
                  << " vs the map configureRings uses " << schedulerConfig.map.channels
                  << ", and the processor offers " << processor->getTotalNumOutputChannels()
                  << " output channels\n";
        if (decision.processed != schedulerConfig.map.channels
            || processor->getTotalNumOutputChannels() != decision.processed)
        {
            std::cout << "  FAIL the gate and the scheduler config disagree about the live triple\n";
            passed = false;
        }
    }

    /* Criterion 10: `configureRings`'s early return is unreachable from an accepted layout. The silent
       path shows as an *unwritten* output buffer — `processBlock` returns before touching it — so the
       probe is one maxHostBlock callback whose buffers start as NaN: a NaN surviving in the main output
       region means the scheduler was never prepared, which is the state AGENTS §13 used to call "silence,
       not a refusal". A peak alone cannot show this, because a wet path with L = 8192 is still ~1e-9 after
       one callback and reads as sounding either way. */
    {
        const int samples = EntangleAudioProcessor::maxHostBlockSamples;
        for (const BusSetup &bus : {kStereoWithB, kStereoNoB, kMonoNoB, kMonoWithB, kMonoWithEightB})
        {
            auto processor = makeProcessor();
            if (!configure(*processor, bus, kDefault))
            {
                std::cout << "  FAIL " << bus.name << ": the gate refuses a layout this item says it takes\n";
                passed = false;
                continue;
            }
            const int mainChannels = processor->getChannelCountOfBus(true, 0);
            const int sideChannels = processor->getChannelCountOfBus(true, 1);
            const int outputChannels = processor->getTotalNumOutputChannels();
            const int bufferChannels
                = std::max(processor->getTotalNumInputChannels(), outputChannels);

            const auto a = noiseStream(mainChannels, samples, 40302U, 0.3f);
            const auto b = noiseStream(sideChannels > 0 ? sideChannels : 1, samples, 40303U, 0.3f);

            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buffer(bufferChannels, samples);
            for (int channel = 0; channel < bufferChannels; ++channel)
                for (int i = 0; i < samples; ++i)
                    buffer.setSample(channel, i, std::numeric_limits<float>::quiet_NaN());
            for (int channel = 0; channel < mainChannels; ++channel)
                for (int i = 0; i < samples; ++i)
                    buffer.setSample(channel, i, a[static_cast<std::size_t>(channel)][i]);
            for (int channel = 0; channel < sideChannels; ++channel)
                for (int i = 0; i < samples; ++i)
                    buffer.setSample(mainChannels + channel, i,
                                     b[static_cast<std::size_t>(channel)][i]);

            processor->processBlock(buffer, midi);

            int unwritten = 0;
            double peak = 0.0;
            for (int channel = 0; channel < outputChannels; ++channel)
                for (int i = 0; i < samples; ++i)
                {
                    const float sample = buffer.getSample(channel, i);
                    if (!std::isfinite(sample))
                    {
                        ++unwritten;
                        continue;
                    }
                    peak = std::max(peak, static_cast<double>(std::abs(sample)));
                }

            const bool active = shellOf(*processor).isSidechainActive();
            std::cout << "       " << bus.name << ": " << mainChannels << " processed, sidechain active "
                      << (active ? "yes" : "no") << ", output channels " << outputChannels
                      << ", unwritten samples " << unwritten << ", peak " << peak << "\n";
            if (mainChannels < 1 || outputChannels != mainChannels || active != (sideChannels > 0)
                || unwritten != 0)
            {
                std::cout << "  FAIL " << bus.name
                          << ": an accepted layout reaches the silent path configureRings guards\n";
                passed = false;
            }
        }
    }

    /* Criterion 12 as item 02 left it, kept as a RESTATEMENT: the figure below re-evaluates item 02's sizing
       rule (rings and hop scratch per *source*, output per processed channel — HopScheduler.hpp) inside this
       harness, so it is a cross-check on the model and not a measurement. The measured footprint — allocated
       and counted through a replaced global operator new — is §5.19 and tools/PrepareFootprint (D-022 C).
       Re-evaluating the scheduler's expression here is precisely the second copy that cannot notice the
       expression changing, which is why it is labelled rather than trusted (F-016). */
    {
        std::cout << "       prepare-time cost of the newly reachable side, restated from the shell's own\n"
                     "       floor and the container's rounding (B-side rings + hop scratch, host block\n"
                     "       floored by maxHostBlockFor at " << EntangleAudioProcessor::maxHostBlockSamples
                  << "):\n";
        for (const int hop : {kDefault.hop, 512})
            for (const int sideChannels : {2, engine::kMaxChannels})
            {
                const std::size_t ringCapacity = container::CircularBuffer<float>(
                                                     static_cast<std::size_t>(
                                                         EntangleAudioProcessor::maxHostBlockFor(kMaxBlock)
                                                         + hop))
                                                     .capacity();
                const double rings = static_cast<double>(sideChannels) * static_cast<double>(ringCapacity)
                                   * sizeof(float);
                const double scratch = static_cast<double>(sideChannels) * static_cast<double>(hop)
                                     * sizeof(float);
                std::cout << "       H=" << hop << " chB=" << sideChannels << ": ring capacity "
                          << ringCapacity << " → " << rings / 1024.0 << " KiB of rings + "
                          << scratch / 1024.0 << " KiB of hop scratch = "
                          << (rings + scratch) / 1024.0 << " KiB (one processed channel either way)\n";
            }
    }

    return passed;
}

/* The other half of criterion 4: the family the gate newly admits has to render as the fold — not as
   channel 0 of the sidechain, not as silence — through the shell, with the same material §5.17 measured
   through the builder. Seeds, length and amplitude are §5.17's so the figures are comparable, and the
   stage is parked at amount 0 for the arithmetic leg for the same reason it is there: it isolates the
   mean from whatever the correction then decides. */
bool checkConsolidatedLayoutFromTheHost()
{
    std::cout << "[5.18] the newly reachable family renders as the fold (M4 item 03 criterion 4). Mono\n"
                 "       main, decorrelated stereo sidechain applied through setBusesLayout, against each\n"
                 "       of its channels arriving as a mono sidechain.\n";
    bool passed = true;

    const Shape shape = kTightest;
    const int total = 3 * static_cast<int>(kSampleRate);
    const std::size_t tail = static_cast<std::size_t>(kSampleRate);
    const auto blocks = blocksSummingTo(total, 0);
    const float amplitude = 0.1f;

    const auto a = noiseStream(1, total, 20261103U, amplitude);
    const auto wide = noiseStream(2, total, 4242424U, amplitude);
    const Stream firstOnly{wide[0]};
    const Stream secondOnly{wide[1]};
    const BusSetup monoSide{"mono-with-1-b", juce::AudioChannelSet::mono(), true,
                            juce::AudioChannelSet::mono()};

    auto folded = makeProcessor();
    auto viaFirst = makeProcessor();
    auto viaSecond = makeProcessor();
    if (!configure(*folded, kMonoWithB, shape) || !configure(*viaFirst, monoSide, shape)
        || !configure(*viaSecond, monoSide, shape))
        return false;

    std::cout << "       applied as " << folded->getChannelCountOfBus(true, 0) << " main / "
              << folded->getChannelCountOfBus(true, 1) << " side → "
              << folded->getTotalNumOutputChannels() << " output channel(s), latency "
              << folded->getLatencySamples() << "\n";
    if (folded->getTotalNumOutputChannels() != 1)
    {
        std::cout << "  FAIL a consolidated layout opened more than one output\n";
        passed = false;
    }

    /* The shell adds nothing on this shape either — the same comparison §5.1 makes for stereo, run on the
       family the gate just opened, so the fold measured here is the fold the builder produces. */
    const auto throughShell = render(*folded, a, &wide, blocks);
    const auto direct = renderReference(shape, a, wide, 1, 2, blocks);
    if (!expectIdentical(throughShell, direct, "the shell's consolidated render is the builder's"))
        passed = false;

    const auto foldRender = throughShell;
    const auto firstRender = render(*viaFirst, a, &firstOnly, blocks);
    const auto secondRender = render(*viaSecond, a, &secondOnly, blocks);

    double differsFromFirst = 0.0;
    double differsFromSecond = 0.0;
    for (std::size_t i = tail; i < static_cast<std::size_t>(total); ++i)
    {
        differsFromFirst = std::max(differsFromFirst,
                                    std::abs(static_cast<double>(foldRender[0][i])
                                             - static_cast<double>(firstRender[0][i])));
        differsFromSecond = std::max(differsFromSecond,
                                     std::abs(static_cast<double>(foldRender[0][i])
                                              - static_cast<double>(secondRender[0][i])));
    }
    std::cout << "       tail peak |fold − ch0 side| = " << differsFromFirst << ", |fold − ch1 side| = "
              << differsFromSecond << " (fold peak " << peakOf(foldRender) << ")\n";
    if (peakOf(foldRender) <= 0.0)
    {
        std::cout << "  FAIL the consolidated layout renders silence\n";
        passed = false;
    }
    if (differsFromFirst < 1e-6 || differsFromSecond < 1e-6)
    {
        std::cout << "  FAIL the consolidated render is one channel of the sidechain, not the mean\n";
        passed = false;
    }

    /* The −3.01 dB arithmetic, with the stage parked: the wet path is linear in B for a fixed A, so the
       mean of two decorrelated channels sits 3.01 dB under one of them. §5.17 gates the same figure
       through the builder; this is the same claim through the gate, the shell and a real layout. */
    for (auto *processor : {folded.get(), viaFirst.get(), viaSecond.get()})
        if (!setFloat(*processor, EntangleAudioProcessor::normalisationAmountParameterId, 0.0f))
            return false;
    const auto parkedFold = render(*folded, a, &wide, blocks);
    const auto parkedFirst = render(*viaFirst, a, &firstOnly, blocks);
    const auto parkedSecond = render(*viaSecond, a, &secondOnly, blocks);
    const double foldLevel = settledRms(parkedFold, tail)[0];
    const double firstLevel = settledRms(parkedFirst, tail)[0];
    const double secondLevel = settledRms(parkedSecond, tail)[0];
    if (foldLevel <= 0.0 || firstLevel <= 0.0 || secondLevel <= 0.0)
    {
        std::cout << "  FAIL the parked comparison has no level to compare\n";
        return false;
    }
    const double againstFirst = decibels(foldLevel) - decibels(firstLevel);
    const double againstSecond = decibels(foldLevel) - decibels(secondLevel);
    std::cout << "       stage parked: fold " << decibels(foldLevel) << " dBFS vs ch0 side "
              << decibels(firstLevel) << " (" << againstFirst << " dB) and ch1 side "
              << decibels(secondLevel) << " (" << againstSecond << " dB)\n";
    if (std::abs(againstFirst + 3.0103) > 0.5 || std::abs(againstSecond + 3.0103) > 0.5)
    {
        std::cout << "  FAIL the consolidated sidechain is not the mean of its channels\n";
        passed = false;
    }
    return passed;
}

//==============================================================================
/*  §5.19 — M4 item 04: what a prepare sizes against, when one happens, and what the floor costs.

    The re-prepare guard, the sizing floor and the latency policy are the shell's, so they are measured through
    a real `AudioProcessor`. The key's equality and the scheduler's byte figures live in `ReconfigureTests`;
    the whole-plugin footprint prints from `tools/PrepareFootprint`. These legs drive `configureRings` the way a
    host does — `setBusesLayout`, `processorLayoutsChanged`, `prepareToPlay` — because what D-022 B changes is
    that an *unchanged* arrival stops disturbing a running transport, and only a transport can show that.

    The comparisons are bit-exact on purpose. A skipped re-prepare leaves the transport exactly where it was; a
    re-prepare that should have been skipped clears the rings, re-applies the latency pre-fill and drops the
    tail, which moves the dry stream by R samples. Both are visible to `!=` long before the wet path arrives, so
    the re-prepare legs run the blend dial off centre (dry live) and render short. */
bool checkReprepareFloorAndLatency()
{
    std::cout << "[5.19] allocation policy and the conditional re-prepare (M4 item 04). The floor of D-022 A\n"
                 "       measured through the shell's own maxHostBlockFor, the latency's one input, and what a\n"
                 "       mid-session layout change, a repeated prepare and a clamp-swallowed dial move each do\n"
                 "       to a transport that is already running.\n";
    bool passed = true;

    const auto blocksOf = [](int total) { return blocksSummingTo(total, 0); };
    const auto callbackAtOrAfter = [](const std::vector<int> &list, int sample) {
        int position = 0;
        for (std::size_t index = 0; index < list.size(); ++index)
        {
            if (position >= sample)
                return index;
            position += list[index];
        }
        return list.size();
    };
    const auto differsFrom = [](const Stream &got, const Stream &want) {
        double worst = 0.0;
        for (std::size_t channel = 0; channel < got.size() && channel < want.size(); ++channel)
            for (std::size_t i = 0; i < got[channel].size() && i < want[channel].size(); ++i)
                worst = std::max(worst, std::abs(static_cast<double>(got[channel][i])
                                                 - static_cast<double>(want[channel][i])));
        return worst;
    };

    /* C12 — the floor is a tested policy, asked of the shell's own function rather than recomputed here. Below
       the floor the host's number must not reach the sizing at all; above it the ring has to hold the whole
       callback, which is the pairing `assert(numSamples <= config.maxHostBlock)` depends on. */
    {
        int wrong = 0;
        std::cout << "       maxHostBlockFor and the ring capacity it implies (capacity in floats):\n";
        for (const int hostBlock : {1, 64, 512, 4095, 4096, 4097, 8192, 65536})
        {
            const int sized = EntangleAudioProcessor::maxHostBlockFor(hostBlock);
            const int expected = hostBlock > EntangleAudioProcessor::maxHostBlockSamples
                                     ? hostBlock
                                     : EntangleAudioProcessor::maxHostBlockSamples;
            if (sized != expected)
                ++wrong;

            std::cout << "         host block " << hostBlock << " -> sized " << sized << ":";
            for (const int hop : EntangleAudioProcessor::hopValues)
            {
                const container::CircularBuffer<float> ring(
                    static_cast<std::size_t>(sized + hop));
                const container::CircularBuffer<float> floored(
                    static_cast<std::size_t>(EntangleAudioProcessor::maxHostBlockSamples + hop));
                if (hostBlock <= EntangleAudioProcessor::maxHostBlockSamples
                    && ring.capacity() != floored.capacity())
                    ++wrong;
                if (ring.capacity() < static_cast<std::size_t>(hostBlock + hop))
                    ++wrong;
                std::cout << "  H=" << hop << " " << ring.capacity() << "f";
            }
            std::cout << "\n";
        }
        if (wrong != 0)
        {
            std::cout << "  FAIL the sizing floor moved somewhere D-022 A did not say it should (" << wrong
                      << ")\n";
            passed = false;
        }
        else
            std::cout << "  ok   the floor holds below 4096 and the host's number only reaches the sizing above it\n";
    }

    /* C11 — R is a function of the hop and nothing else: the same latency at every hop across every layout the
       gate accepts, and a latency that moves when — only when — the hop dial does. */
    {
        int moved = 0;
        for (const BusSetup &bus : {kStereoWithB, kStereoNoB, kMonoNoB, kMonoWithB, kMonoWithEightB})
        {
            for (const int hop : EntangleAudioProcessor::hopValues)
            {
                auto processor = makeProcessor();
                if (!configure(*processor, bus, Shape{hop, 1024}))
                    return false;
                if (processor->getLatencySamples() != hop)
                {
                    std::cout << "  FAIL " << bus.name << " at H=" << hop << ": latency "
                              << processor->getLatencySamples() << "\n";
                    ++moved;
                }
            }

            auto walking = makeProcessor();
            if (!configure(*walking, bus, kDefault))
                return false;
            for (const int hop : EntangleAudioProcessor::hopValues)
            {
                if (!setChoice(*walking, EntangleAudioProcessor::hopParameterId, indexOfHop(hop)))
                    return false;
                walking->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
                walking->prepareToPlay(kSampleRate, kMaxBlock);
                if (walking->getLatencySamples() != hop)
                {
                    std::cout << "  FAIL " << bus.name << ": the hop dial did not move the latency to " << hop
                              << " (" << walking->getLatencySamples() << ")\n";
                    ++moved;
                }
            }
        }
        if (moved != 0)
            passed = false;
        else
            std::cout << "  ok   20 layout x hop preparations: latency == hop, and the dial moves it\n";
    }

    /* C5 — the first prepare is never skipped. `processBlock` returns before touching the buffers when no
       prepare happened, so a NaN surviving the first callback is the unprepared state; silence would not show
       it, and at L = 1024 the wet response arrives inside this callback, so a non-zero peak says the engine
       ran rather than the output ring merely draining its pre-fill. */
    {
        auto processor = makeProcessor();
        if (!configure(*processor, kMonoWithB, kTightest))
            return false;
        const int samples = EntangleAudioProcessor::maxHostBlockSamples;
        const int mainChannels = processor->getChannelCountOfBus(true, 0);
        const int sideChannels = processor->getChannelCountOfBus(true, 1);
        const int outputChannels = processor->getTotalNumOutputChannels();
        const int bufferChannels = std::max(processor->getTotalNumInputChannels(), outputChannels);
        const auto a = noiseStream(mainChannels, samples, 51900U, 0.3f);
        const auto b = noiseStream(sideChannels, samples, 51901U, 0.3f);

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(bufferChannels, samples);
        for (int channel = 0; channel < bufferChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(channel, i, std::numeric_limits<float>::quiet_NaN());
        for (int channel = 0; channel < mainChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(channel, i, a[static_cast<std::size_t>(channel)][i]);
        for (int channel = 0; channel < sideChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(mainChannels + channel, i, b[static_cast<std::size_t>(channel)][i]);

        processor->processBlock(buffer, midi);

        int unwritten = 0;
        double peak = 0.0;
        for (int channel = 0; channel < outputChannels; ++channel)
            for (int i = 0; i < samples; ++i)
            {
                const float sample = buffer.getSample(channel, i);
                if (!std::isfinite(sample))
                {
                    ++unwritten;
                    continue;
                }
                peak = std::max(peak, static_cast<double>(std::abs(sample)));
            }
        std::cout << "       a fresh instance's first callback: " << unwritten << " unwritten samples, peak "
                  << peak << " at H=" << kTightest.hop << " L=" << kTightest.chosen << "\n";
        if (unwritten != 0 || peak <= 0.0)
        {
            std::cout << "  FAIL the first prepare on a fresh instance was skipped\n";
            passed = false;
        }
    }

    /* C3 — a real layout change re-prepares, and lands bit-identically on an instance prepared at the
       destination layout from the start.

        The comparison is bit-exact, as the plan asks, but not for the reason the plan gives. Its premise — "the
        two paths allocate the same buffers and run the same hops, so any difference is the guard leaving state
        behind" — is false of a transport that has been fed material: the grain history, the DC blocker's IIR
        state and the follower envelope legitimately carry what they were given, and the cold instance carries
        nothing, so the first version of this leg failed 110 dB below the signal and told us nothing about the
        guard (§11). Silence is what makes the claim exact. Both instances reach the switch with provably zero
        state, because exact silence is exact in the detector and a no-op through the blocker, and the switch
        sits on a callback boundary that is also a multiple of H, so the R-sample pre-fill the re-prepare applies
        and the R samples the continuous instance already pushed through its output ring are the same zeros —
        which is why the input stays silent for R samples *past* the switch too. From the switch on there is one
        transport, one layout, and two ways of having got there. `expectIdentical` compares whole streams, so the
        legs below compare from the switch; before it the two are different layouts, and equal only by accident
        of the silence. */
    {
        const Shape shape = kTightest;
        const int split = 8192; ///< a multiple of every H in hopValues and of the 4096 floor
        const int total = split + 4 * shape.chosen + 4 * shape.hop;
        const int silentUntil = split + shape.hop;

        std::vector<int> blocks;
        for (int position = 0; position < split; position += 512)
            blocks.push_back(512);
        for (int position = split; position < total; position += 1024)
            blocks.push_back(std::min(1024, total - position));
        const std::size_t switchAt = callbackAtOrAfter(blocks, split);

        const auto silent = [&](Stream stream) {
            for (auto &channel : stream)
                std::fill(channel.begin(),
                          channel.begin() + static_cast<std::ptrdiff_t>(silentUntil), 0.0f);
            return stream;
        };
        /* Two main rows and eight side rows: every callback is written from the layout in force at that
           callback, so the widest phase of the render sets what the streams have to carry. */
        const auto a = silent(noiseStream(2, total, 20261105U, 0.3f));
        const auto wide = silent(noiseStream(engine::kMaxChannels, total, 20261106U, 0.3f));

        const auto identicalFrom = [&](const Stream &got, const Stream &want, const char *what) {
            for (std::size_t channel = 0; channel < got.size() && channel < want.size(); ++channel)
                for (std::size_t i = static_cast<std::size_t>(split);
                     i < got[channel].size() && i < want[channel].size(); ++i)
                    if (got[channel][i] != want[channel][i])
                    {
                        std::cout << "  FAIL " << what << ": ch" << channel << " n" << i << " got "
                                  << got[channel][i] << " want " << want[channel][i] << "\n";
                        return false;
                    }
            std::cout << "  ok   " << what << "\n";
            return true;
        };

        struct Direction
        {
            const char *what;
            const BusSetup *from;
            const BusSetup *to;
            int rowsAfter;
        };
        for (const Direction &case3 : {Direction{"(2,2,2) -> (1,8,1)", &kStereoWithB, &kMonoWithEightB, 1},
                                      Direction{"(1,8,1) -> (2,2,2)", &kMonoWithEightB, &kStereoWithB, 2}})
        {
            auto fresh = makeProcessor();
            if (!configure(*fresh, *case3.to, shape))
                return false;
            if (!setFloat(*fresh, EntangleAudioProcessor::blendParameterId, 0.5f))
                return false;
            const auto expected = renderSwept(*fresh, a, &wide, blocks, [](int) {}, false);

            double preChangePeak = 0.0;
            for (const auto &channel : expected)
                for (int i = 0; i < split; ++i)
                    preChangePeak = std::max(preChangePeak,
                                             static_cast<double>(std::abs(channel[static_cast<std::size_t>(i)])));
            if (preChangePeak != 0.0)
            {
                std::cout << "  FAIL the silent prefix is not silent (peak " << preChangePeak
                          << " before the switch), so the comparison below proves nothing\n";
                passed = false;
            }

            auto warm = makeProcessor();
            if (!configure(*warm, *case3.from, shape))
                return false;
            if (!setFloat(*warm, EntangleAudioProcessor::blendParameterId, 0.5f))
                return false;
            bool applied = true;
            std::size_t fired = 0;
            const auto gesture = [&](int)
            {
                if (fired++ != switchAt)
                    return;
                applied = applyLayout(*warm, *case3.to);
                warm->processorLayoutsChanged();
                /* And the prepare a host always follows a layout change with. The call above already
                   re-configures — `processorLayoutsChanged` goes straight to `reconfigureSafely`
                   (EntangleProcessor.cpp:283), no async update involved — so this second arrival carries an
                   unchanged key and is the one D-022 B skips. Sending only the first would test the guard
                   against half of what a host actually sends. What the harness must NOT do is fill the
                   callback's buffer before the change lands: that hands the new configuration the old
                   layout's channels and segfaults in `HopScheduler::process` (§11). */
                warm->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
                warm->prepareToPlay(kSampleRate, kMaxBlock);
            };
            const auto resumed = renderSwept(*warm, a, &wide, blocks, gesture, false, case3.rowsAfter);

            std::cout << "       " << case3.what << " mid-session at sample " << split
                      << ": layout applied " << applied << ", latency after the change "
                      << warm->getLatencySamples() << ", output channels "
                      << warm->getTotalNumOutputChannels() << "\n";
            if (!applied)
                passed = false;
            if (!identicalFrom(resumed, expected,
                               (std::string("a mid-session ") + case3.what
                                + " change lands on a fresh instance's render")
                                    .c_str()))
                passed = false;
        }

        /* §5 arm 11's tripwire: `releaseResources()` is the one lifecycle call that may drop DSP state without
           re-preparing, and D-022 B's guard would talk the next prepare out of putting it back — so the call clears
           the key (EntangleProcessor.cpp:307).

           The plan's wording was "prepare → releaseResources() → prepare with unchanged numbers must equal a fresh
           instance's render", and on a silent prefix that assertion cannot fail: with nothing in the rings and every
           follower at zero, re-preparing and skipping the re-prepare produce the same samples, which is why the first
           version of this leg passed before the key was cleared. What separates them is the latency pre-fill, so the
           leg asserts that instead — after a release the next R output samples must be the R zeros a prepare writes,
           with live material on both sides of them — and §2.3's leg below already carries the other half: a second
           prepare *without* a release must not insert them. (§11.) */
        {
            /* Live material on both sides of the switch, deliberately: C3's silent prefix is what makes its
               comparison exact and what would make this one vacuous, because the R-sample pre-fill window would
               then be silence either way. */
            const auto liveA = noiseStream(2, total, 51601U, 0.3f);
            const auto liveB = noiseStream(2, total, 51602U, 0.3f);

            auto released = makeProcessor();
            if (!configure(*released, kStereoWithB, shape)
                || !setFloat(*released, EntangleAudioProcessor::blendParameterId, 0.5f))
                return false;
            std::size_t fired = 0;
            const auto gesture = [&](int)
            {
                if (fired++ != switchAt)
                    return;
                released->releaseResources();
                released->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
                released->prepareToPlay(kSampleRate, kMaxBlock);
            };
            const auto afterRelease = renderSwept(*released, liveA, &liveB, blocks, gesture, false);

            int kept = 0;
            for (const auto &channel : afterRelease)
                for (int i = 0; i < shape.hop; ++i)
                    if (channel[static_cast<std::size_t>(split + i)] != 0.0f)
                        ++kept;

            double later = 0.0;
            for (const auto &channel : afterRelease)
                for (int i = shape.hop; i < total; ++i)
                    later = std::max(later, static_cast<double>(
                                                std::abs(channel[static_cast<std::size_t>(i)])));

            std::cout << "       after a release and a prepare with unchanged numbers: " << kept << " of "
                      << shape.hop << " pre-fill samples non-zero, peak beyond them " << later << "\n";
            if (kept != 0 || later <= 0.0)
            {
                std::cout << "  FAIL the prepare after releaseResources() was skipped, or the leg is vacuous\n";
                passed = false;
            }
            else
                std::cout << "  ok   a releaseResources() followed by a prepare with unchanged numbers re-applies "
                             "the latency pre-fill\n";
        }
    }

    /* §2.3 — what stops happening. A `setBusesLayout` that lands on the layout already in force, and a second
       `prepareToPlay` with the numbers it just had, must leave a running transport sample for sample. With the
       guard removed both clear the rings and re-apply the pre-fill, which shifts the dry stream by R and shows
       up here at once — this is the leg §5 arm 2 is written against. */
    {
        const Shape shape = kTightest;
        const int total = 4 * shape.chosen + 4 * shape.hop;
        const auto blocks = blocksOf(total);
        const auto a = noiseStream(2, total, 5150U, 0.3f);
        const auto b = noiseStream(2, total, 5151U, 0.3f);
        const std::size_t pokeAt = callbackAtOrAfter(blocks, total / 2);

        auto control = makeProcessor();
        if (!configure(*control, kStereoWithB, shape)
            || !setFloat(*control, EntangleAudioProcessor::blendParameterId, 0.5f))
            return false;
        const auto uninterrupted = renderSwept(*control, a, &b, blocks, [](int) {}, false);

        auto poked = makeProcessor();
        if (!configure(*poked, kStereoWithB, shape)
            || !setFloat(*poked, EntangleAudioProcessor::blendParameterId, 0.5f))
            return false;
        std::size_t fired = 0;
        const auto gesture = [&](int) {
            if (fired++ != pokeAt)
                return;
            applyLayout(*poked, kStereoWithB);
            poked->processorLayoutsChanged();
            poked->processorLayoutsChanged();
            poked->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
            poked->prepareToPlay(kSampleRate, kMaxBlock);
        };
        const auto continued = renderSwept(*poked, a, &b, blocks, gesture, false);
        if (!expectIdentical(continued, uninterrupted,
                             "an unchanged layout re-applied and a repeated prepareToPlay leave the transport "
                             "alone"))
            passed = false;
    }

    /* C6 — the clamp is part of the configuration, not a change to it. At H = 256 the ceiling is 128 × 256 =
       32768, so 32768 and 65536 both run 32768 and the move must not re-prepare; at H = 512 the ceiling is
       65536, so the same move doubles the running length and must. Both halves are the same measurement read
       two ways, and the `clampGrainLength` facts underneath them are asserted first or the leg is decoration. */
    {
        const bool clampFactsHold = EntangleAudioProcessor::clampGrainLength(32768, 256) == 32768
                                    && EntangleAudioProcessor::clampGrainLength(65536, 256) == 32768
                                    && EntangleAudioProcessor::clampGrainLength(32768, 512) == 32768
                                    && EntangleAudioProcessor::clampGrainLength(65536, 512) == 65536;
        if (!clampFactsHold)
        {
            std::cout << "  FAIL clampGrainLength no longer swallows 65536 at H=256 and not at H=512 — the C6 "
                         "half-cases below are measuring something else\n";
            passed = false;
        }

        struct DialCase
        {
            int hop;
            bool changeIsReal;
        };
        for (const DialCase &dial : {DialCase{256, false}, DialCase{512, true}})
        {
            const Shape shape{dial.hop, 32768};
            const int total = 4 * 4096;
            const auto blocks = blocksOf(total);
            const auto a = noiseStream(2, total, 9001U, 0.3f);
            const auto b = noiseStream(2, total, 9002U, 0.3f);
            const std::size_t pokeAt = callbackAtOrAfter(blocks, total / 2);

            auto control = makeProcessor();
            if (!configure(*control, kStereoWithB, shape)
                || !setFloat(*control, EntangleAudioProcessor::blendParameterId, 0.5f))
                return false;
            const auto uninterrupted = renderSwept(*control, a, &b, blocks, [](int) {}, false);

            auto moved = makeProcessor();
            if (!configure(*moved, kStereoWithB, shape)
                || !setFloat(*moved, EntangleAudioProcessor::blendParameterId, 0.5f))
                return false;
            std::size_t fired = 0;
            const auto gesture = [&](int) {
                if (fired++ != pokeAt)
                    return;
                setChoice(*moved, EntangleAudioProcessor::grainLengthParameterId, indexOfLength(65536));
                moved->processorLayoutsChanged();
            };
            const auto continued = renderSwept(*moved, a, &b, blocks, gesture, false);

            const double difference = differsFrom(continued, uninterrupted);
            std::cout << "       H=" << dial.hop << ": dial 32768 -> 65536 runs L = "
                      << shellOf(*moved).grainLengthSamples() << " (was " << shape.chosen
                      << "), peak |difference| " << difference << " — expected "
                      << (dial.changeIsReal ? "a re-prepare" : "no re-prepare") << "\n";
            if (dial.changeIsReal)
            {
                if (difference <= 0.0)
                {
                    std::cout << "  FAIL a grain-length change the clamp does not swallow left the transport "
                                  "untouched — the key is not carrying it\n";
                    passed = false;
                }
            }
            else if (difference > 0.0)
            {
                std::cout << "  FAIL a dial move the clamp swallows re-prepared and dropped the tail\n";
                passed = false;
            }
        }
    }

    /* C7 — every (hop, grain) pair the two dials can express is a configuration the engine accepts, and none
       of them reaches a callback with nothing prepared. The clamp rule itself is §5.9's; what is new here is
       the drive-through — `configureRings` is now the path a live session takes repeatedly, and a throw from
       inside it would cross a `suspendProcessing` boundary, while a processed count of 0 would show as the
       same unwritten-buffer state §5.18 uses for the silent path. */
    {
        const int samples = 256;
        int pairs = 0;
        int rejected = 0;
        int unwrittenTotal = 0;
        for (const int hop : EntangleAudioProcessor::hopValues)
        {
            for (const int chosen : EntangleAudioProcessor::grainLengthValues)
            {
                auto processor = makeProcessor();
                if (!applyLayout(*processor, kStereoWithB))
                    return false;
                if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId, indexOfHop(hop))
                    || !setChoice(*processor, EntangleAudioProcessor::grainLengthParameterId,
                                  indexOfLength(chosen)))
                    return false;
                processor->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
                processor->prepareToPlay(kSampleRate, kMaxBlock);
                ++pairs;

                const int running = shellOf(*processor).grainLengthSamples();
                if (running % hop != 0 || running < 2 * hop || hop > running)
                {
                    std::cout << "  FAIL H=" << hop << " L=" << chosen << " -> " << running
                              << ": not a configuration the engine's own prepare() guards accept\n";
                    ++rejected;
                }

                const int mainChannels = processor->getChannelCountOfBus(true, 0);
                const int sideChannels = processor->getChannelCountOfBus(true, 1);
                const int outputChannels = processor->getTotalNumOutputChannels();
                const int bufferChannels =
                    std::max(processor->getTotalNumInputChannels(), outputChannels);
                const auto a = noiseStream(mainChannels, samples, 707U, 0.3f);
                const auto b = noiseStream(sideChannels, samples, 709U, 0.3f);

                juce::MidiBuffer midi;
                juce::AudioBuffer<float> buffer(bufferChannels, samples);
                for (int channel = 0; channel < bufferChannels; ++channel)
                    for (int i = 0; i < samples; ++i)
                        buffer.setSample(channel, i, std::numeric_limits<float>::quiet_NaN());
                for (int channel = 0; channel < mainChannels; ++channel)
                    for (int i = 0; i < samples; ++i)
                        buffer.setSample(channel, i, a[static_cast<std::size_t>(channel)][i]);
                for (int channel = 0; channel < sideChannels; ++channel)
                    for (int i = 0; i < samples; ++i)
                        buffer.setSample(mainChannels + channel, i, b[static_cast<std::size_t>(channel)][i]);

                processor->processBlock(buffer, midi);
                for (int channel = 0; channel < outputChannels; ++channel)
                    for (int i = 0; i < samples; ++i)
                        if (!std::isfinite(buffer.getSample(channel, i)))
                            ++unwrittenTotal;
            }
        }
        std::cout << "       " << pairs << " (hop, grain) pairs prepared and delivered a callback: " << rejected
                  << " outside the engine's own guard band, " << unwrittenTotal
                  << " unwritten samples\n";
        if (rejected != 0 || unwrittenTotal != 0 || pairs != 28)
        {
            std::cout << "  FAIL a configuration the UI can express reaches a prepare it cannot honour\n";
            passed = false;
        }
    }

    /* C13, the half a host can reach. A refused layout is never applied, so the running configuration is the
       accepted one and survives two consecutive re-configure pokes unchanged — including the render, which is
       the state a skipped re-prepare must not disturb. The ordering itself (the guard sitting after the gate
       rather than before it) is not observable from a layout a host can apply: §5 arm 10 records that as an
       equivalent mutant, and the key-level transition table is `ReconfigureTests`. */
    {
        const Shape shape = kTightest;
        const int total = 4 * shape.chosen + 4 * shape.hop;
        const auto blocks = blocksOf(total);
        const auto a = noiseStream(2, total, 31337U, 0.3f);
        const auto b = noiseStream(2, total, 31338U, 0.3f);

        auto processor = makeProcessor();
        if (!configure(*processor, kStereoWithB, shape)
            || !setFloat(*processor, EntangleAudioProcessor::blendParameterId, 0.5f))
            return false;
        const auto before = renderSwept(*processor, a, &b, blocks, [](int) {});

        auto quad = processor->getBusesLayout();
        quad.getChannelSet(true, 0) = juce::AudioChannelSet::quadraphonic();
        quad.getChannelSet(false, 0) = juce::AudioChannelSet::quadraphonic();
        const bool refused = !processor->setBusesLayout(quad);
        processor->processorLayoutsChanged();
        processor->processorLayoutsChanged();

        const bool stillStereo = processor->getBusesLayout()
                                 == layoutOf(*processor, LayoutRow{juce::AudioChannelSet::stereo(),
                                                                   juce::AudioChannelSet::stereo(),
                                                                   juce::AudioChannelSet::stereo()});
        const auto after = renderSwept(*processor, a, &b, blocks, [](int) {});
        std::cout << "       a quadraphonic request applied " << (refused ? "nothing" : "something")
                  << ", the layout afterwards is still stereo: " << (stillStereo ? "yes" : "no") << "\n";
        if (!refused || !stillStereo)
        {
            std::cout << "  FAIL a refused layout reached the live configuration\n";
            passed = false;
        }
        if (!expectIdentical(after, before, "two re-configures after a refusal leave the running render alone"))
            passed = false;
    }

    return passed;
}

/* ------------------------------------------------------------------- §5.20 ---
   The channel matrix: every layout the gate accepts, rendered, and independence proven with samples
   (M4 §4 item 7, and with it the three rows of M4 §7 that had no render-level owner — the mapping table at
   the sample level, the independence criterion, and buffer-size invariance for every accepted layout rather
   than for the one §5.2 happens to drive).

   Nine of the twelve triples `gateLayout` takes with the main bus at mono or stereo are cells, each driven
   through `createPluginFilter()`, the APVTS dials, `setBusesLayout` and `processBlock`:

     (1,0,1)  no sidechain       b[0] = kSilentSource — "no bus", which is not a silent mono bus (D-002)
     (1,1,1)  broadcast          b[0] = 0, one address repeated (item 02)
     (1,2,1)  consolidated       the fold (D-018)
     (1,3,1)  consolidated       non-dyadic 1/3
     (1,5,1)  consolidated       non-dyadic 1/5 — and the same triple again at kDefault {128, 8192}
     (1,8,1)  consolidated       the ceiling, kMaxChannels
     (2,0,2)  no sidechain
     (2,1,2)  broadcast          to both slots: nothing in this repo had ever rendered it
     (2,2,2)  diagonal           the layout most of §5.1…§5.19 runs on

   Left out: (1,4,1), (1,6,1), (1,7,1). Item 03 criterion 8 measured the gate count-blind across 94 named
   `AudioChannelSet`s, so the rule cannot branch on the width; what varies with `chB` is the fold's
   arithmetic, and it varies in exactly two ways — whether 1/chB is dyadic, and how many ascending adds feed
   the one multiply that `HopScheduler::foldSidechain` applies the reciprocal after. {2, 3, 5, 8} samples
   dyadic, odd non-dyadic, odd again at a different add count, and the ceiling. The exhaustive claim about
   widths belongs to `ChannelMapTests` and `LayoutGateTests`, where `static_assert` pins it; re-running it on
   samples would cost render time for a claim that is already built into the binary. A wide main is not a
   cell either — D-021 B defers quad, 5.1 and 7.1, and §5.18 owns the refusal.

   Exactness, three levels, each named at the assertion it governs:
     bit-exact       — block-plan invariance, shell-versus-reference identity, the dry path delayed by R.
     exactly 0.0f    — an output whose mapped source is silent. The FFT of an all-zero hop is exactly zero,
                       so are its product with a spectrum and its inverse; the DC blocker's state stays zero
                       on zero input from zero initial state; and tanh(0) = 0. A non-zero there is not
                       rounding, it is a leak. Scoped to the stage parked at amount 0, because at amount 1 a
                       gain multiplier is in the path and this section is not a proof that 0 × g == 0.
     kDeltaTolerance — the fold scaling. The fold applies 1/chB once, after an ascending float sum, so a
                       harness mean cannot be bit-identical to it at chB ∈ {3, 5}. §5.18 gates the same
                       relation at 0.5 dB; the material here is impulses, so the figure is in sample units.

   Dials: the mapping legs run at p = 0, where the law's dry weight is exactly 0 and the output is pure
   convolution, so the dry bus can neither hide a routing bug nor create one. The chB == 0 cells run at the
   shell's own pinned p = −1 (`effectiveBlend`) because that is what ships without a sidechain, and the leg
   there is the dry path's per-channel identity. Gain 0 dB, bypass off, and the level stage parked at
   amount = 0 for every arithmetic leg: with the stage parked the wet path is linear in B, which is what
   makes the single-channel and fold models exact, and it takes a non-linear, history-carrying stage out of
   the comparison. The one leg that wants the stage live is F3, at amount = 1, and its name says so. Every
   leg names the chain variant it ran on (M3 item 2's C1).

   What this section does not claim: no per-channel cost figure — that is M7's, since F-007 is per-callback
   and D-019 deferred the shared-spectrum question on exactly that absence — and nothing about audibility,
   which is M9 rows 1 and 2. Neither the fold's level nor the per-channel gain delta is decided here, and
   INDEX §3 rule 4 is why: no console harness can hear either of them. */

/** The matrix's shape: the largest hop the UI can pick with the shortest grain it can express at it, so
    F = 2048 and one cell costs microseconds. Equal to kTightest; named here for what it is doing.

    The matrix's claims are routing claims — which source reaches which output, whether the callback size
    leaks into the DSP, whether a channel is written at all — and none of them mentions the window length.
    That is an assumption, so it is tested rather than trusted: the table below runs (1,5,1) a second time at
    kDefault, the configuration the plugin actually ships on. */
constexpr Shape kMatrix{512, 1024};

/** The dial state every arithmetic leg runs: p = 0, 0 dB, bypass off, the level stage parked at amount 0.
    See the section header's dial discipline. */
const MixSetting kMatrixMix{0.0f, 0.0f, false, 0.0f, dsp::LevelNormalizer::defaultFloorDb};

/** Parks the level stage on the shell side of a leg, so it matches the `kMatrixMix` the reference renders
    run: amount 0 is bit-exact identity (§5.12's leg for that is `checkLevelStageMovesNothing`), which makes
    the wet path linear in B and keeps a non-linear, history-carrying stage out of the comparisons below. */
bool parkLevelStage(juce::AudioProcessor &processor)
{
    return setFloat(processor, EntangleAudioProcessor::normalisationAmountParameterId, 0.0f);
}

/** The broadband level the matrix's material runs at, and why it is not §5.1's 0.4.

    D-008's wet gain at the matrix's `L` = 1024 is 0.798 — eight times the default grain length's — so two
    uncorrelated streams at 0.3 convolve to a peak of 1.26, which is above `SoftClipper::knee` = 1.0. Nothing
    in the matrix asserts anything about that curve, and the stage parked at amount 0 removes only the
    normalizer, not the clipper: the fold and mean models below are linear-in-B arguments, so they are wrong
    the moment the clipper bends the stream. At 0.1 the peak lands near 0.14 and every comparison in this
    section is against a path that is linear by construction. The impulse material in M4 and M5 is quieter
    still (response peaks near 0.3) and needs no such care. */
constexpr float kMatrixAmplitude = 0.1f;

/** One row of the matrix: a layout, the shape it runs at, and the mapping the harness expects to hear.

    The `a`/`b` arrays are hand-written on purpose, not read back from `resolve`: expectations derived from
    the rule would make this section self-fulfilling, and a wrong rule would render wrongly while agreeing
    with itself. The first leg asserts the table against `resolve` for every cell before a single sample is
    rendered. The table is evidence, not a second decider — nothing in the product or this harness routes
    anything from it, and `ChannelMap.hpp`'s own `static_assert`s remain the pins on the rule. */
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

    /** 8L — 16 hops at kMatrix, leaving 2L of room after an impulse at n0 = 0, whose response occupies
        [n0 + L, n0 + 2L − 1]. A run has to clear the bounds its assertions land after, so the driver checks
        this per cell before any leg runs (criterion 2): an assertion mid-transient is not evidence, and
        shortening this section is the likeliest way to break it silently. */
    int runningLengthSamples() const noexcept
    {
        return 8 * EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop);
    }
};

const MatrixCell kMatrixCells[] = {
    { "(1,0,1)", &kMonoNoB, kMatrix, 1, 0, 1, 1, "no sidechain",
      { 0 }, { engine::kSilentSource } },
    { "(1,1,1)", &kMonoWithMonoB, kMatrix, 1, 1, 1, 1, "broadcast",
      { 0 }, { 0 } },
    { "(1,2,1)", &kMonoWithB, kMatrix, 1, 2, 1, 1, "consolidated",
      { 0 }, { engine::kConsolidatedSource } },
    { "(1,3,1)", &kMonoWithThreeB, kMatrix, 1, 3, 1, 1, "consolidated, non-dyadic 1/3",
      { 0 }, { engine::kConsolidatedSource } },
    { "(1,5,1)", &kMonoWithFiveB, kMatrix, 1, 5, 1, 1, "consolidated, non-dyadic 1/5",
      { 0 }, { engine::kConsolidatedSource } },
    { "(1,8,1)", &kMonoWithEightB, kMatrix, 1, 8, 1, 1, "consolidated, the ceiling",
      { 0 }, { engine::kConsolidatedSource } },
    { "(2,0,2)", &kStereoNoB, kMatrix, 2, 0, 2, 2, "no sidechain",
      { 0, 1 }, { engine::kSilentSource, engine::kSilentSource } },
    { "(2,1,2)", &kStereoWithMonoB, kMatrix, 2, 1, 2, 2, "broadcast to both slots",
      { 0, 1 }, { 0, 0 } },
    { "(2,2,2)", &kStereoWithB, kMatrix, 2, 2, 2, 2, "diagonal",
      { 0, 1 }, { 0, 1 } },
    /* The shape assumption, witnessed: the widest odd consolidated triple at the grain length and hop the
       plugin ships on. A row of the table rather than a special case in the loop. */
    { "(1,5,1)@default", &kMonoWithFiveB, kDefault, 1, 5, 1, 1,
      "consolidated, at the shipped shape",
      { 0 }, { engine::kConsolidatedSource } },
};

/** The kMatrix row for a pair of counts, or null. The shape is part of the key because the table holds two
    (1,5,1) rows — one at the matrix shape, one at the shipped default. */
const MatrixCell *matrixCell(int chA, int chB)
{
    for (const MatrixCell &cell : kMatrixCells)
        if (cell.chA == chA && cell.chB == chB && cell.shape.hop == kMatrix.hop
            && cell.shape.chosen == kMatrix.chosen)
            return &cell;
    return nullptr;
}

/** A cell rendered through a fresh shell at its own layout and shape with the stage parked. The family legs
    need two cells at a time and so cannot share the driver's rig; a failed configure prints its own FAIL
    inside `configure` and renders nothing, which the caller's comparison then reports as a shape mismatch. */
Stream renderOf(const MatrixCell &cell, const Stream &a, const Stream *side,
                const std::vector<int> &blocks)
{
    auto processor = makeProcessor();
    if (!configure(*processor, *cell.bus, cell.shape) || !parkLevelStage(*processor))
        return {};
    return render(*processor, a, side, blocks);
}

/** One line per (cell, leg), naming both and carrying the figure that decided it. `OfflineRender` cannot be
    filtered per case (INDEX §4), so the printout is the only index to a failure, and the tally is what lets
    the summary say how much was actually decided. */
struct MatrixReport
{
    int decided = 0;
    int failed = 0;

    bool line(bool condition, const char *cell, const char *leg, const juce::String &figure)
    {
        ++decided;
        failed += condition ? 0 : 1;
        std::cout << (condition ? "  ok   " : "  FAIL ") << cell << "  " << leg << ": " << figure << "\n";
        return condition;
    }
};

/** The comparison a leg reports: worst |got − want| with its address, and whether the two are bit-identical.
    §5.1's `expectIdentical` prints its own line and stops at the first difference; this section reports one
    line per (cell, leg) with the worst delta in it, so the tally and the figure agree. */
struct StreamDelta
{
    double worst = 0.0;
    std::size_t channel = 0;
    std::size_t sample = 0;
    bool shapeMismatch = false;
    bool bitIdentical = true;
};

StreamDelta compareChannel(const Stream &got, std::size_t gotChannel, const Stream &want,
                           std::size_t wantChannel)
{
    StreamDelta delta;
    if (gotChannel >= got.size() || wantChannel >= want.size())
    {
        delta.shapeMismatch = true;
        delta.bitIdentical = false;
        return delta;
    }
    const auto &from = got[gotChannel];
    const auto &to = want[wantChannel];
    if (from.size() != to.size())
        delta.shapeMismatch = true;
    for (std::size_t i = 0; i < std::min(from.size(), to.size()); ++i)
    {
        const double difference = std::abs(static_cast<double>(from[i]) - static_cast<double>(to[i]));
        if (difference > delta.worst)
        {
            delta.worst = difference;
            delta.channel = gotChannel;
            delta.sample = i;
        }
        if (from[i] != to[i])
            delta.bitIdentical = false;
    }
    return delta;
}

StreamDelta compareStreams(const Stream &got, const Stream &want)
{
    StreamDelta delta;
    if (got.size() != want.size())
    {
        delta.shapeMismatch = true;
        delta.bitIdentical = false;
        return delta;
    }
    for (std::size_t channel = 0; channel < got.size(); ++channel)
    {
        const auto one = compareChannel(got, channel, want, channel);
        if (one.worst > delta.worst)
        {
            delta.worst = one.worst;
            delta.channel = channel;
            delta.sample = one.sample;
        }
        delta.bitIdentical = delta.bitIdentical && one.bitIdentical;
    }
    return delta;
}

juce::String describe(const StreamDelta &delta)
{
    if (delta.shapeMismatch)
        return juce::String("shape mismatch (max ") + juce::String::formatted("%.3e", delta.worst) + ")";
    return juce::String::formatted("max %.3e at ch%d n%u%s", delta.worst,
                                   static_cast<int>(delta.channel),
                                   static_cast<unsigned>(delta.sample),
                                   delta.bitIdentical ? ", bit-identical" : "");
}

double channelPeak(const Stream &stream, std::size_t channel)
{
    if (channel >= stream.size())
        return 0.0;
    double peak = 0.0;
    for (const float sample : stream[channel])
        peak = std::max(peak, static_cast<double>(std::abs(sample)));
    return peak;
}

/** E's second level: every sample exactly 0.0f. Reported per channel because a leak in one output is the
    finding, and a peak over the whole render would hide which one. */
bool channelIsExactSilence(const Stream &stream, std::size_t channel)
{
    if (channel >= stream.size())
        return false;
    for (const float sample : stream[channel])
        if (sample != 0.0f)
            return false;
    return true;
}

/** The dry-path model: `in` delayed by `latency`, zeros before it. Same arithmetic as §5.5's
    `expectDelayedBitExact` without that helper's print, because this section tallies its own lines. */
Stream delayedBy(const Stream &in, int latency)
{
    Stream want(in.size());
    for (std::size_t channel = 0; channel < in.size(); ++channel)
    {
        want[channel].assign(in[channel].size(), 0.0f);
        for (std::size_t i = static_cast<std::size_t>(latency); i < in[channel].size(); ++i)
            want[channel][i] = in[channel][i - static_cast<std::size_t>(latency)];
    }
    return want;
}

Stream scaledCopy(const Stream &in, float scale)
{
    Stream out = in;
    for (auto &channel : out)
        for (auto &sample : channel)
            sample = sample * scale;
    return out;
}

/** `HopScheduler::foldSidechain`'s arithmetic restated in the harness: ascending source order, the
    reciprocal applied once, after the sum. F2's model — the scheduler's fold has to land on this, and
    `bitIdentical` is the figure that says whether it does. */
Stream meanWithOneReciprocal(const Stream &side, float scale)
{
    Stream mean(1, std::vector<float>(side.empty() ? 0 : side[0].size(), 0.0f));
    for (std::size_t i = 0; i < mean[0].size(); ++i)
    {
        float sum = 0.0f;
        for (const auto &channel : side)
            sum += channel[i];
        mean[0][i] = sum * scale;
    }
    return mean;
}

/** The variant the fold does *not* implement: the scale applied to every term before the sum. Dyadic
    reciprocals make the two identical; 1/3 and 1/5 do not, and the gap printed from here is what a future
    "optimisation" that scales per term would move. */
Stream meanScalingEveryTerm(const Stream &side, float scale)
{
    Stream mean(1, std::vector<float>(side.empty() ? 0 : side[0].size(), 0.0f));
    for (std::size_t i = 0; i < mean[0].size(); ++i)
    {
        float sum = 0.0f;
        for (const auto &channel : side)
            sum += channel[i] * scale;
        mean[0][i] = sum;
    }
    return mean;
}

/** 0.5 · (x + y) per sample: F1's model of "this render hears the mean of two sides", computed on the two
    *renders* rather than on their inputs, so it is not the fold's own arithmetic read back to it. The wet
    path is linear in B with the stage parked, which is what makes the two agree to float rounding. */
Stream meanOfRenders(const Stream &first, const Stream &second)
{
    Stream mean = first;
    for (std::size_t channel = 0; channel < mean.size(); ++channel)
        for (std::size_t i = 0; i < mean[channel].size(); ++i)
            mean[channel][i] = 0.5f * (first[channel][i] + second[channel][i]);
    return mean;
}

/** What slot `slot` reads, spelled out for the printout: the A source and the B side the table names. */
juce::String slotModelName(const MatrixCell &cell, std::size_t slot)
{
    const std::uint16_t source = cell.b[slot];
    if (source == engine::kSilentSource)
        return "A" + juce::String(static_cast<int>(cell.a[slot])) + " x silence";
    if (source == engine::kConsolidatedSource)
        return "A" + juce::String(static_cast<int>(cell.a[slot])) + " x mean(B0..B"
               + juce::String(cell.chB - 1) + ")";
    return "A" + juce::String(static_cast<int>(cell.a[slot])) + " x B"
           + juce::String(static_cast<int>(source));
}

/** The reference for one slot, rendered alone: one A channel and, on the B side, exactly what the map says
    that slot reads — channel 0 for a broadcast slot, channel `c` for a diagonal one, the whole side for a
    consolidated slot (whose fold the reference computes itself). Nothing else is in the layout, which is the
    point: the claim is that the slot hears no other channel. */
Stream slotModel(const MatrixCell &cell, const Stream &a, std::size_t slot, const Stream &side,
                 const std::vector<int> &blocks)
{
    const Stream oneA{ a[cell.a[slot]] };
    const std::uint16_t source = cell.b[slot];
    if (source == engine::kConsolidatedSource)
        return renderReference(cell.shape, oneA, side, 1, cell.chB, blocks, kMatrixMix);
    const Stream oneB{ side[source] };
    return renderReference(cell.shape, oneA, oneB, 1, 1, blocks, kMatrixMix);
}

/** conv(A[a[c]], B[j]) scaled by `scale`, rendered alone on one A channel and one B channel. With scale 1
    this is the broadcast and diagonal model; with 1/chB it is the model for a consolidated slot fed a single
    sidechain channel, because the fold is linear and the mean of one channel is that channel over chB. The
    scale lands on the *output* here and on the *input* in the scheduler, and the rounding difference between
    those two is exactly what kDeltaTolerance is for. */
Stream monoPairModel(const MatrixCell &cell, const Stream &a, std::size_t slot, const Stream &side,
                     std::size_t sideChannel, float scale, const std::vector<int> &blocks)
{
    const Stream oneA{ a[cell.a[slot]] };
    const Stream oneB{ side[sideChannel] };
    return scaledCopy(renderReference(cell.shape, oneA, oneB, 1, 1, blocks, kMatrixMix), scale);
}

/** The three callback plans M3 runs over the same total: the two ragged lists the file already had, and the
    steady 512-sample callback neither of them contains. */
struct BlockPlan
{
    const char *name;
    std::vector<int> (*build)(int total);
};

const std::array<BlockPlan, 3> kBlockPlans{{
    { "ragged {1..4096} + seeded", [](int total) { return blocksSummingTo(total, 0); } },
    { "{1,4,4096}", [](int total) { return blocksSummingTo(total, 1); } },
    { "constant 512", [](int total) { return constantBlocks(total, 512); } },
}};

/** Everything a per-cell leg needs: the shell prepared at the cell's layout and shape with the stage parked
    and the dial at the centre, the block list, and the broadband material. */
struct MatrixRig
{
    MatrixReport &report;
    juce::AudioProcessor &processor;
    const MatrixCell &cell;
    const std::vector<int> &blocks;
    const Stream &a;
    const Stream &b;

    Stream renderBroadband() const
    {
        return render(processor, a, cell.chB > 0 ? &b : nullptr, blocks);
    }
};

/** Criterion 1, and the circularity guard on the whole section: the hand-written table *is* the rule. Run
    before any sample is rendered, so a rule that moved reddens here rather than turning every leg below it
    into a green restatement of an obsolete mapping. The gate is asked too, because a cell the gate refuses is
    not a cell. */
bool matrixTableIsTheRule(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    const auto resolved = engine::ChannelMap::resolve(cell.chA, cell.chB, cell.chOut);
    const auto decision = engine::gateLayout(cell.chA, cell.chB, cell.chOut);

    bool same = resolved.status == engine::ChannelMapStatus::ok
                && decision.verdict == engine::LayoutVerdict::accepted
                && decision.processed == cell.processed
                && resolved.map.channels == cell.processed;
    for (int slot = 0; slot < cell.processed; ++slot)
    {
        const auto index = static_cast<std::size_t>(slot);
        same = same && resolved.map.a[index] == cell.a[index] && resolved.map.b[index] == cell.b[index];
    }

    juce::String table;
    for (int slot = 0; slot < cell.processed; ++slot)
    {
        if (slot != 0)
            table += ", ";
        table += slotModelName(cell, static_cast<std::size_t>(slot));
    }
    return rig.report.line(same, cell.name, "M0 the literal table is resolve's",
                           juce::String(cell.family) + "; gate accepted, processed "
                           + juce::String(cell.processed) + "; " + table);
}

/** M1 — the geometry the cell claims, and the state every other leg depends on: one callback into
    NaN-prefilled output rows leaves nothing unwritten. Item 03 criterion 10's method, reused verbatim for
    the reason it records there — a zero peak measures the FFT's rounding, not the shell, and only a NaN
    survives a `configureRings` that returned early. The callback is delivered here rather than through
    `render()` because `render()` resets, and the first callback after a prepare is the one under test. */
bool matrixLegGeometry(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    juce::AudioProcessor &processor = rig.processor;
    const int samples = kMaxBlock;
    const int mainChannels = processor.getChannelCountOfBus(true, 0);
    const int sideChannels = processor.getChannelCountOfBus(true, 1);
    const int outputChannels = processor.getTotalNumOutputChannels();
    const int bufferChannels = std::max(processor.getTotalNumInputChannels(), outputChannels);

    const auto a = noiseStream(cell.chA, samples, 5150U, kMatrixAmplitude);
    const auto b = noiseStream(cell.chB, samples, 5151U, kMatrixAmplitude);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer(bufferChannels, samples);
    for (int channel = 0; channel < bufferChannels; ++channel)
        for (int i = 0; i < samples; ++i)
            buffer.setSample(channel, i, std::numeric_limits<float>::quiet_NaN());
    /* Bounded by the streams, not by the counts in force. This is the leg that reports a cell whose layout is
       not the one it declares, and a fill driven by the applied counts would read past a stream narrower than
       the bus the shell applied — the mismatch would arrive as a segfault instead of as this line. */
    const int fillMain = std::min(mainChannels, static_cast<int>(a.size()));
    const int fillSide = std::min(sideChannels, static_cast<int>(b.size()));
    for (int channel = 0; channel < fillMain; ++channel)
        for (int i = 0; i < samples; ++i)
            buffer.setSample(channel, i, a[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]);
    for (int channel = 0; channel < fillSide; ++channel)
        buffer.copyFrom(mainChannels + channel, 0,
                        b[static_cast<std::size_t>(channel)].data(), samples);

    processor.processBlock(buffer, midi);

    int unwritten = 0;
    double peak = 0.0;
    for (int channel = 0; channel < outputChannels; ++channel)
        for (int i = 0; i < samples; ++i)
        {
            const float sample = buffer.getSample(channel, i);
            if (!std::isfinite(sample))
                ++unwritten;
            else
                peak = std::max(peak, static_cast<double>(std::abs(sample)));
        }

    const auto decision = engine::gateLayout(cell.chA, cell.chB, cell.chOut);
    const bool geometry = mainChannels == cell.chA && sideChannels == cell.chB
                          && outputChannels == cell.chOut && processor.getLatencySamples() == cell.shape.hop
                          && decision.verdict == engine::LayoutVerdict::accepted
                          && decision.processed == cell.chOut && unwritten == 0;
    return rig.report.line(geometry, cell.name, "M1 geometry",
                           juce::String::formatted("%d out / %d side, R=%d, gate %s with processed %d, %d "
                                                   "unwritten of %d, peak %.4f",
                                                   outputChannels, sideChannels,
                                                   processor.getLatencySamples(),
                                                   decision.verdict == engine::LayoutVerdict::accepted
                                                       ? "accepted" : "refused",
                                                   decision.processed, unwritten, samples, peak));
}

/** M2 — §5.1's claim, generalised to every accepted layout: the shell adds nothing, so anything proven about
    the chain can be reasoned about wherever the chain is more convenient than the shell. The broadcast and
    odd-fold families had never had this leg. Shipped variant, stage parked on both sides. */
bool matrixLegShellIdentity(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    const auto throughShell = rig.renderBroadband();
    const auto direct = renderReference(cell.shape, rig.a, rig.b, cell.chA, cell.chB, rig.blocks,
                                        kMatrixMix);
    const auto delta = compareStreams(throughShell, direct);
    return rig.report.line(!delta.shapeMismatch && delta.bitIdentical, cell.name,
                           "M2 shell identity (Shipped)",
                           describe(delta) + ", "
                           + juce::String(static_cast<int>(rig.blocks.size())) + " callbacks");
}

/** M3 — M4 §7's buffer-size row, at whole-stack level for every accepted layout, including the consolidated
    ones with their per-hop cross-channel sum above the seam (the scheduler-level half of the same claim is
    `02` §10 criterion 6; this is not that leg re-derived). */
bool matrixLegBlockInvariance(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    const auto first = render(rig.processor, rig.a, cell.chB > 0 ? &rig.b : nullptr,
                              kBlockPlans[0].build(cell.runningLengthSamples()));

    double worst = 0.0;
    bool identical = true;
    juce::String plans;
    for (std::size_t plan = 1; plan < kBlockPlans.size(); ++plan)
    {
        const auto blocks = kBlockPlans[plan].build(cell.runningLengthSamples());
        const auto again = render(rig.processor, rig.a, cell.chB > 0 ? &rig.b : nullptr, blocks);
        const auto delta = compareStreams(again, first);
        worst = std::max(worst, delta.worst);
        identical = identical && !delta.shapeMismatch && delta.bitIdentical;
        plans += plan == 1 ? " " : ", ";
        plans += kBlockPlans[plan].name;
        plans += " (" + juce::String(static_cast<int>(blocks.size())) + ")";
    }
    return rig.report.line(identical, cell.name, "M3 block invariance",
                           "plan " + juce::String(kBlockPlans[0].name) + " ("
                           + juce::String(static_cast<int>(rig.blocks.size())) + ") bit-identical to"
                           + plans + ", max " + juce::String::formatted("%.3e", worst));
}

/** M4 — M4 §7's independence row, stated over mapped sources: an impulse in one A channel, every other A
    channel exactly zero, and every output whose table entry differs from that channel exactly 0.0f. The
    output whose entry matches has to be the pair rendered alone, which is what says the response came from
    the channel the map names and not from a neighbour through a shared buffer — the cheapest proof that
    item 02's pointer repetition did not alias two slots onto one scratch.

    The chB == 0 cells have no wet path to route wrongly: the shell pins p to −1 with no sidechain, so
    channel c can only ever carry A[c], and the whole render is A delayed by R bit-exact — the same
    statement about isolation, made through the dry path. */
bool matrixLegASideIndependence(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    const int total = cell.runningLengthSamples();
    const int latency = rig.processor.getLatencySamples();
    bool passed = true;

    for (int source = 0; source < cell.chA; ++source)
    {
        const auto a = singleImpulse(cell.chA, total, source, 0);
        const auto side = noiseStream(cell.chB, total, 60601U + static_cast<unsigned>(source), 0.3f);
        const auto out = render(rig.processor, a, cell.chB > 0 ? &side : nullptr, rig.blocks);

        double worst = 0.0;
        bool bitExact = true;
        int silentWrong = 0;
        int responding = 0;
        int deadMapped = 0;
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
            ++responding;
            const double slotPeak = channelPeak(out, index);
            responsePeak = std::max(responsePeak, slotPeak);
            /* The mapped slot has to respond in absolute terms as well as match its model. `slotModel` drives
               the same `HopScheduler` the shell does, so a resolution error inside the scheduler moves both
               sides of the comparison together and the delta stays zero — §5's arm 6 is that mutation, and the
               non-zero requirement is the matrix's only witness against it. */
            if (!(slotPeak > 0.0))
                ++deadMapped;
            /* With no sidechain the shell pins p to -1, so the response *is* the dry path and the model is
               A delayed by R — compared over the whole render, because every channel of it is part of the
               claim. With a wet path the model is the mapped pair rendered alone. */
            const auto delta = cell.chB == 0
                                   ? compareStreams(out, delayedBy(a, latency))
                                   : compareChannel(out, index,
                                                    slotModel(cell, a, index, side, rig.blocks), 0);
            worst = std::max(worst, delta.worst);
            bitExact = bitExact && !delta.shapeMismatch && delta.bitIdentical;
        }

        passed = rig.report.line(responding > 0 && deadMapped == 0 && silentWrong == 0 && bitExact, cell.name,
                                 ("M4 impulse in A" + juce::String(source)
                                  + (cell.chB == 0 ? " (no sidechain: the dry path)" : "")).toRawUTF8(),
                                 juce::String::formatted("%d slot(s) responded, %d silent, %d dead, max %.3e%s, "
                                                         "response peak %.4f",
                                                         responding, silentWrong, deadMapped, worst,
                                                         bitExact ? ", bit-exact" : "", responsePeak))
                 && passed;
    }
    return passed;
}

/** M5 — the B side of the same claim, per mapped source: broadcast slots answer their side's channel 0,
    consolidated slots answer the mean at 1/chB, diagonal slots answer their own index, and a slot whose
    source was not fed is exactly 0.0f. A modulo mapping, a broadcast where a fold belongs, or a fold applied
    to a stereo main all redden here. */
bool matrixLegBSideMapping(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    const int total = cell.runningLengthSamples();
    bool passed = true;

    for (int source = 0; source < cell.chB; ++source)
    {
        const auto side = singleImpulse(cell.chB, total, source, 0);
        const auto out = render(rig.processor, rig.a, &side, rig.blocks);

        const float scale = cell.b[0] == engine::kConsolidatedSource
                                ? 1.0f / static_cast<float>(cell.chB)
                                : 1.0f;
        const juce::String scaleText = scale == 1.0f
                                           ? juce::String()
                                           : " at 1/" + juce::String(cell.chB) + " of the mono-side model";

        double worst = 0.0;
        bool bitExact = true;
        int responding = 0;
        int silentWrong = 0;
        int deadMapped = 0;
        double responsePeak = 0.0;
        for (int slot = 0; slot < cell.chOut; ++slot)
        {
            const auto index = static_cast<std::size_t>(slot);
            const std::uint16_t selected = cell.b[index];
            const bool readsThis = selected == engine::kConsolidatedSource
                                   || selected == static_cast<std::uint16_t>(source);
            if (!readsThis)
            {
                if (!channelIsExactSilence(out, index))
                    ++silentWrong;
                continue;
            }
            ++responding;
            const double slotPeak = channelPeak(out, index);
            responsePeak = std::max(responsePeak, slotPeak);
            /* And in absolute terms, for the reason M4's leg gives: a mapped slot has to carry something even
               where a model that shares the scheduler would agree with a silenced one. */
            if (!(slotPeak > 0.0))
                ++deadMapped;
            const auto model = monoPairModel(cell, rig.a, index, side,
                                             static_cast<std::size_t>(source), scale, rig.blocks);
            const auto delta = compareChannel(out, index, model, 0);
            worst = std::max(worst, delta.worst);
            bitExact = bitExact && !delta.shapeMismatch && delta.bitIdentical;
        }

        passed = rig.report.line(responding > 0 && deadMapped == 0 && silentWrong == 0
                                     && worst <= kDeltaTolerance,
                                 cell.name, ("M5 impulse in B" + juce::String(source)).toRawUTF8(),
                                 juce::String::formatted("%d slot(s) answered, %d dead", responding,
                                                         deadMapped)
                                 + scaleText + juce::String::formatted(", max %.3e", worst)
                                 + (bitExact ? ", bit-identical" : "")
                                 + juce::String::formatted(", peak %.4f", responsePeak))
                 && passed;
    }

    if (cell.chB == 0)
    {
        /* D-002's distinction measured through a layout a host can apply rather than at the map: with the bus
           *disabled* the shell pins p to −1 and the render is the dry path, while the same main with the bus
           *connected and silent* at p = 0 is exact zeros. One rule that read "no sidechain" as "a silent mono
           sidechain" would render the second of these in the first case, which is what this line separates. */
        const auto *connected = matrixCell(cell.chA, 1);
        if (connected == nullptr)
            return rig.report.line(false, cell.name, "M5 no sidechain vs a silent sidechain",
                                   "the table has no mono-sidechain row for this main");

        const auto disabled = rig.renderBroadband();
        auto connectedProcessor = makeProcessor();
        if (!configure(*connectedProcessor, *connected->bus, connected->shape)
            || !parkLevelStage(*connectedProcessor))
            return false;
        const auto silent = render(*connectedProcessor, rig.a, nullptr, rig.blocks);

        bool silentIsExactZero = true;
        for (const auto &channel : silent)
            for (const float sample : channel)
                silentIsExactZero = silentIsExactZero && sample == 0.0f;

        bool disabledCarriesAudio = false;
        for (std::size_t channel = 0; channel < disabled.size() && !disabledCarriesAudio; ++channel)
            disabledCarriesAudio = channelPeak(disabled, channel) > 0.0;

        passed = rig.report.line(silentIsExactZero && disabledCarriesAudio, cell.name,
                                 "M5 no sidechain vs the same bus connected and silent",
                                 juce::String::formatted(
                                     "disabled: %s (peak %.4f, the pinned dry path); connected and silent: %s",
                                     disabledCarriesAudio ? "carries A" : "silence", peakOf(disabled),
                                     silentIsExactZero ? "exact 0.0f" : "NOT exact zeros"))
                 && passed;
    }
    return passed;
}

/** M6 — the dry path per channel, on the two cells that ship without a sidechain: output c is A[c] delayed
    by exactly R, bit-exact, which is what the latency pre-fill is for. */
bool matrixLegDryIdentity(MatrixRig &rig)
{
    const MatrixCell &cell = rig.cell;
    if (cell.chB != 0)
        return true; ///< the leg's domain is the no-sidechain cells; the others have a wet path to test

    const auto a = noiseStream(cell.chA, cell.runningLengthSamples(), 6060U, 0.4f);
    const auto out = render(rig.processor, a, nullptr, rig.blocks);
    const auto delta = compareStreams(out, delayedBy(a, rig.processor.getLatencySamples()));
    return rig.report.line(!delta.shapeMismatch && delta.bitIdentical, cell.name,
                           "M6 dry path per channel (p pinned to -1)",
                           describe(delta) + ", delayed by R="
                           + juce::String(rig.processor.getLatencySamples()));
}

/** F1 — the two asymmetric resolutions are different audio, each matching its own model.

    This is not the retired commutativity leg and it must not be read as one (F-013): after D-018 the two
    families have different channel counts, so there is nothing to swap and compare. What discriminates is
    that each resolution hears what its own rule says it hears — a mono main with two sidechain channels
    processes one channel and hears their mean, a stereo main with one sidechain channel processes two and
    hears that channel twice — and that the two renders of the same material are not the same samples. A
    modulo mapping or a broadcast applied where a fold belongs collapses one side of that pair. */
bool familyResolutionsDiffer(MatrixReport &report)
{
    const auto *monoMain = matrixCell(1, 2);
    const auto *stereoMain = matrixCell(2, 1);
    if (monoMain == nullptr || stereoMain == nullptr)
        return report.line(false, "F1", "the two resolutions", "the table has no (1,2,1) or (2,1,2) row");

    const int total = monoMain->runningLengthSamples();
    const auto blocks = blocksSummingTo(total, 0);
    const auto first = noiseStream(1, total, 717171U, kMatrixAmplitude);
    const auto second = noiseStream(1, total, 727272U, kMatrixAmplitude);
    const auto sideA = noiseStream(1, total, 818181U, kMatrixAmplitude);
    const auto sideB = noiseStream(1, total, 828282U, kMatrixAmplitude);
    const Stream mainA{ first[0] };                       ///< the mono main's one A channel
    const Stream wideSide{ sideA[0], sideB[0] };          ///< the consolidated pair, decorrelated
    const Stream mainB{ first[0], second[0] };            ///< the stereo main's two A channels
    const Stream oneSide{ sideA[0] };                     ///< the broadcast side: one channel

    const auto byFold = renderOf(*monoMain, mainA, &wideSide, blocks);
    const auto byBroadcast = renderOf(*stereoMain, mainB, &oneSide, blocks);

    const auto meanModel = meanOfRenders(renderReference(monoMain->shape, mainA, oneSide, 1, 1, blocks,
                                                         kMatrixMix),
                                         renderReference(monoMain->shape, mainA,
                                                         Stream{ sideB[0] }, 1, 1, blocks, kMatrixMix));
    const auto slot0Model = renderReference(stereoMain->shape, mainA, oneSide, 1, 1, blocks, kMatrixMix);
    const auto slot1Model = renderReference(stereoMain->shape, Stream{ second[0] }, oneSide, 1, 1, blocks,
                                            kMatrixMix);

    const auto foldDelta = compareChannel(byFold, 0, meanModel, 0);
    const auto firstSlot = compareChannel(byBroadcast, 0, slot0Model, 0);
    const auto secondSlot = compareChannel(byBroadcast, 1, slot1Model, 0);
    const auto apart = compareChannel(byFold, 0, byBroadcast, 0);

    bool passed = report.line(!foldDelta.shapeMismatch && foldDelta.worst <= kDeltaTolerance,
                              monoMain->name, "F1 hears the mean of the two sides", describe(foldDelta)
                              + ", peak " + juce::String::formatted("%.4f", peakOf(byFold)));
    passed = report.line(!firstSlot.shapeMismatch && firstSlot.worst <= kDeltaTolerance, stereoMain->name,
                         "F1 slot 0 hears channel 0 of the side", describe(firstSlot)) && passed;
    passed = report.line(!secondSlot.shapeMismatch && secondSlot.worst <= kDeltaTolerance, stereoMain->name,
                         "F1 slot 1 hears channel 0 of the side", describe(secondSlot)) && passed;
    passed = report.line(apart.worst > kDeltaTolerance, "(1,2,1) vs (2,1,2)",
                         "F1 the two resolutions are different audio",
                         describe(apart) + " — " + juce::String::formatted(
                             "peaks %.4f vs %.4f", peakOf(byFold), peakOf(byBroadcast))) && passed;
    return passed;
}

/** F3 — the level stage is confined to one channel, measured through the shell.

    `LevelNormalizer.hpp` states the claim and `DspTests` proves it by driving `processChannel` directly;
    nothing above `Dsp` had a witness, and D-020 ships independent per-channel gain on the strength of it.
    Here the stage is live — amount 1, the only leg in this section that runs it — on a stereo main whose
    channels are 20 dB apart, and the second render silences channel 0. Channel 1 cannot know what channel 0
    did: per-channel detection, per-channel gating, so its output is the same samples to the bit. What the
    resulting gain delta *sounds* like is M9 row 1's, and the peaks printed here are reported, not asserted
    (INDEX §3 rule 4). */
bool familyPerChannelConfinement(MatrixReport &report)
{
    const auto *cell = matrixCell(2, 1);
    if (cell == nullptr)
        return report.line(false, "F3", "per-channel confinement", "the table has no (2,1,2) row");

    const int total = cell->runningLengthSamples();
    const auto blocks = blocksSummingTo(total, 0);
    const auto loud = noiseStream(1, total, 313131U, 0.3f);
    const auto quiet = noiseStream(1, total, 424242U, 0.03f);
    const auto side = noiseStream(1, total, 535353U, 0.3f);
    const Stream bothChannels{ loud[0], quiet[0] };
    const Stream quietOnly{ std::vector<float>(quiet[0].size(), 0.0f), quiet[0] };

    auto processor = makeProcessor();
    if (!configure(*processor, *cell->bus, cell->shape))
        return false; ///< amount stays at the shipped default 1 — the stage is the subject of this leg

    dsp::LevelNormalizer stage; ///< the bound, asked of the class rather than restated: window + release
    stage.prepare(kSampleRate, 1, cell->shape.hop);
    const double stageBound = static_cast<double>(stage.windowSamples())
                            + kSampleRate * dsp::LevelNormalizer::releaseMs * 0.001;

    const auto withLoud = render(*processor, bothChannels, &side, blocks);
    const auto withoutLoud = render(*processor, quietOnly, &side, blocks);
    const auto delta = compareChannel(withLoud, 1, withoutLoud, 1);

    return report.line(delta.bitIdentical && static_cast<double>(total) > stageBound, cell->name,
                       "F3 per-channel confinement (Shipped, amount 1)",
                       describe(delta) + "; ch0 peak "
                       + juce::String::formatted("%.4f -> %.4f, ch1 peak %.4f -> %.4f",
                                                 channelPeak(withLoud, 0), channelPeak(withoutLoud, 0),
                                                 channelPeak(withLoud, 1), channelPeak(withoutLoud, 1))
                       + "; run " + juce::String(total) + " > window "
                       + juce::String(static_cast<int>(stage.windowSamples())) + " + release "
                       + juce::String(static_cast<int>(kSampleRate
                                                       * dsp::LevelNormalizer::releaseMs * 0.001))
                       + " (the ch0 delta is M9 row 1's, not this leg's)");
}

/** F2 — the fold's reciprocal, and the rounding pattern that goes with it.

    `foldSidechain` sums in ascending source order and applies 1/chB once, after the sum, for precisely the
    reason this leg prints: at chB ∈ {3, 5} the reciprocal is not representable, so scaling once and scaling
    every term are different floats. The leg gates the fold against the harness's own one-reciprocal mean and
    prints the gap a per-term scaling would produce, so the number is on the record and a future
    "optimisation" that moves the multiply inside the loop shows up as a changed render rather than as a
    silent rounding drift. The dyadic widths print a zero gap — 1/2 and 1/8 scale exactly — which is the
    control that keeps the non-zero ones meaningful. */
bool familyFoldReciprocal(MatrixReport &report)
{
    bool passed = true;
    for (const int width : {2, 3, 5, 8})
    {
        const auto *cell = matrixCell(1, width);
        if (cell == nullptr)
        {
            passed = report.line(false, "F2", "the fold's reciprocal",
                                 "the table has no (1," + juce::String(width) + ",1) row") && passed;
            continue;
        }
        const int total = cell->runningLengthSamples();
        const auto blocks = blocksSummingTo(total, 0);
        const auto a = noiseStream(1, total, 919191U, kMatrixAmplitude);
        const auto side = noiseStream(width, total, 929292U + static_cast<unsigned>(width),
                                      kMatrixAmplitude);
        const float scale = 1.0f / static_cast<float>(width);

        auto processor = makeProcessor();
        if (!configure(*processor, *cell->bus, cell->shape) || !parkLevelStage(*processor))
            return false;
        const auto folded = render(*processor, a, &side, blocks);

        const auto once = meanWithOneReciprocal(side, scale);
        const auto perTerm = meanScalingEveryTerm(side, scale);
        const auto onceModel = renderReference(cell->shape, a, once, 1, 1, blocks, kMatrixMix);
        const auto perTermModel = renderReference(cell->shape, a, perTerm, 1, 1, blocks, kMatrixMix);

        const auto matched = compareChannel(folded, 0, onceModel, 0);
        const auto gap = compareStreams(onceModel, perTermModel);
        const bool dyadic = (width & (width - 1)) == 0;

        /* The gate is the fold against the one-reciprocal mean. The gap is asserted only in the direction
           that keeps this leg meaningful: a dyadic width must show no difference between the two scalings,
           and a non-dyadic one must show some, or the leg would be unable to tell them apart. */
        const juce::String dyadism = dyadic ? "dyadic, so the two scalings are the same float"
                                            : "not dyadic, so they are not";
        passed = report.line(matched.worst <= kDeltaTolerance && !matched.shapeMismatch
                                 && (dyadic ? !(gap.worst > 0.0) : gap.worst > 0.0),
                             cell->name, "F2 the fold's reciprocal",
                             juce::String::formatted("fold == one-reciprocal mean, max %.3e", matched.worst)
                             + (matched.bitIdentical ? ", bit-identical" : "") + "; 1/"
                             + juce::String(width) + " is " + dyadism
                             + " — scaling every term instead would move the render by "
                             + juce::String::formatted("%.3e", gap.worst))
                     && passed;
    }
    return passed;
}

/** §5.20 in full: the nine cells (plus the shipped-shape witness), six per-cell legs each, then the three
    family legs. Returns the section's verdict; its own wall clock is printed by `main`, which is where the
    pair of `juce::Time::getCurrentTime()` calls around this call lives. */
bool checkChannelMatrix()
{
    std::cout << "[5.20] the channel matrix (M4 item 07). Nine of the twelve gate-accepted triples at\n"
                 "       H=512 L=1024, plus (1,5,1) again at the shipped default: the literal mapping table\n"
                 "       checked against resolve, then geometry, shell identity, three block plans,\n"
                 "       A-side and B-side independence per mapped source, and the dry path. Family legs:\n"
                 "       the two asymmetric resolutions each matching their own model (no commutativity —\n"
                 "       F-013), the level stage confined to one channel with the stage live, and the fold's\n"
                 "       reciprocal. No product code, no per-channel cost (M7), no audibility (M9).\n";
    bool passed = true;
    MatrixReport report;

    /* Criterion 2 before anything else: a run shorter than the bounds it asserts across cannot decide them.
       The level stage's window is asked of the class at the matrix's hop, which is the widest of the two
       shapes here and therefore the conservative figure. */
    dsp::LevelNormalizer bounds;
    bounds.prepare(kSampleRate, 1, kMatrix.hop);
    const int releaseSamples = static_cast<int>(kSampleRate * dsp::LevelNormalizer::releaseMs * 0.001);
    const int stageBound = static_cast<int>(bounds.windowSamples()) + releaseSamples;
    std::cout << "       bounds every assertion lands after: 2L (the response to an impulse at n0 = 0\n"
                 "       occupies [L, 2L-1]), R, and the level stage's window + release = "
              << bounds.windowSamples() << " + " << releaseSamples << " = " << stageBound << " samples\n";

    for (const MatrixCell &cell : kMatrixCells)
    {
        const int total = cell.runningLengthSamples();
        const int length = EntangleAudioProcessor::clampGrainLength(cell.shape.chosen, cell.shape.hop);
        const bool longEnough = total > 2 * length && total > cell.shape.hop && total > stageBound;
        std::cout << "       " << cell.name << ": H=" << cell.shape.hop << " L=" << length << ", " << total
                  << " samples (" << total / cell.shape.hop << " hops) vs 2L=" << 2 * length << ", R="
                  << cell.shape.hop << ", stage " << stageBound << "\n";
        if (!longEnough)
        {
            passed = report.line(false, cell.name, "M0 run length",
                                 juce::String::formatted("%d samples does not clear 2L=%d, R=%d and the "
                                                         "stage's %d", total, 2 * length, cell.shape.hop,
                                                         stageBound)) && passed;
            continue;
        }

        auto processor = makeProcessor();
        if (!configure(*processor, *cell.bus, cell.shape) || !parkLevelStage(*processor))
            return false;

        const auto blocks = blocksSummingTo(total, 0);
        const auto a = noiseStream(cell.chA, total, 5150U, kMatrixAmplitude);
        const auto b = noiseStream(cell.chB, total, 5151U, kMatrixAmplitude);
        MatrixRig rig{ report, *processor, cell, blocks, a, b };

        const auto cellStart = juce::Time::getCurrentTime();
        passed = matrixTableIsTheRule(rig) && passed;
        /* The five sample legs index the cell's streams by the counts the shell applied, so they run only once
           M1 has established those agree. A cell that is not in the layout it declares gets that one FAIL line
           and the rest of the cell is skipped — the honest verdict, where without the guard the divergence
           reads past a stream and the run dies inside renderSwept. */
        const bool geometryOk = matrixLegGeometry(rig);
        passed = geometryOk && passed;
        if (geometryOk)
        {
            passed = matrixLegShellIdentity(rig) && passed;
            passed = matrixLegBlockInvariance(rig) && passed;
            passed = matrixLegASideIndependence(rig) && passed;
            passed = matrixLegBSideMapping(rig) && passed;
            passed = matrixLegDryIdentity(rig) && passed;
        }
        std::cout << "       " << cell.name << " cost "
                  << juce::String::formatted("%.2f s",
                                             (juce::Time::getCurrentTime() - cellStart).inSeconds())
                  << "\n";
    }

    passed = familyResolutionsDiffer(report) && passed;
    passed = familyPerChannelConfinement(report) && passed;
    passed = familyFoldReciprocal(report) && passed;

    std::cout << "       " << (sizeof(kMatrixCells) / sizeof(kMatrixCells[0])) << " cells x 6 per-cell legs\n"
              << "       plus the table check and 3 family legs: " << report.decided << " lines decided, "
              << report.failed << " FAIL\n";
    return passed && report.failed == 0;
}
/* ------------------------------------------------------------------- §5.21 ---
   The layout report (M4 item 08): the line a human in a host reads, kept honest from a console.

   `EntangleAudioProcessor::layoutReport()` is the instrument `08` is built around. No host has ever loaded
   this plugin so far as this repo's evidence goes, so the answers D-018's reachability note and D-021 B's
   third trigger are waiting on have to be *records* of what the plugin was told rather than readings of a
   host's labels — F-015 is what that reading produced last time. A record nobody can check is how a report
   turns into a lie, so every field below is compared against an authority outside the report: the accessors
   for the counts, the layout the harness applied for the names, `gateLayout` for the verdict pair,
   `clampGrainLength` for the running length, and the two counters against what `configureRings` was asked
   to do.

   C1  every field of a real report, for all twelve gate-accepted triples.
   C1b what JUCE 9.0.4 calls each bus shape — the surround and ambisonic half of D-021 B's third trigger,
       answerable without a host, since the report carries `getDescription()` verbatim.
   C2  `prepares=`/`skipped=` count prepares and declined arrivals rather than pokes (D-022 B from outside).
   C3  a refused layout moves neither the report's configuration nor the running render, and the refusal
       vocabulary the gate produces is pinned where it is observable.
   C4  exactly one log line per real prepare, captured through a `juce::Logger` subclass.
   C5  the guard's negative branch: a prepare on a non-message thread logs nothing, crashes nothing, and the
       next message-thread prepare refreshes the report and logs.

   C6 is `tools/PrepareFootprint`'s before/after table and C7 the unchanged §5.1…§5.20 legs plus the
   no-allocation suites; neither belongs in this binary (D-022 C: the allocation hook cannot live here).
   What this section cannot test at all is stated in `08` §5 arm 8: the editor label is unreachable without a
   display, so P0/H0 is its only judge. */

/** Reads a `key=value` field out of a report line. A bracketed value can contain spaces — JUCE's own channel
    descriptions do, "5.1 Surround" and "Discrete #8" among them — so the scan ends at the closing bracket
    rather than at the next whitespace. Keys are matched at a token boundary, or `side=` would find the
    `side` of `sideActive=`. */
juce::String reportField(const juce::String &report, const juce::String &key)
{
    const auto padded = " " + report + " ";
    const auto at = padded.indexOf(" " + key + "=");
    if (at < 0)
        return {};

    const auto value = padded.substring(at + key.length() + 2);
    if (!value.startsWith("["))
        return value.upToFirstOccurrenceOf(" ", false, false);

    const auto close = value.indexOf("]");
    return close < 0 ? value : value.substring(0, close + 1);
}

/** The configuration fields of a report, in the order they are written. The counters are deliberately not in
    it: a refusal poked with `processorLayoutsChanged` is a skipped arrival, and a counter that grew is the
    evidence D-022 B is read from — `08` §11 records why C3's "byte-identical report" cannot mean the whole
    line. */
const char *const kReportConfigurationKeys[] = {"inBuses",   "outBuses",   "mainIn",  "mainInName",
                                                "mainInEn",  "side",       "sideName", "sideEn",
                                                "mainOut",   "mainOutName", "mainOutEn", "gate",
                                                "processed", "outCh",      "rate",     "hop",
                                                "Lreq",      "Lrun",       "R",        "block",
                                                "sideActive", "blendEff"};

/** Every configuration field in one string: what a refused layout must not move. */
juce::String reportConfiguration(const juce::String &report)
{
    auto text = juce::String(" ");
    for (const char *key : kReportConfigurationKeys)
        text += juce::String(key) + "=" + reportField(report, key) + " ";
    return text;
}

/** The configuration keys whose value differs between two reports, in field order — "hop,R" for a hop move,
    empty for two reports of one configuration. C3's claim is about *which* fields moved, so the answer is a
    name list rather than a yes/no on the whole line. */
juce::String reportDifference(const juce::String &before, const juce::String &after)
{
    juce::String changed;
    for (const char *key : kReportConfigurationKeys)
    {
        if (reportField(before, key) == reportField(after, key))
            continue;
        if (changed.isNotEmpty())
            changed += ",";
        changed += key;
    }
    return changed;
}

/** The two counters read back out of a live report — the only way to see D-022 B's decision from outside the
    class, and the counters exist (D-023 D) because bit-exact renders say nothing to a human in a session. */
struct ReportCounters
{
    int prepares = 0;
    int skipped = 0;
};

ReportCounters countersOf(const juce::String &report)
{
    return {reportField(report, "prepares").getIntValue(), reportField(report, "skipped").getIntValue()};
}

/** Swallows the plugin's report lines for every section except §5.21, forwarding anything else to stdout the
    way JUCE's default logger does.

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

/** Counts the report lines the plugin writes while installed as JUCE's logger (C4, C5). */
struct ReportLogCapture : juce::Logger
{
    int lines = 0;

    void logMessage(const juce::String &message) override
    {
        if (message.contains("entangle layout"))
            ++lines;
    }
};

/** The bus set a host would route for a given width, so the report's name field has an expectation the
    harness chose rather than one it read back: 1 and 2 are JUCE's named sets, above that a host offers
    discrete channels, and 0 is the disabled state — which is not a mono sidechain (D-002). */
juce::AudioChannelSet setForWidth(int channels)
{
    if (channels <= 0)
        return juce::AudioChannelSet::disabled();
    if (channels == 1)
        return juce::AudioChannelSet::mono();
    if (channels == 2)
        return juce::AudioChannelSet::stereo();
    return juce::AudioChannelSet::discreteChannels(channels);
}

juce::String tripleName(int chA, int chB)
{
    return "(" + juce::String(chA) + "," + juce::String(chB) + "," + juce::String(chA) + ")";
}

/** C1 — every field of a report written by a real prepare, for the twelve triples `gateLayout` accepts with
    the main bus at mono or stereo (§1.5 of the plan). Each expectation comes from outside the report: a
    field that only agrees with itself would not be evidence. */
bool reportFieldsMatchTheShell(MatrixReport &report)
{
    const int chAs[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2};
    const int chBs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2};

    for (std::size_t index = 0; index < sizeof(chAs) / sizeof(chAs[0]); ++index)
    {
        const int chA = chAs[index];
        const int chB = chBs[index];
        const auto name = tripleName(chA, chB);

        auto processor = makeProcessor();
        BusSetup bus{name.toRawUTF8(), setForWidth(chA), chB > 0, setForWidth(chB)};
        if (!configure(*processor, bus, kDefault))
            return false;

        const auto text = shellOf(*processor).layoutReport();
        const auto decision = engine::gateLayout(chA, chB, chA);
        int checked = 0;
        int wrong = 0;

        auto want = [&](const char *key, const juce::String &expected)
        {
            ++checked;
            const auto got = reportField(text, key);
            if (got == expected)
                return;
            ++wrong;
            std::cout << "  FAIL " << name << ": report says " << key << "=" << got
                      << ", the authority says " << expected << "\n";
        };

        want("prepares", "1");
        want("skipped", "0");
        want("inBuses", juce::String(processor->getBusCount(true)));
        want("outBuses", juce::String(processor->getBusCount(false)));
        want("mainIn", juce::String(processor->getChannelCountOfBus(true, 0)));
        want("mainInName", "[" + bus.mainSet.getDescription() + "]");
        want("mainInEn", "1");
        want("side", juce::String(processor->getChannelCountOfBus(true, 1)));
        want("sideName", "[" + bus.sideSet.getDescription() + "]");
        want("sideEn", chB > 0 ? "1" : "0");
        want("mainOut", juce::String(processor->getChannelCountOfBus(false, 0)));
        want("mainOutName", "[" + bus.mainSet.getDescription() + "]");
        want("mainOutEn", "1");
        want("gate", juce::String(verdictName(decision.verdict)) + "/" + ruleName(decision.rule));
        want("processed", juce::String(decision.processed));
        want("outCh", juce::String(processor->getTotalNumOutputChannels()));
        want("rate", juce::String(processor->getSampleRate()));
        want("hop", juce::String(kDefault.hop));
        want("Lreq", juce::String(kDefault.chosen));
        want("Lrun", juce::String(EntangleAudioProcessor::clampGrainLength(kDefault.chosen, kDefault.hop)));
        want("R", juce::String(processor->getLatencySamples()));
        want("block", juce::String(kMaxBlock) + "/"
                         + juce::String(EntangleAudioProcessor::maxHostBlockFor(kMaxBlock)));
        want("sideActive", shellOf(*processor).isSidechainActive() ? "1" : "0");
        want("blendEff", juce::String(shellOf(*processor).effectiveBlendPosition(), 3));

        const bool prefixed = text.startsWith("entangle layout");
        if (!prefixed)
        {
            ++wrong;
            std::cout << "  FAIL " << name << ": the report does not start with its own tag: " << text
                      << "\n";
        }

        report.line(wrong == 0, name.toRawUTF8(), "C1 fields vs their authorities",
                    juce::String::formatted("%d fields, %d wrong", checked, wrong) + " — " + text);
    }
    return true;
}

/** C1b — D-021 B's third trigger, the half that needs no host. The report carries
    `AudioChannelSet::getDescription()` verbatim, so these are the words a surround or ambisonic bus arrives
    under the day the main bus is widened; pinning them is what keeps the answer from being re-derived (and
    silently changed) by the next session. */
bool channelSetNamesAreOnTheRecord(MatrixReport &report)
{
    struct Named
    {
        juce::AudioChannelSet set;
        const char *expected;
    };
    /* The first row is the surprise this leg exists to catch: JUCE tests `isDiscreteLayout()` before it
       compares against `disabled()` (juce_AudioChannelSet.cpp:424), so a 0-channel bus is named
       "Discrete #0", not "Disabled". The report carries that verbatim — D-023's whole argument is that the
       host's words are the record — and `side=0 sideEn=0` alongside it is what tells a human reading a
       host table that the bus is absent rather than a 0-width one. Pinned here because the alternative is
       the next session "fixing" the report to say Disabled and losing the fidelity. */
    const std::vector<Named> shapes = {
        {juce::AudioChannelSet::disabled(), "Discrete #0"},
        {juce::AudioChannelSet::mono(), "Mono"},
        {juce::AudioChannelSet::stereo(), "Stereo"},
        {juce::AudioChannelSet::quadraphonic(), "Quadraphonic"},
        {juce::AudioChannelSet::create5point1(), "5.1 Surround"},
        {juce::AudioChannelSet::create7point1(), "7.1 Surround"},
        {juce::AudioChannelSet::octagonal(), "Octagonal"},
        {juce::AudioChannelSet::ambisonic(1), "1st Order Ambisonics"},
        {juce::AudioChannelSet::ambisonic(2), "2nd Order Ambisonics"},
        {juce::AudioChannelSet::discreteChannels(8), "Discrete #8"},
        {juce::AudioChannelSet::discreteChannels(9), "Discrete #9"}};

    int wrong = 0;
    auto listed = juce::String("size:name");
    for (const auto &shape : shapes)
    {
        const auto described = shape.set.getDescription();
        if (described != shape.expected)
        {
            ++wrong;
            std::cout << "  FAIL JUCE calls a " << shape.set.size() << "-channel set \"" << described
                      << "\", pinned as \"" << shape.expected << "\"\n";
        }
        listed += " " + juce::String(shape.set.size()) + ":" + described;
    }

    return report.line(wrong == 0, "naming", "C1b JUCE's own words for a bus shape",
                       juce::String::formatted("%d shapes, %d off the pinned name:",
                                               static_cast<int>(shapes.size()), wrong)
                           + listed);
}

/** C2 — the counters count prepares and declined arrivals, not pokes. Each step says what a host does and
    what the pair has to say about it; the arithmetic is D-022 B's, seen from outside the class. */
bool countersCountPreparesNotPokes(MatrixReport &report)
{
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const auto read = [&processor] { return countersOf(shellOf(*processor).layoutReport()); };
    const auto start = read();
    if (!report.line(start.prepares == 1 && start.skipped == 0, "(2,2,2)",
                     "C2 first prepare of a fresh instance",
                     juce::String::formatted("prepares=%d skipped=%d, expected 1 and 0", start.prepares,
                                             start.skipped)))
        return false;

    /* An unchanged layout re-applied. JUCE's `updateBuses` ends with `processorLayoutsChanged()`
       (juce_AudioProcessor.cpp:880), so `setBusesLayout` is already one arrival and the explicit call is the
       second one a host adds. Neither may rebuild. */
    applyLayout(*processor, kStereoWithB);
    processor->processorLayoutsChanged();
    const auto repeated = read();
    if (!report.line(repeated.prepares == start.prepares && repeated.skipped > start.skipped, "(2,2,2)",
                     "C2 unchanged layout re-applied",
                     juce::String::formatted("prepares=%d skipped=%d, expected %d and more than %d",
                                             repeated.prepares, repeated.skipped, start.prepares,
                                             start.skipped)))
        return false;

    /* A second `prepareToPlay` with the numbers it just had — the F-001 pattern, hosts that re-prepare
       without meaning anything by it. */
    processor->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
    processor->prepareToPlay(kSampleRate, kMaxBlock);
    const auto replayed = read();
    if (!report.line(replayed.prepares == repeated.prepares && replayed.skipped == repeated.skipped + 1,
                     "(2,2,2)", "C2 repeated prepareToPlay",
                     juce::String::formatted("prepares=%d skipped=%d, expected %d and %d", replayed.prepares,
                                             replayed.skipped, repeated.prepares, repeated.skipped + 1)))
        return false;

    /* A hop move is a real change, so exactly one prepare arrives — the dial itself reconfigures through an
       AsyncUpdater this harness never runs, so the explicit poke below is the one arrival. */
    if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId, indexOfHop(256)))
        return false;
    processor->processorLayoutsChanged();
    const auto moved = read();
    if (!report.line(moved.prepares == replayed.prepares + 1 && moved.skipped == replayed.skipped, "(2,2,2)",
                     "C2 hop dial 128 -> 256",
                     juce::String::formatted("prepares=%d skipped=%d, expected %d and %d", moved.prepares,
                                             moved.skipped, replayed.prepares + 1, replayed.skipped)))
        return false;

    /* And a layout the gate takes, arriving on JUCE's own call rather than ours. */
    applyLayout(*processor, kMonoWithEightB);
    const auto widened = read();
    const auto text = shellOf(*processor).layoutReport();
    report.line(widened.prepares == moved.prepares + 1 && reportField(text, "gate") == "accepted/ok"
                    && reportField(text, "side") == "8",
                "(1,8,1)", "C2 layout change the gate takes",
                juce::String::formatted("prepares=%d skipped=%d, expected %d; report now side=%s gate=%s",
                                        widened.prepares, widened.skipped, moved.prepares + 1,
                                        reportField(text, "side").toRawUTF8(),
                                        reportField(text, "gate").toRawUTF8()));
    return true;
}

/** C3 — a refused layout moves nothing a host can read or hear, and the verdict words a refusal produces are
    pinned here rather than in the report: `configureRings` reads the *live* buses, and JUCE applies nothing
    for a layout the gate refuses (§5.18: 0 of 192 probes land on a refused layout), so a report built from
    what got built can never carry `mainWidth` or `asymmetric`. That is `08` §11's correction to C1's refusal
    clause, and the counters are what shows the refused attempt was seen at all. */
bool refusalMovesNothing(MatrixReport &report)
{
    struct Refusal
    {
        const char *what;
        LayoutRow row;
        const char *gate;
    };
    const std::vector<Refusal> refusals = {
        {"stereo main, 3-wide side",
         {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::discreteChannels(3),
          juce::AudioChannelSet::stereo()},
         "channelRule/asymmetric"},
        {"quad main, no side",
         {juce::AudioChannelSet::quadraphonic(), juce::AudioChannelSet::disabled(),
          juce::AudioChannelSet::quadraphonic()},
         "mainWidth/ok"},
        {"mono in, stereo out",
         {juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled(),
          juce::AudioChannelSet::stereo()},
         "mainCountMismatch/countMismatch"}};

    for (const auto &refusal : refusals)
    {
        const auto decision = engine::gateLayout(refusal.row.mainIn.size(), sideCountOf(refusal.row.side),
                                                 refusal.row.mainOut.size());
        const auto named = juce::String(verdictName(decision.verdict)) + "/" + ruleName(decision.rule);

        auto processor = makeProcessor();
        /* A real prepare first, because that is the state a refusal arrives *into*: with no stored key the
           guard has nothing to compare and the counters would read 0/0 no matter what the refusal did. */
        if (!configure(*processor, kStereoWithB, kDefault))
            return false;
        const auto before = shellOf(*processor).layoutReport();

        const bool rejected = !processor->setBusesLayout(layoutOf(*processor, refusal.row));
        const auto after = shellOf(*processor).layoutReport();

        processor->processorLayoutsChanged();
        processor->processorLayoutsChanged();
        const auto poked = shellOf(*processor).layoutReport();

        const auto counters = countersOf(poked);
        report.line(rejected && named == refusal.gate && after == before
                        && reportConfiguration(poked) == reportConfiguration(before)
                        && counters.skipped == 2 && counters.prepares == 1,
                    refusal.what, "C3 refusal moves no field",
                    juce::String::formatted("gate=%s (pinned %s), layout %s, report %s, "
                                            "prepares=%d skipped=%d after two pokes",
                                            named.toRawUTF8(), refusal.gate,
                                            rejected ? "applied nothing" : "WAS APPLIED",
                                            after == before ? "byte-identical" : "CHANGED",
                                            counters.prepares, counters.skipped));
    }

    /* C3's other half: the reconfigure that *is* real. A hop move has to reach the record, and the only
       fields it may reach are the hop-derived ones — the buses, the gate words, the rate and the block are the
       same configuration and must read the same. Named as a diff set rather than a field list because which
       of the `L` fields moves depends on where the clamp catches the dial, and the leg is about the ones that
       do not move. */
    {
        auto processor = makeProcessor();
        if (!configure(*processor, kStereoWithB, kDefault))
            return false;
        const auto before = shellOf(*processor).layoutReport();

        if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId, indexOfHop(512)))
            return false;
        processor->processorLayoutsChanged();
        const auto after = shellOf(*processor).layoutReport();

        const auto moved = reportDifference(before, after);
        report.line(moved == "hop,R" && reportField(after, "hop") == "512"
                        && reportField(after, "R") == "512" && countersOf(after).prepares == 2,
                    "(2,2,2)", "C3 hop-only reconfigure moves the hop-derived fields",
                    "changed: " + (moved.isEmpty() ? juce::String("<nothing>") : moved)
                        + "; report after: " + after);
    }

    /* The render half, §5.19's method: a refused request and two re-configure pokes on a transport that is
       already running. */
    {
        const Shape shape = kTightest;
        const int total = 4 * shape.chosen + 4 * shape.hop;
        const auto blocks = blocksSummingTo(total, 0);
        const auto a = noiseStream(2, total, 80808U, 0.3f);
        const auto b = noiseStream(2, total, 80809U, 0.3f);

        auto processor = makeProcessor();
        if (!configure(*processor, kStereoWithB, shape)
            || !setFloat(*processor, EntangleAudioProcessor::blendParameterId, 0.5f))
            return false;
        const auto before = render(*processor, a, &b, blocks);

        auto quad = processor->getBusesLayout();
        quad.getChannelSet(true, 0) = juce::AudioChannelSet::quadraphonic();
        quad.getChannelSet(false, 0) = juce::AudioChannelSet::quadraphonic();
        const bool rejected = !processor->setBusesLayout(quad);
        processor->processorLayoutsChanged();
        processor->processorLayoutsChanged();

        const auto after = render(*processor, a, &b, blocks);
        const auto running = shellOf(*processor).layoutReport();
        const auto counters = countersOf(running);
        std::cout << "       the render half: a quadraphonic request "
                  << (rejected ? "was refused" : "WAS ACCEPTED") << ", and the report still names "
                  << reportField(running, "mainIn") << " in / " << reportField(running, "mainOut")
                  << " out with prepares=" << counters.prepares << " skipped=" << counters.skipped << "\n";
        return expectIdentical(after, before,
                               "a refused layout and two re-configures leave the render alone")
               && rejected;
    }
}

/** C4 and C5 — the log line and its guard. C4 is the count a host record depends on: one line per real
    prepare, none per declined arrival. C5 says plainly that it exercises the branch and not a race: this
    plugin has no audio thread in a console, so what the leg can prove is that a prepare on a thread the
    message manager does not know writes no line, refreshes the report anyway, and leaves the next
    message-thread prepare free to log. */
bool logLineFollowsTheGuard(MatrixReport &report, ReportLogCapture &capture)
{
    const int linesAtEntry = capture.lines;
    auto processor = makeProcessor();
    if (!configure(*processor, kStereoWithB, kDefault))
        return false;

    const int firstLine = capture.lines - linesAtEntry;
    if (!report.line(firstLine == 1, "(2,2,2)", "C4 one line for the first prepare",
                     juce::String::formatted("%d captured lines, expected 1", firstLine)))
        return false;

    /* Declined arrivals add none — the whole point of tying the write to the prepare rather than to the
       poke. Two arrivals here: JUCE's own call and the host's `prepareToPlay` after it. */
    const int skippedAtFirst = countersOf(shellOf(*processor).layoutReport()).skipped;
    processor->processorLayoutsChanged();
    processor->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
    processor->prepareToPlay(kSampleRate, kMaxBlock);
    const auto skippedReport = shellOf(*processor).layoutReport();
    report.line(capture.lines == linesAtEntry + 1
                    && countersOf(skippedReport).skipped == skippedAtFirst + 2,
                "(2,2,2)", "C4 two declined arrivals, no line",
                juce::String::formatted("%d captured lines (expected still 1), skipped %d -> %d",
                                        capture.lines - linesAtEntry, skippedAtFirst,
                                        countersOf(skippedReport).skipped));

    /* Three real prepares — three lines. */
    const int hops[] = {256, 512, 64};
    const int before = capture.lines;
    for (const int hop : hops)
    {
        if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId, indexOfHop(hop)))
            return false;
        processor->processorLayoutsChanged();
    }
    const auto counted = shellOf(*processor).layoutReport();
    report.line(capture.lines == before + 3 && reportField(counted, "hop") == "64"
                    && reportField(counted, "R") == "64",
                "(2,2,2)", "C4 three real prepares, three lines",
                juce::String::formatted("%d new lines for 3 prepares; hop=%s R=%s prepares=%s",
                                        capture.lines - before, reportField(counted, "hop").toRawUTF8(),
                                        reportField(counted, "R").toRawUTF8(),
                                        reportField(counted, "prepares").toRawUTF8()));

    /* C5 — the negative branch. A different block size is a key change, so this prepare is real and would
       log if the guard were missing; the pair in `block=` is also D-022 A's floor, visible from a host. */
    const int offThreadBefore = capture.lines;
    std::thread offThread([&processor] {
        processor->setRateAndBufferSizeDetails(kSampleRate, 2048);
        processor->prepareToPlay(kSampleRate, 2048);
    });
    offThread.join();

    const auto offThreadReport = shellOf(*processor).layoutReport();
    report.line(capture.lines == offThreadBefore && reportField(offThreadReport, "block") == "2048/4096"
                    && countersOf(offThreadReport).prepares == 5,
                "(2,2,2)", "C5 non-message-thread prepare: no line, report refreshed",
                juce::String::formatted("%d new lines (expected 0), block=%s prepares=%d",
                                        capture.lines - offThreadBefore,
                                        reportField(offThreadReport, "block").toRawUTF8(),
                                        countersOf(offThreadReport).prepares));

    /* And the message-thread prepare after it both refreshes and logs, which is what proves the leg tested a
       thread and not a broken logger. */
    if (!setChoice(*processor, EntangleAudioProcessor::hopParameterId, indexOfHop(512)))
        return false;
    processor->processorLayoutsChanged();
    const auto refreshed = shellOf(*processor).layoutReport();
    report.line(capture.lines == offThreadBefore + 1 && reportField(refreshed, "hop") == "512"
                    && reportField(refreshed, "block") == "2048/4096",
                "(2,2,2)", "C5 the next message-thread prepare logs and refreshes",
                juce::String::formatted("%d new line (expected 1), hop=%s block=%s",
                                        capture.lines - offThreadBefore,
                                        reportField(refreshed, "hop").toRawUTF8(),
                                        reportField(refreshed, "block").toRawUTF8()));
    return true;
}

/** §5.21 in full: the instrument that turns a host answer into a record, checked field by field from a
    console so it cannot rot between host runs. One line per decision, tallied by `MatrixReport`, so a field
    that drifted names itself instead of leaving a bare FAIL for the section. */
bool checkLayoutReport(ReportLogFilter &filter)
{
    std::cout << "[5.21] the layout report (M4 item 08). Every field of a real report against an authority\n"
                 "       outside it, for the twelve gate-accepted triples; JUCE's own words for each bus\n"
                 "       shape (D-021 B's third trigger, the half that needs no host); the two counters\n"
                 "       against what configureRings was asked to do (D-022 B from outside); a refused\n"
                 "       layout moving neither the report nor the render; and the log guard on both of its\n"
                 "       branches. C6 is PrepareFootprint's before/after table and C7 the unchanged legs of\n"
                 "       §5.1…§5.20 — neither belongs in this binary (D-022 C).\n";

    /* C4's precondition, checked rather than assumed: `ScopedJuceInitialiser_GUI` in `main()` calls
       `initialiseJuce_GUI()`, whose `MessageManager` constructor records the thread that built it as the
       message thread (juce_MessageManager.cpp:38). So the guard's positive branch is live here without the
       harness asking for it — the measured fact behind F-018, the reason the filter installed in `main()` has
       to be *replaced* rather than supplemented to see a line, and the fact H0 records about a real host,
       where the VST3 wrapper designates its own thread (juce_audio_plugin_client_VST3.cpp:280) and a host that
       prepares on the audio thread therefore shows nothing in its log. */
    ReportLogCapture capture;
    juce::Logger::setCurrentLogger(&capture);

    MatrixReport report;
    const bool onMessageThread = juce::MessageManager::existsAndIsCurrentThread();
    bool passed = report.line(onMessageThread, "premise", "C4 precondition: this thread is the message one",
                              juce::String::formatted("existsAndIsCurrentThread() = %s",
                                                      onMessageThread ? "true" : "false"));
    passed = reportFieldsMatchTheShell(report) && passed;
    passed = channelSetNamesAreOnTheRecord(report) && passed;
    passed = countersCountPreparesNotPokes(report) && passed;
    passed = refusalMovesNothing(report) && passed;
    passed = logLineFollowsTheGuard(report, capture) && passed;

    juce::Logger::setCurrentLogger(&filter);

    std::cout << "       " << report.decided << " lines decided, " << report.failed << " FAIL; "
              << capture.lines << " report lines reached the installed Logger across the section\n";
    return passed && report.failed == 0;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui; ///< the APVTS flush timer needs a message manager

    /* Installed before the first section because the plugin logs its report on every prepare this thread
       makes, and this thread is the message thread the moment `gui` exists (F-018). §5.21 is the section that
       tests the delivery; every other section gets the output it had before the report existed, which is what
       makes C7's byte comparison possible. */
    ReportLogFilter logFilter;
    juce::Logger::setCurrentLogger(&logFilter);

    const juce::File outputDirectory =
        juce::File::getCurrentWorkingDirectory().getChildFile("offline-render");
    outputDirectory.createDirectory();

    /* Render length is computed from the largest L in the configuration under test rather than left
       to whatever the block list happens to sum to: the delta anchor needs n0 + 2L - 1 samples of
       room after the impulse, and every other check needs more than L so that the wet stream has
       actually started inside the rendered window. */
    const int defaultTotal = 4 * kDefault.chosen + 2 * kDefault.hop;
    const int tightestTotal = 4 * kTightest.chosen + 2 * kTightest.hop;

    bool passed = true;
    passed = checkShellAddsNothing(defaultTotal) && passed;
    passed = checkBlockInvariance(defaultTotal) && passed;
    passed = checkDeltaAnchor(kDefault, outputDirectory, true) && passed;
    passed = checkDeltaAnchor(kTightest, outputDirectory, false) && passed;
    passed = checkDeltaAnchor(kLongest, outputDirectory, true) && passed;
    passed = checkLatencyAndPrepareSpace() && passed;
    passed = checkGrainLengthClamp() && passed;
    passed = checkDryPath(kTightest, tightestTotal) && passed;
    passed = checkDryPath(kDefault, defaultTotal) && passed;
    passed = checkNumericHygiene(kDefault, defaultTotal) && passed;
    passed = reportHeadroom(kDefault, defaultTotal) && passed;
    passed = checkLifecycle(defaultTotal) && passed;
    passed = checkMixLaw() && passed;
    passed = checkWetDcComponent() && passed;
    passed = checkParameterStateCompatibility() && passed;
    passed = checkLevelStageMovesNothing(kDefault) && passed;
    passed = checkLevelStageMovesNothing(kTightest) && passed;
    passed = checkLevelAnchoring() && passed;
    passed = checkLevelHeadroom() && passed;
    passed = checkLevelAcrossTravel() && passed;
    passed = checkLevelZipper() && passed;
    passed = checkConsolidatedSidechainLevel() && passed;
    passed = checkLayoutNegotiation() && passed;
    passed = checkConsolidatedLayoutFromTheHost() && passed;
    passed = checkReprepareFloorAndLatency() && passed;

    /* §5.20's own wall clock, measured here because the bound is on the section and not on the suite: §1.1 of
       the plan measures the same binary's full-suite time moving by 11 s on run order (26 s when it runs
       alone), so no suite-level figure can decide a 5 s budget. If this line goes over, something is rendering
       at kDefault that should be at kMatrix — the per-cell cost lines above say which — and the fix is the
       shape, not a dropped cell. */
    const auto matrixStart = juce::Time::getCurrentTime();
    const bool matrixPassed = checkChannelMatrix();
    const double matrixSeconds = (juce::Time::getCurrentTime() - matrixStart).inSeconds();
    std::cout << "       §5.20 wall clock " << juce::String::formatted("%.2f s", matrixSeconds)
              << " against the 5 s bound\n";
    if (matrixSeconds >= 5.0)
        std::cout << "  FAIL the channel matrix costs " << matrixSeconds
                  << " s — criterion 13's bound is on this figure, not on the suite's\n";
    passed = matrixPassed && matrixSeconds < 5.0 && passed;

    /* §5.21 runs last on purpose: it replaces the logger installed above with its own counting one, and it is
       the only section whose legs assume the message-thread premise that `gui` establishes. Nothing after it
       needs either, so the sections that predate the report keep the run they always had. */
    passed = checkLayoutReport(logFilter) && passed;

    std::cout << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}
