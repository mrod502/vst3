#include <juce_gui_basics/juce_gui_basics.h>
#include "EntangleEditor.h"
#include "EntangleProcessor.h"

EntangleAudioProcessorEditor::EntangleAudioProcessorEditor (EntangleAudioProcessor &p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    setSize (400, 300);
    otherLookAndFeel.setColour (juce::Slider::thumbColourId, juce::Colours::red);

    blend.setLookAndFeel (&otherLookAndFeel);
    blend.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    blend.setRotaryParameters(-2*M_PI/3.0,2*M_PI/3.0,true);
    blend.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0);
    blend.setPopupDisplayEnabled (true, false, this);
    addAndMakeVisible (&blend);

    for (int index = 0; index < static_cast<int> (EntangleAudioProcessor::hopValues.size()); ++index)
        hopSelector.addItem (juce::String (EntangleAudioProcessor::hopValues[static_cast<std::size_t> (index)]), index + 1);
    hopSelector.setSelectedId (EntangleAudioProcessor::defaultHopIndex + 1, juce::dontSendNotification);
    addAndMakeVisible (&hopSelector);

    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (p.apvts, EntangleAudioProcessor::blendParameterId, blend);
    hopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, EntangleAudioProcessor::hopParameterId, hopSelector);
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

    const bool connected = processorRef.isSidechainActive();
    g.setFont (12.0f);
    g.setColour (connected ? juce::Colours::grey : juce::Colours::orangered);
    g.drawFittedText (connected ? "B connected" : "B not connected",
                      0, getHeight() - 25, getWidth(), 20, juce::Justification::centred, 1);
}

void EntangleAudioProcessorEditor::resized()
{
    blend.setBounds (40, 40, 110, 110);
    hopSelector.setBounds (180, 50, 120, 25);
}
