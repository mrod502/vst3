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

/* spec §2's chosen column, transcribed rather than derived: the mockup's own sizes are a scatter of
   ~20 hand-dragged transforms of two declared sizes, and the ramp exists to stop that scatter
   arriving in code. */
constexpr Row kTable[] = {
    {ui::TypeRole::wordmark, ui::TypeWeight::bold, 64.0f, 1.00f},
    {ui::TypeRole::caption, ui::TypeWeight::thin, 56.0f, 1.00f},
    {ui::TypeRole::barLabel, ui::TypeWeight::thin, 80.0f, 0.65f},
    {ui::TypeRole::barValue, ui::TypeWeight::thin, 63.0f, 1.10f},
    {ui::TypeRole::rotatedCaption, ui::TypeWeight::thin, 52.0f, 1.00f},
    {ui::TypeRole::figure, ui::TypeWeight::bold, 35.0f, 1.00f},
    {ui::TypeRole::micro, ui::TypeWeight::thin, 22.0f, 1.00f},
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
    int thin = 0;
    for (ui::TypeRole role : ui::kAllRoles)
    {
        switch (ui::typeSpec(role).weight)
        {
            case ui::TypeWeight::bold:
                ++bold;
                break;
            case ui::TypeWeight::thin:
                ++thin;
                break;
        }
    }

    CHECK(bold == 2);
    CHECK(thin == 5);
    CHECK(bold + thin == static_cast<int>(ui::kRoleCount));

    SECTION("and they are the mockup's two: the wordmark and the node figures")
    {
        CHECK(ui::typeSpec(ui::TypeRole::wordmark).weight == ui::TypeWeight::bold);
        CHECK(ui::typeSpec(ui::TypeRole::figure).weight == ui::TypeWeight::bold);
        CHECK(ui::typeSpec(ui::TypeRole::caption).weight == ui::TypeWeight::thin);
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
        CHECK(ui::resolve(ui::TypeRole::micro, 0.5f) == Catch::Approx(11.0).margin(1e-5));
    }
}

TEST_CASE("ui: the squeeze belongs to two roles only")
{
    for (ui::TypeRole role : ui::kAllRoles)
    {
        const float scale = ui::typeSpec(role).horizontalScale;
        const bool squeezed = scale != 1.00f;
        const bool named = role == ui::TypeRole::barLabel || role == ui::TypeRole::barValue;
        CHECK(squeezed == named);
    }

    /* barLabel's 0.65 is the ramp's most distinctive value and the one nearest JUCE's own squeeze
       floor, which is why the skin reads that floor and prints it (criterion 3, F-022). */
    CHECK(ui::typeSpec(ui::TypeRole::barLabel).horizontalScale == Catch::Approx(0.65).margin(1e-6));
    CHECK(ui::typeSpec(ui::TypeRole::barValue).horizontalScale == Catch::Approx(1.10).margin(1e-6));
}

TEST_CASE("ui: barLabel is taller than the bar it labels, on purpose")
{
    /* The mockup draws an 80 px label beside a 66.9 px track, and that is its most distinctive move:
       the words are the interface and the bar is the thing they sit next to. Pinning the inequality
       is what stops a later "tidy" from reproducing the conventional ratio instead. */
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
