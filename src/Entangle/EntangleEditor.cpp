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

    // Item text is the sample count and the list is never rebuilt, so the parameter index keeps
    // pointing at the same value across sample rates and saved presets stay put. The millisecond
    // figure is the readout below, which is display-only.
    for (int index = 0; index < static_cast<int> (EntangleAudioProcessor::grainLengthValues.size()); ++index)
        grainSelector.addItem (juce::String (EntangleAudioProcessor::grainLengthValues[static_cast<std::size_t> (index)]), index + 1);
    grainSelector.setSelectedId (EntangleAudioProcessor::defaultGrainLengthIndex + 1, juce::dontSendNotification);
    addAndMakeVisible (&grainSelector);

    hopCaption.setText ("Hop (samples)", juce::dontSendNotification);
    grainCaption.setText ("Grain length (samples)", juce::dontSendNotification);
    for (juce::Label* caption : { &hopCaption, &grainCaption, &grainReadout, &blendReadout })
    {
        caption->setFont (11.0f);
        caption->setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (caption);
    }

    outputGain.setLookAndFeel (&otherLookAndFeel);
    outputGain.setSliderStyle (juce::Slider::LinearHorizontal);
    outputGain.setRange (EntangleAudioProcessor::outputGainMinDb,
                         EntangleAudioProcessor::outputGainMaxDb,
                         EntangleAudioProcessor::outputGainStepDb);
    outputGain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    outputGain.setTextValueSuffix (" dB");
    addAndMakeVisible (&outputGain);

    bypassToggle.setClickingTogglesState (true);
    bypassToggle.setLookAndFeel (&otherLookAndFeel);
    addAndMakeVisible (&bypassToggle);

    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (p.apvts, EntangleAudioProcessor::blendParameterId, blend);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (p.apvts, EntangleAudioProcessor::outputGainParameterId, outputGain);
    hopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, EntangleAudioProcessor::hopParameterId, hopSelector);
    grainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, EntangleAudioProcessor::grainLengthParameterId, grainSelector);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (p.apvts, EntangleAudioProcessor::bypassParameterId, bypassToggle);

    // Either dial moves the 2x-hop floor, so both refresh the readout.
    auto refresh = [this] { updateGrainReadout(); };
    hopSelector.onChange = refresh;
    grainSelector.onChange = refresh;
    updateGrainReadout();

    // The blend readout reports the pinned position, and the pin follows the bus layout, which can
    // change with no parameter event reaching this editor. Ticking is the only honest way to keep it
    // truthful; the value it reads is the processor's cached bus state, not DSP state.
    updateBlendReadout();
    startTimer (100);
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

void EntangleAudioProcessorEditor::updateGrainReadout()
{
    const int requested = processorRef.requestedGrainLengthSamples();
    const int effective = processorRef.grainLengthSamples();
    const double sampleRate = processorRef.getSampleRate();

    auto text = juce::String (effective) + " samples";
    if (sampleRate > 0.0)
        text += " = " + juce::String (1000.0 * static_cast<double> (effective) / sampleRate, 1) + " ms";

    // The clamp is part of what is running: showing only the dial position would be a control
    // that lies.
    if (effective > requested)
        text += "  (raised from " + juce::String (requested) + ": floor is 2 x hop)";
    else if (effective < requested)
        text += "  (reduced from " + juce::String (requested) + ": ceiling is 128 x hop)";

    grainReadout.setText (text, juce::dontSendNotification);
}

void EntangleAudioProcessorEditor::timerCallback()
{
    updateBlendReadout();
}

void EntangleAudioProcessorEditor::updateBlendReadout()
{
    const float dial = static_cast<float> (blend.getValue());
    const float effective = processorRef.effectiveBlendPosition();

    auto describe = [] (float position)
    {
        if (position == 0.0f)
            return juce::String ("wet only");
        return juce::String (position, 2) + (position < 0.0f ? " (dry A)" : " (dry B)");
    };

    auto text = juce::String ("Blend ") + describe (effective);

    // The pin is part of what is running, so it is reported rather than the dial alone: with no
    // sidechain bus the plugin renders dry A whatever this control says (D-011).
    if (effective != dial)
        text += "  (dial " + juce::String (dial, 2) + ": no sidechain, so the effect is pinned off)";

    blendReadout.setText (text, juce::dontSendNotification);
}

void EntangleAudioProcessorEditor::resized()
{
    blend.setBounds (40, 40, 110, 110);
    hopSelector.setBounds (180, 50, 120, 25);
    hopCaption.setBounds (180, 32, 160, 16);
    grainSelector.setBounds (180, 90, 120, 25);
    grainCaption.setBounds (180, 72, 160, 16);
    grainReadout.setBounds (180, 118, 210, 16);
    blendReadout.setBounds (40, 210, 320, 16);
    outputGain.setBounds (40, 165, 250, 30);
    bypassToggle.setBounds (300, 168, 80, 25);
}
