#include <Palette.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cstdint>
#include <utility>

namespace
{
struct Named
{
    const char* name;
    std::uint32_t argb;
};

constexpr Named kTints[] = {{"ground", ui::kGround}, {"fill", ui::kFill},
                            {"track", ui::kTrack}, {"trace", ui::kTrace}};

constexpr int kTintCount = 4;
}

TEST_CASE("ui: the five tokens are exactly the five values")
{
    CHECK(ui::kGround == 0xFFFCD5F7u);
    CHECK(ui::kFill == 0xFFF9E9D5u);
    CHECK(ui::kTrack == 0xFFD3E6F8u);
    CHECK(ui::kTrace == 0xFFD3F9DAu);
    CHECK(ui::kInk == 0xFF000000u);

    SECTION("all five are opaque, so no token smuggles an alpha")
    {
        for (const Named& tint : kTints)
            CHECK((tint.argb >> 24) == 0xFFu);
        CHECK((ui::kInk >> 24) == 0xFFu);
    }

    SECTION("no two tokens are the same value")
    {
        for (int i = 0; i < kTintCount; ++i)
            for (int j = i + 1; j < kTintCount; ++j)
                CHECK(kTints[i].argb != kTints[j].argb);
    }
}

TEST_CASE("ui: relative luminance is the WCAG formula, not a plausible one")
{
    /* Known answers from outside the palette, so a formula that merely agrees with itself cannot
       pass: WCAG's own worked figures are 21.0 for black on white and 4.48 for #777777 on white. */
    CHECK(ui::contrastRatio(0xFFFFFFFFu, 0xFF000000u) == Catch::Approx(21.0).margin(1e-6));
    CHECK(ui::contrastRatio(0xFF777777u, 0xFFFFFFFFu) == Catch::Approx(4.478089).margin(1e-4));
    CHECK(ui::relativeLuminance(0xFFFFFFFFu) == Catch::Approx(1.0).margin(1e-9));
    CHECK(ui::relativeLuminance(0xFF000000u) == Catch::Approx(0.0).margin(1e-9));

    SECTION("the ratio is symmetric and ignores alpha")
    {
        CHECK(ui::contrastRatio(ui::kInk, ui::kGround) == ui::contrastRatio(ui::kGround, ui::kInk));
        CHECK(ui::contrastRatio(0x00FCD5F7u, ui::kGround) == Catch::Approx(1.0).margin(1e-12));
    }
}

TEST_CASE("ui: every tint pair is too quiet to carry a state")
{
    /* The measured table,, from these five values. Spec §1 rule 2 quotes four of the six
       pairs; ground↔trace and fill↔trace were never measured, and ground↔trace is 1.1458 — outside
       the 1.02–1.12 this item's criterion 2 predicted. The design conclusion survives at 1.15 (no
       tint can be the difference between two states), so the band is restated to what the tints are
       rather than the tokens retuned to fit the prediction. Item §11 carries the correction. */
    struct Pair
    {
        std::uint32_t a;
        std::uint32_t b;
        double measured;
    };

    constexpr Pair kPairs[] = {
        {ui::kGround, ui::kFill, 1.102780},
        {ui::kGround, ui::kTrack, 1.027756},
        {ui::kGround, ui::kTrace, 1.145787},
        {ui::kFill, ui::kTrack, 1.072997},
        {ui::kFill, ui::kTrace, 1.038999},
        {ui::kTrack, ui::kTrace, 1.114843},
    };

    for (const Pair& p : kPairs)
        CHECK(ui::contrastRatio(p.a, p.b) == Catch::Approx(p.measured).margin(5e-5));

    SECTION("and every pair is inside the band the scheme is built on")
    {
        for (const Pair& p : kPairs)
        {
            const double ratio = ui::contrastRatio(p.a, p.b);
            CHECK(ratio >= 1.02);
            CHECK(ratio <= 1.15);
        }
    }

    SECTION("nothing in the palette is bright enough to be a warning colour")
    {
        /* spec §1's "L 90–91 %" column is HSL lightness, not WCAG relative luminance — ground is
           91.18 % of the one and 75.00 % of the other. The four tints are one tint, and that is the
           property that makes the scheme quiet, so it is the one pinned here.

           The epsilon is not a loosened tolerance: track's lightness is exactly (248/255 + 211/255)/2
           = 459/510 = 0.9, and the floating-point sum lands one ulp under it, so an exact-bound
           compare on a boundary value is a coin toss on the last bit. */
        for (const Named& tint : kTints)
        {
            CHECK(ui::hslLightness(tint.argb) >= 0.90 - 1e-9);
            CHECK(ui::hslLightness(tint.argb) <= 0.9125);
        }
        CHECK(ui::hslLightness(ui::kGround) == Catch::Approx(0.911765).margin(1e-6));
        CHECK(ui::hslLightness(ui::kTrack) == Catch::Approx(0.900000).margin(1e-6));
        CHECK(ui::relativeLuminance(ui::kTrace) <= 0.92);
    }
}

TEST_CASE("ui: ink against every tint is at least sixteen to one")
{
    /* Measured. ground is 15.999869 — spec §1's "≥ 16.0" is 1.3e-4 optimistic, and an
       assertion written at exactly 16.0 is red on the palette as chosen. The bound below is the
       design's rule with the measurement in it; the discrepancy is recorded in item §11 rather than
       tuned out of the token. */
    CHECK(ui::contrastRatio(ui::kInk, ui::kGround) == Catch::Approx(15.999869).margin(5e-5));
    CHECK(ui::contrastRatio(ui::kInk, ui::kFill) == Catch::Approx(17.644332).margin(5e-5));
    CHECK(ui::contrastRatio(ui::kInk, ui::kTrack) == Catch::Approx(16.443965).margin(5e-5));
    CHECK(ui::contrastRatio(ui::kInk, ui::kTrace) == Catch::Approx(18.332441).margin(5e-5));

    for (const Named& tint : kTints)
        CHECK(ui::contrastRatio(ui::kInk, tint.argb) >= 15.9);
}

TEST_CASE("ui: a saturated tint reddens the scheme, not just the equality")
{
    /* What the contrast band is for, kept in the suite: #ADC9EB is the same hue as `track` at
       L 80 % / S 60.8 %, so a hex equality would not catch a swap made in a hurry. It contrasts
       1.3335 against track and 1.2975 against ground, which is what the band leg catches. */
    constexpr std::uint32_t kRejectedC4 = 0xFFADC9EBu;
    const double againstTrack = ui::contrastRatio(kRejectedC4, ui::kTrack);
    const double againstGround = ui::contrastRatio(kRejectedC4, ui::kGround);

    CHECK(againstTrack > 1.15);
    CHECK(againstGround > 1.15);
    CHECK(ui::hslLightness(kRejectedC4) < 0.90);
}
