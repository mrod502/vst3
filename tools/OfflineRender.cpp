/** Plugin-seam harness: the Mode 1 engine inside EntangleAudioProcessor.

    The only place shell, scheduler and engine meet, so it is where the item-6 acceptance criteria
    live. Every check is a bit-exact or tolerance-bound measurement of the shell rather than an
    assumption about it, and the program is plain console PASS/FAIL with a nonzero exit.

    The checks map to plan/M2-item-6-impl-plan.md §5, with M3 item 1 re-pointing 3 and 5 at the mix
    law and adding 10, and M3 item 2 splitting 3 by chain variant and adding 11:
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
                                        the wet stream through the two safety stages
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

    Inputs are seeded noise in a playing range, not the raw integer ramp this harness used to feed:
    §5.7 asks for a peak-level number, which is meaningless on a stream that reaches 1e6. */

#include <juce_audio_utils/juce_audio_utils.h>

#include <MixEngine.hpp>
#include <GrainEngine.hpp>
#include <HopScheduler.hpp>
#include <DcBlocker.hpp>
#include <SoftClipper.hpp>

#include "EngineChain.h"
#include "EntangleProcessor.h"
#include "JuceTransformFactory.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <random>
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
};

/* juce::AudioChannelSet is not a literal type, so these are const rather than constexpr. */
const BusSetup kStereoWithB{"stereo-with-b", juce::AudioChannelSet::stereo(), true};
const BusSetup kStereoNoB{"stereo-no-b", juce::AudioChannelSet::stereo(), false};
const BusSetup kMonoNoB{"mono-no-b", juce::AudioChannelSet::mono(), false};

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
        bus.withSidechain ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();
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

int totalOf(const std::vector<int> &blocks)
{
    int total = 0;
    for (int block : blocks)
        total += block;
    return total;
}

/** Feeds a whole stream through the processor as host callbacks.

    Channels are laid out the way AudioProcessor::getChannelIndexInProcessBlockBuffer wants them:
    main input at 0, sidechain after it, main output aliasing the main input — so this also
    exercises the ingest-then-drain order the scheduler depends on. */
Stream render(juce::AudioProcessor &processor, const Stream &in, const Stream *sideIn,
              const std::vector<int> &blocks)
{
    const int mainChannels = processor.getChannelCountOfBus(true, 0);
    const int sideChannels = processor.getChannelCountOfBus(true, 1);
    const int bufferChannels =
        std::max(processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());

    Stream out(static_cast<std::size_t>(mainChannels),
               std::vector<float>(static_cast<std::size_t>(totalOf(blocks)),
                                  std::numeric_limits<float>::quiet_NaN()));

    juce::MidiBuffer midi;
    int position = 0;
    processor.reset(); ///< each pass is a fresh transport, so the rings must start pre-filled

    for (const int samples : blocks)
    {
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

        for (int channel = 0; channel < mainChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                out[static_cast<std::size_t>(channel)][static_cast<std::size_t>(position + i)] =
                    buffer.getSample(channel, i);
        position += samples;
    }

    return out;
}

/** The three parameters the shell reads on every callback, as a reference render should run them. */
struct MixSetting
{
    float dial = 0.0f;
    float gainDb = 0.0f;
    bool bypass = false;
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
    chain.maxHostBlock =
        std::max(kMaxBlock, EntangleAudioProcessor::maxHostBlockSamples);

    const engine::SchedulerConfig config = entangle::makeSchedulerConfig(chain);

    /* Targets before prepare so the coefficients snap onto them, then again on every callback — the
        order EntangleAudioProcessor uses in configureRings and processBlock. A static setting makes
        the two identical; the point of mirroring it is that a moving one stays comparable. */
    const float effectiveDial = entangle::effectiveBlend(mix.dial, sidechainChannels > 0);
    wet->setBlendTarget(effectiveDial);
    wet->setOutputGainTarget(mix.gainDb);
    wet->setBypassTarget(mix.bypass);

    engine::HopScheduler scheduler(*wet);
    scheduler.prepare(config);
    scheduler.reset();

    const int total = totalOf(blocks);
    Stream out(static_cast<std::size_t>(channels),
               std::vector<float>(static_cast<std::size_t>(total),
                                  std::numeric_limits<float>::quiet_NaN()));

    int position = 0;
    for (const int samples : blocks)
    {
        wet->setBlendTarget(effectiveDial);
        wet->setOutputGainTarget(mix.gainDb);
        wet->setBypassTarget(mix.bypass);

        std::vector<const float *> aPtrs(static_cast<std::size_t>(channels));
        std::vector<const float *> bPtrs(static_cast<std::size_t>(channels));
        std::vector<float *> outPtrs(static_cast<std::size_t>(channels));
        for (int channel = 0; channel < channels; ++channel)
        {
            aPtrs[static_cast<std::size_t>(channel)] =
                a[static_cast<std::size_t>(channel)].data() + position;
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data() + position;
            outPtrs[static_cast<std::size_t>(channel)] =
                out[static_cast<std::size_t>(channel)].data() + position;
        }
        scheduler.process(aPtrs.data(), bPtrs.data(), outPtrs.data(), channels, samples);
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

    engine::SchedulerConfig config;
    config.engine.sampleRate = kSampleRate;
    config.engine.hop = shape.hop;
    config.engine.maxChannels = channels;
    config.engine.latencySamples = shape.hop;
    config.engine.grainLength = EntangleAudioProcessor::clampGrainLength(shape.chosen, shape.hop);
    config.channels = channels;
    config.sidechainChannels = sidechainChannels;
    config.maxHostBlock =
        std::max(kMaxBlock, EntangleAudioProcessor::maxHostBlockSamples);

    engine::HopScheduler scheduler(wet);
    scheduler.prepare(config);
    scheduler.reset();

    const int total = totalOf(blocks);
    Stream out(static_cast<std::size_t>(channels),
               std::vector<float>(static_cast<std::size_t>(total), 0.0f));

    int position = 0;
    for (const int samples : blocks)
    {
        std::vector<const float *> aPtrs(static_cast<std::size_t>(channels));
        std::vector<const float *> bPtrs(static_cast<std::size_t>(channels));
        std::vector<float *> outPtrs(static_cast<std::size_t>(channels));
        for (int channel = 0; channel < channels; ++channel)
        {
            aPtrs[static_cast<std::size_t>(channel)] =
                a[static_cast<std::size_t>(channel)].data() + position;
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data() + position;
            outPtrs[static_cast<std::size_t>(channel)] =
                out[static_cast<std::size_t>(channel)].data() + position;
        }
        scheduler.process(aPtrs.data(), bPtrs.data(), outPtrs.data(), channels, samples);
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
    if (gainIndex < 0 || bypassIndex < 0)
    {
        std::cout << "  FAIL the state has no outputGain/bypass child to strip (" << gainIndex
                  << ", " << bypassIndex << ")\n";
        return false;
    }
    /* Highest index first, or the first removal invalidates the index of the second. */
    legacyState.removeChild(std::max(gainIndex, bypassIndex), nullptr);
    legacyState.removeChild(std::min(gainIndex, bypassIndex), nullptr);
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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui; ///< the APVTS flush timer needs a message manager

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

    std::cout << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}
