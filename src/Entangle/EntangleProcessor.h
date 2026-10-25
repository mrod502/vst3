#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <map>
#include <complex>
#include <memory>
#include <vector>

#include <Engine.hpp>
#include <HopScheduler.hpp>
#include <PassthroughEngine.hpp>

using namespace std;
//==============================================================================
class EntangleAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener,
                                     private juce::AsyncUpdater
{
public:
    //==============================================================================
    static const inline juce::String blendParameterId { "blend" };
    static const inline juce::String hopParameterId { "hop" };

    /** The allowed hop set (D-003); the UI names are derived from these, so there is one
        source of truth for what a hop may be. */
    static constexpr std::array<int, 4> hopValues { 64, 128, 256, 512 };
    static constexpr int defaultHopIndex = 1;

    /** Every ring is sized once per prepare for the largest configuration it may ever see,
        so processBlock never allocates. */
    static constexpr int maxHostBlockSamples = 4096;

    //==============================================================================
    EntangleAudioProcessor();
    ~EntangleAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processorLayoutsChanged() override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Cached sidechain state, refreshed off the audio thread in prepareToPlay and
        layoutChanged: processBlock must never query a bus it is not allowed to read. */
    bool isSidechainActive() const noexcept            { return sidechainActive; }

    /** Blend, ramped on the audio thread. The M1 engine does not consume it yet — M3's mix
        does, which is why the ramp already lives in the callback. */
    float smoothedBlend() const noexcept               { return smoothedBlendValue; }

    juce::AudioProcessorValueTreeState apvts;

    vector<complex<float>> fftBuf;
    vector<float> buf;
    double sampleRate;
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntangleAudioProcessor)
    int phase;

    void combineBlocks(juce::AudioBuffer<float>&, juce::AudioBuffer<float>&);

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

    int currentHopIndex() const;
    void configureRings (double sampleRateToUse, int samplesPerBlock);
    void reconfigureSafely();
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    std::unique_ptr<engine::Engine> engine;
    engine::HopScheduler scheduler;
    engine::EngineConfig engineConfig;

    int activeChannels = 0;
    bool sidechainActive = false;
    float smoothedBlendValue = 0.0f;

    static constexpr float blendSmoothingPerCallback = 0.02f;
};
