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
    caption,         ///< section captions: "Volume Ctrl", "Window Size"
    barLabel,        ///< the 3-letter label left of a bar — taller than the bar it labels, by design
    barValue,        ///< the text inside a bar
    rotatedCaption,  ///< "EQ", "Waveform", "Vibes"
    figure,          ///< node numbers inside a display panel
    micro,           ///< diagnostics only, and the one role not in the mockup
};

/** Two weights, no third: the file declares Thin 200 on 18 text elements and Bold 700 on 4. */
enum class TypeWeight
{
    thin,
    bold,
};

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

/** The ramp. One size per role at the reference width of 315 mm, which is the only place these px
    figures mean anything — multiply by k for any other window.

    barLabel is deliberately taller than the 66.9 px bar it labels. That is the mockup's most
    distinctive move — the words are the interface and the bar is the thing they sit next to — and
    tidying the label down to the bar's height is the way to lose the design. */
[[nodiscard]] constexpr TypeSpec typeSpec(TypeRole role) noexcept
{
    switch (role)
    {
        case TypeRole::wordmark:
            return {TypeWeight::bold, 64.0f, 1.00f};
        case TypeRole::caption:
            return {TypeWeight::thin, 56.0f, 1.00f};
        case TypeRole::barLabel:
            return {TypeWeight::thin, 80.0f, 0.65f};
        case TypeRole::barValue:
            return {TypeWeight::thin, 63.0f, 1.10f};
        case TypeRole::rotatedCaption:
            return {TypeWeight::thin, 52.0f, 1.00f};
        case TypeRole::figure:
            return {TypeWeight::bold, 35.0f, 1.00f};
        case TypeRole::micro:
            return {TypeWeight::thin, 22.0f, 1.00f};
    }

    return {TypeWeight::thin, 22.0f, 1.00f};
}

/** The em height in px for a role at window scale k. Monotone in k for every role, strictly for
    every role, because the scale enters as a product and nothing clamps it here. */
[[nodiscard]] constexpr float resolve(TypeRole role, float k) noexcept
{
    return typeSpec(role).emPx * k;
}
}
