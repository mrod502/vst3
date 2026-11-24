#pragma once

#include <array>
#include <cstddef>

namespace ui
{
/** The seven type roles. A component asks for one; it never names a size (D-024).

    The mockup's own type is a scatter of about twenty sizes because every one of them is a
    hand-dragged Affinity transform of two declared sizes (spec §2). A kit that let each widget pick a
    size would reproduce that scatter in code instead of in a drawing program, which is why the ramp
    is data here and a role lookup is the only way to get text. */
enum class TypeRole
{
    wordmark,        ///< "Entangle"
    caption,         ///< section captions: "Level Ctrl", "Window Size"
    barLabel,        ///< the 3-letter label left of a bar, level with the body it names (D-031)
    barValue,        ///< the text inside a bar
    rotatedCaption,  ///< "EQ", "Waveform", "Vibes"
    figure,          ///< node numbers inside a display panel
    micro,           ///< diagnostics only, and the one role not in the mockup
};

/** The two weights the ramp binds, named for the faces that draw them.

    Clash Display ships six weights and no Thin (D-037); a weight enters the enum when the ramp binds it. */
enum class TypeWeight
{
    light,  ///< Clash Display Light, OS/2 usWeightClass 300
    bold,   ///< Clash Display Bold, OS/2 usWeightClass 700
};

/** The ascent and descent the layout is declared to use, per em — the line box, not the ink.

    The face's own `hhea` pair is 0.890/0.250, 27 % taller than the 0.714/0.184 the reserves were drawn
    against on the face this ramp replaced (F-030), so `makeFont` pins this pair through
    `FontOptions::withAscentOverride`/`withDescentOverride`. The em figures below hold the *cap* height
    across the face change, which raises the em by 4.5 %; the line box therefore grows by that much and
    not by the face's 27 %. */
inline constexpr double kDeclaredAscentPerEm = 0.714;
inline constexpr double kDeclaredDescentPerEm = 0.184;

/** What a role costs at the reference sheet: a weight, an em height in px at k = 1, and a
    horizontal squeeze applied with Font::withHorizontalScale, never with a scaled CTM. */
struct TypeSpec
{
    TypeWeight weight;
    float emPx;
    float horizontalScale;
};

inline constexpr std::size_t kRoleCount = 7;

/** Every role, in table order, so a test can walk the ramp without restating its length. */
inline constexpr std::array<TypeRole, kRoleCount> kAllRoles{
    TypeRole::wordmark,
    TypeRole::caption,
    TypeRole::barLabel,
    TypeRole::barValue,
    TypeRole::rotatedCaption,
    TypeRole::figure,
    TypeRole::micro,
};

/** Both weights, so the face legs walk the shipped set without naming it. */
inline constexpr std::array<TypeWeight, 2> kAllWeights{TypeWeight::light, TypeWeight::bold};

/** The ramp: one size per role at the reference sheet, multiplied by k for any other window.
    The ems hold each role's drawn cap height across the face change and the squeezes are measured against the
    room that role has; D-037 carries the table, the one role that does not hold its cap, and how each was fit. */
[[nodiscard]] constexpr TypeSpec typeSpec(TypeRole role) noexcept
{
    switch (role)
    {
        case TypeRole::wordmark:
            return {TypeWeight::bold, 68.0f, 0.77f};
        case TypeRole::caption:
            return {TypeWeight::light, 59.5f, 0.67f};
        case TypeRole::barLabel:
            return {TypeWeight::light, 67.5f, 0.65f};
        case TypeRole::barValue:
            return {TypeWeight::light, 67.0f, 0.70f};
        case TypeRole::rotatedCaption:
            return {TypeWeight::light, 55.3f, 0.66f};
        case TypeRole::figure:
            return {TypeWeight::bold, 37.2f, 1.00f};
        case TypeRole::micro:
            return {TypeWeight::light, 23.4f, 1.00f};
    }

    return {TypeWeight::light, 23.4f, 1.00f};
}

/** The em height in px for a role at window scale k. Monotone in k for every role, strictly for
    every role, because the scale enters as a product and nothing clamps it here. */
[[nodiscard]] constexpr float resolve(TypeRole role, float k) noexcept
{
    return typeSpec(role).emPx * k;
}
}
