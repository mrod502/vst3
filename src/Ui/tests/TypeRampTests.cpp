#include <TypeRamp.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cstdint>

namespace
{
constexpr float kReference = static_cast<float>(1190.0 / 315.0);

struct Row
{
    ui::TypeRole role;
    ui::TypeWeight weight;
    float emPx;
    float horizontalScale;
};

/* The shipped column: ems that hold each role's drawn cap height across the face change (the outgoing face
   drew caps at 0.7120 em, this one at 0.6700 em — every em is the old figure ×1.0627), and squeezes measured
   per role as the largest scale at which the widest string that role draws fits the room it is given at the
   reference k. Transcribed from the built binary, not derived here — D-037 carries the table it came from. */
constexpr Row kTable[] = {
    {ui::TypeRole::wordmark, ui::TypeWeight::bold, 68.0f, 0.77f},
    {ui::TypeRole::caption, ui::TypeWeight::light, 59.5f, 0.67f},
    {ui::TypeRole::barLabel, ui::TypeWeight::light, 67.5f, 0.65f},
    {ui::TypeRole::barValue, ui::TypeWeight::light, 67.0f, 0.70f},
    {ui::TypeRole::rotatedCaption, ui::TypeWeight::light, 55.3f, 0.66f},
    {ui::TypeRole::figure, ui::TypeWeight::bold, 37.2f, 1.00f},
    {ui::TypeRole::micro, ui::TypeWeight::light, 23.4f, 1.00f},
};
}

TEST_CASE("ui: the ramp is seven roles against the spec's chosen column")
{
    STATIC_CHECK(ui::kRoleCount == 7);
    STATIC_CHECK(std::size(kTable) == ui::kRoleCount);

    for (const Row& row : kTable)
    {
        const ui::TypeSpec spec = ui::typeSpec(row.role);
        CHECK(spec.weight == row.weight);
        CHECK(spec.emPx == row.emPx);
        CHECK(spec.horizontalScale == row.horizontalScale);
    }

    SECTION("every role appears once in kAllRoles and kAllRoles covers the enum")
    {
        for (ui::TypeRole role : ui::kAllRoles)
            CHECK(static_cast<int>(role) >= 0);
        STATIC_CHECK(ui::kAllRoles.size() == static_cast<std::size_t>(ui::kRoleCount));
        for (std::size_t i = 0; i < ui::kAllRoles.size(); ++i)
            CHECK(ui::kAllRoles[i] == kTable[i].role);
    }
}

TEST_CASE("ui: two weights, no third")
{
    int bold = 0;
    int light = 0;
    for (ui::TypeRole role : ui::kAllRoles)
    {
        switch (ui::typeSpec(role).weight)
        {
            case ui::TypeWeight::bold:
                ++bold;
                break;
            case ui::TypeWeight::light:
                ++light;
                break;
        }
    }

    CHECK(bold == 2);
    CHECK(light == 5);
    CHECK(bold + light == static_cast<int>(ui::kRoleCount));

    SECTION("and they are the two the family has: the wordmark and the node figures")
    {
        /* Clash Display has no Thin (D-037), which is why the enum member that used to be called `thin`
           is called `light`: the weight a role names is the weight that draws. */
        CHECK(ui::typeSpec(ui::TypeRole::wordmark).weight == ui::TypeWeight::bold);
        CHECK(ui::typeSpec(ui::TypeRole::figure).weight == ui::TypeWeight::bold);
        CHECK(ui::typeSpec(ui::TypeRole::caption).weight == ui::TypeWeight::light);
    }
}

TEST_CASE("ui: resolve is strictly monotone in k for every role")
{
    constexpr float kScales[] = {0.5f, 1.0f, 2.0f, kReference};

    for (ui::TypeRole role : ui::kAllRoles)
    {
        for (std::size_t i = 1; i < std::size(kScales); ++i)
            CHECK(ui::resolve(role, kScales[i - 1]) < ui::resolve(role, kScales[i]));

        CHECK(ui::resolve(role, 1.0f) == ui::typeSpec(role).emPx);
        CHECK(ui::resolve(role, 2.0f) == Catch::Approx(2.0 * ui::typeSpec(role).emPx).margin(1e-4));
    }

    SECTION("k = 1 is the sheet itself, and the reference k is 3.778")
    {
        CHECK(kReference > 3.777f);
        CHECK(kReference < 3.778f);
        CHECK(ui::resolve(ui::TypeRole::micro, 0.5f) == Catch::Approx(11.7).margin(1e-5));
    }
}

TEST_CASE("ui: every squeeze is fitted, and only the confined roles need one")
{
    for (ui::TypeRole role : ui::kAllRoles)
    {
        const float scale = ui::typeSpec(role).horizontalScale;
        const bool squeezed = scale != 1.00f;
        /* Five of the seven roles are confined by a box their words cannot widen — the cell is the limit. The
           two that are not answer a deficit another way: `figure` draws inside a panel that sizes itself to
           it, and `micro` wraps into a reserve counted in lines rather than words. */
        const bool confined = role != ui::TypeRole::figure && role != ui::TypeRole::micro;
        CHECK(squeezed == confined);
        CHECK(scale > 0.0f);
        /* Nothing stretches any more: a scale above 1 buys width a confined role does not have, and the
           1.10 this table used to carry on `barValue` is a figure from the face that left (D-037). */
        CHECK(scale <= 1.00f);
    }

    /* barLabel's 0.65 is the ramp's most distinctive value and the one nearest JUCE's own squeeze
       floor, which is why the skin reads that floor and prints it (criterion 3, F-022). */
    CHECK(ui::typeSpec(ui::TypeRole::barLabel).horizontalScale == Catch::Approx(0.65).margin(1e-6));
}

TEST_CASE("ui: barLabel is taller than the bar it labels, on purpose")
{
    /* The mockup draws an 85 px label beside a 67 px track. The role no longer reaches that: the row pitch is
       the fixed thing, and "Adj" carries a descender that 85 px of cap cannot share it with, so the label now
       stands level with the body it labels (D-037). Pinning the floor is what stops a later tidy re-inflating
       the role and clipping the j again — the drawn cap is checked in the shell, not here. */
    CHECK(ui::resolve(ui::TypeRole::barLabel, 1.0f) > 66.9f);
    CHECK(ui::resolve(ui::TypeRole::barValue, 1.0f) < ui::resolve(ui::TypeRole::barLabel, 1.0f));
}

TEST_CASE("ui: one size per role, so there is no eighth size to find")
{
    /* Seven roles with one em each means the distinct size count is seven. A role that grew a second
       size would make the count eight and this leg red, which is the ramp's rule in the light half;
       the source-policy case is the same rule in the skin and the shell. */
    float sizes[ui::kRoleCount]{};
    for (std::size_t i = 0; i < ui::kRoleCount; ++i)
        sizes[i] = ui::typeSpec(ui::kAllRoles[i]).emPx;

    int distinct = 0;
    for (std::size_t i = 0; i < ui::kRoleCount; ++i)
    {
        bool seenBefore = false;
        for (std::size_t j = 0; j < i; ++j)
            seenBefore = seenBefore || sizes[i] == sizes[j];
        if (!seenBefore)
            ++distinct;
    }

    CHECK(distinct == static_cast<int>(ui::kRoleCount));
}
