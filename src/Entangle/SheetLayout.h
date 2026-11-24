#pragma once

#include <Bar.hpp>
#include <ChoiceBar.hpp>
#include <EnvelopePanel.hpp>
#include <Grid.hpp>
#include <PieDial.hpp>
#include <ResponsePanel.hpp>
#include <Text.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace entangle::sheet
{
/** One window's worth of sheet measurements: every bar and type figure a box's arithmetic is taken from, read
    once per layout pass so no box names a size of its own. Type figures are *bands* — `ui::reserveFor` of the
    measured run — so a word that draws ink below the baseline gets the room it actually uses. */
struct Geometry
{
    int barHeight{};
    /** The mockup's 19.8 mm column, which is the dial's inset. Bar rows take theirs from `ui::Theme`'s carrier
        instead: measured off their own words, widened to them when the words are wider (D-031). */
    int dialColumn{};
    int rowPitch{};
    int wordmarkBand{};
    int captionBand{};
    /** The `micro` em in device pixels, unrounded — the truth stack's unit (see `placeTruthColumn`). */
    double microEm{};
    /** Cap + descent of the widest rotated caption: the band across its track once the run is turned. */
    int rotatedBand{};
    int rotatedInset{};
    /** The column cell 9's choice labels own; the choice bars start to the right of it. */
    int choiceLabelColumn{};
};

[[nodiscard]] int rowPitchFor(int cellHeight, int barHeight) noexcept;

/** n `micro` ems in device pixels, rounded once — the truth stack's reserve unit. */
[[nodiscard]] int reserveInEms(int ems, double emPx) noexcept;

void placeMasterStrip(ui::Text& wordmark, ui::Bar& outputGain, juce::TextButton& bypass,
                      const ui::Rect& box, const Geometry& sheet);

void placeEdgeCaption(ui::Text& caption, const ui::Rect& box, const Geometry& sheet);

/** The sheet's only text box: the heading holds the bottom edge and the readouts stack upward from it.
    `stackLines` is each readout's measured line count at the column's width, floored at the mockup's
    allocation. */
void placeTruthColumn(ui::Text& status, ui::Text& summary, ui::Text& grain, ui::Text& blendWords,
                      ui::Text& heading, ui::Text& edgeCaption, const std::array<int, 4>& stackLines,
                      const ui::Rect& box, const Geometry& sheet);

void placeBlendDial(ui::Text& caption, ui::PieDial& dial, const ui::Rect& box, const Geometry& sheet);

void placeLevelStage(ui::Bar& amount, ui::Bar& floor, ui::Bar& meter, const ui::Rect& box,
                     const Geometry& sheet);

void placeDisplayPanels(ui::ResponsePanel& response, ui::EnvelopePanel& envelopes, const ui::Grid& grid);

/** The coupled pair under its caption, each bar named by a `micro` word in the column to its left. */
void placeWindowSize(ui::Text& caption, ui::Text& grainLabel, ui::ChoiceBar& grain, ui::Text& hopLabel,
                     ui::ChoiceBar& hop, const ui::Rect& box, const Geometry& sheet);
}
