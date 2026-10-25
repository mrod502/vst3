#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <random>
#include <vector>

/** Defined by the plugin's shared code, which this harness links rather than recompiling. */
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kMaxBlock = 2048;
constexpr int kImpulseAt = 1000; ///< >= any hop in the allowed set, so the delay is visible

/** Integer-valued and distinct: exact float equality is the only comparison that can pass. */
float pattern(int channel, int index)
{
    return static_cast<float>(index * 4 + channel) - 1000.0f;
}

using Stream = std::vector<std::vector<float>>;

Stream makeStream(int channels, int samples, bool impulse)
{
    Stream data(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(samples), 0.0f));
    if (impulse)
    {
        data[0][static_cast<std::size_t>(kImpulseAt)] = 1.0f;
        return data;
    }
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] =
                pattern(channel, i);
    return data;
}

std::vector<int> blockSequence()
{
    std::vector<int> blocks;
    for (int size : {1, 7, 32, 64, 128, 512, 1000, 2048})
    {
        blocks.push_back(0);
        blocks.push_back(size);
    }

    std::mt19937 rng(20261022U);
    std::uniform_int_distribution<int> size(0, kMaxBlock);
    for (int i = 0; i < 300; ++i)
        blocks.push_back(size(rng));

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

    Channels are laid out the way AudioProcessor::getChannelIndexInProcessBlockBuffer wants
    them: main input at 0, sidechain after it, main output aliasing the main input. Because
    output aliases input, this also exercises the ingest-then-drain order the scheduler
    depends on.
*/
Stream render(juce::AudioProcessor &processor, const Stream &in, const Stream *sideIn,
              int mainChannels, int sideChannels, const std::vector<int> &blocks)
{
    const int bufferChannels =
        std::max(processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());
    const int totalSamples = totalOf(blocks);

    Stream out(static_cast<std::size_t>(mainChannels),
               std::vector<float>(static_cast<std::size_t>(totalSamples),
                                  std::numeric_limits<float>::quiet_NaN()));

    juce::MidiBuffer midi;
    int position = 0;
    processor.reset(); ///< each pass is a fresh transport, so the rings must start pre-filled

    for (int samples : blocks)
    {
        // Sized per callback like a real host: the processor takes its sample count from
        // buffer.getNumSamples(), so a fixed-capacity buffer would silently lie to it.
        juce::AudioBuffer<float> buffer(bufferChannels, samples);
        for (int channel = 0; channel < mainChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(channel, i,
                                 in[static_cast<std::size_t>(channel)][static_cast<std::size_t>(
                                     position + i)]);
        // Always written, so a connected-but-unfed sidechain reaches the plugin as silence
        // rather than as uninitialised memory.
        for (int channel = 0; channel < sideChannels; ++channel)
            for (int i = 0; i < samples; ++i)
                buffer.setSample(
                    mainChannels + channel, i,
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

int firstNonZeroIndex(const Stream &out)
{
    for (std::size_t i = 0; i < out[0].size(); ++i)
        for (const auto &channel : out)
            if (channel[i] != 0.0f)
                return static_cast<int>(i);
    return -1;
}

bool writeWav(const juce::File &file, const Stream &data, const juce::AudioChannelSet &layout)
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
                             .withChannelLayout(layout)
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
    else
        std::cout << "  ok   wrote " << file.getFullPathName().toRawUTF8() << "\n";
    return written;
}

struct BusConfig
{
    const char *name;
    juce::AudioChannelSet mainSet;
    bool withSidechain;
};

bool runConfiguration(const BusConfig &bus, const juce::File &outputDirectory,
                      const std::vector<int> &blocks)
{
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    if (processor == nullptr)
    {
        std::cout << "FAIL createPluginFilter returned null\n";
        return false;
    }

    auto layout = processor->getBusesLayout();
    layout.getChannelSet(true, 0) = bus.mainSet;
    layout.getChannelSet(false, 0) = bus.mainSet;
    layout.getChannelSet(true, 1) =
        bus.withSidechain ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();
    if (!processor->setBusesLayout(layout))
    {
        std::cout << "FAIL " << bus.name << ": layout rejected\n";
        return false;
    }

    processor->setRateAndBufferSizeDetails(kSampleRate, kMaxBlock);
    processor->prepareToPlay(kSampleRate, kMaxBlock);

    const int mainChannels = processor->getChannelCountOfBus(true, 0);
    const int sideChannels = processor->getChannelCountOfBus(true, 1);
    const int latency = processor->getLatencySamples();
    const int samples = totalOf(blocks);

    std::cout << bus.name << ": main=" << mainChannels << " side=" << sideChannels
              << " reportedLatency=" << latency << " callbacks=" << blocks.size() << "\n";

    const auto patternIn = makeStream(mainChannels, samples, false);
    const auto impulseIn = makeStream(mainChannels, samples, true);

    bool passed = true;
    passed = expectDelayedBitExact(render(*processor, patternIn, nullptr, mainChannels,
                                          sideChannels, blocks),
                                   patternIn, latency, "pattern in") &&
             passed;

    const auto renderedImpulse = render(*processor, impulseIn, nullptr, mainChannels, sideChannels,
                                        blocks);
    const int measuredDelay = firstNonZeroIndex(renderedImpulse) - kImpulseAt;
    if (measuredDelay != latency)
    {
        std::cout << "  FAIL impulse: measured " << measuredDelay << " != reported " << latency
                  << "\n";
        passed = false;
    }
    else
    {
        std::cout << "  ok   impulse: measured " << measuredDelay
                  << " == reported latency, and exactly input>>delay\n";
    }
    passed = expectDelayedBitExact(renderedImpulse, impulseIn, latency, "impulse in") && passed;

    if (bus.withSidechain)
    {
        // B must not touch A's passthrough, connected or not (D-002, tripwire T3).
        const auto side = makeStream(sideChannels, samples, false);
        const auto withSide =
            render(*processor, patternIn, &side, mainChannels, sideChannels, blocks);
        const auto withoutSide =
            render(*processor, patternIn, nullptr, mainChannels, sideChannels, blocks);
        if (withSide != withoutSide)
        {
            std::cout << "  FAIL sidechain content changed the A passthrough\n";
            passed = false;
        }
        else
        {
            std::cout << "  ok   sidechain present and absent render identically\n";
        }
    }

    if (!writeWav(outputDirectory.getChildFile(juce::String(bus.name) + "-pattern.wav"), patternIn,
                  bus.mainSet) ||
        !writeWav(outputDirectory.getChildFile(juce::String(bus.name) + "-out.wav"),
                  renderedImpulse, bus.mainSet))
        passed = false;

    processor->releaseResources();
    return passed;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui; ///< the APVTS flush timer needs a message manager

    const juce::File outputDirectory =
        juce::File::getCurrentWorkingDirectory().getChildFile("offline-render");
    outputDirectory.createDirectory();

    const auto blocks = blockSequence();

    bool passed = true;
    for (const BusConfig &bus : {BusConfig{"stereo-no-b", juce::AudioChannelSet::stereo(), false},
                                 BusConfig{"stereo-with-b", juce::AudioChannelSet::stereo(), true},
                                 BusConfig{"mono-no-b", juce::AudioChannelSet::mono(), false}})
        passed = runConfiguration(bus, outputDirectory, blocks) && passed;

    std::cout << (passed ? "PASS" : "FAIL") << "\n";
    return passed ? 0 : 1;
}
