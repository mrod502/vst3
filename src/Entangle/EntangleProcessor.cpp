#include "EntangleProcessor.h"
#include "EntangleEditor.h"
#include "Entangle.h"
#include <iostream>
#include <Fft.hpp>
#include <Sin.hpp>
#include <juce_dsp/maths/juce_FastMathApproximations.h>

EntangleAudioProcessor::EntangleAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
    buf.resize(1 << 16);
}

EntangleAudioProcessor::~EntangleAudioProcessor()
{
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
void EntangleAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    std::cout << "Sample Rate: " << sampleRate << std::endl;
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void EntangleAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool EntangleAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
}

void EntangleAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                               juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    const auto sr = this->getSampleRate();
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        uint64_t ns = buffer.getNumSamples();
        auto sinWave = dsp::sin(ns, sr, 1000.0 ,0.0);

        for (int sample = 0; sample < buffer.getNumSamples(); sample++)
        {
            channelData[sample] = 0.1* juce::dsp::FastMathApproximations::sin((phase + sample)* 6*signalFade*10/sr);
        }
    }
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
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);
}

void EntangleAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);
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
