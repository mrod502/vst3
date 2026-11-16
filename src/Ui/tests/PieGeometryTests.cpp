#include <Grid.hpp>
#include <PieGeometry.hpp>

#include <catch_amalgamated.hpp>

#include <cmath>
#include <numbers>

/* The wedge's arithmetic. Criterion 8 is decided here and in the probe: the datum is where the blend
   law says it is, the wedge grows the way the law says it grows, and the geometry a `juce::Path` is
   built from closes. Angles are JUCE's — 0 at 3 o'clock, clockwise — and these legs are written in that
   convention on purpose, so a sign flip reddens something. */
namespace
{
constexpr double kReferenceK = 1190.0 / 315.0;
constexpr double kPi = std::numbers::pi;

/** Normalised to [0, 2π) so two angles that differ by a turn compare equal. */
double turn(double radians)
{
    const double wrapped = std::fmod (radians, ui::kFullTurnRadians);
    return wrapped < 0.0 ? wrapped + ui::kFullTurnRadians : wrapped;
}

double angleOf(const ui::PointF& from, const ui::PointF& point)
{
    return std::atan2 (point.y - from.y, point.x - from.x);
}

ui::Rect faceAt(double k)
{
    return {0, 0, ui::faceWidthAt (k), ui::faceHeightAt (k)};
}
}

TEST_CASE("ui: dial: the wedge is monotone in value across the full turn")
{
    using ui::PieMode;

    double previous = -1.0;

    for (int step = 0; step <= 200; ++step)
    {
        const double value = static_cast<double> (step) / 200.0;
        const ui::WedgeAngles angles = ui::wedgeAngles (value, PieMode::proportional);

        INFO("value " << value << " sweeps " << angles.sweepRadians ());
        CHECK(angles.sweepRadians () > previous);
        previous = angles.sweepRadians ();

        CHECK(angles.fromRadians == ui::kSixOClockRadians);
        CHECK(angles.toRadians == ui::kSixOClockRadians + ui::kFullTurnRadians * value);
    }

    SECTION("no wedge at 0, one full turn at 1, a quarter at a quarter")
    {
        CHECK(ui::wedgeAngles (0.0, PieMode::proportional).sweepRadians () == 0.0);
        CHECK(ui::wedgeAngles (1.0, PieMode::proportional).sweepRadians ()
              == Catch::Approx (ui::kFullTurnRadians));
        CHECK(ui::wedgeAngles (0.25, PieMode::proportional).sweepRadians ()
              == Catch::Approx (ui::kHalfTurnRadians * 0.5));
    }

    SECTION("past 270° the angles keep growing rather than wrapping")
    {
        /* 0.75 of a turn is 270°, and the wrap bug §5 arm 10 predicts bites between there and the top
           of the range: an angle reduced modulo a turn would make the wedge shrink as the value grows. */
        const double from270 = ui::wedgeAngles (0.75, PieMode::proportional).toRadians;

        for (const double value : {0.8, 0.9, 0.99, 1.0})
            CHECK(ui::wedgeAngles (value, PieMode::proportional).toRadians > from270);

        CHECK(ui::wedgeAngles (1.0, PieMode::proportional).toRadians
              == Catch::Approx (ui::kSixOClockRadians + ui::kFullTurnRadians));
    }

    SECTION("and in datumAtZero across the signed range")
    {
        double lastSweep = -1.0 * ui::kFullTurnRadians;

        for (int step = 0; step <= 200; ++step)
        {
            const double value = -1.0 + static_cast<double> (step) / 100.0;
            const ui::WedgeAngles angles = ui::wedgeAngles (value, PieMode::datumAtZero);

            INFO("p = " << value << " sweeps " << angles.sweepRadians ());
            CHECK(angles.sweepRadians () > lastSweep);
            lastSweep = angles.sweepRadians ();
        }
    }
}

TEST_CASE("ui: dial: datumAtZero has no wedge at zero and mirrors its sign")
{
    using ui::PieMode;

    SECTION("the datum is 12 o'clock here and 6 o'clock in proportional")
    {
        CHECK(ui::datumRadians (PieMode::datumAtZero) == ui::kTwelveOClockRadians);
        CHECK(ui::datumRadians (PieMode::proportional) == ui::kSixOClockRadians);

        /* 12 o'clock is straight up, which in screen coordinates (y down) is the negative y axis, and
           6 o'clock the positive one. Restated as a direction so a sign convention cannot slip. */
        CHECK(ui::datumRadians (PieMode::datumAtZero) == Catch::Approx (-kPi / 2.0));
        CHECK(ui::datumRadians (PieMode::proportional) == Catch::Approx (kPi / 2.0));
    }

    SECTION("p = 0 sweeps nothing")
    {
        const ui::WedgeAngles zero = ui::wedgeAngles (0.0, PieMode::datumAtZero);
        CHECK(zero.sweepRadians () == 0.0);
        CHECK(zero.fromRadians == zero.toRadians);
    }

    SECTION("p = ±0.5 are mirror images, and so are the extremes")
    {
        for (const double magnitude : {0.1, 0.25, 0.5, 0.75, 1.0})
        {
            const ui::WedgeAngles up = ui::wedgeAngles (magnitude, PieMode::datumAtZero);
            const ui::WedgeAngles down = ui::wedgeAngles (-magnitude, PieMode::datumAtZero);

            INFO("magnitude " << magnitude);
            CHECK(up.sweepRadians () == Catch::Approx (-down.sweepRadians ()));
            CHECK(up.sweepRadians () > 0.0);
            CHECK(down.sweepRadians () < 0.0);
            CHECK(up.fromRadians == down.fromRadians);
        }
    }

    SECTION("clockwise for positive, counter-clockwise for negative")
    {
        /* From the 12 o'clock datum, +1 reaches 6 o'clock by way of 3 o'clock and −1 by way of 9. */
        const ui::WedgeAngles full = ui::wedgeAngles (1.0, PieMode::datumAtZero);
        const ui::WedgeAngles empty = ui::wedgeAngles (-1.0, PieMode::datumAtZero);

        CHECK(turn (full.toRadians) == Catch::Approx (turn (ui::kSixOClockRadians)).margin (1e-9));
        CHECK(turn (empty.toRadians) == Catch::Approx (turn (ui::kSixOClockRadians)).margin (1e-9));

        CHECK(ui::wedgeAngles (0.5, PieMode::datumAtZero).toRadians == Catch::Approx (0.0));
        CHECK(turn (ui::wedgeAngles (-0.5, PieMode::datumAtZero).toRadians)
              == Catch::Approx (turn (kPi)).margin (1e-9));
    }

    SECTION("a value off the range is clamped, not drawn past the rim")
    {
        CHECK(ui::wedgeAngles (7.0, PieMode::datumAtZero).sweepRadians ()
              == Catch::Approx (ui::kHalfTurnRadians));
        CHECK(ui::wedgeAngles (-7.0, PieMode::datumAtZero).sweepRadians ()
              == Catch::Approx (-ui::kHalfTurnRadians));
    }
}

TEST_CASE("ui: dial: the rotary angles bracket what the modes claim")
{
    using ui::PieMode;

    SECTION("proportional runs from 6 o'clock for one full turn")
    {
        const ui::RotaryAngles angles = ui::rotaryAnglesFor (PieMode::proportional);
        CHECK(angles.startRadians == ui::kSixOClockRadians);
        CHECK(angles.endRadians - angles.startRadians == Catch::Approx (ui::kFullTurnRadians));
    }

    SECTION("datumAtZero centres a full turn on 12 o'clock")
    {
        const ui::RotaryAngles angles = ui::rotaryAnglesFor (PieMode::datumAtZero);
        const double middle = (angles.startRadians + angles.endRadians) * 0.5;

        CHECK(middle == Catch::Approx (ui::kTwelveOClockRadians));
        CHECK(angles.endRadians - angles.startRadians == Catch::Approx (ui::kFullTurnRadians));

        /* JUCE maps a value linearly onto these two angles, so the value's own angle is the wedge's
           leading edge — and this is the leg §5 arm 6 (datum 12 o'clock → 6 o'clock) reddens. */
        const auto angleFor = [angles](double value)
        {
            const double normalised = (value + 1.0) * 0.5;
            return angles.startRadians + (angles.endRadians - angles.startRadians) * normalised;
        };

        CHECK(angleFor (0.0) == Catch::Approx (ui::kTwelveOClockRadians));
        CHECK(turn (angleFor (1.0)) == Catch::Approx (turn (ui::kSixOClockRadians)).margin (1e-9));
        CHECK(turn (angleFor (-1.0)) == Catch::Approx (turn (ui::kSixOClockRadians)).margin (1e-9));
        CHECK(angleFor (0.5) == Catch::Approx (0.0));
        CHECK(turn (angleFor (-0.5)) == Catch::Approx (turn (kPi)).margin (1e-9));
    }
}

TEST_CASE("ui: dial: the wedge polygon closes and stays on the rim")
{
    using ui::PieMode;

    const ui::Rect face = faceAt (kReferenceK);
    const ui::PointF centre = ui::centreOf (face);
    const double radius = ui::radiusFor (face);

    REQUIRE(face.w == 209);
    REQUIRE(face.h == 218);
    CHECK(radius == Catch::Approx (104.5));
    CHECK(centre.x == Catch::Approx (104.5));
    CHECK(centre.y == Catch::Approx (109.0));

    SECTION("an empty sweep is the centre alone, which fills nothing")
    {
        CHECK(ui::wedgePolygon (centre, radius, ui::wedgeAngles (0.0, PieMode::proportional)).size ()
              == 1);
        CHECK(ui::wedgePolygon (centre, radius, ui::wedgeAngles (0.0, PieMode::datumAtZero)).size ()
              == 1);
    }

    for (const double value : {0.2, 0.5, 1.0, -0.4, -1.0})
    {
        const ui::PieMode mode = value >= 0.0 ? PieMode::proportional : PieMode::datumAtZero;
        const ui::WedgeAngles angles = ui::wedgeAngles (std::abs (value), mode);
        const auto polygon = ui::wedgePolygon (centre, radius, angles, 48);

        INFO("value " << value << " in " << (mode == PieMode::proportional ? "proportional"
                                                                           : "datumAtZero"));
        REQUIRE(polygon.size () == 50);

        SECTION("starts on the datum, ends on the leading edge, closes back to the centre")
        {
            CHECK(polygon.front () == centre);
            CHECK(angleOf (centre, polygon[1]) == Catch::Approx (angles.fromRadians).margin (1e-9));
            CHECK(angleOf (centre, polygon.back ())
                  == Catch::Approx (angles.toRadians).margin (1e-9));
        }

        SECTION("every arc point is on the rim, and none of them is the centre")
        {
            for (std::size_t i = 1; i < polygon.size (); ++i)
            {
                const double dx = polygon[i].x - centre.x;
                const double dy = polygon[i].y - centre.y;
                CHECK(std::hypot (dx, dy) == Catch::Approx (radius).margin (1e-9));
            }
        }

        SECTION("the arc runs in the sweep's direction, monotonically")
        {
            const double expected = angles.sweepRadians () / 48.0;

            for (std::size_t i = 2; i < polygon.size (); ++i)
            {
                double delta = angleOf (centre, polygon[i]) - angleOf (centre, polygon[i - 1]);

                /* Wrapping into (−π, π] so the crossing of ±π reads as a step, not a jump of a turn. */
                while (delta > kPi)
                    delta -= ui::kFullTurnRadians;
                while (delta <= -kPi)
                    delta += ui::kFullTurnRadians;

                INFO("segment " << i << ": delta " << delta << ", expected " << expected);
                CHECK(std::abs (delta - expected) < 1e-9);
            }
        }
    }
}

TEST_CASE("ui: dial: the face is a circle, so its corners are refused")
{
    const ui::Rect face = faceAt (kReferenceK);
    const ui::PointF centre = ui::centreOf (face);
    const double radius = ui::radiusFor (face);

    CHECK(ui::hitTestPie (centre, centre, radius));
    CHECK(ui::hitTestPie ({centre.x, centre.y + radius - 1.0}, centre, radius));
    CHECK(ui::hitTestPie ({centre.x - radius + 1.0, centre.y}, centre, radius));

    SECTION("the four corners of the face rect are outside the pie")
    {
        CHECK_FALSE(ui::hitTestPie ({static_cast<double> (face.x), static_cast<double> (face.y)},
                                    centre, radius));
        CHECK_FALSE(ui::hitTestPie ({static_cast<double> (face.right () - 1),
                                     static_cast<double> (face.y)}, centre, radius));
        CHECK_FALSE(ui::hitTestPie ({static_cast<double> (face.x),
                                     static_cast<double> (face.bottom () - 1)}, centre, radius));
        CHECK_FALSE(ui::hitTestPie ({static_cast<double> (face.right () - 1),
                                     static_cast<double> (face.bottom () - 1)}, centre, radius));
    }

    SECTION("and one pixel outside the rim is outside")
    {
        CHECK_FALSE(ui::hitTestPie ({centre.x, centre.y + radius + 1.0}, centre, radius));
    }

    SECTION("the face is the measured 55.4 × 57.8 mm at three scales")
    {
        for (const double k : {1.0, 2.0, kReferenceK})
        {
            INFO("k = " << k);
            CHECK(ui::faceWidthAt (k) == static_cast<int> (std::lround (55.4 * k)));
            CHECK(ui::faceHeightAt (k) == static_cast<int> (std::lround (57.8 * k)));
            CHECK(ui::datumRadialWidthAt (k) >= 1);
        }

        CHECK(ui::faceWidthAt (kReferenceK) == 209);
        CHECK(ui::faceHeightAt (kReferenceK) == 218);
        CHECK(ui::datumRadialWidthAt (kReferenceK) == 3);
    }
}

TEST_CASE("ui: dial: the disc the skin fills is the circle the pointer takes")
{
    /* Added with the skin's own fix: the dial stopped filling its widget rect and started filling
       the inscribed disc, so the shape on the sheet is decided here rather than in a paint hook — and it is
       the same circle hitTestPie answers with, which is the whole reason the two are computed from one
       radiusFor rather than twice. */
    for (const double k : {1.0, 2.0, kReferenceK})
    {
        const ui::Rect face {40, 40, ui::faceWidthAt (k), ui::faceHeightAt (k)};
        const ui::Rect disc = ui::discRectOf (face);
        const ui::PointF centre = ui::centreOf (face);
        const double radius = ui::radiusFor (face);

        INFO("k = " << k << ", face " << face.w << "x" << face.h << ", disc "
                    << disc.x << "," << disc.y << " " << disc.w << "x" << disc.h);

        SECTION("one side to the rim, on the short axis, centred on the long one")
        {
            CHECK(disc.w == static_cast<int> (std::lround (2.0 * radius)));
            CHECK(disc.h == disc.w);
            CHECK(std::abs ((disc.x + disc.w * 0.5) - centre.x) <= 0.5);
            CHECK(std::abs ((disc.y + disc.h * 0.5) - centre.y) <= 0.5);
            CHECK(disc.x >= face.x);
            CHECK(disc.right() <= face.right ());
            CHECK(disc.y >= face.y);
            CHECK(disc.bottom() <= face.bottom ());
        }

        SECTION("and the shape the hit test accepts, edge for edge")
        {
            CHECK(ui::hitTestPie ({disc.x + 1.0, centre.y}, centre, radius));
            CHECK(ui::hitTestPie ({disc.right () - 1.0, centre.y}, centre, radius));
            CHECK(ui::hitTestPie ({centre.x, disc.y + 1.0}, centre, radius));
            CHECK(ui::hitTestPie ({centre.x, disc.bottom () - 1.0}, centre, radius));
            CHECK_FALSE(ui::hitTestPie ({static_cast<double> (disc.x), static_cast<double> (disc.y)},
                                        centre, radius));
            CHECK_FALSE(ui::hitTestPie ({static_cast<double> (disc.right () - 1),
                                         static_cast<double> (disc.bottom () - 1)}, centre, radius));
        }
    }

    SECTION("a square face leaves nothing to inset")
    {
        const ui::Rect square {0, 0, 100, 100};
        CHECK(ui::discRectOf (square) == square);
    }

    SECTION("the reference sheet's dial is 209 px of disc in a 209 × 218 face")
    {
        const ui::Rect face {40, 40, ui::faceWidthAt (kReferenceK), ui::faceHeightAt (kReferenceK)};
        const ui::Rect disc = ui::discRectOf (face);

        CHECK(disc.w == 209);
        CHECK(disc.y > face.y);
        CHECK(disc.bottom () < face.bottom ());
    }
}
