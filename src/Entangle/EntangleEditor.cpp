#include "EntangleEditor.h"

#include <BarGeometry.hpp>
#include <Palette.hpp>
#include <TextGeometry.hpp>

#include <array>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace
{
/** One window's worth of sheet measurements: the grid's `k` plus every bar and type figure a box's
    arithmetic is taken from, read once per layout pass so no box names a size of its own. */
struct SheetGeometry
{
    int barHeight{};
    int labelColumn{};
    int rowPitch{};
    int wordmarkCap{};
    int captionCap{};
    /* The `micro` em in device pixels, unrounded — the truth stack's unit, because the stack's edges are taken
       off it in multiples (see `placeTruthColumn`). It is also the height of one drawn line: JUCE 9 lays a
       wrapped line one em below the last, ascent + descent being the em rather than the cap. */
    double microEm{};
    int rotatedCap{};
    int rotatedInset{};
};

/** Row top to row top for a group whose last row closes the cell's bottom edge. The bar body is a
    measurement and the inter-row gap is not, so the fit is taken up at the gap — the mockup's own box-6
    pitch is 0.85 mm more than the cell allows for three rows. */
[[nodiscard]] int rowPitchFor (int cellHeight, int barHeight) noexcept
{
    return cellHeight > barHeight ? (cellHeight - barHeight) / 2 : barHeight;
}

/** n `micro` ems in device pixels, rounded once. The truth stack's reserve unit — see `placeTruthColumn`. */
[[nodiscard]] int reserveInEms (int ems, double emPx) noexcept
{
    return static_cast<int> (std::lround (static_cast<double> (ems) * emPx));
}

/** The wordmark's cap top sits on the box's top edge, which is the sheet's 10 mm margin line; the master
    strip's two rows start below its cap band. */
void placeMasterStrip (ui::Text& wordmark, ui::Bar& outputGain, juce::TextButton& bypass,
                       const ui::Rect& box, const SheetGeometry& sheet)
{
    const int firstRow = box.y + sheet.wordmarkCap;

    wordmark.setBounds (box.x, box.y, box.w, firstRow - box.y);
    outputGain.setBounds (box.x, firstRow, box.w, sheet.barHeight);
    bypass.setBounds (box.x, firstRow + sheet.rowPitch, box.w, sheet.barHeight);
}

/** The rotated caption that names the cell to its right, in its own cell's right edge. Ink runs rightward
    from a rotated caption's pivot, so the component is one cap inside the box's right edge and the cell it
    names stays pure ground. */
void placeEdgeCaption (ui::Text& caption, const ui::Rect& box, const SheetGeometry& sheet)
{
    caption.setBounds (box.right() - sheet.rotatedCap - sheet.rotatedInset, box.y,
                       sheet.rotatedCap + sheet.rotatedInset, box.h);
}

/** The sheet's only text box. The group heading holds its bottom edge and the four lines stack upward from
    it, so a readout that wraps into a second line grows into the empty ground above rather than over the
    heading: the clamp clause is 42 characters and the column is one cell wide at the `micro` role.

    Six ems of reserve, 1:1:2:2, and every edge taken off the em at once rather than stepped off the previous
    line's rounded bottom. Stepping multiplies the em's own half-pixel by the six lines it passes through —
    4 px between the reference sheet and a window 0.75× it, on the one stack where the sheet would then not be
    scaling as one. */
void placeTruthColumn (ui::Text& status, ui::Text& summary, ui::Text& grain, ui::Text& blendWords,
                       ui::Text& heading, ui::Text& edgeCaption,
                       const ui::Rect& box, const SheetGeometry& sheet)
{
    const int headingTop = box.bottom() - sheet.captionCap;

    heading.setBounds (box.x, headingTop, box.w, box.bottom() - headingTop);

    const int stackTop = headingTop - reserveInEms (6, sheet.microEm);
    int laid = 0;

    for (const auto& line : { std::pair{ &status, 1 }, std::pair{ &summary, 1 },
                              std::pair{ &grain, 2 }, std::pair{ &blendWords, 2 } })
    {
        line.first->setBounds (box.x, stackTop + reserveInEms (laid, sheet.microEm), box.w,
                               reserveInEms (line.second, sheet.microEm));
        laid += line.second;
    }

    placeEdgeCaption (edgeCaption, box, sheet);
}

/** The dial takes what the caption column leaves, square, so the pair closes on the cell and the face the
    wedge is inscribed in is the rect that is painted. */
void placeBlendDial (ui::Text& caption, ui::PieDial& dial, const ui::Rect& box, const SheetGeometry& sheet)
{
    const int side = std::min (box.w - sheet.labelColumn, box.h);

    caption.setBounds (box.x, box.y, box.w, box.h);
    dial.setBounds (box.x + sheet.labelColumn, box.y + (box.h - side) / 2, side, side);
}

/** The level stage's two labelled bars. The third row is the pitch's, not a widget's. */
void placeLevelStage (ui::Bar& amount, ui::Bar& floor, const ui::Rect& box, const SheetGeometry& sheet)
{
    amount.setBounds (box.x, box.y, box.w, sheet.barHeight);
    floor.setBounds (box.x, box.y + sheet.rowPitch, box.w, sheet.barHeight);
}

/** The coupled pair adjacent under its group caption, the lower choice closing the cell's bottom edge. */
void placeWindowSize (ui::Text& caption, ui::ChoiceBar& grain, ui::ChoiceBar& hop,
                      const ui::Rect& box, const SheetGeometry& sheet)
{
    const int lower = box.bottom() - sheet.barHeight;

    caption.setBounds (box.x, box.y, box.w, sheet.captionCap);
    grain.setBounds (box.x, lower - sheet.rowPitch, box.w, sheet.barHeight);
    hop.setBounds (box.x, lower, box.w, sheet.barHeight);
}

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
      volumeCaption { "volumeCaption", "Volume Ctrl", ui::TypeRole::caption },
      windowCaption { "windowCaption", "Window Size", ui::TypeRole::caption },
      eqCaption { "eqCaption", "EQ", ui::TypeRole::rotatedCaption },
      vibesCaption { "vibesCaption", "Vibes", ui::TypeRole::rotatedCaption },
      waveformCaption { "waveformCaption", "Waveform", ui::TypeRole::rotatedCaption },
      outputGain { EntangleAudioProcessor::outputGainParameterId },
      normalisationAmount { EntangleAudioProcessor::normalisationAmountParameterId },
      silenceFloor { EntangleAudioProcessor::silenceFloorParameterId },
      blend { ui::PieMode::datumAtZero, EntangleAudioProcessor::blendParameterId },
      hopSelector { EntangleAudioProcessor::hopParameterId },
      grainSelector { EntangleAudioProcessor::grainLengthParameterId }
{
    /* Before anything is added: a component added first inherits the default look and feel through its
       parent, and the first paint of a widget wearing somebody else's skin is the failure §0 G's ordering
       rule exists to prevent. */
    setLookAndFeel (&theme);

    outputGain.setLabelWords ("Out");
    normalisationAmount.setLabelWords ("Amt");
    silenceFloor.setLabelWords ("Flr");

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

    const std::array<juce::Component*, 17> children { &wordmark,     &statusLine,
                                                     &layoutReadout, &grainReadout,
                                                     &blendReadout,  &volumeCaption,
                                                     &windowCaption, &eqCaption,
                                                     &vibesCaption,  &waveformCaption,
                                                     &outputGain,    &normalisationAmount,
                                                     &silenceFloor,  &blend,
                                                     &hopSelector,   &grainSelector,
                                                     &bypassToggle };
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
    return true;
}

bool EntangleAudioProcessorEditor::updateBlendReadout()
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

    if (text == shownBlendText)
        return false;

    shownBlendText = text;
    blendReadout.setText (text, juce::dontSendNotification);
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

    return true;
}

void EntangleAudioProcessorEditor::timerCallback()
{
    refreshEngineTruths();
}

void EntangleAudioProcessorEditor::resized()
{
    const ui::Grid grid (getWidth(), getHeight());
    theme.setScale (grid.k ());
    const float rampScale = ui::Theme::fontScaleFor (grid.k ());
    const auto capOf = [this, rampScale] (ui::TypeRole role)
                       {
                           return static_cast<int> (std::lround (theme.capHeightOf (role, rampScale)));
                       };

    SheetGeometry sheet;
    sheet.barHeight = ui::BarGeometry::bodyHeightAt (grid.k ());
    sheet.labelColumn = ui::BarGeometry::labelColumnAt (grid.k ());
    sheet.rowPitch = rowPitchFor (grid.cellPx().h, sheet.barHeight);
    sheet.wordmarkCap = capOf (ui::TypeRole::wordmark);
    sheet.captionCap = capOf (ui::TypeRole::caption);
    sheet.microEm = theme.fontFor (ui::TypeRole::micro, rampScale).getHeight ();
    sheet.rotatedCap = capOf (ui::TypeRole::rotatedCaption);
    sheet.rotatedInset = ui::rotatedCaptionInsetAt (grid.k ());

    placeMasterStrip (wordmark, outputGain, bypassToggle, grid.box (1), sheet);
    placeTruthColumn (statusLine, layoutReadout, grainReadout, blendReadout, volumeCaption, eqCaption,
                      grid.box (2), sheet);
    placeBlendDial (vibesCaption, blend, grid.box (4), sheet);
    placeLevelStage (normalisationAmount, silenceFloor, grid.box (6), sheet);
    placeWindowSize (windowCaption, grainSelector, hopSelector, grid.box (9), sheet);
    placeEdgeCaption (waveformCaption, grid.box (10), sheet);
}
