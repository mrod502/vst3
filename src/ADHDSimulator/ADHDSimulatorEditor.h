#pragma once

#include "ADHDSimulatorProcessor.h"

//==============================================================================
class ADHDSimulatorAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Slider::Listener
{
public:
    explicit ADHDSimulatorAudioProcessorEditor (ADHDSimulatorAudioProcessor&);
    ~ADHDSimulatorAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ADHDSimulatorAudioProcessor& processorRef;
    juce::Slider prescriptionStrength;
    juce::LookAndFeel_V4 otherLookAndFeel;
    void adderallPrescriptionChanged(juce::Slider* slider);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADHDSimulatorAudioProcessorEditor)
};
