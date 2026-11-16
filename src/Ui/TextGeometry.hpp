#pragma once

#include "Grid.hpp"
#include "Palette.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ui
{
/** A caption set into the edge of a cell reads bottom-to-top. The mockup draws "Vibes" at +91° and the
    rest at −90°; the rule is −90° and the disagreement is recorded in spec §2 rather than harmonised, so
    this constant is the rule and the +91° figure is a measurement of one hand-drawn element. */
inline constexpr double kRotationRadians = -0.5 * std::numbers::pi;

/** How far a rotated caption sits inside the cell whose edge it names. Like the ink panel's inset, the
    mockup sets its edge captions by eye; this is the same 2 mm the sheet's own edge furniture takes,
    declared once so a caption and the panel beside it cannot drift apart. */
inline constexpr double kRotatedCaptionInsetMm = 2.0;

enum class TextOrientation
{
    horizontal,
    rotated,  ///< −90°: kRotationRadians, reading bottom-to-top.
};

/** Horizontal placement of a word inside the box the caller gave it. Three, because the sheet has three:
    a caption reads from the left, a figure sits in the middle of its node, and a bar's label hangs off the
    right of nothing — it is the leftmost thing in its row and still reads as belonging to the bar. */
enum class TextAlignment
{
    left,
    centre,
    right,
};

/** Which of the design's two text modes a word is drawn in. There is no third: black is the only dark
    value in the mockup and there is no white, so every surface picks one of these two and a word can only
    be ink on a tint or a tint on ink. */
enum class TextSurface
{
    tinted,   ///< the word is ink: on ground, on a track body, on a fill.
    inkField, ///< the word is a tint: inside an ink display panel.
};

/** The tint that carries words inside an ink field.

    Spec §1 rule 1 requires *a* tint and does not name which, and the mockup's black panels hold traces
    and node numbers rather than prose, so the choice belongs to the kit rather than to the drawing.
    ground is the answer because it is the sheet's own surface colour, which keeps every ink field's words
    inside the four tints already measured and never asks for a fifth. */
inline constexpr std::uint32_t kInkFieldWordToken = kGround;

/** The token a word takes on a given surface — the only place the kit decides what colour type is. */
[[nodiscard]] constexpr std::uint32_t wordToken(TextSurface surface) noexcept
{
    return surface == TextSurface::inkField ? kInkFieldWordToken : kInk;
}

/** Where a rotated caption goes, in the frame the skin rotates into.

    A −90° rotation about a pivot sends local +x up the screen and local −y to the screen's left, so a
    line drawn with its cap top at local (0, 0) lands with its caps inset from the cell's left edge and
    its near end on the cell's bottom. The pivot is therefore the caption's bottom-left corner as the
    reader sees it, and the extent is the room the line has to grow upward into — the two numbers a
    rotated caption needs and the only two the skin is allowed to compute nothing about. */
struct RotatedPlacement
{
    PointF pivot;
    int extentPx{};

    constexpr bool operator==(const RotatedPlacement&) const noexcept = default;
};

/** The rotated caption's inset in px at window scale k. */
[[nodiscard]] inline int rotatedCaptionInsetAt(double k) noexcept
{
    return static_cast<int>(std::lround(kRotatedCaptionInsetMm * k));
}

/** The placement for a caption set into the left edge of a cell — the edge nearest the thing it names,
    which for every rotated caption in the mockup is the left one. */
[[nodiscard]] inline RotatedPlacement rotatedPlacementFor(Rect cell, double k) noexcept
{
    const int inset = rotatedCaptionInsetAt(k);
    return {{static_cast<double>(cell.x + inset), static_cast<double>(cell.bottom() - inset)},
            std::max(0, cell.h - 2 * inset)};
}
}
