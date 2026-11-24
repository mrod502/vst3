#include "EntangleProcessor.h"
#include "EntangleEditor.h"
#include "Entangle.h"
#include <LayoutGate.hpp>
#include <PanelGeometry.hpp>
#include <algorithm>
#include <iostream>

namespace
{
/** The report's two gate words. The harness keeps its own mapping and compares this one
    against it field by field, so a report that names a verdict the gate did not return
    reddens a leg rather than quietly describing a layout that is not running. */
const char* gateVerdictText(engine::LayoutVerdict verdict) noexcept
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
    return "unknown";
}

const char* gateRuleText(engine::ChannelMapStatus status) noexcept
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
    return "unknown";
}

/* The counters are formatted where the report is read, not where its body was built: a declined arrival
   never reaches refreshLayoutReport, so this is the only outside view of a skipped re-prepare. Callers
   hold layoutReportLock. */
juce::String layoutReportCounters(int prepares, int skipped)
{
    return "entangle layout prepares=" + juce::String(prepares)
           + " skipped=" + juce::String(skipped) + " ";
}
}

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
            juce::NormalisableRange<float>{0.0f, 1.0f, blendStep}, blendDefaultQ),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{hopParameterId, 1}, "Hop size", hopNames, defaultHopIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable(false)),
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{grainLengthParameterId, 1}, "Grain length", grainLengthNames,
            defaultGrainLengthIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable(false)),
        /* Appended, never inserted: the two choice parameters above are indexed by position in a saved
           preset, so their declaration order is part of the file format. The float and bool here are looked
           up by id, so they can arrive at the end without moving anything. */
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{outputGainParameterId, 1}, "Output gain",
            juce::NormalisableRange<float>{outputGainMinDb, outputGainMaxDb, outputGainStepDb},
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")),
        std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{bypassParameterId, 1}, "Bypass", false),
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{normalisationAmountParameterId, 1}, "Normalisation amount",
            juce::NormalisableRange<float>{normalisationAmountMin, normalisationAmountMax,
                                           normalisationAmountStep},
            1.0f),
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{silenceFloorParameterId, 1}, "Silence floor",
            juce::NormalisableRange<float>{silenceFloorMinDb, silenceFloorMaxDb,
                                           silenceFloorStepDb},
            silenceFloorDefaultDb,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
    return layout;
}

/* With no sidechain bus B is silence (D-002), so the law pins the effective dial to pure A (D-011) rather
   than handing back the silence a pure-wet shell would. The chain is built by the same shared builder the
   harness uses for its reference render, so a harness can never drift from what ships. */
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
    // 65536 cap. Item 6's plan keeps the declaration at zero on that number.
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
    /* The rule is engine::gateLayout's (D-021): this only translates the host's bus shapes into the three
       counts it takes, from the layout it is handed and never from a cached member, because a host asks about
       layouts it has not applied. The main bus stays mono/stereo — some hosts only load stereo-capable plugins. */
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto sidechain = layouts.getChannelSet(true, 1);
    const int sidechainChannels = sidechain.isDisabled() ? 0 : sidechain.size();

    return engine::gateLayout(mainIn.size(), sidechainChannels, mainOut.size()).verdict
           == engine::LayoutVerdict::accepted;
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
    auto *sidechainBus = getBus(true, 1);
    const bool sidechainIsLive = sidechainBus != nullptr && sidechainBus->isEnabled();

    entangle::ChainConfig chain;
    chain.sampleRate = sampleRateToUse;
    chain.hop = hopValues[static_cast<std::size_t>(currentHopIndex())];
    chain.grainLength = clampGrainLength(requestedGrainLengthSamples(), chain.hop);
    chain.channels = getChannelCountOfBus(true, 0);
    chain.sidechainChannels = sidechainIsLive ? getChannelCountOfBus(true, 1) : 0;
    chain.outputChannels = getChannelCountOfBus(false, 0);
    chain.maxHostBlock = maxHostBlockFor(samplesPerBlock);

    /* One triple, two readers: the gate decides whether it may run at all and the scheduler config turns the
       map it resolved into ring sizes, and `processed == map.channels` on every triple the gate accepts. The
       early return therefore means "the gate said no"; it stays because processBlock has to be total. */
    const engine::LayoutDecision layout =
        engine::gateLayout(chain.channels, chain.sidechainChannels, chain.outputChannels);
    const engine::SchedulerConfig schedulerConfig = entangle::makeSchedulerConfig(chain);

    activeChannels = layout.processed;
    sidechainActive = sidechainIsLive;

    if (activeChannels <= 0)
    {
        /* A refusal is never remembered. Storing the key of a triple that was not prepared would make the
           next one skippable, and a skipped prepare on a configuration that was never built is silence with
           no reason attached. */
        preparedKey.reset();
        return;
    }

    /* Conditional re-prepare (D-022 B): the seven-field key is the whole configuration the rings, the engine
       and the pre-fill were built for, so an arrival that changes none of them rebuilds nothing and drops no
       tail. `grainLength` is the post-clamp value, so a dial move the clamp swallows reads as no change. */
    const engine::ReconfigureKey key{sampleRateToUse,
                                     samplesPerBlock,
                                     chain.channels,
                                     chain.sidechainChannels,
                                     chain.outputChannels,
                                     chain.hop,
                                     chain.grainLength};
    if (preparedKey.has_value() && *preparedKey == key)
    {
        {
            const juce::ScopedLock scoped(layoutReportLock);
            ++skippedArrivalCount;
        }
        return;
    }

    preparedKey = key;
    engineConfig = schedulerConfig.engine;

    pushParameterTargets();

    /* The display's window is rebuilt with every other ring here, and before the engine is prepared: the
       sink holds a bare pointer to it, so the ring has to be the thing that exists first (D-027). */
    installTracePublication (sampleRateToUse, chain.hop);

    scheduler.prepare(schedulerConfig);
    setLatencySamples(schedulerConfig.engine.latencySamples);
    refreshLayoutReport(layout, chain, sampleRateToUse, samplesPerBlock);
}

void EntangleAudioProcessor::installTracePublication (double sampleRateToUse, int hop)
{
    /* The window is the panels', read here because the shell is the one place both halves are visible
       (AGENTS §8). +1 so a full window always fits rather than lapping itself by one hop at the exact
       boundary; the ring rounds up to a power of two, and its capacity is the figure `traceDepth` reports. */
    const auto hops = sampleRateToUse > 0.0 && hop > 0
                          ? static_cast<std::size_t> (ui::kTraceWindowSeconds * sampleRateToUse
                                                      / static_cast<double> (hop) + 1.0)
                          : 1;

    const juce::ScopedLock scoped (traceLock);

    /* Park, rebuild, re-borrow: the sink is read on the hop path and is not atomic, so the swap is staged
       through the null state rather than pointing at a ring that is half-replaced. */
    engine->installTraceSink ({});
    traceRing = std::make_unique<container::TraceRing<engine::TraceSnapshot>> (hops);
    engine->installTraceSink (engine::TraceSink {traceRing.get()});
}

std::size_t EntangleAudioProcessor::traceWindow (std::span<engine::TraceSnapshot> destination) const
{
    const juce::ScopedLock scoped (traceLock);
    return traceRing != nullptr ? traceRing->peekNewestInto (destination) : 0;
}

bool EntangleAudioProcessor::traceLatest (engine::TraceSnapshot& destination) const
{
    const juce::ScopedLock scoped (traceLock);
    return traceRing != nullptr && traceRing->newest (destination);
}

double EntangleAudioProcessor::traceSecondsPerHop() const noexcept
{
    return engineConfig.sampleRate > 0.0 && engineConfig.hop > 0
               ? static_cast<double> (engineConfig.hop) / engineConfig.sampleRate
               : 0.0;
}

std::size_t EntangleAudioProcessor::traceDepth() const noexcept
{
    const juce::ScopedLock scoped (traceLock);
    return traceRing != nullptr ? traceRing->capacity() : 0;
}

juce::String EntangleAudioProcessor::layoutReport() const
{
    const juce::ScopedLock scoped(layoutReportLock);
    return layoutReportCounters(prepareCount, skippedArrivalCount) + layoutReportBody;
}

/* The one place the report is written (D-023 B): the configuration just built, read back off the live buses
   rather than from `chain`, so a field that disagrees with an accessor is a bug in one of the two. The log
   line goes out only when a message manager exists and this thread is it — `writeToLog` can block. */
void EntangleAudioProcessor::refreshLayoutReport(const engine::LayoutDecision& decision,
                                                 const entangle::ChainConfig& chain,
                                                 double sampleRateToUse, int samplesPerBlock)
{
    auto describeBus = [](const char* name, const juce::AudioProcessor::Bus* bus)
    {
        if (bus == nullptr)
            return juce::String(name) + "=0 " + juce::String(name) + "Name=[none] " + name + "En=0";

        const auto& set = bus->getCurrentLayout();
        return juce::String(name) + "=" + juce::String(set.size())
               + " " + name + "Name=[" + set.getDescription() + "]"
               + " " + name + "En=" + juce::String(bus->isEnabled() ? 1 : 0);
    };

    juce::String body;
    juce::String report;
    {
        const juce::ScopedLock scoped(layoutReportLock);
        ++prepareCount;

        body = "inBuses=" + juce::String(getBusCount(true))
               + " outBuses=" + juce::String(getBusCount(false))
               + " " + describeBus("mainIn", getBus(true, 0))
               + " " + describeBus("side", getBus(true, 1))
               + " " + describeBus("mainOut", getBus(false, 0))
               + " gate=" + gateVerdictText(decision.verdict)
               + "/" + gateRuleText(decision.rule)
               + " processed=" + juce::String(decision.processed)
               + " outCh=" + juce::String(getTotalNumOutputChannels())
               + " rate=" + juce::String(sampleRateToUse)
               + " hop=" + juce::String(chain.hop)
               + " Lreq=" + juce::String(requestedGrainLengthSamples())
               + " Lrun=" + juce::String(chain.grainLength)
               + " R=" + juce::String(getLatencySamples())
               + " block=" + juce::String(samplesPerBlock) + "/"
               + juce::String(chain.maxHostBlock)
               + " sideActive=" + juce::String(sidechainActive ? 1 : 0)
               + " blendEff=" + juce::String(effectiveBlendPosition(), 3);

        layoutReportBody = body;
        report = layoutReportCounters(prepareCount, skippedArrivalCount) + body;
    }

    if (juce::MessageManager::existsAndIsCurrentThread())
        juce::Logger::writeToLog(report);
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
    engine->setNormalisationAmountTarget(
        rawParameter(normalisationAmountParameterId, dsp::LevelNormalizer::defaultAmount));
    engine->setSilenceFloorTarget(
        rawParameter(silenceFloorParameterId, dsp::LevelNormalizer::defaultFloorDb));
}

void EntangleAudioProcessor::prepareToPlay(double sampleRateToUse, int samplesPerBlock)
{
    configureRings(sampleRateToUse, samplesPerBlock);
}

void EntangleAudioProcessor::reconfigureSafely()
{
    /* A layout change can arrive before the host hands out a rate: getSampleRate() is 0 until prepareToPlay,
       and a setBusesLayout during construction lands in that window. Nothing can be sized against a rate of
       zero — the DC blocker asserts rather than derive a filter for it — so this declines and leaves the
       prepareToPlay that follows a layout change to do the work. */
    if (getSampleRate() <= 0.0)
        return;

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
    preparedKey.reset(); /* and forget what was prepared: the host must call prepareToPlay again, and the
                            guard of D-022 B must not talk it out of doing the work. Freeing something here
                            behind a remembered key would be the one way to run a transport on state that is
                            gone — §5 arm 11, and the leg in OfflineRender §5.19 that reddens it. */

    /* The same rule reaches the display's window: the mix stage borrows the ring, so it is parked before the
       ring is freed. A hop that arrived after this point would find a null sink and publish nothing rather
       than write into freed memory — the leg §5.23 runs under the sanitizer tree for arm 11. */
    {
        const juce::ScopedLock scoped (traceLock);
        engine->installTraceSink ({});
        traceRing.reset();
    }
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
