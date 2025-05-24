#include <juce_gui_basics/juce_gui_basics.h>
#include "ADHDSimulatorEditor.h"
#include "ADHDSimulatorProcessor.h"
#include <iostream>

ADHDSimulatorAudioProcessorEditor::ADHDSimulatorAudioProcessorEditor (ADHDSimulatorAudioProcessor &p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (400, 300);
    otherLookAndFeel.setColour (juce::Slider::thumbColourId, juce::Colours::red);
    prescriptionStrength.setLookAndFeel (&otherLookAndFeel);
    prescriptionStrength.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    prescriptionStrength.setRotaryParameters(-2*M_PI/3.0,2*M_PI/3.0,true);
    prescriptionStrength.setRange (0.0, 100.0, 1.0);
    prescriptionStrength.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0);
    prescriptionStrength.setPopupDisplayEnabled (true, false, this);
    prescriptionStrength.setTextValueSuffix (" mg");
    prescriptionStrength.setValue (1.0);
    prescriptionStrength.setBounds (50, 50, 100, 100);
    prescriptionStrength.addListener(this);
    addAndMakeVisible (&prescriptionStrength);
}

ADHDSimulatorAudioProcessorEditor::~ADHDSimulatorAudioProcessorEditor()
{
}

//==============================================================================
void ADHDSimulatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::green);
    g.setFont (15.0f);
    g.drawFittedText ("Adderall Strength (mg)", 0, 0, getWidth(), 30, juce::Justification::centred, 1);
}

void ADHDSimulatorAudioProcessorEditor::resized()
{
    prescriptionStrength.setBounds (40, 30, 20, getHeight() - 60);
}

 void ADHDSimulatorAudioProcessorEditor::sliderValueChanged(juce::Slider* slider) {
    const auto value = slider->getValue();
    std::cout << "value: "<<value<<"\n";
    processorRef.adderallDose = value;
}