#include "EntangleProcessor.h"
#include "EntangleEditor.h"
#include "Entangle.h"
#include <algorithm>
#include <iostream>

juce::AudioProcessorValueTreeState::ParameterLayout EntangleAudioProcessor::makeLayout()
{
    juce::StringArray hopNames;
    for (const int hop : hopValues)
        hopNames.add(juce::String(hop));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{blendParameterId, 1}, "Blend",
            juce::NormalisableRange<float>{-1.0f, 1.0f, 0.01f}, 0.0f),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{hopParameterId, 1}, "Hop size", hopNames, defaultHopIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable(false)));
    return layout;
}

EntangleAudioProcessor::EntangleAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", makeLayout()),
      engine(std::make_unique<engine::PassthroughEngine>()),
      scheduler(*engine)
{
    apvts.addParameterListener(hopParameterId, this);
    buf.resize(1 << 16);
}

EntangleAudioProcessor::~EntangleAudioProcessor()
{
    apvts.removeParameterListener(hopParameterId, this);
}

const juce::String EntangleAudioProcessor::getName() const
{
    return entangle::APPLICATION_NAME;
}

bool EntangleAudioProcessor::acceptsMidi() const
{
    return false;
}

bool EntangleAudioProcessor::producesMidi() const
{
    return false;
}

bool EntangleAudioProcessor::isMidiEffect() const
{
    return false;
}

double EntangleAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EntangleAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int EntangleAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EntangleAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EntangleAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EntangleAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
bool EntangleAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    // The passthrough hands A back on the channel set it arrived on; M4 relaxes this once
    // the engine owns the channel map.
    if (mainIn != mainOut)
        return false;

    // Some hosts, such as certain GarageBand versions, will only load plugins that support
    // a stereo bus layout.
    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    // A host that offers no second bus, or leaves it switched off, is a supported state
    // rather than a rejected one: B is silence (D-001, D-002).
    const auto sidechain = layouts.getChannelSet(true, 1);
    if (sidechain.isDisabled())
        return true;

    return sidechain == juce::AudioChannelSet::mono() || sidechain == juce::AudioChannelSet::stereo();
}

int EntangleAudioProcessor::currentHopIndex() const
{
    const auto *raw = apvts.getRawParameterValue(hopParameterId);
    if (raw == nullptr)
        return defaultHopIndex;

    return juce::jlimit(0, static_cast<int>(hopValues.size()) - 1, static_cast<int>(raw->load()));
}

void EntangleAudioProcessor::configureRings(double sampleRateToUse, int samplesPerBlock)
{
    const int channels = std::min(getChannelCountOfBus(true, 0), getChannelCountOfBus(false, 0));
    auto *sidechainBus = getBus(true, 1);
    const bool sidechainIsLive = sidechainBus != nullptr && sidechainBus->isEnabled();

    activeChannels = channels;
    sidechainActive = sidechainIsLive;

    if (channels <= 0)
        return;

    engineConfig.sampleRate = sampleRateToUse;
    engineConfig.hop = hopValues[static_cast<std::size_t>(currentHopIndex())];
    engineConfig.maxChannels = channels;
    engineConfig.latencySamples = engineConfig.hop; ///< M1 placeholder policy (D-004)

    engine::SchedulerConfig schedulerConfig;
    schedulerConfig.engine = engineConfig;
    schedulerConfig.channels = channels;
    schedulerConfig.sidechainChannels = sidechainIsLive ? getChannelCountOfBus(true, 1) : 0;
    schedulerConfig.maxHostBlock = std::max(samplesPerBlock, maxHostBlockSamples);

    scheduler.prepare(schedulerConfig);
    setLatencySamples(engineConfig.latencySamples);
}

void EntangleAudioProcessor::prepareToPlay(double sampleRateToUse, int samplesPerBlock)
{
    configureRings(sampleRateToUse, samplesPerBlock);
}

void EntangleAudioProcessor::reconfigureSafely()
{
    suspendProcessing(true);
    configureRings(getSampleRate(), getBlockSize());
    suspendProcessing(false);
}

void EntangleAudioProcessor::processorLayoutsChanged()
{
    reconfigureSafely();
}

void EntangleAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    juce::ignoreUnused(newValue);

    // A hop change re-prepares every ring. AsyncUpdater moves that work to the message
    // thread even if a host poked the parameter from elsewhere, which is what makes the
    // suspend round-trip in reconfigureSafely safe to make.
    if (parameterID == hopParameterId)
        triggerAsyncUpdate();
}

void EntangleAudioProcessor::handleAsyncUpdate()
{
    reconfigureSafely();
}

void EntangleAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void EntangleAudioProcessor::reset()
{
    // Restores the output ring's latency pre-fill. Without it the tail of the previous take
    // plays back over the new one, and the latency reported to the host stops matching the
    // delay the host can actually measure.
    scheduler.reset();
}

void EntangleAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                          juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    if (const auto *blend = apvts.getRawParameterValue(blendParameterId))
        smoothedBlendValue += (blend->load() - smoothedBlendValue) * blendSmoothingPerCallback;

    if (activeChannels <= 0)
        return;

    const auto mainIn = getBusBuffer(buffer, true, 0);
    auto mainOut = getBusBuffer(buffer, false, 0);
    const int numSamples = buffer.getNumSamples();

    if (sidechainActive)
    {
        const auto sideIn = getBusBuffer(buffer, true, 1);
        scheduler.process(mainIn.getArrayOfReadPointers(), sideIn.getArrayOfReadPointers(),
                          mainOut.getArrayOfWritePointers(), activeChannels, numSamples);
        return;
    }

    scheduler.process(mainIn.getArrayOfReadPointers(), nullptr,
                      mainOut.getArrayOfWritePointers(), activeChannels, numSamples);
}

//==============================================================================
bool EntangleAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *EntangleAudioProcessor::createEditor()
{
    return new EntangleAudioProcessorEditor(*this);
}

//==============================================================================
void EntangleAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    const auto xml = apvts.copyState().toXmlString();
    destData.replaceWith(xml.toRawUTF8(), xml.getNumBytesAsUTF8());
}

void EntangleAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    auto state = juce::ValueTree::fromXml(
        juce::String::fromUTF8(static_cast<const char *>(data), sizeInBytes));
    if (state.isValid())
        apvts.replaceState(state);
}

void EntangleAudioProcessor::combineBlocks(juce::AudioBuffer<float> &a, juce::AudioBuffer<float> &b)
{
    if ((a.getNumChannels() != b.getNumChannels()) || a.getNumSamples() != b.getNumSamples())
    {
        std::cerr << "incompatible buffers: (" << a.getNumChannels() << "," << b.getNumChannels() << "),(" << a.getNumSamples() << "," << b.getNumSamples() << ")\n";
        return;
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new EntangleAudioProcessor();
}
