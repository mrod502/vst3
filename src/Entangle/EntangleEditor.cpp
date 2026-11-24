#include "EntangleEditor.h"
#include "SheetLayout.h"

#include <BarGeometry.hpp>
#include <LevelNormalizer.hpp>
#include <Palette.hpp>
#include <TextGeometry.hpp>

#include <array>
#include <cmath>
#include <algorithm>
#include <initializer_list>
#include <utility>

namespace layout = entangle::sheet;

namespace
{
/** Reads one `key=value` field out of the layout report, stopping at the next space. Every key this editor
    reads is unique in the line and none is a prefix of another that carries an `=` — the report's own
    format (§5.21 checks it field by field) is what makes the search total. */
[[nodiscard]] juce::String reportField (const juce::String& report, const juce::String& key)
{
    const juce::String needle = key + "=";
    const int at = report.indexOf (needle);

    if (at < 0)
        return {};

    return report.substring (at + needle.length()).upToFirstOccurrenceOf (" ", false, false);
}
}

EntangleAudioProcessorEditor::EntangleAudioProcessorEditor (EntangleAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      wordmark { "wordmark", "Entangle", ui::TypeRole::wordmark },
      statusLine { "statusLine", {}, ui::TypeRole::micro },
      layoutReadout { "layoutReadout", {}, ui::TypeRole::micro },
      grainReadout { "grainReadout", {}, ui::TypeRole::micro },
      blendReadout { "blendReadout", {}, ui::TypeRole::micro },
      levelCaption { "levelCaption", "Level Ctrl", ui::TypeRole::caption },
      windowCaption { "windowCaption", "Window Size", ui::TypeRole::caption },
      grainLabel { "grainLabel", "Grain", ui::TypeRole::micro },
      hopLabel { "hopLabel", "Hop", ui::TypeRole::micro },
      eqCaption { "eqCaption", "EQ", ui::TypeRole::rotatedCaption },
      vibesCaption { "vibesCaption", "Vibes", ui::TypeRole::rotatedCaption },
      waveformCaption { "waveformCaption", "Waveform", ui::TypeRole::rotatedCaption },
      outputGain { EntangleAudioProcessor::outputGainParameterId },
      normalisationAmount { EntangleAudioProcessor::normalisationAmountParameterId },
      silenceFloor { EntangleAudioProcessor::silenceFloorParameterId },
      blend { ui::PieMode::proportional, EntangleAudioProcessor::blendParameterId },
      hopSelector { EntangleAudioProcessor::hopParameterId },
      grainSelector { EntangleAudioProcessor::grainLengthParameterId },
      appliedCorrection { "appliedCorrection" }
{
    /* Before anything is added: a component added first inherits the default look and feel through its
       parent, and the first paint of a widget wearing somebody else's skin is the failure §0 G's ordering
       rule exists to prevent. */
    setLookAndFeel (&theme);

    outputGain.setLabelWords ("Out");
    normalisationAmount.setLabelWords ("Amt");
    silenceFloor.setLabelWords ("Flr");

    /* The meter is one-sided — |correction| against the stage's own bound, the sign in its words — because
       the mockup draws no bipolar bar, so a bar that filled from the middle would be a design element
       nobody drew (§2.3). Its range is the stage's, not a 0 dB to 100 % guess. */
    appliedCorrection.setReadOnly (true);
    appliedCorrection.setLabelWords ("Adj");
    appliedCorrection.setRange (0.0, 1.0);

    for (int index = 0; index < static_cast<int> (EntangleAudioProcessor::hopValues.size()); ++index)
        hopSelector.addChoice (juce::String (EntangleAudioProcessor::hopValues[static_cast<std::size_t> (index)]),
                               index + 1);
    hopSelector.setSelectedId (EntangleAudioProcessor::defaultHopIndex + 1, juce::dontSendNotification);

    // Item text is the sample count and the list is never rebuilt, so the parameter index keeps
    // pointing at the same value across sample rates and saved presets stay put. The millisecond
    // figure is the readout in box 2, which is display-only.
    for (int index = 0; index < static_cast<int> (EntangleAudioProcessor::grainLengthValues.size()); ++index)
        grainSelector.addChoice (juce::String (EntangleAudioProcessor::grainLengthValues[static_cast<std::size_t> (index)]),
                                 index + 1);
    grainSelector.setSelectedId (EntangleAudioProcessor::defaultGrainLengthIndex + 1, juce::dontSendNotification);

    bypassToggle.setName (EntangleAudioProcessor::bypassParameterId);
    bypassToggle.setButtonText ("Bypass");
    bypassToggle.setClickingTogglesState (true);

    const std::array<juce::Component*, 22> children { &wordmark,     &statusLine,
                                                     &layoutReadout, &grainReadout,
                                                     &blendReadout,  &levelCaption,
                                                     &windowCaption, &grainLabel,
                                                     &hopLabel,      &eqCaption,
                                                     &vibesCaption,  &waveformCaption,
                                                     &outputGain,    &normalisationAmount,
                                                     &silenceFloor,  &blend,
                                                     &hopSelector,   &grainSelector,
                                                     &bypassToggle,  &appliedCorrection,
                                                     &responsePanel, &envelopePanel };
    for (juce::Component* child : children)
        addAndMakeVisible (child);

    eqCaption.setOrientation (ui::TextOrientation::rotated);
    vibesCaption.setOrientation (ui::TextOrientation::rotated);
    waveformCaption.setOrientation (ui::TextOrientation::rotated);

    /* Resizable, with the sheet scaling uniformly. There is nothing
       to re-flow because there is nothing but `k`: resized() recomputes every box, bar, cap and font from the
       window's width, so a resize draws the same sheet bigger or smaller.

       The aspect is locked on the constrainer the editor already owns, which is what the standalone wrapper
       reads and what a host that asks IPlugView::checkSizeConstraint is told. A host that ignores it still
       gets a proportional sheet — the geometry comes from the width alone, so the surplus or shortfall lands
       as ground below the last row rather than as a stretched box.

       No corner grip: `setResizable`'s second argument would add JUCE's own `ResizableCornerComponent`, which
       paints through `LookAndFeel::drawCornerResizer` in colours this design never named. The host's window
       chrome is the resize affordance. */
    setResizable (true, false);
    setResizeLimits (ui::kMinimumWindowWidthPx, ui::kMinimumWindowHeightPx,
                     ui::kMaximumWindowWidthPx, ui::kMaximumWindowHeightPx);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (ui::kSheetAspectRatio);

    setSize (ui::kReferenceWindowWidthPx, ui::kReferenceWindowHeightPx);

    blendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (p.apvts, EntangleAudioProcessor::blendParameterId, blend);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (p.apvts, EntangleAudioProcessor::outputGainParameterId, outputGain);
    normalisationAmountAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (p.apvts, EntangleAudioProcessor::normalisationAmountParameterId, normalisationAmount);
    silenceFloorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (p.apvts, EntangleAudioProcessor::silenceFloorParameterId, silenceFloor);
    hopAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, EntangleAudioProcessor::hopParameterId, hopSelector);
    grainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (p.apvts, EntangleAudioProcessor::grainLengthParameterId, grainSelector);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (p.apvts, EntangleAudioProcessor::bypassParameterId, bypassToggle);

    // Either dial moves the 2x-hop floor, so both refresh the engine truths.
    hopSelector.onChange = [this] { refreshEngineTruths(); };
    grainSelector.onChange = [this] { refreshEngineTruths(); };

    /* Both panels' scales come from the product, never from a figure written beside the panel: the response
       panel's ± bound is the level stage's own clamp, and the envelope panel's axis is the gate parameter's
       own bound while its rule follows the parameter itself. Below the attachments because until a widget
       carries its parameter's value it carries its own default, and a rule taken from that is a lie about
       the gate. The tick re-applies the rule every 100 ms after this. */
    responsePanel.setCorrectionRange (dsp::LevelNormalizer::gainRangeDb);
    envelopePanel.setAxisDbRange (static_cast<double> (EntangleAudioProcessor::silenceFloorMinDb), 0.0);
    envelopePanel.setGateDb (static_cast<double> (silenceFloor.getValue()));

    refreshEngineTruths();

    // The blend and layout readouts follow the bus layout, which can change with no parameter event
    // reaching this editor, so ticking is the only way to keep them truthful. The values read are the
    // processor's cached bus state, not DSP state.
    startTimer (100);
}

EntangleAudioProcessorEditor::~EntangleAudioProcessorEditor()
{
}

//==============================================================================
void EntangleAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::toColour (ui::kGround));
}

int EntangleAudioProcessorEditor::refreshEngineTruths()
{
    return (updateGrainReadout() ? 1 : 0)
           + (updateBlendReadout() ? 1 : 0)
           + (updateLayoutReadout() ? 1 : 0)
           + updateInertStates();
}

bool EntangleAudioProcessorEditor::updateGrainReadout()
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

    if (text == shownGrainText)
        return false;

    shownGrainText = text;
    grainReadout.setText (text, juce::dontSendNotification);

    /* The truth stack's reserve is counted in the lines these words actually make, so new words are new
       geometry: without this the stack keeps whatever it measured before the first truth arrived. */
    resized();
    return true;
}

bool EntangleAudioProcessorEditor::updateBlendReadout()
{
    const float dial = static_cast<float> (blend.getValue());
    const float effectiveQ = processorRef.effectiveBlendPosition() + 1.0f;

    /* The ends are named in words because A-2 took the dial's only ink mark and the palette forbids a tint
       carrying state: a proportional wedge is empty at q = 0 and full at q = 1, so the sentence carries
       "which end am I at" (D-038). */
    auto describe = [] (float q)
    {
        if (q <= 0.0f)
            return juce::String ("fully dry A");
        if (q >= 1.0f)
            return juce::String ("fully wet");
        return juce::String ("dry A + wet");
    };

    auto text = juce::String ("Blend ") + juce::String (effectiveQ, 2) + " " + describe (effectiveQ);

    // The pin is part of what is running, so it is reported rather than the dial alone: with no
    // sidechain bus the plugin renders dry A whatever this control says (D-011).
    if (effectiveQ != dial)
        text += "  (dial " + juce::String (dial, 2) + ": no sidechain, so the effect is pinned off)";

    if (text == shownBlendText)
        return false;

    shownBlendText = text;
    blendReadout.setText (text, juce::dontSendNotification);
    resized();   // new words, new line count, new stack reserve
    return true;
}

bool EntangleAudioProcessorEditor::updateLayoutReadout()
{
    const auto report = processorRef.layoutReport();
    if (report == shownReport)
        return false;

    shownReport = report;

    /* The visible line is a reduction of the record, not a second reading of the buses: everything in it
        comes out of the same string the log carries, so the two cannot disagree about what the plugin was
        told. The whole line stays reachable — here on hover, and in juce::Logger (D-023 C). */
    layoutReadout.setText (reportField (report, "mainIn") + "/" + reportField (report, "side")
                               + "/" + reportField (report, "mainOut")
                           + " R" + reportField (report, "R")
                           + " H" + reportField (report, "hop")
                           + " L" + reportField (report, "Lrun"),
                           juce::dontSendNotification);
    layoutReadout.setTooltip (report);
    resized();   // new words, new line count, new stack reserve

    return true;
}

bool EntangleAudioProcessorEditor::updateInertStates()
{
    const bool connected = processorRef.isSidechainActive();
    if (shownSidechain.has_value() && *shownSidechain == connected)
        return false;

    shownSidechain = connected;

    /* Two words, never a colour: the palette has no red, so the news is the absence of `fill` on the
       controls the engine has stopped listening to, plus these words (§0 G). `setEnabled` reaches the
       widget's pointer only — the parameters behind them stay live, saved and automatable (§0 J). */
    statusLine.setText (connected ? "B connected" : "no sidechain", juce::dontSendNotification);
    blend.setEnabled (connected);
    normalisationAmount.setEnabled (connected);
    silenceFloor.setEnabled (connected);
    resized();   // new words, new line count, new stack reserve

    return true;
}

void EntangleAudioProcessorEditor::timerCallback()
{
    refreshEngineTruths();
    refreshTraces();
}

void EntangleAudioProcessorEditor::refreshTraces()
{
    engine::TraceSnapshot latest {};

    /* The gate rule is the live parameter, not a function of new data: dragging `Flr` moves the rule with the
       transport stopped, because a rule that only moves when the audio does is a photograph of the last hop.
       It is set above the news test for that reason, and its setter no-ops on an unchanged value, so a tick
       that changes nothing still costs nothing. */
    envelopePanel.setGateDb (static_cast<double> (silenceFloor.getValue ()));

    /* Two ways a tick carries no news: nothing has published since the last prepare, and the newest hop is
       the one already drawn. Both take the same path — hand the panels "nothing advancing", which leaves the
       traces where they stopped and the readouts saying `idle` — and neither reads the window, decimates a
       column or repaints, which is criterion 9 and the reason the read below is not first. */
    if (!processorRef.traceLatest (latest)
        || (shownTraceHop.has_value() && latest.hopIndex == *shownTraceHop))
    {
        responsePanel.setCorrections (nullptr, nullptr, 0, 0.0, false);
        envelopePanel.setEnvelopes (nullptr, nullptr, nullptr, 0, 0.0, false);
        return;
    }

    ++traceDrains;
    shownTraceHop = latest.hopIndex;

    /* One read of the whole window per news-carrying tick, into scratch that was sized once. The ring is the
       history: the panels are redrawn from it rather than accumulating a trace of their own. */
    traceWindow.resize (processorRef.traceDepth());
    const std::size_t count = processorRef.traceWindow (
        {traceWindow.data(), static_cast<std::size_t> (traceWindow.size())});

    correctionMinimum.resize (count);
    correctionMaximum.resize (count);
    envelopeMain.resize (count);
    envelopeSide.resize (count);
    envelopeConv.resize (count);

    for (std::size_t i = 0; i < count; ++i)
    {
        const engine::TraceSnapshot& shot = traceWindow[i];
        const auto channels = std::min (static_cast<std::size_t> (shot.channels),
                                        static_cast<std::size_t> (engine::kMaxChannels));

        /* The two extremes of the hop's per-channel corrections: with one channel the pair coincides, which
           is what one channel looks like, and with eight the gap between them is the figure M9 row 1 is
           still asking about. */
        std::array<float, static_cast<std::size_t> (engine::kMaxChannels)> gains {};
        for (std::size_t channel = 0; channel < channels; ++channel)
            gains[channel] = shot.reading[channel].gainDb;

        const ui::Spread spread = ui::spreadOf (gains.data(), channels);
        correctionMinimum[i] = spread.minimum;
        correctionMaximum[i] = spread.maximum;

        envelopeMain[i] = shot.reading[0].envelopeA;
        envelopeSide[i] = shot.reading[0].envelopeB;
        envelopeConv[i] = shot.reading[0].envelopeConv;
    }

    const double secondsPerHop = processorRef.traceSecondsPerHop ();
    const auto drawnChannels = std::clamp (latest.channels, 1U,
                                           static_cast<std::uint32_t> (engine::kMaxChannels));

    /* The gate rule was already applied above, on every tick; the channel count is only known from a
       publication, so it travels with the traces it names. */
    envelopePanel.setChannelCount (static_cast<int> (drawnChannels));

    responsePanel.setCorrections (correctionMinimum.data (), correctionMaximum.data (), count,
                                  secondsPerHop, true);
    envelopePanel.setEnvelopes (envelopeMain.data (), envelopeSide.data (), envelopeConv.data (), count,
                                secondsPerHop, true);

    updateMeter (latest);
}

void EntangleAudioProcessorEditor::updateMeter (const engine::TraceSnapshot& latest)
{
    const float gain = latest.channels > 0 ? latest.reading[0].gainDb : 0.0f;
    const double fraction = std::min (1.0, std::abs (static_cast<double> (gain))
                                              / dsp::LevelNormalizer::gainRangeDb);

    if (appliedCorrection.getValue() != fraction)
        appliedCorrection.setValue (fraction, juce::dontSendNotification);

    /* The sign lives in the words because the bar itself is one-sided; a fill that only ever grows would
       report a −24 dB cut and a +24 dB boost identically, which is a meter that lies. Whole decibels only: the
       words share the body with the fill and a tenth-of-a-dB figure is ink the stage cannot resolve (D-037). */
    const auto words = juce::String (gain < 0.0f ? "-" : "+") + juce::String (std::abs (gain), 0);
    if (words != shownMeterText)
    {
        shownMeterText = words;
        appliedCorrection.setValueWords (words);
    }
}

void EntangleAudioProcessorEditor::resized()
{
    const ui::Grid grid (getWidth(), getHeight());
    theme.setScale (grid.k ());
    const float rampScale = ui::Theme::fontScaleFor (grid.k ());
    const auto cell = grid.cellPx();

    /* Measure-then-place: every word the sheet draws is shaped once here and the boxes are built from that pass,
       not from the millimetre figures, which are the floor and not the fact. A resize measures; a repaint draws
       what the resize decided — nothing here is reached from paint(). */
    const auto extentOf = [this, rampScale] (const juce::String& words, ui::TypeRole role)
                          {
                              return theme.measureText (words, role, rampScale).extent;
                          };
    const auto reserveOf = [this, rampScale] (const juce::String& words, ui::TypeRole role, int roomPx)
                           {
                               return ui::reserveFor (theme.measureRun (words, role, rampScale, roomPx).extent);
                           };

    layout::Geometry geom;
    geom.barHeight = ui::BarGeometry::bodyHeightAt (grid.k ());
    geom.rowPitch = layout::rowPitchFor (cell.h, geom.barHeight);
    geom.dialColumn = ui::BarGeometry::labelColumnAt (grid.k ());
    geom.rotatedInset = ui::rotatedCaptionInsetAt (grid.k ());
    geom.microEm = theme.fontFor (ui::TypeRole::micro, rampScale).getHeight ();

    geom.wordmarkBand = ui::reserveFor (extentOf (wordmark.getText (), ui::TypeRole::wordmark));
    geom.captionBand = std::max (reserveOf (levelCaption.getText (), ui::TypeRole::caption, cell.w),
                                  reserveOf (windowCaption.getText (), ui::TypeRole::caption, cell.w));
    /* A rotated run's line axis is the cell's height less the inset at both ends — that is the room the skin
       hands it, so that is the width it is measured at, and its band is what the column must then hold. */
    const int rotatedExtent = cell.h - 2 * geom.rotatedInset;
    geom.rotatedBand = std::max ({reserveOf (eqCaption.getText (), ui::TypeRole::rotatedCaption, rotatedExtent),
                                   reserveOf (vibesCaption.getText (), ui::TypeRole::rotatedCaption,
                                              rotatedExtent),
                                   reserveOf (waveformCaption.getText (), ui::TypeRole::rotatedCaption,
                                              rotatedExtent)});

    /* One column, four labels: the widest word decides it, and the Theme carries the answer to the body, the
       value's start and the pointer's mapping, which is the only way those three cannot disagree (01 risk 1).
       Cell 9's pair is named at the `micro` role, so it earns its own column rather than widening the bar rows. */
    double widestBarLabel{};
    for (const ui::Bar* bar : { &outputGain, &normalisationAmount, &silenceFloor, &appliedCorrection })
        widestBarLabel = std::max (widestBarLabel, extentOf (bar->labelWords (), ui::TypeRole::barLabel).advancePx);

    const double widestChoiceLabel = std::max (extentOf (grainLabel.getText (), ui::TypeRole::micro).advancePx,
                                               extentOf (hopLabel.getText (), ui::TypeRole::micro).advancePx);

    theme.setLabelColumnPx (ui::labelColumnFor (widestBarLabel, grid.k ()));
    geom.choiceLabelColumn = ui::labelColumnFor (widestChoiceLabel, grid.k ());

    /* The mockup's own allocation — one line of status, one of layout, two each for the grain and blend
       clauses — is the floor, so a readout that has not been written yet still holds what it will need, and a
       truth longer than the sheet was drawn for takes the ground above instead of losing its last line. */
    const auto stackLines = [this, rampScale, width = cell.w] (const ui::Text& readout, int minimum)
                            {
                                return std::max (minimum, theme.measureRun (readout.getText (),
                                                                            ui::TypeRole::micro, rampScale,
                                                                            width).lineCount);
                            };
    const std::array<int, 4> truthStack { stackLines (statusLine, 1), stackLines (layoutReadout, 1),
                                          stackLines (grainReadout, 2), stackLines (blendReadout, 2) };

    layout::placeMasterStrip (wordmark, outputGain, bypassToggle, grid.box (1), geom);
    layout::placeTruthColumn (statusLine, layoutReadout, grainReadout, blendReadout, levelCaption, eqCaption,
                              truthStack, grid.box (2), geom);
    layout::placeBlendDial (vibesCaption, blend, grid.box (4), geom);
    layout::placeLevelStage (normalisationAmount, silenceFloor, appliedCorrection, grid.box (6), geom);
    layout::placeWindowSize (windowCaption, grainLabel, grainSelector, hopLabel, hopSelector,
                             grid.box (9), geom);
    layout::placeEdgeCaption (waveformCaption, grid.box (10), geom);
    layout::placeDisplayPanels (responsePanel, envelopePanel, grid);
}
