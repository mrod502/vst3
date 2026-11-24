#pragma once

#include <Bar.hpp>
#include <ChoiceBar.hpp>
#include <EnvelopePanel.hpp>
#include <Grid.hpp>
#include <PieDial.hpp>
#include <ResponsePanel.hpp>
#include <Text.hpp>
#include <Theme.hpp>
#include <TraceSink.hpp>

#include <cstdint>
#include <optional>
#include <vector>

#include "EntangleProcessor.h"

//==============================================================================
class EntangleAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit EntangleAudioProcessorEditor (EntangleAudioProcessor&);
    ~EntangleAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pushes every engine truth onto the widgets that reports it, and returns how many of the four truths
        it actually wrote.

        The number is the point: a truth that has not moved must not be written, because a tick that
        dirties a label ten times a second repaints the whole sheet. `timerCallback` runs this for the
        session; `OfflineRender` §5.22 runs it once at a time to count, which is the only way that
        figure can be measured without a message loop. */
    int refreshEngineTruths();

    /** Reads the level stage's window once and pushes it onto the two display panels and the meter.

        Separate from `refreshEngineTruths` because it has a different clock: the truths above move when a
        parameter or a layout does, which is a few times a session, and the traces move once per hop, which
        is 375 times a second at the default. Keeping them apart is what lets a tick with no new hop do
        nothing at all — no read of the window, no decimation, no repaint — which is `04` criterion 9 and the
        counter below is how its leg sees it. */
    void refreshTraces();

    /** The harness legs' read path for the two panels' axes, rules and counters. Production never calls
        these; the panels are placed and fed here and nowhere else. */
    [[nodiscard]] ui::ResponsePanel& responseDisplay() noexcept { return responsePanel; }
    [[nodiscard]] ui::EnvelopePanel& envelopeDisplay() noexcept { return envelopePanel; }
    [[nodiscard]] const ui::Bar& correctionMeter() const noexcept { return appliedCorrection; }
    [[nodiscard]] int traceDrainCount() const noexcept { return traceDrains; }

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object the plugin is running with.
    EntangleAudioProcessor& processorRef;
    ui::Theme theme;

    ui::Text wordmark;
    ui::Text statusLine;
    ui::Text layoutReadout;
    ui::Text grainReadout;
    ui::Text blendReadout;
    ui::Text levelCaption;
    ui::Text windowCaption;
    /** The words that name cell 9's coupled pair — the choice bars are inset by the column these measure into,
        because the pair is coupled by the 2×-hop floor and two identical-looking bars must not be guessed. */
    ui::Text grainLabel;
    ui::Text hopLabel;
    ui::Text eqCaption;
    ui::Text vibesCaption;
    ui::Text waveformCaption;

    ui::Bar outputGain;
    ui::Bar normalisationAmount;
    ui::Bar silenceFloor;
    ui::PieDial blend;
    ui::ChoiceBar hopSelector;
    ui::ChoiceBar grainSelector;
    juce::TextButton bypassToggle;

    /** Box 6's third row: the correction the level stage is applying to channel 0, read-only, one-sided with
        the sign in its words — the mockup has no bipolar bar, so inventing one would be a design change
        rather than a display (`04` §2.3). */
    ui::Bar appliedCorrection;

    ui::ResponsePanel responsePanel;
    ui::EnvelopePanel envelopePanel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> normalisationAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> silenceFloorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hopAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> grainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    bool updateGrainReadout();
    bool updateBlendReadout();
    bool updateLayoutReadout();
    bool updateInertStates();
    void updateMeter(const engine::TraceSnapshot& latest);
    void timerCallback() override;

    /** The last text each readout carried, and the last sidechain state the inert widgets saw. An empty
        string means "never pushed", so the first pass after construction writes and every later pass writes
        only when the truth it reports has actually moved. */
    std::optional<bool> shownSidechain;
    juce::String shownGrainText;
    juce::String shownBlendText;
    juce::String shownReport;

    /** The hop the panels were last fed. Absent until a hop has been seen, which is the difference between
        "nothing has published" and "published hop 0" — the first of them is idle and the other is news. */
    std::optional<std::uint64_t> shownTraceHop;

    /** The tick's scratch, sized once to the ring's depth and reused: the message thread may allocate, but
        not ten times a second, and never inside a `paint()`. */
    std::vector<engine::TraceSnapshot> traceWindow;
    std::vector<float> correctionMinimum;
    std::vector<float> correctionMaximum;
    std::vector<float> envelopeMain;
    std::vector<float> envelopeSide;
    std::vector<float> envelopeConv;
    juce::String shownMeterText;
    int traceDrains = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntangleAudioProcessorEditor)
};
