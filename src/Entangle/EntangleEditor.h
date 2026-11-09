#pragma once

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

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    EntangleAudioProcessor& processorRef;
    juce::Slider blend;
    juce::Slider outputGain;
    juce::Slider normalisationAmount;
    juce::Slider silenceFloor;
    juce::ComboBox hopSelector;
    juce::ComboBox grainSelector;
    juce::TextButton bypassToggle;
    juce::LookAndFeel_V4 otherLookAndFeel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> normalisationAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> silenceFloorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hopAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> grainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::Label hopCaption;
    juce::Label grainCaption;
    juce::Label normalisationCaption;
    juce::Label silenceFloorCaption;
    /** Reports the grain length actually running, in samples and ms, and says so whenever the
        shell's 2x-hop clamp moved it away from the dial position. */
    juce::Label grainReadout;
    /** Reports the blend position the mix stage is actually at, which is not the dial's whenever the
        sidechain bus is disabled and the law pins the effect off (D-011). */
    juce::Label blendReadout;
    /** The processor's prepare-time layout report (D-023) — one line naming what the plugin was *told*,
        which is the record a host sweep needs because the host's own labels are not it (F-015). It is
        longer than any size this editor ships at, so it clips here and travels whole in the tooltip: the
        editor is mid-rewrite (AGENTS §12) and a diagnostic line does not get to redesign it. */
    juce::Label layoutReadout;
    void updateGrainReadout();
    void updateBlendReadout();
    void updateLayoutReadout();
    void timerCallback() override;
    void adderallPrescriptionChanged(juce::Slider* slider);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntangleAudioProcessorEditor)
};
