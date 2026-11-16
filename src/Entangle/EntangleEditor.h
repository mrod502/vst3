#pragma once

#include <Bar.hpp>
#include <ChoiceBar.hpp>
#include <Grid.hpp>
#include <PieDial.hpp>
#include <Text.hpp>
#include <Theme.hpp>

#include <optional>

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

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    EntangleAudioProcessor& processorRef;
    ui::Theme theme;

    ui::Text wordmark;
    ui::Text statusLine;
    ui::Text layoutReadout;
    ui::Text grainReadout;
    ui::Text blendReadout;
    ui::Text volumeCaption;
    ui::Text windowCaption;
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
    void timerCallback() override;

    /** The last text each readout carried, and the last sidechain state the inert widgets saw. An empty
        string means "never pushed", so the first pass after construction writes and every later pass writes
        only when the truth it reports has actually moved. */
    std::optional<bool> shownSidechain;
    juce::String shownGrainText;
    juce::String shownBlendText;
    juce::String shownReport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntangleAudioProcessorEditor)
};
