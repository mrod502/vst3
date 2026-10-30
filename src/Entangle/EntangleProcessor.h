#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <map>
#include <complex>
#include <memory>
#include <vector>

#include <Engine.hpp>
#include <HopScheduler.hpp>
#include <MixEngine.hpp>

#include "EngineChain.h"
#include "JuceTransformFactory.h"

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
    static const inline juce::String grainLengthParameterId { "grainLength" };
    static const inline juce::String outputGainParameterId { "outputGain" };
    static const inline juce::String bypassParameterId { "bypass" };

    /** The blend dial's quantum, dyadic so that every position on it is exactly representable.

        It replaces the 0.01 the parameter shipped with, for a measured reason. JUCE snaps a float
        parameter to `start + interval * floor ((v - start) / interval + 0.5)`, so with 0.01 the
        default becomes -1 + 0.01f * 100, which this Release build folds into a single FMA and
        returns as -2.23517418e-08 rather than 0 — measured 2026-10-29 on the shell's own held value,
        and visible as 9.26e-09 of dry A in the §5.1 render that is supposed to be pure convolution.
        A p that stops short of 1.0 also never reaches the law's endpoint snap, so +1 keeps 7.05e-08
        of the wet stream instead of being bit-exact. A power of two makes that arithmetic exact in
        every evaluation order; 1/128 is the dyadic step nearest the one it replaces, and finer, so
        no dial position the old grid could reach is lost. */
    static constexpr float blendStep = 1.0f / 128.0f;

    /** Output gain in dB with 0 dB at exactly unity.

        The 0.125 dB interval is what makes that exact, and it is load-bearing rather than cosmetic:
        0 is not a dyadic fraction of [-60, +12], so the normalised float JUCE stores for the default
        comes back as -1.96e-06 dB — measured 2026-10-29 on this layout — which drives the stage at
        0.99999977 instead of 1.0f and breaks every bit-exact endpoint the law is required to hold.
        A power-of-two quantum is the one step size that survives the round trip through the
        normalised domain: -60 + n * 0.125 is representable for every n in range, so the default
        denormalises back onto exactly 0. */
    static constexpr float outputGainMinDb = -60.0f;
    static constexpr float outputGainMaxDb = 12.0f;
    static constexpr float outputGainStepDb = 0.125f;

    /** The allowed hop set (D-003); the UI names are derived from these, so there is one
        source of truth for what a hop may be. */
    static constexpr std::array<int, 4> hopValues { 64, 128, 256, 512 };
    static constexpr int defaultHopIndex = 1;

    /** The allowed grain-length set (D-008), in samples. Powers of two, so with the
        power-of-two hop set of D-003 every pair the UI can express satisfies H | L and pins
        F = nextPow2(2L-1) = 2L exactly. The ms label the editor shows is display-only: these
        numbers never move with the sample rate, because renumbering the choice list would
        shift every saved preset. */
    static constexpr std::array<int, 7> grainLengthValues { 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
    static constexpr int defaultGrainLengthIndex = 3;

    /** The footprint bound stated next to the cap (AGENTS §8): stereo, 9F + 6 floats of engine
        buffers plus the transform's 2F scratch, so ~704 KB at the default 8192 and ~5.5 MB at
        this cap. All of it prepare-time; nothing on the audio path grows. */
    static constexpr int maxGrainLengthSamples = grainLengthValues.back();

    /** One clamp rule for L, shared by configureRings and the editor.

        The lower bound: H | L alone admits H = L, where Hann has no overlap to sum and the
        hop-class sum is w[phase] rather than the COLA constant C = L/(2H) the engine's gain
        assumes, so L must reach 2H.

        The upper bound is the CPU gate (D-008). Cost per second is F log F / H, so the overlap
        ratio L/H — not L — is what crosses a load budget, which is why the pair is bounded rather
        than L: the measured stereo table has every pair at or under L = 128*H inside 25 % of one
        core (L = 65536 at H = 512 costs 20.2 %, L = 16384 at H = 128 costs 14.5 %) and the next
        step above it outside (L = 32768 at H = 128 is 30.6 %). Measured on a host ~2x faster than
        the Apple column in the plan's table, so on slower hardware the same pairs sit higher on the
        scale; the constant is the knob for that, not the arithmetic. */
    static constexpr int maxGrainToHopRatio = 128;

    static constexpr int clampGrainLength (int chosen, int hop) noexcept
    {
        const int ceiling = std::min (maxGrainLengthSamples, maxGrainToHopRatio * hop);
        return std::min (std::max (chosen, 2 * hop), ceiling);
    }

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

    /** The grain length actually running: the choice parameter after clampGrainLength. The
        editor reports this rather than the dial position, because a control that lies about what
        is running is worse than no control. */
    int grainLengthSamples() const;

    /** The choice parameter as the host set it, before the clamp moved it. */
    int requestedGrainLengthSamples() const;

    /** The dial position that actually runs, after the no-sidechain pin of D-011. The editor
        reports this rather than the blend parameter, because with no sidechain bus the plugin runs
        pure A whatever the dial says and a control that lies about what is running is worse than no
        control. */
    float effectiveBlendPosition() const noexcept
    {
        return entangle::effectiveBlend (rawParameter (blendParameterId, 0.0f), sidechainActive);
    }

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
    int currentGrainLengthIndex() const;

    /** A parameter's denormalised value, or the fallback where the layout has no such id. */
    float rawParameter (const juce::String& parameterId, float fallback) const noexcept;

    /** Hands the blend, output-gain and bypass targets to the mix stage. Called from processBlock
        on the audio thread, and from configureRings with processing suspended — the latter is what
        lets prepare() snap the coefficients onto where the host left them instead of ramping
        through the first 20 ms of a fresh transport. */
    void pushParameterTargets();

    void configureRings (double sampleRateToUse, int samplesPerBlock);
    void reconfigureSafely();
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    /** Declared above engine because GrainConvolutionEngine only borrows it and members are
        destroyed in reverse declaration order: this has to outlive the borrower. */
    JuceTransformFactory transformFactory;
    std::unique_ptr<engine::MixEngine> engine;
    engine::HopScheduler scheduler;
    engine::EngineConfig engineConfig;

    int activeChannels = 0;
    bool sidechainActive = false;
};
