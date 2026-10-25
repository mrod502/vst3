#pragma once

#include "EntangleProcessor.h"

//==============================================================================
class EntangleAudioProcessorEditor final : public juce::AudioProcessorEditor
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
    juce::ComboBox hopSelector;
    juce::LookAndFeel_V4 otherLookAndFeel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> blendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hopAttachment;
    void adderallPrescriptionChanged(juce::Slider* slider);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntangleAudioProcessorEditor)
};
