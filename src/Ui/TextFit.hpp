#pragma once

#include "BarGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ui
{
/** What one run of type occupies, as figures.

    The skin measures these (`GlyphArrangement` lives on the JUCE side of the seam); this header only decides
    what they mean, which is what makes the verdict reachable from `UiTests` and keeps `paint()` out of measuring. */
struct TextExtent
{
    /** One line laid out end to end, the digit pitch included. */
    double advancePx{};

    /** Ink above the baseline: the cap top for a caps-only run. Never `getAscent()`, which on this face is
        0.890 em against a 0.670 em cap (F-030). */
    double capPx{};

    /** Ink below the baseline, positive. Zero for a run with no descender. */
    double descentPx{};

    /** Contains a space, so an over-wide line wraps instead of overflowing. */
    bool hasBreakPoint{};

    constexpr bool operator==(const TextExtent&) const noexcept = default;
};

/** The band a single-line word needs: its ink, top to bottom, rounded up. */
[[nodiscard]] inline int reserveFor(const TextExtent& extent) noexcept
{
    return static_cast<int>(std::ceil(extent.capPx + extent.descentPx));
}

/** The reserve a stack of `lineCount` lines needs: the first line pays its own ink, every later one one line
    pitch. The pitch stays unrounded until this sum — rounding it per line charges the box for ink it never
    draws (F-024 item 2). */
[[nodiscard]] inline int stackReservePx(double inkPx, int lineCount, double lineHeightPx) noexcept
{
    return static_cast<int>(std::ceil(inkPx + static_cast<double>(std::max(lineCount - 1, 0)) * lineHeightPx));
}

/** Whether the run's own advance fits `boxWidthPx`. Exactly zero slack: the sheet has no ellipsis and no
    shrink-to-fit, so a run that is a pixel wide of its box loses a glyph. */
[[nodiscard]] constexpr bool fitsWidth(const TextExtent& extent, int boxWidthPx) noexcept
{
    return static_cast<int>(std::ceil(extent.advancePx)) <= boxWidthPx;
}

/** The label column of a bar row: the millimetre figure is the floor and the words are the fact, so the column
    is the wider of the two plus the 1 px the value needs to clear it. */
[[nodiscard]] inline int labelColumnFor(double widestLabelAdvancePx, double k) noexcept
{
    const int measured = static_cast<int>(std::ceil(widestLabelAdvancePx)) + 1;
    return std::max(BarGeometry::labelColumnAt(k), measured);
}

/** Whether a bar's label clears its value: the value starts at the column's right edge plus the inset, so the
    label needs the column to be its own width plus 1 px. */
[[nodiscard]] constexpr bool labelClearsValue(double labelAdvancePx, int labelColumnPx) noexcept
{
    return static_cast<int>(std::ceil(labelAdvancePx)) + 1 <= labelColumnPx;
}

/** Which characters take the fixed pitch: the digits.

    Punctuation keeps its own advance — every readout that must hold still changes only in digits, and pitching
    `.` and `/` to the digit width widens the prose for stability nothing asked for (D-037). */
[[nodiscard]] constexpr bool isFixedPitchCharacter(char32_t codepoint) noexcept
{
    return codepoint >= U'0' && codepoint <= U'9';
}
}
