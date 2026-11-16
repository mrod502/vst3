#include "HarnessShell.hpp"

#include <DcBlocker.hpp>
#include <GrainEngine.hpp>
#include <HopScheduler.hpp>
#include <MixEngine.hpp>
#include <SoftClipper.hpp>

#include "JuceTransformFactory.h"

#include <cmath>

namespace harness
{

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
/* The four layouts no leg above used: the whole broadcast family — the mono main with one sidechain
   channel and the stereo main with one — and two of the consolidated widths the matrix samples (odd
   non-dyadic 1/3 and 1/5; 2 and 8 are sampled above). Callers read them through their own cell table,
   which carries the expected mapping next to each. */
const BusSetup kMonoWithMonoB{"mono-with-1-b", juce::AudioChannelSet::mono(), true,
                              juce::AudioChannelSet::mono()};
const BusSetup kStereoWithMonoB{"stereo-with-1-b", juce::AudioChannelSet::stereo(), true,
                                juce::AudioChannelSet::mono()};
const BusSetup kMonoWithThreeB{"mono-with-3-b", juce::AudioChannelSet::mono(), true,
                               juce::AudioChannelSet::discreteChannels(3)};
const BusSetup kMonoWithFiveB{"mono-with-5-b", juce::AudioChannelSet::mono(), true,
                              juce::AudioChannelSet::discreteChannels(5)};
/* The three widths that take the accepted set to twelve triples: 4 is the second dyadic reciprocal, 6
   and 7 the second pair of non-dyadic ones, and 7 is the widest sidechain that is neither the ceiling
   nor sampled above. */
const BusSetup kMonoWithFourB{"mono-with-4-b", juce::AudioChannelSet::mono(), true,
                              juce::AudioChannelSet::discreteChannels(4)};
const BusSetup kMonoWithSixB{"mono-with-6-b", juce::AudioChannelSet::mono(), true,
                             juce::AudioChannelSet::discreteChannels(6)};
const BusSetup kMonoWithSevenB{"mono-with-7-b", juce::AudioChannelSet::mono(), true,
                               juce::AudioChannelSet::discreteChannels(7)};

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

Stream render(juce::AudioProcessor &processor, const Stream &in, const Stream *sideIn,
              const std::vector<int> &blocks)
{
    return renderSwept(processor, in, sideIn, blocks, [](int) {});
}

/** The shipped chain with no shell in it, built by the same builder the shell calls
    (entangle::makeChain) at the configuration the shell is configured to. §5.1 — anything the
    shell converts, delays or gains shows up here and nowhere else. Composing the chain a second way
    would stop this comparing against what ships, which is why the builder is shared.

    The variant is part of the leg's identity, not a switch on convenience: M3 item 2 criterion C1
    requires every leg to say which chain it runs on, because an anchor measured through a chain that
    has quietly gained a high-pass no longer pins the engine's gain. */
Stream renderReference(const Shape &shape, const Stream &a, const Stream &b, int channels,
                       int sidechainChannels, const std::vector<int> &blocks,
                       const MixSetting &mix,
                       entangle::ChainVariant variant)
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

/* The two safety stages re-applied by hand to a raw wet stream, in the chain's order: block the convolved
   samples, fold the law's centre coefficient in, clip last. Duplicated from MixEngine::processHop on purpose,
   so the shipped chain is asserted against something independent of itself. */
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

namespace
{
/** A leg label is not a file name: labels carry parentheses, commas and spaces. */
juce::String dumpFileName(const char *label)
{
    juce::String name;
    for (const char *c = label; *c != '\0'; ++c)
    {
        const bool word = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z')
                          || (*c >= '0' && *c <= '9');
        name += word ? *c : '-';
    }
    return name;
}

/** The one place a failing expectation writes its evidence, so every hook behaves alike: one file per red leg
    named after the leg, never one per differing sample, and nothing at all when the run did not ask for audio. */
bool dumped(const char *what, FailDump dump, const Stream &offending)
{
    if (dump.directory != nullptr)
    {
        const juce::File target = dump.directory->getChildFile(dumpFileName(what) + ".wav");
        if (writeWav(target, offending))
            std::cout << "       wrote " << target.getFullPathName().toRawUTF8() << "\n";
    }
    return false;
}
}

bool expectIdentical(const Stream &got, const Stream &want, const char *what, FailDump dump)
{
    if (got.size() != want.size())
    {
        std::cout << "  FAIL " << what << ": " << got.size() << " channels vs " << want.size()
                  << "\n";
        return dumped(what, dump, got);
    }
    for (std::size_t channel = 0; channel < got.size(); ++channel)
    {
        if (got[channel].size() != want[channel].size())
        {
            std::cout << "  FAIL " << what << ": ch" << channel << " length "
                      << got[channel].size() << " vs " << want[channel].size() << "\n";
            return dumped(what, dump, got);
        }
        for (std::size_t i = 0; i < got[channel].size(); ++i)
        {
            if (got[channel][i] != want[channel][i])
            {
                std::cout << "  FAIL " << what << ": ch" << channel << " n" << i << " got "
                          << got[channel][i] << " want " << want[channel][i] << "\n";
                return dumped(what, dump, got);
            }
        }
    }
    std::cout << "  ok   " << what << "\n";
    return true;
}

bool expectDelayedBitExact(const Stream &out, const Stream &in, int latency, const char *what, FailDump dump)
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
                return dumped(what, dump, out);
            }
        }
    }
    std::cout << "  ok   " << what << ": bit-exact, delayed by " << latency << "\n";
    return true;
}

bool expectFinite(const Stream &out, const char *what, FailDump dump)
{
    for (std::size_t channel = 0; channel < out.size(); ++channel)
        for (std::size_t i = 0; i < out[channel].size(); ++i)
            if (!std::isfinite(out[channel][i]))
            {
                std::cout << "  FAIL " << what << ": non-finite at ch" << channel << " n" << i
                          << " = " << out[channel][i] << "\n";
                return dumped(what, dump, out);
            }
    std::cout << "  ok   " << what << ": no NaN, no inf\n";
    return true;
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

const MixSetting kMatrixMix{0.0f, 0.0f, false, 0.0f, dsp::LevelNormalizer::defaultFloorDb};

bool parkLevelStage(juce::AudioProcessor &processor)
{
    return setFloat(processor, EntangleAudioProcessor::normalisationAmountParameterId, 0.0f);
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

/* The twelve triples engine::gateLayout accepts, in one table shared by every consumer: a section may
   select a subset at its own shape and a matrix runs all twelve. A function-local static because
   juce::AudioChannelSet is not a literal type, so the BusSetups these point at are dynamically initialised
   and a namespace-scope array here would be a static-initialisation-order dependency across two binaries. */
const std::array<AcceptedLayout, 12> &acceptedLayouts()
{
    static const std::array<AcceptedLayout, 12> layouts{{
        AcceptedLayout{ "(1,0,1)", &kMonoNoB, 1, 0, 1, 1, "no sidechain",
                       { 0 }, { engine::kSilentSource } },
        AcceptedLayout{ "(1,1,1)", &kMonoWithMonoB, 1, 1, 1, 1, "broadcast",
                       { 0 }, { 0 } },
        AcceptedLayout{ "(1,2,1)", &kMonoWithB, 1, 2, 1, 1, "consolidated",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(1,3,1)", &kMonoWithThreeB, 1, 3, 1, 1, "consolidated, non-dyadic 1/3",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(1,4,1)", &kMonoWithFourB, 1, 4, 1, 1, "consolidated, 1/4",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(1,5,1)", &kMonoWithFiveB, 1, 5, 1, 1, "consolidated, non-dyadic 1/5",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(1,6,1)", &kMonoWithSixB, 1, 6, 1, 1, "consolidated, non-dyadic 1/6",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(1,7,1)", &kMonoWithSevenB, 1, 7, 1, 1, "consolidated, non-dyadic 1/7",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(1,8,1)", &kMonoWithEightB, 1, 8, 1, 1, "consolidated, the ceiling",
                       { 0 }, { engine::kConsolidatedSource } },
        AcceptedLayout{ "(2,0,2)", &kStereoNoB, 2, 0, 2, 2, "no sidechain",
                       { 0, 1 }, { engine::kSilentSource, engine::kSilentSource } },
        AcceptedLayout{ "(2,1,2)", &kStereoWithMonoB, 2, 1, 2, 2, "broadcast to both slots",
                       { 0, 1 }, { 0, 0 } },
        AcceptedLayout{ "(2,2,2)", &kStereoWithB, 2, 2, 2, 2, "diagonal",
                       { 0, 1 }, { 0, 1 } },
    }};
    return layouts;
}

const AcceptedLayout *acceptedLayout(int chA, int chB) noexcept
{
    for (const AcceptedLayout &layout : acceptedLayouts())
        if (layout.chA == chA && layout.chB == chB)
            return &layout;
    return nullptr;
}

MatrixCell matrixCellOf(const AcceptedLayout &layout, const Shape &shape,
                        const char *displayName, const char *family) noexcept
{
    return MatrixCell{ displayName != nullptr ? displayName : layout.name,
                       layout.bus,
                       shape,
                       layout.chA,
                       layout.chB,
                       layout.chOut,
                       layout.processed,
                       family != nullptr ? family : layout.family,
                       layout.a,
                       layout.b };
}

Stream renderOf(const MatrixCell &cell, const Stream &a, const Stream *side,
                const std::vector<int> &blocks)
{
    auto processor = makeProcessor();
    if (!configure(*processor, *cell.bus, cell.shape) || !parkLevelStage(*processor))
        return {};
    return render(*processor, a, side, blocks);
}

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

Stream monoPairModel(const MatrixCell &cell, const Stream &a, std::size_t slot, const Stream &side,
                     std::size_t sideChannel, float scale, const std::vector<int> &blocks)
{
    const Stream oneA{ a[cell.a[slot]] };
    const Stream oneB{ side[sideChannel] };
    return scaledCopy(renderReference(cell.shape, oneA, oneB, 1, 1, blocks, kMatrixMix), scale);
}

}
