#include <BarGeometry.hpp>
#include <Grid.hpp>

#include <catch_amalgamated.hpp>

#include <cmath>
#include <numbers>

/* The bar's arithmetic, with no display and no JUCE. Every figure here is the mockup's,
   measured in mm and rounded once by the light half; the k values are the ones criterion 2 names —
   1, 2 and the reference 3.778 — because a rounding mistake is invisible at one scale and obvious at
   another.

   Case names carry the "ui: " prefix, which is this suite's filter contract: ctest -R "ui:" selects
   the whole of items 01 and 02, and a target name selects nothing at all. */
namespace
{
constexpr double kReferenceK = 1190.0 / 315.0;

/** A bar body as the sheet would place it at window width: full cell across, the measured height. */
ui::Rect bodyAt(int windowWidth)
{
    const ui::Grid grid(windowWidth, static_cast<int> (std::lround(windowWidth * 210.0 / 315.0)));
    return {grid.marginPx(), grid.marginPx(), grid.cellPx().w, ui::BarGeometry::bodyHeightAt (grid.k ())};
}
}

TEST_CASE("ui: bar: the measured geometry, at three scales")
{
    SECTION("at the reference k the mm figures land where spec §5 measured them")
    {
        CHECK(ui::BarGeometry::bodyHeightAt (kReferenceK) == 67);
        CHECK(ui::BarGeometry::insetAt (kReferenceK) == 3);
        CHECK(ui::BarGeometry::labelColumnAt (kReferenceK) == 75);
        CHECK(ui::BarGeometry::valueInsetAt (kReferenceK) == 4);

        /* Box 6's own arithmetic: a 19.8 mm label column beside a 48.6 mm body fits inside the
           70 mm cell with the gutter to spare, which is what makes the label column part of the
           bar's geometry rather than a second bar. */
        const int label = ui::BarGeometry::labelColumnAt (kReferenceK);
        const int barBody = static_cast<int> (std::lround (48.6 * kReferenceK));
        const ui::Rect cell = ui::Grid::reference ().box (6);
        INFO("label " << label << " + body " << barBody << " in a " << cell.w << " px cell");
        CHECK(label + barBody <= cell.w);
    }

    for (const double k : {1.0, 2.0, kReferenceK})
    {
        INFO("k = " << k);
        CHECK(ui::BarGeometry::bodyHeightAt (k) == static_cast<int> (std::lround (17.7 * k)));
        CHECK(ui::BarGeometry::insetAt (k) == static_cast<int> (std::lround (0.8 * k)));
        CHECK(ui::BarGeometry::labelColumnAt (k) == static_cast<int> (std::lround (19.8 * k)));
        CHECK(ui::BarGeometry::valueInsetAt (k) == static_cast<int> (std::lround (1.0 * k)));

        /* The inset means something at every scale: at k = 1 it is one row of pixels and at the
           reference k three, and it is never the 0 that would erase the state vocabulary. */
        CHECK(ui::BarGeometry::insetAt (k) >= 1);
    }
}

TEST_CASE("ui: bar: the fill inset is applied once, and symmetrically")
{
    for (const int width : {315, 630, 1190})
    {
        const ui::Grid grid (width, 210);
        const ui::BarGeometry bar =
            ui::BarGeometry::forBody (bodyAt (width), grid.k (), 0.645);
        const ui::Rect fill = bar.fillRect ();

        INFO("window " << width << " px, inset " << bar.insetPx);
        REQUIRE(bar.insetPx >= 1);

        /* Once, and on both edges: the fill's top edge is inset by the same rounded figure as its
           bottom edge is, so 2 · inset of the body reads as sheet. Top-only is the rounding mistake
           §7 predicts and §5 arm 2 mutates. */
        CHECK(fill.y - bar.body.y == bar.insetPx);
        CHECK(bar.body.bottom () - fill.bottom () == bar.insetPx);
        CHECK(fill.y - bar.body.y == bar.body.bottom () - fill.bottom ());
        CHECK(bar.body.h - fill.h == 2 * bar.insetPx);

        /* And the fill is inset from nothing else: flush to the body's left edge, because the value
           grows out of the left and a gap there would read as an empty margin on every bar. */
        CHECK(fill.x == bar.body.x);
    }
}

TEST_CASE("ui: bar: the fill is the body times the value")
{
    const ui::BarGeometry empty = ui::BarGeometry::forBody (bodyAt (1190), kReferenceK, 0.0);
    const ui::BarGeometry full = ui::BarGeometry::forBody (bodyAt (1190), kReferenceK, 1.0);
    const ui::BarGeometry mockup = ui::BarGeometry::forBody (bodyAt (1190), kReferenceK, 0.645);

    SECTION("value 0 draws nothing")
    {
        CHECK(empty.fillRect ().w == 0);
        CHECK(empty.fillRect (true).w == 0);
    }

    SECTION("value 1 is the body's width, minus the inset in height")
    {
        CHECK(full.fillRect ().w == full.body.w);
        CHECK(full.fillRect ().h == full.body.h - 2 * full.insetPx);
    }

    SECTION("0.645 — the ratio measured across every bar and the dial wedge alike")
    {
        CHECK(mockup.fillRect ().w == static_cast<int> (std::lround (0.645 * mockup.body.w)));
        CHECK(static_cast<double> (mockup.fillRect ().w)
              == Catch::Approx (0.645 * mockup.body.w).margin (0.5));
    }

    SECTION("a value off the range is clamped, not extrapolated past the body")
    {
        CHECK(ui::BarGeometry::forBody (empty.body, kReferenceK, -3.0).fillRect ().w == 0);
        CHECK(ui::BarGeometry::forBody (full.body, kReferenceK, 4.0).fillRect ().w == full.body.w);
    }

    SECTION("and the fill never leaves the body, at any value or scale")
    {
        for (const int width : {315, 630, 1190})
        {
            const ui::Grid grid (width, 210);
            const ui::Rect body = bodyAt (width);

            for (int step = 0; step <= 20; ++step)
            {
                const ui::BarGeometry bar =
                    ui::BarGeometry::forBody (body, grid.k (), static_cast<double> (step) / 20.0);
                const ui::Rect rest = bar.fillRect ();
                const ui::Rect active = bar.fillRect (true);

                REQUIRE(rest.w <= body.w);
                REQUIRE(rest.right () <= body.right ());
                REQUIRE(rest.y >= body.y);
                REQUIRE(rest.bottom () <= body.bottom ());
                REQUIRE(active.y == body.y);
                REQUIRE(active.bottom () == body.bottom ());
            }
        }
    }
}

TEST_CASE("ui: bar: a click is position-proportional across the body")
{
    const ui::Rect body = bodyAt (1190);
    const ui::BarGeometry bar = ui::BarGeometry::forBody (body, kReferenceK, 0.0);

    REQUIRE(body.w % 4 == 0);

    SECTION("the centre is the midpoint and the quarter point is the quarter")
    {
        CHECK(bar.valueForX (body.x + body.w / 2) == 0.5);
        CHECK(bar.valueForX (body.x + body.w / 4) == 0.25);
        CHECK(bar.valueForX (body.x + (3 * body.w) / 4) == 0.75);
    }

    SECTION("no thumb-offset concept: the body's own left edge is 0 and its right edge is 1")
    {
        CHECK(bar.valueForX (body.x) == 0.0);
        CHECK(bar.valueForX (body.right ()) == 1.0);
        CHECK(bar.valueForX (body.x - 50) == 0.0);
        CHECK(bar.valueForX (body.right () + 50) == 1.0);
    }

    SECTION("and the same holds at k = 1 and k = 2, in cells the grid actually produces")
    {
        for (const int width : {315, 630})
        {
            const ui::Grid grid (width, 210);
            const ui::BarGeometry scaled =
                ui::BarGeometry::forBody (bodyAt (width), grid.k (), 0.0);

            INFO("window " << width << ", body width " << scaled.body.w);
            CHECK(scaled.valueForX (scaled.body.x) == 0.0);
            CHECK(scaled.valueForX (scaled.body.right ()) == 1.0);
            CHECK(scaled.valueForX (scaled.body.x + scaled.body.w / 2)
                  == Catch::Approx (0.5).margin (0.5 / scaled.body.w));
        }
    }

    SECTION("a zero-width body answers 0 rather than dividing by itself")
    {
        const ui::BarGeometry nothing = ui::BarGeometry::forBody ({body.x, body.y, 0, body.h},
                                                                  kReferenceK, 0.5);
        CHECK(nothing.valueForX (body.x) == 0.0);
        CHECK(nothing.insetPx == 3);
    }
}

TEST_CASE("ui: bar: valueForX and xForValue round-trip")
{
    const ui::Rect body = bodyAt (1190);
    const ui::BarGeometry bar = ui::BarGeometry::forBody (body, kReferenceK, 0.0);
    const double halfPixel = 0.5 / body.w;

    SECTION("at both ends, exactly")
    {
        CHECK(bar.xForValue (0.0) == body.x);
        CHECK(bar.xForValue (1.0) == body.right ());
        CHECK(bar.valueForX (bar.xForValue (0.0)) == 0.0);
        CHECK(bar.valueForX (bar.xForValue (1.0)) == 1.0);
    }

    SECTION("and at the middle")
    {
        CHECK(bar.valueForX (bar.xForValue (0.5)) == Catch::Approx (0.5).margin (halfPixel));
    }

    SECTION("across a sweep, to within the pixel that was rounded away")
    {
        for (int step = 0; step <= 64; ++step)
        {
            const double value = static_cast<double> (step) / 64.0;
            const int pixel = bar.xForValue (value);

            INFO("value " << value << " → x " << pixel);
            CHECK(bar.valueForX (pixel) == Catch::Approx (value).margin (halfPixel));
            CHECK(pixel >= body.x);
            CHECK(pixel <= body.right ());
        }
    }

    SECTION("the round-trip is exact wherever the value is already on the pixel grid")
    {
        for (int pixel = body.x; pixel <= body.right (); ++pixel)
            CHECK(bar.xForValue (bar.valueForX (pixel)) == pixel);
    }
}

TEST_CASE("ui: bar: snapToQuantum is the design's one quantisation expression")
{
    /* F-010's neighbourhood. The parameter quanta are dyadic on purpose, so the division and the
       multiply are exact and a value on the grid stays on it. §5 arm 4 drops the widget's interval and
       this is the expression the widget's answer is compared against. */
    constexpr double kBlendStep = 1.0 / 128.0;
    constexpr double kDbStep = 0.125;

    SECTION("1/128 and 0.125 reproduce exactly, on the grid and nowhere else")
    {
        for (int n = 0; n <= 128; ++n)
        {
            const double onGrid = static_cast<double> (n) * kBlendStep;
            CHECK(ui::snapToQuantum (onGrid, kBlendStep) == onGrid);
        }

        for (int n = -256; n <= 256; ++n)
        {
            const double onGrid = static_cast<double> (n) * kDbStep;
            CHECK(ui::snapToQuantum (onGrid, kDbStep) == onGrid);
        }
    }

    SECTION("nothing comes out off-grid, whatever goes in")
    {
        for (int n = 0; n <= 1000; ++n)
        {
            const double in = -1.0 + 2.0 * static_cast<double> (n) / 1000.0;

            for (const double quantum : {kBlendStep, kDbStep, 1.0, 512.0})
            {
                const double snapped = ui::snapToQuantum (in, quantum);
                const double steps = snapped / quantum;

                INFO("value " << in << " quantum " << quantum << " → " << snapped);
                CHECK(std::abs (steps - std::round (steps)) < 1e-9);
                CHECK(std::abs (snapped - in) <= quantum * 0.5 + 1e-12);
                CHECK(ui::snapToQuantum (snapped, quantum) == snapped);
            }
        }
    }

    SECTION("a quantised value is the nearest one, both ways from the midpoint")
    {
        /* 38/128 = 0.296875 and 39/128 = 0.3046875, so the midpoint is 0.30078125. */
        CHECK(ui::snapToQuantum (0.3, kBlendStep) == 38.0 / 128.0);
        CHECK(ui::snapToQuantum (0.302, kBlendStep) == 39.0 / 128.0);

        /* −7.25 and −7.375 meet at −7.3125. */
        CHECK(ui::snapToQuantum (-7.3, kDbStep) == -7.25);
        CHECK(ui::snapToQuantum (-7.32, kDbStep) == -7.375);
    }

    SECTION("a quantum of 0 is a continuous range and passes the value through")
    {
        CHECK(ui::snapToQuantum (0.3000001, 0.0) == 0.3000001);
        CHECK(ui::snapToQuantum (-2.75, 0.0) == -2.75);
    }
}

TEST_CASE("ui: bar: a read-only body is a meter, not a control")
{
    const ui::Rect body = bodyAt (1190);
    const ui::BarGeometry meter = ui::BarGeometry::forBody (body, kReferenceK, 0.4, true);
    const ui::BarGeometry control = ui::BarGeometry::forBody (body, kReferenceK, 0.4, false);

    SECTION("it has no hit rect, so the pointer goes through it")
    {
        CHECK(meter.hitRect() == ui::Rect{});
        CHECK_FALSE(ui::contains (meter.hitRect (), body.x + body.w / 2, body.y + body.h / 2));
    }

    SECTION("the interactive one answers across its whole body and nowhere else")
    {
        CHECK(ui::contains (control.hitRect (), body.x, body.y));
        CHECK(ui::contains (control.hitRect (), body.x + body.w / 2, body.y + body.h / 2));
        CHECK(ui::contains (control.hitRect (), body.right () - 1, body.bottom () - 1));
        CHECK_FALSE(ui::contains (control.hitRect (), body.right (), body.y));
        CHECK_FALSE(ui::contains (control.hitRect (), body.x - 1, body.y));
    }

    SECTION("and it has no active state: asked while active, it still draws the inset fill")
    {
        CHECK(meter.fillRect (true) == meter.fillRect ());
        CHECK(control.fillRect (true) != control.fillRect ());
        CHECK(control.fillRect (true).y == control.body.y);
    }
}

TEST_CASE("ui: bar: the value words sit ~1 mm inside the body's left edge")
{
    for (const int width : {315, 630, 1190})
    {
        const ui::Grid grid (width, 210);
        const ui::BarGeometry bar = ui::BarGeometry::forBody (bodyAt (width), grid.k (), 0.5);
        const ui::Rect text = bar.valueTextRect (grid.k ());

        INFO("window " << width << " px");
        CHECK(text.x == bar.body.x + ui::BarGeometry::valueInsetAt (grid.k ()));
        CHECK(text.h == bar.body.h);
        CHECK(text.w == bar.body.w - ui::BarGeometry::valueInsetAt (grid.k ()));
        CHECK(text.right () == bar.body.right ());
    }
}
