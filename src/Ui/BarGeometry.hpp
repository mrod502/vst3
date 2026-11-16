#pragma once

#include "Grid.hpp"

#include <algorithm>
#include <cmath>

namespace ui
{
/** The bar's millimetres, measured off the mockup: a 17.7 mm body in every box that has bars,
    a value fill inset 0.8 mm from the body's top and bottom and flush to its left edge, the 19.8 mm
    label column of box 6 (label 74.8 px + body 183.6 px = 258.4 inside a 264.6 px cell), and the ~1 mm
    the value words sit off the body's left edge. */
inline constexpr double kBarBodyHeightMm = 17.7;
inline constexpr double kBarFillInsetMm = 0.8;
inline constexpr double kBarLabelColumnMm = 19.8;
inline constexpr double kBarValueInsetMm = 1.0;

/** Whether a point lies in a rect — the light half's hit test, so a widget's refusal of a pointer is one
    line over an answer a console program can check. */
[[nodiscard]] constexpr bool contains(Rect area, int x, int y) noexcept
{
    return x >= area.x && x < area.right() && y >= area.y && y < area.bottom();
}

/** The design's one quantisation expression: the nearest multiple of the quantum to a value.

    Every parameter quantum in the product is dyadic — blendStep 1/128, outputGainStepDb and
    silenceFloorStepDb 0.125 — which is the whole point of F-010: the division and the multiply are exact
    in binary, so a value that lands on the grid stays there through this expression, and a widget whose
    range carries the same interval cannot disagree with it. A quantum of 0 means a continuous range and
    returns the value untouched rather than inventing a grid.

    UiProbe's drag legs assert that the value a widget holds after a click equals this function's answer
    for the same pixel: that identity is the F-010 non-regression check, and it is the reason the
    expression exists in the light half instead of inside a mouse handler. */
[[nodiscard]] inline double snapToQuantum(double value, double quantum) noexcept
{
    if (!(quantum > 0.0))
        return value;

    return std::round(value / quantum) * quantum;
}

/** A bar body, its value and its state: the arithmetic behind every pixel of a bar, and behind every
    pixel the probe predicts for one.

    value is normalised over the parameter's range, so the bar knows nothing about decibels; the widget
    that owns a range converts. The body is the widget's own bounds — JUCE's track area for a horizontal
    slider with no thumb radius *is* its bounds, which is what makes a click at the body's centre the
    range's midpoint and not the midpoint ± half a thumb. */
struct BarGeometry
{
    Rect body;

    /** round(0.8 mm × k): the one place the fill inset becomes an integer, held here so the skin and the
        probe predict the same row. */
    int insetPx{};

    /** Normalised over the range, clamped: 0 draws nothing, 1 draws the body's full width. */
    double value{};

    /** A meter: no active state, and no hit rect. */
    bool readOnly{};

    /** The body height in px for window scale k — 67 px at the reference k, 18 px at k = 1. */
    [[nodiscard]] static int bodyHeightAt(double k) noexcept
    {
        return static_cast<int>(std::lround(kBarBodyHeightMm * k));
    }

    /** The label column of box 6, in px at k. */
    [[nodiscard]] static int labelColumnAt(double k) noexcept
    {
        return static_cast<int>(std::lround(kBarLabelColumnMm * k));
    }

    /** The value words' inset from the body's left edge, in px at k. */
    [[nodiscard]] static int valueInsetAt(double k) noexcept
    {
        return static_cast<int>(std::lround(kBarValueInsetMm * k));
    }

    /** The fill's inset for a window scale: applied once, here, and never re-rounded downstream. */
    [[nodiscard]] static int insetAt(double k) noexcept
    {
        return static_cast<int>(std::lround(kBarFillInsetMm * k));
    }

    [[nodiscard]] static BarGeometry forBody(Rect bodyToDraw, double k, double valueToDraw,
                                             bool readOnlyBody = false) noexcept
    {
        return {bodyToDraw, insetAt(k), std::clamp(valueToDraw, 0.0, 1.0), readOnlyBody};
    }

    /** The value fill at rest: flush left, inset top and bottom, so 2·inset of the body's 17.7 mm reads
        as the sheet behind the value. At value 0 the fill is empty; at 1 it is the body's width and
        body-height minus the inset. */
    [[nodiscard]] Rect fillRect() const noexcept { return fillRect(false); }

    /** The active state in full — the fill goes flush, and nothing else changes. A read-only body has no
        active state, so it stays inset however it is asked. */
    [[nodiscard]] Rect fillRect(bool active) const noexcept
    {
        const int inset = (active && !readOnly) ? 0 : insetPx;
        const int height = std::max(0, body.h - 2 * inset);
        return {body.x, body.y + inset, widthPx(), height};
    }

    /** The box for the value words: the body's height, inset ~1 mm from its left edge. */
    [[nodiscard]] Rect valueTextRect(double k) const noexcept
    {
        const int inset = valueInsetAt(k);
        return {body.x + inset, body.y, std::max(0, body.w - inset), body.h};
    }

    /** Normalised value for a pixel: position-proportional across the body, with no thumb-offset concept,
        because there is no thumb. A pixel left of the body is 0 and one right of it is 1. */
    [[nodiscard]] double valueForX(int x) const noexcept
    {
        if (body.w <= 0)
            return 0.0;

        return std::clamp(static_cast<double>(x - body.x) / static_cast<double>(body.w), 0.0, 1.0);
    }

    /** The pixel a normalised value lands on — the inverse of valueForX up to the pixel roundings, which
        is why the round-trip legs carry a half-pixel tolerance rather than none. */
    [[nodiscard]] int xForValue(double valueToDraw) const noexcept
    {
        return body.x + static_cast<int>(std::lround(body.w * std::clamp(valueToDraw, 0.0, 1.0)));
    }

    /** What a pointer may act on. A read-only bar is a meter, so it has no hit rect and the pointer goes
        through it: a control that answers to a drag while claiming to show the engine's value is a
        control that lies about being a control. */
    [[nodiscard]] constexpr Rect hitRect() const noexcept { return readOnly ? Rect{} : body; }

private:
    [[nodiscard]] int widthPx() const noexcept
    {
        return static_cast<int>(std::lround(body.w * value));
    }
};
}
