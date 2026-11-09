#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <map>
#include <complex>
#include <memory>
#include <optional>
#include <vector>

#include <Engine.hpp>
#include <HopScheduler.hpp>
#include <LayoutGate.hpp>
#include <MixEngine.hpp>
#include <ReconfigureKey.hpp>

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
    static const inline juce::String normalisationAmountParameterId { "normalisationAmount" };
    static const inline juce::String silenceFloorParameterId { "silenceFloor" };

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

    /** How hard the level correction works: 0 passes the wet path through unchanged, 1 holds it at
        the geometric mean of the two dry readings (D-015). Default 1 because the correction is the
        feature; off-by-default is one character and changes what a fresh instance sounds like.

        The quantum is the blend dial's, for the blend dial's reason (F-010): with a 0.01 step a
        position on [0, 1] need not denormalise onto itself, and the amount is an exponent — a step
        short of 1.0 is a weaker correction, not a rounding detail. */
    static constexpr float normalisationAmountMin = 0.0f;
    static constexpr float normalisationAmountMax = 1.0f;
    static constexpr float normalisationAmountStep = 1.0f / 128.0f;

    /** Where the correction gives up and passes the wet path through (D-016). The gate is on any of
        the three readings, so a floored bus means unity rather than the partial boost a clamp would
        apply, and this dial is the mechanism every silent-bus criterion rests on.

        [-90, -20] dBFS at 0.125 dB — dyadic step, both endpoints exactly representable, and the
        default sits where item 03 measured exact silence: 180 dB above `LevelDetector`'s own
        1e-12 envelope floor, so the two silence rules cannot interact. The value most likely to want
        moving; confirm by ear. */
    static constexpr float silenceFloorMinDb = -90.0f;
    static constexpr float silenceFloorMaxDb = -20.0f;
    static constexpr float silenceFloorStepDb = 0.125f;
    static constexpr float silenceFloorDefaultDb = -60.0f;

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

    /** The footprint the cap implies, measured 2026-11-05 by `tools/PrepareFootprint` (Linux aarch64, gcc 13.3,
        Release, `fft::JuceFft`) rather than derived beside it. The engine's own buffers are 12.5F + 6 floats at
        two processed channels and 10.5F + 6 at one — 2F per channel of history plus accumulator, 6F + 6 of shared
        scratch, the F/2 window table, and the transform's 2F of in-place scratch — which an earlier version of
        this comment understated by 1.5F (F-016). Measured whole-plugin figures, rings, hop scratch and latency
        pre-fill included: 1.00 MiB at the default 8192, 1.78 MiB at this dial position clamped by the CPU rule
        below, and 6.51 MiB at H = 512 — the only hop that admits 65536 — for a stereo pair; the 8-wide sidechain
        costs +130 KiB of scheduler and saves 133 KiB of engine against stereo, so it is not the expensive shape.
        Measured bytes run 0.5–1.6 % over the bound: `juce::dsp::FFT`'s twiddle tables, which the bound excludes
        and this figure does not re-derive, because the backend is chosen at runtime (vDSP, IPP, MKL, FFTW, or the
        fallback). All of it prepare-time; nothing on the audio path grows. */
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

    /** The callback size the rings are actually sized for: a floor, not a cap (D-022 A).

        Below this the host's number never reaches the sizing at all, so a host running 64-sample callbacks
        pays the same 8192-float ring as one running 4096. What the floor buys is that the sizing does not
        depend on host behaviour: a callback larger than the prepared block is caught by
        `assert(numSamples <= config.maxHostBlock)` in an assert build, where sizing to the prepared block
        instead would degrade to a short `CircularBuffer::write` and a hole in the output. The trade, with the
        figures: at the ceiling triple the floor costs ~320 KiB of rings where the delay lines themselves need
        ~13 KiB. Measured and tabulated by `tools/PrepareFootprint`, which is not in CTest.

        Named as a function rather than left inline at the call site so the policy has a leg — without it the
        floor is an expression inside a private function and the only way to test it is to allocate and count
        bytes, which item 04 decided against. */
    static constexpr int maxHostBlockFor(int hostBlock) noexcept
    {
        return std::max(hostBlock, maxHostBlockSamples);
    }


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

    /** One line naming the configuration the last real prepare built, for a human in a host to read and
        paste (D-023).

        The host's own labels are not evidence of what it routed — F-015 measured this plugin's gate answer
        and JUCE's substitution behaviour disagreeing — so the record has to come from what the plugin was
        *told*: bus inventory, live counts and `AudioChannelSet::getDescription()` for each of the three
        buses, the gate's verdict and the map's rule, the processed count, the declared latency, the hop,
        the requested and running grain length, the host block against `maxHostBlockFor`, and the two
        counters below. Assembled at the end of a prepare that actually rebuilt something, so it describes
        what is running rather than an arrival the D-022 B guard declined; nothing in it reaches the audio
        path.

        Returned by value under a lock: `prepareToPlay` may run on any thread — which is §5.21 C5's own
        premise — while the editor reads it at idle on the message thread. */
    juce::String layoutReport() const;

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

    /** Hands the blend, output-gain, bypass and the two level-stage targets to the mix stage. Called
        from processBlock on the audio thread, and from configureRings with processing suspended — the
        latter is what lets prepare() snap the coefficients onto where the host left them instead of
        ramping through the first 20 ms of a fresh transport. */
    void pushParameterTargets();

    void configureRings (double sampleRateToUse, int samplesPerBlock);
    void reconfigureSafely();
    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    /** Formats the report's configuration half into `layoutReportBody` and writes the assembled line to
        `juce::Logger` once, under the message-thread guard of D-023 C. Called from the tail of
        `configureRings` only — the guard's early return takes neither half, which is what makes the counters
        mean prepares and declined arrivals rather than pokes. */
    void refreshLayoutReport (const engine::LayoutDecision&, const entangle::ChainConfig&,
                              double sampleRateToUse, int samplesPerBlock);

    /** Declared above engine because GrainConvolutionEngine only borrows it and members are
        destroyed in reverse declaration order: this has to outlive the borrower. */
    JuceTransformFactory transformFactory;
    std::unique_ptr<engine::MixEngine> engine;
    engine::HopScheduler scheduler;
    engine::EngineConfig engineConfig;

    int activeChannels = 0;
    bool sidechainActive = false;

    /** The configuration the rings, the engine and the latency pre-fill were last built for. Empty until a
        prepare succeeds and cleared by a refusal, so a refusal can never be remembered (D-022 B). */
    std::optional<engine::ReconfigureKey> preparedKey;

    /** The report's configuration half, rebuilt by every real prepare, and the two counters that make
        D-022 B readable from outside the class: `prepares` counts arrivals that rebuilt, `skipped` counts
        arrivals the key guard declined. The counters are formatted at read time rather than stored in the
        body, because a declined arrival rebuilds nothing — and a `skipped=` nobody can see until the next real
        prepare is not the record item 08 exists to provide. Neither is reachable from `processBlock`; the lock
        is here because a host may prepare on a thread the editor is not on, and a `juce::String` assigned
        under a reader is a crash, not a cosmetic race. */
    mutable juce::CriticalSection layoutReportLock;
    juce::String layoutReportBody;
    int prepareCount = 0;
    int skippedArrivalCount = 0;
};
