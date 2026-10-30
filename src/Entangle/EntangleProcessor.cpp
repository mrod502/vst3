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

    juce::StringArray grainLengthNames;
    for (const int length : grainLengthValues)
        grainLengthNames.add(juce::String(length));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{blendParameterId, 1}, "Blend",
            juce::NormalisableRange<float>{-1.0f, 1.0f, blendStep}, 0.0f),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{hopParameterId, 1}, "Hop size", hopNames, defaultHopIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable(false)),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{grainLengthParameterId, 1}, "Grain length", grainLengthNames,
            defaultGrainLengthIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable(false)),
        /* Appended, never inserted. The two choice parameters above are indexed by position in a
           saved preset, so the order they are declared in is part of the file format; the float and
           bool parameters added here are looked up by id, so they can arrive at the end without
           moving anything. */
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{outputGainParameterId, 1}, "Output gain",
            juce::NormalisableRange<float>{outputGainMinDb, outputGainMaxDb, outputGainStepDb},
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")),
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{bypassParameterId, 1}, "Bypass", false));
    return layout;
}

/* The mode 1 engine under the M3 mix law: with no sidechain bus B is silence (D-002), so the law
   pins the effective dial to pure A there (D-011) rather than handing back the silence a pure-wet
   shell would. The chain itself is built by the shared builder OfflineRender's §5.1 comparator
   uses too, so the harness can never drift from what ships. */
EntangleAudioProcessor::EntangleAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", makeLayout()),
      engine(entangle::makeShippedChain(transformFactory)),
      scheduler(*engine)
{
    apvts.addParameterListener(hopParameterId, this);
    apvts.addParameterListener(grainLengthParameterId, this);
    buf.resize(1 << 16);
}

EntangleAudioProcessor::~EntangleAudioProcessor()
{
    apvts.removeParameterListener(hopParameterId, this);
    apvts.removeParameterListener(grainLengthParameterId, this);
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
    // The wet response to an impulse occupies [n0+L, n0+2L-1], so the tail past the declared R is
    // (2L-1)-R: measured through processBlock at 0.34 s for the 8192 default and 2.72 s at the
    // 65536 cap (OfflineRender §5.3). Item 6's plan keeps the declaration at zero on that number.
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

    // The engine emits one output channel per input channel and the dry add reads the same index
    // on both buses; M4 relaxes this once the engine owns the channel map.
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

int EntangleAudioProcessor::currentGrainLengthIndex() const
{
    const auto *raw = apvts.getRawParameterValue(grainLengthParameterId);
    if (raw == nullptr)
        return defaultGrainLengthIndex;

    return juce::jlimit(0, static_cast<int>(grainLengthValues.size()) - 1,
                        static_cast<int>(raw->load()));
}

int EntangleAudioProcessor::requestedGrainLengthSamples() const
{
    return grainLengthValues[static_cast<std::size_t>(currentGrainLengthIndex())];
}

int EntangleAudioProcessor::grainLengthSamples() const
{
    return clampGrainLength(requestedGrainLengthSamples(),
                            hopValues[static_cast<std::size_t>(currentHopIndex())]);
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

    entangle::ChainConfig chain;
    chain.sampleRate = sampleRateToUse;
    chain.hop = hopValues[static_cast<std::size_t>(currentHopIndex())];
    chain.grainLength = clampGrainLength(requestedGrainLengthSamples(), chain.hop);
    chain.channels = channels;
    chain.sidechainChannels = sidechainIsLive ? getChannelCountOfBus(true, 1) : 0;
    chain.maxHostBlock = std::max(samplesPerBlock, maxHostBlockSamples);

    const engine::SchedulerConfig schedulerConfig = entangle::makeSchedulerConfig(chain);
    engineConfig = schedulerConfig.engine;

    pushParameterTargets();

    scheduler.prepare(schedulerConfig);
    setLatencySamples(schedulerConfig.engine.latencySamples);
}

float EntangleAudioProcessor::rawParameter(const juce::String &parameterId, float fallback) const noexcept
{
    const auto *raw = apvts.getRawParameterValue(parameterId);
    return raw != nullptr ? raw->load() : fallback;
}

void EntangleAudioProcessor::pushParameterTargets()
{
    engine->setBlendTarget(effectiveBlendPosition());
    engine->setOutputGainTarget(rawParameter(outputGainParameterId, 0.0f));
    engine->setBypassTarget(rawParameter(bypassParameterId, 0.0f) >= 0.5f);
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

    // A hop or grain-length change re-prepares every ring. AsyncUpdater moves that work to the
    // message thread even if a host poked the parameter from elsewhere, which is what makes the
    // suspend round-trip in reconfigureSafely safe to make.
    if (parameterID == hopParameterId || parameterID == grainLengthParameterId)
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
    /* scheduler.reset() restores the output ring's latency pre-fill, without which the previous
       take's tail plays over the new one. Targets go in first because that call also snaps the mix
       ramps onto the targets they hold — after it, the new transport opens on the last setting. */
    pushParameterTargets();
    scheduler.reset();
}

void EntangleAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                          juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    /* Targets only: the ramp is the mix stage's and lives in the sample domain, so no time
       constant here can depend on the host's callback size (D-011). */
    pushParameterTargets();

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
