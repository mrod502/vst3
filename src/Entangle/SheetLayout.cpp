#include "SheetLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace entangle::sheet
{
int rowPitchFor(int cellHeight, int barHeight) noexcept
{
    /* Row top to row top for a group whose last row closes the cell's bottom edge. The bar body is a
       measurement and the inter-row gap is not, so the fit is taken up at the gap (D-024). */
    return cellHeight > barHeight ? (cellHeight - barHeight) / 2 : barHeight;
}

int reserveInEms(int ems, double emPx) noexcept
{
    return static_cast<int>(std::lround(static_cast<double>(ems) * emPx));
}

void placeMasterStrip(ui::Text& wordmark, ui::Bar& outputGain, juce::TextButton& bypass,
                      const ui::Rect& box, const Geometry& sheet)
{
    /* The wordmark's cap top sits on the box's top edge — the sheet's 10 mm margin line — and the strip's two
       rows start below the band its whole word occupies, descender included. */
    const int firstRow = box.y + sheet.wordmarkBand;

    wordmark.setBounds(box.x, box.y, box.w, firstRow - box.y);
    outputGain.setBounds(box.x, firstRow, box.w, sheet.barHeight);
    bypass.setBounds(box.x, firstRow + sheet.rowPitch, box.w, sheet.barHeight);
}

void placeEdgeCaption(ui::Text& caption, const ui::Rect& box, const Geometry& sheet)
{
    /* Ink runs rightward from a rotated caption's pivot, so the component sits one band inside the box's right
       edge and the cell it names stays pure ground. */
    caption.setBounds(box.right() - sheet.rotatedBand - sheet.rotatedInset, box.y,
                      sheet.rotatedBand + sheet.rotatedInset, box.h);
}

void placeTruthColumn(ui::Text& status, ui::Text& summary, ui::Text& grain, ui::Text& blendWords,
                      ui::Text& heading, ui::Text& edgeCaption, const std::array<int, 4>& stackLines,
                      const ui::Rect& box, const Geometry& sheet)
{
    const int headingTop = box.bottom() - sheet.captionBand;

    heading.setBounds(box.x, headingTop, box.w, box.bottom() - headingTop);

    const int totalLines = stackLines[0] + stackLines[1] + stackLines[2] + stackLines[3];
    const int stackTop = headingTop - reserveInEms(totalLines, sheet.microEm);
    int laid = 0;

    /* Every edge comes off the stack's own top as `round(n × em)`, never off the previous line's rounded
       bottom: stepping rounded figures carries the em's half-pixel once per line (F-024 item 2). */
    for (const auto& line : {std::pair{&status, stackLines[0]}, std::pair{&summary, stackLines[1]},
                             std::pair{&grain, stackLines[2]}, std::pair{&blendWords, stackLines[3]}})
    {
        line.first->setBounds(box.x, stackTop + reserveInEms(laid, sheet.microEm), box.w,
                              reserveInEms(line.second, sheet.microEm));
        laid += line.second;
    }

    placeEdgeCaption(edgeCaption, box, sheet);
}

void placeBlendDial(ui::Text& caption, ui::PieDial& dial, const ui::Rect& box, const Geometry& sheet)
{
    /* What the caption column leaves, square, so the pair closes on the cell and the face the wedge is
       inscribed in is the rect that is painted. */
    const int side = std::min(box.w - sheet.dialColumn, box.h);

    caption.setBounds(box.x, box.y, box.w, box.h);
    dial.setBounds(box.x + sheet.dialColumn, box.y + (box.h - side) / 2, side, side);
}

void placeLevelStage(ui::Bar& amount, ui::Bar& floor, ui::Bar& meter, const ui::Rect& box,
                     const Geometry& sheet)
{
    /* The third row closes the cell's bottom edge, which is why the meter — the only read-only widget in the
       group — sits there rather than between the two controls it reports on. */
    amount.setBounds(box.x, box.y, box.w, sheet.barHeight);
    floor.setBounds(box.x, box.y + sheet.rowPitch, box.w, sheet.barHeight);
    meter.setBounds(box.x, box.y + 2 * sheet.rowPitch, box.w, sheet.barHeight);
}

void placeDisplayPanels(ui::ResponsePanel& response, ui::EnvelopePanel& envelopes, const ui::Grid& grid)
{
    /* A span is the grid's own object, so a panel's edges are its first and last box's edges and the gutter
       between them is inside the ink rather than a ground stripe through it. */
    const ui::Rect correction = grid.span(2, 1, 2);
    const ui::Rect levels = grid.span(2, 2, 2);

    response.setBounds(correction.x, correction.y, correction.w, correction.h);
    envelopes.setBounds(levels.x, levels.y, levels.w, levels.h);
}

void placeWindowSize(ui::Text& caption, ui::Text& grainLabel, ui::ChoiceBar& grain, ui::Text& hopLabel,
                     ui::ChoiceBar& hop, const ui::Rect& box, const Geometry& sheet)
{
    /* The pair is coupled by the 2×-hop floor and nothing on a bar itself says which is which, so each one is
       named by a `micro` word in the column to its left (D-031). */
    const int lower = box.bottom() - sheet.barHeight;
    const int column = sheet.choiceLabelColumn;

    caption.setBounds(box.x, box.y, box.w, sheet.captionBand);
    grainLabel.setBounds(box.x, lower - sheet.rowPitch, column, sheet.barHeight);
    grain.setBounds(box.x + column, lower - sheet.rowPitch, box.w - column, sheet.barHeight);
    hopLabel.setBounds(box.x, lower, column, sheet.barHeight);
    hop.setBounds(box.x + column, lower, box.w - column, sheet.barHeight);
}
}
