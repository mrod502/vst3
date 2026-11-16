#pragma once

#include "Grid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace ui
{
/** The dial's millimetres: a face of 55.4 × 57.8 mm inscribed in the cell with the rotated
    caption taking that cell's left edge, and the datum's radial. */
inline constexpr double kDialFaceWidthMm = 55.4;
inline constexpr double kDialFaceHeightMm = 57.8;

/** The datum's one ink mark. The mockup gives no width for it; it takes the sheet's own hairline, the
    0.8 mm the bar fill is inset by, because that is the only line weight this design has. */
inline constexpr double kDatumRadialWidthMm = 0.8;

/** Angles in this file are JUCE's: 0 at 3 o'clock and increasing clockwise, because screen y grows
    down. Any other convention makes the wedge run the wrong way for the law, which is criterion 8. */
inline constexpr double kHalfTurnRadians = std::numbers::pi;
inline constexpr double kFullTurnRadians = 2.0 * std::numbers::pi;
inline constexpr double kSixOClockRadians = 0.5 * std::numbers::pi;
inline constexpr double kTwelveOClockRadians = -0.5 * std::numbers::pi;

/** How a wedge grows from its datum.

    proportional is the mockup's own drawing rule — "how much of the range", from 6 o'clock, clockwise,
    one full turn for the range. datumAtZero is what a bipolar morph needs and what `blend` ships with:
    the wedge grows clockwise for positive and counter-clockwise for negative from a 12 o'clock datum, so
    the geometry alone says which side of centre the value is on. Half a turn each way, which puts either
    extreme at 6 o'clock and keeps the whole range at one turn, as proportional does. */
enum class PieMode
{
    proportional,
    datumAtZero,
};

/** A pie sector, from one angle to another. The signed difference is the direction of the sweep: positive
    clockwise, negative counter-clockwise, zero nothing to draw. */
struct WedgeAngles
{
    double fromRadians{};
    double toRadians{};

    [[nodiscard]] constexpr double sweepRadians() const noexcept { return toRadians - fromRadians; }

    constexpr bool operator==(const WedgeAngles&) const noexcept = default;
};

/** The two angles to hand `juce::Slider::setRotaryParameters`, which are also the two the paint hook is
    handed back.

    For proportional they are the datum and one turn past it, so the value's own angle is the wedge's
    leading edge. For datumAtZero the range is symmetric about zero and the angles bracket a full turn
    centred on the datum, which makes JUCE's linear value-to-angle map land 0 on 12 o'clock and each
    extreme on 6 o'clock — arriving from opposite sides, exactly as the wedge does. */
struct RotaryAngles
{
    double startRadians{};
    double endRadians{};

    constexpr bool operator==(const RotaryAngles&) const noexcept = default;
};

/** Where a mode's datum is. */
[[nodiscard]] constexpr double datumRadians(PieMode mode) noexcept
{
    return mode == PieMode::proportional ? kSixOClockRadians : kTwelveOClockRadians;
}

/** value is normalised for proportional (0…1) and signed for datumAtZero (−1…+1); either way it is
    clamped, because a value off the range must not draw a wedge past the rim.

    Monotone in value over the whole range in both modes, and deliberately not reduced modulo a turn: the
    angles grow past 2π rather than wrapping, which is what keeps a full-turn wedge drawable. */
[[nodiscard]] constexpr WedgeAngles wedgeAngles(double value, PieMode mode) noexcept
{
    const double datum = datumRadians(mode);

    if (mode == PieMode::proportional)
        return {datum, datum + kFullTurnRadians * std::clamp(value, 0.0, 1.0)};

    return {datum, datum + kHalfTurnRadians * std::clamp(value, -1.0, 1.0)};
}

[[nodiscard]] constexpr RotaryAngles rotaryAnglesFor(PieMode mode) noexcept
{
    if (mode == PieMode::proportional)
        return {kSixOClockRadians, kSixOClockRadians + kFullTurnRadians};

    return {kTwelveOClockRadians - kHalfTurnRadians, kTwelveOClockRadians + kHalfTurnRadians};
}

/** The centre of a face rect. */
[[nodiscard]] constexpr PointF centreOf(Rect face) noexcept
{
    return {static_cast<double>(face.x) + face.w * 0.5, static_cast<double>(face.y) + face.h * 0.5};
}

/** The pie's radius: the circle inscribed in the face, so the 55.4 × 57.8 mm face leaves the odd
    millimetre of slack the mockup leaves rather than squashing the wedge into an ellipse. */
[[nodiscard]] constexpr double radiusFor(Rect face) noexcept
{
    return static_cast<double>(std::min(face.w, face.h)) * 0.5;
}

/** A point is on the face if it is inside the inscribed circle — which rejects the face's corners, and is
    the difference between a dial and a square that happens to have a wedge in it. */
[[nodiscard]] constexpr bool hitTestPie(PointF point, PointF centre, double radius) noexcept
{
    const double dx = point.x - centre.x;
    const double dy = point.y - centre.y;
    return dx * dx + dy * dy <= radius * radius;
}

/** The square the dial's disc is inscribed in: centred in the face, one side to the rim, so a widget rect
    that is not square still gets a round dial.

    This is what the skin fills, and the same circle `hitTestPie` accepts: the shape the sheet shows and the
    shape the pointer takes are one answer. Filling the widget's rect instead — which the skin did until
    2026-11-15 — paints `track` into four corners the mockup leaves ground and the hit test refuses, which is
    the dial reading as a blue block, and it is what "the dial is inscribed in the face" was always saying. */
[[nodiscard]] constexpr Rect discRectOf(Rect face) noexcept
{
    const double radius = radiusFor(face);
    const PointF centre = centreOf(face);

    return {static_cast<int>(std::lround(centre.x - radius)),
            static_cast<int>(std::lround(centre.y - radius)),
            static_cast<int>(std::lround(radius * 2.0)), static_cast<int>(std::lround(radius * 2.0))};
}

/** The same outline walked rather than collected: the centre, then the arc's `steps` + 1 points.

    A callback because the skin walks this from `paint()`, where the vector below would be an allocation
    per frame — `02` §4 criterion 10's rule, reaching the message thread. */
template <class Visitor>
void forEachWedgePoint(PointF centre, double radius, WedgeAngles angles, int steps, Visitor&& visit)
{
    visit(centre);

    if (radius <= 0.0 || angles.sweepRadians() == 0.0 || steps < 1)
        return;

    for (int step = 0; step <= steps; ++step)
    {
        const double t = static_cast<double>(step) / static_cast<double>(steps);
        const double angle = angles.fromRadians + angles.sweepRadians() * t;
        visit(PointF{centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)});
    }
}

/** The sector as a point list: the centre, then the arc from `from` to `to` in steps segments, closing
    back to the centre in the caller's path. An empty sweep yields the centre alone, which fills nothing.

    A list of points rather than a juce::Path so the monotonicity and closure legs run in a console
    program: the skin's job is to fill the answer, not to be the arithmetic. This is
    `forEachWedgePoint` with a vector behind it, so the walked and collected outlines cannot disagree. */
[[nodiscard]] inline std::vector<PointF> wedgePolygon(PointF centre, double radius,
                                                      WedgeAngles angles, int steps = 64) noexcept
{
    std::vector<PointF> polygon;
    forEachWedgePoint(centre, radius, angles, steps,
                      [&polygon](PointF point) { polygon.push_back(point); });

    return polygon;
}

/** The datum radial: the four corners of the mark from the centre out to the rim along `angle`, `width`
    px across. A quad rather than a stroked line because a stroke is a renderer's decision about caps and
    joins, and this mark is a rectangle whose width the mockup measured. */
[[nodiscard]] inline std::array<PointF, 4> radialQuad(PointF centre, double radius, double angle,
                                                     double width) noexcept
{
    const double ux = std::cos(angle);
    const double uy = std::sin(angle);
    const double half = std::max(0.0, width) * 0.5;
    const double nx = -uy * half;
    const double ny = ux * half;
    const PointF rim{centre.x + ux * radius, centre.y + uy * radius};

    return {{PointF{centre.x + nx, centre.y + ny}, PointF{rim.x + nx, rim.y + ny},
             PointF{rim.x - nx, rim.y - ny}, PointF{centre.x - nx, centre.y - ny}}};
}

/** The dial's face size in px at window scale k. */
[[nodiscard]] inline int faceWidthAt(double k) noexcept
{
    return static_cast<int>(std::lround(kDialFaceWidthMm * k));
}

[[nodiscard]] inline int faceHeightAt(double k) noexcept
{
    return static_cast<int>(std::lround(kDialFaceHeightMm * k));
}

/** The datum radial's width in px at k — never thinner than a pixel, because a sub-pixel ink line is
    antialiased away to a tint, and this is the one mark on a control that has to stay legible. */
[[nodiscard]] inline int datumRadialWidthAt(double k) noexcept
{
    return std::max(1, static_cast<int>(std::lround(kDatumRadialWidthMm * k)));
}
}
