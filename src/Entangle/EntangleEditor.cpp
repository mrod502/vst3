#include <juce_gui_basics/juce_gui_basics.h>
#include "EntangleEditor.h"
#include "EntangleProcessor.h"
#include <iostream>

EntangleAudioProcessorEditor::EntangleAudioProcessorEditor (EntangleAudioProcessor &p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (400, 300);
    otherLookAndFeel.setColour (juce::Slider::thumbColourId, juce::Colours::red);
    signalFade.setLookAndFeel (&otherLookAndFeel);
    signalFade.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    signalFade.setRotaryParameters(-2*M_PI/3.0,2*M_PI/3.0,true);
    signalFade.setRange (0.0, 100.0, 1.0);
    signalFade.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0);
    signalFade.setPopupDisplayEnabled (true, false, this);
    signalFade.setTextValueSuffix (" mg");
    signalFade.setValue (1.0);
    signalFade.setBounds (50, 50, 100, 100);
    signalFade.addListener(this);
    addAndMakeVisible (&signalFade);
}

EntangleAudioProcessorEditor::~EntangleAudioProcessorEditor()
{
}

//==============================================================================
void EntangleAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::green);
    g.setFont (15.0f);
    g.drawFittedText ("Morph Amount", 0, 0, getWidth(), 30, juce::Justification::centred, 1);
}

void EntangleAudioProcessorEditor::resized()
{
    signalFade.setBounds (40, 30, 20, getHeight() - 60);
}

 void EntangleAudioProcessorEditor::sliderValueChanged(juce::Slider* slider) {
    const auto value = slider->getValue();
    processorRef.signalFade = value;
    std::cout << "to value: "<<processorRef.signalFade<<"\n";
}