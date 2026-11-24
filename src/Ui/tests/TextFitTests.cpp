#include <BarGeometry.hpp>
#include <TextFit.hpp>

#include <catch_amalgamated.hpp>

#include <cmath>

namespace
{
constexpr double kReference = 1190.0 / 315.0;

constexpr ui::TextExtent run(const double advance, const double cap, const double descent) noexcept
{
    return {advance, cap, descent, false};
}
}

TEST_CASE("ui: fit: a word's band is its ink, rounded up once")
{
    CHECK (ui::reserveFor (run (300.0, 22.40, 0.0)) == 23);
    CHECK (ui::reserveFor (run (300.0, 22.40, 5.20)) == 28);
    CHECK (ui::reserveFor (run (300.0, 22.00, 0.0)) == 22);

    SECTION ("a descender is paid for: the box is the ink, not the cap")
    {
        CHECK (ui::reserveFor (run (300.0, 22.40, 5.20)) > ui::reserveFor (run (300.0, 22.40, 0.0)));
    }
}

TEST_CASE("ui: fit: the width gate is exactly zero slack")
{
    CHECK (ui::fitsWidth (run (100.0, 20.0, 0.0), 100));
    CHECK (ui::fitsWidth (run (99.2, 20.0, 0.0), 100));
    CHECK_FALSE (ui::fitsWidth (run (100.0001, 20.0, 0.0), 100));
    CHECK_FALSE (ui::fitsWidth (run (101.0, 20.0, 0.0), 100));

    /* The bound is the drawn ink, so a fraction of a pixel is a deficit: there is no ellipsis and nothing
       shrinks, so that fraction is the glyph that goes missing. */
    SECTION ("a pixel of slack is the smallest pass and a fraction short is the smallest fail")
    {
        CHECK (ui::fitsWidth (run (99.0001, 20.0, 0.0), 100));
        CHECK_FALSE (ui::fitsWidth (run (100.0001, 20.0, 0.0), 100));
    }
}

TEST_CASE("ui: fit: a stack pays its ink once and every pitch unrounded")
{
    /* The reference sheet's truth column: seven `micro` lines of a 23.41 px pitch under 22.70 px of ink on
       the first line. Stepping the pitch through a rounded figure (23) under-charges by 3 px, which is the
       defect that made a word that fitted fail its own box (F-024 item 2). */
    CHECK (ui::stackReservePx (22.70, 7, 23.41) == 164);
    CHECK (ui::stackReservePx (22.70, 7, 23.41)
           > std::ceil (22.70) + 6 * std::round (23.41));

    SECTION ("one line is its ink and nothing else")
    {
        CHECK (ui::stackReservePx (22.70, 1, 23.41) == 23);
    }

    SECTION ("the pitch is what grows with the line count, one unrounded step at a time")
    {
        CHECK (ui::stackReservePx (22.70, 2, 23.41) - ui::stackReservePx (22.70, 1, 23.41) == 24);
        CHECK (ui::stackReservePx (22.70, 3, 23.41) - ui::stackReservePx (22.70, 2, 23.41) == 23);
    }
}

TEST_CASE("ui: fit: the label column is the millimetre floor or the words, whichever is wider")
{
    const int floorPx = ui::BarGeometry::labelColumnAt (kReference);

    SECTION ("words the design fits leave the millimetre figure alone")
    {
        CHECK (ui::labelColumnFor (40.0, kReference) == floorPx);
    }

    SECTION ("a word the design does not fit widens the column, and the value keeps its 1 px")
    {
        CHECK (ui::labelColumnFor (98.6, kReference) == 100);
        CHECK (ui::labelColumnFor (98.6, kReference) > floorPx);
    }

    SECTION ("the clearance leg agrees with the column rule at the same boundary")
    {
        CHECK (ui::labelClearsValue (98.6, 100));
        CHECK_FALSE (ui::labelClearsValue (98.6, 99));
        CHECK (ui::labelClearsValue (98.0, 99));
    }
}

TEST_CASE("ui: fit: the fixed pitch is the digits and nothing else")
{
    for (const char32_t digit : {U'0', U'1', U'5', U'9'})
        CHECK (ui::isFixedPitchCharacter (digit));

    /* Separators and signs keep their own advance: pitching them to the digit width spaces every prose readout
       on the sheet for a stability nothing asked for (D-037). */
    for (const char32_t other : {U'.', U',', U'-', U'+', U'/', U'=', U' ', U'B', U'm', U's'})
        CHECK_FALSE (ui::isFixedPitchCharacter (other));
}
