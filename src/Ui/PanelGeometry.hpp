#pragma once

#include "Grid.hpp"
#include "Palette.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ui
{
/** The ink display panel's content inset. The mockup draws its black panels as fields with type set in
    from the edge and gives no figure for the gap, so this takes the sheet's own content unit — one
    gutter — rather than a new number, and both panels and every future one read it from here.
    Re-pointing it is this line and nothing else. */
inline constexpr double kInkPanelContentInsetMm = kGutterMm;

/** The inset in px at window scale k. */
[[nodiscard]] inline int inkPanelInsetAt(double k) noexcept
{
    return static_cast<int>(std::lround(kInkPanelContentInsetMm * k));
}

/** What is drawn inside an ink panel: the panel's rect, inset once, symmetric on both axes. Everything
    inside is a tint, because the panel inverts the sheet's rule (spec §1 rule 1) and a tint on ink is the
    only other mode the palette has. */
[[nodiscard]] inline Rect inkContentRect(Rect panel, double k) noexcept
{
    const int inset = inkPanelInsetAt(k);
    return {panel.x + inset, panel.y + inset, std::max(0, panel.w - 2 * inset),
            std::max(0, panel.h - 2 * inset)};
}

/** The scrolling window both display panels show, in seconds. The ring above the seam is sized against
    this figure, so a panel never asks for history its publication cannot hold. */
inline constexpr double kTraceWindowSeconds = 2.0;

/** The tint the one rule inside an ink field takes. Spec §2 calls it "the ink rule", and inside an ink field
    that name is the inverted idiom — tints are the ink there (spec §1 rule 1) — so the rule takes the field's
    word tint rather than ink on ink, which is invisible. `ground` keeps it clear of the three trace tints
    without asking the palette for a sixth. */
inline constexpr std::uint32_t kInkFieldRuleToken = kGround;

/** Upper bound on the columns one trace is decimated into. The widest ink field the sheet can produce is a
    two-cell span at the resize ceiling — `Grid (2380, 1588).span (2, 1, 2)` is 1096 px, less one content
    inset at each edge — which is 1020 px, and this is that figure rounded up to the next ten. Every trace
    buffer is this size and none of them grows, so neither a resize nor a busy transport can allocate. */
inline constexpr std::size_t kMaxTraceColumns = 1030;

/** Upper bound on the per-hop history a panel holds between layout passes: `kTraceWindowSeconds` at the
    fastest hop and highest rate the shell can be configured with — 2 s at H = 64 and 192 kHz is 6000 hops.
    A host faster than that gets a trace that stops short of the panel's left edge rather than a buffer that
    grows: the x axis comes from each reading's age, so a short trace is honest and a long buffer is not. */
inline constexpr std::size_t kMaxTraceHistory = 6000;

/** The trace's thickness in device pixels: one column of the sheet's own pixel grid at the reference width,
    two at the resize ceiling. Not a millimetre figure — one column is the narrowest thing the grid can
    draw, and the mockup's curves are hand-drawn hairlines with no measurement to inherit. */
[[nodiscard]] inline int traceThicknessAt(double k) noexcept
{
    return std::max(1, static_cast<int>(std::lround(k / kReferenceScale)));
}

/** px per second for `widthPx` of panel showing `windowSeconds` of history. */
[[nodiscard]] inline double tracePixelsPerSecond(int widthPx, double windowSeconds) noexcept
{
    return windowSeconds > 0.0 ? static_cast<double>(widthPx) / windowSeconds : 0.0;
}

/** The response panel's y axis: ±`rangeDb`. The bound is handed in from the level stage rather than named
    here, because `src/Ui` owns no DSP figure and criterion 5's leg moves the axis by moving this input. */
struct CorrectionAxis
{
    double rangeDb = 0.0;
};

/** The envelope panel's y axis: dBFS between a floor and a ceiling, both handed in — the floor is the live
    gate parameter, which is the difference between a rule and a photograph. */
struct EnvelopeAxis
{
    double floorDb = 0.0;
    double ceilingDb = 0.0;
};

/** y for a correction in dB on a symmetric axis: 0 dB on the area's middle line, +range on its top edge.
    Clamped, because a reading the axis cannot hold is a reading the panel must not place outside itself. */
[[nodiscard]] inline int correctionYAt(double decibels, CorrectionAxis axis, Rect area) noexcept
{
    if (axis.rangeDb <= 0.0)
        return area.y + area.h / 2;

    const double ratio = std::clamp(decibels / axis.rangeDb, -1.0, 1.0);
    return area.y + static_cast<int>(std::lround(area.h * 0.5 * (1.0 - ratio)));
}

/** y for a level in dBFS: the floor on the area's bottom edge, the ceiling on its top. */
[[nodiscard]] inline int envelopeYAt(double decibels, EnvelopeAxis axis, Rect area) noexcept
{
    const double span = axis.ceilingDb - axis.floorDb;
    if (span <= 0.0)
        return area.bottom();

    const double ratio = std::clamp((decibels - axis.floorDb) / span, 0.0, 1.0);
    return area.bottom() - static_cast<int>(std::lround(ratio * area.h));
}

/** A linear envelope reading in dBFS. Silence returns the lowest finite double rather than an invented
    floor, so it reaches the axis's bottom edge through the same clamp as every other reading instead of
    resting on a number this library had to make up. */
[[nodiscard]] inline double amplitudeToDb(double amplitude) noexcept
{
    return amplitude > 0.0 ? 20.0 * std::log10(amplitude)
                           : std::numeric_limits<double>::lowest();
}

/** Which extreme one decimated column keeps. Two policies rather than a signed magnitude, because the two
    panels ask different questions: an envelope column wants the loudest moment inside it, and the
    correction panel wants its two extremes as two traces. */
enum class HoldPolicy
{
    maximum,
    minimum,
};

/** Peak-hold decimation of one series onto the panel's pixel columns.

    `values` is one reading per hop, oldest first, the newest pinned to the last column; column *c* takes
    the readings whose x falls in it and keeps the largest (or smallest) of them. A column no reading fell
    in carries the previous filled column's value — a step, never an invented midpoint.

    @return the index of the first column that received a reading, or `columnCount` when the series is empty
    or older than the window, which the caller renders as no trace at all rather than as a flat line. Every
    column from that index on holds a real reading: the NaN used mid-decimation never reaches the caller. */
[[nodiscard]] inline std::size_t decimateToColumns(const float* values, std::size_t count,
                                                   double secondsPerValue, double pixelsPerSecond,
                                                   HoldPolicy policy, float* out,
                                                   std::size_t columnCount) noexcept
{
    constexpr float kUnfilled = std::numeric_limits<float>::quiet_NaN();

    if (columnCount == 0)
        return 0;

    for (std::size_t column = 0; column < columnCount; ++column)
        out[column] = kUnfilled;

    if (pixelsPerSecond <= 0.0 || secondsPerValue <= 0.0)
        return columnCount;

    std::size_t firstFilled = columnCount;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double fromRight = static_cast<double>(count - 1 - i) * secondsPerValue * pixelsPerSecond;
        if (fromRight < 0.0 || fromRight >= static_cast<double>(columnCount))
            continue;

        const std::size_t column = columnCount - 1
                                   - static_cast<std::size_t>(std::floor(fromRight));
        const float reading = values[i];
        const bool untouched = std::isnan(out[column]);

        if (untouched)
            out[column] = reading;
        else if (policy == HoldPolicy::maximum)
            out[column] = std::max(out[column], reading);
        else
            out[column] = std::min(out[column], reading);

        if (column < firstFilled)
            firstFilled = column;
    }

    /* The carry-forward runs from the first fill, so a column between two readings takes the earlier
       one's value and the drawn trace steps rather than gapping; a leading run of empties stays before the
       returned index and is simply not drawn. */
    if (firstFilled < columnCount)
    {
        for (std::size_t column = firstFilled + 1; column < columnCount; ++column)
        {
            if (std::isnan(out[column]))
                out[column] = out[column - 1];
        }
    }

    return firstFilled;
}

/** The two extremes of a run of readings — the spread M9 row 1's open question is about, and the pair the
    response panel draws as its two traces. One reading gives two equal figures, which is the honest
    rendering of a single channel rather than a special case. */
struct Spread
{
    float minimum = 0.0f;
    float maximum = 0.0f;
};

[[nodiscard]] inline Spread spreadOf(const float* values, std::size_t count) noexcept
{
    if (count == 0)
        return {};

    Spread spread {values[0], values[0]};
    for (std::size_t i = 1; i < count; ++i)
    {
        spread.minimum = std::min(spread.minimum, values[i]);
        spread.maximum = std::max(spread.maximum, values[i]);
    }
    return spread;
}
}
