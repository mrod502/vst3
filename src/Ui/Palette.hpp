#pragma once

#include <cmath>
#include <cstdint>

namespace ui
{
/** The five tokens. Every colour the product names starts here, in 0xAARRGGBB.

    Values are data rather than juce::Colour because this header is JUCE-light and the light half is
    where the scheme's arithmetic is testable (D-024). The skin converts once, in
    ui::Theme::toColour(); nothing else constructs a colour.

    The four tints are one tint — HSL lightness 90.0–91.2 % — and the tint is what makes the scheme
    quiet: any two of them contrast by 1.03–1.15, so no tint can carry a state against another. ink
    against any tint is 16.0 or better, so type never needs a rescue. contrastRatio() is the leg that
    keeps that statement true when a value is edited. */
inline constexpr std::uint32_t kGround = 0xFFFCD5F7u;

/** Value: bar fill, dial wedge, node dots, the selected item behind ink. */
inline constexpr std::uint32_t kFill = 0xFFF9E9D5u;

/** Surface: bar bodies, dial faces, choice-bar bodies, any popup's ground. */
inline constexpr std::uint32_t kTrack = 0xFFD3E6F8u;

/** Spent exactly once, on the third trace of the waveform panel. Spending it on a
    control would spend the scheme's last piece of information design. */
inline constexpr std::uint32_t kTrace = 0xFFD3F9DAu;

/** Every glyph, both display panels, and the only line the product ever draws. */
inline constexpr std::uint32_t kInk = 0xFF000000u;

/** WCAG 2.1 relative luminance of the RGB half of an 0xAARRGGBB token: sRGB components linearised
    and weighted 0.2126 R + 0.7152 G + 0.0722 B. Alpha is ignored — a token is opaque by rule. */
inline double relativeLuminance(std::uint32_t argb) noexcept
{
    const auto linearised = [](unsigned shifted) noexcept
    {
        const double srgb = static_cast<double>(shifted & 0xFFu) / 255.0;
        return srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
    };

    return 0.2126 * linearised(argb >> 16) + 0.7152 * linearised(argb >> 8) +
           0.0722 * linearised(argb);
}

/** WCAG contrast ratio of two tokens, lighter over darker, in [1, 21]. Symmetric in its arguments. */
inline double contrastRatio(std::uint32_t a, std::uint32_t b) noexcept
{
    const double first = relativeLuminance(a);
    const double second = relativeLuminance(b);
    const double lighter = first > second ? first : second;
    const double darker = first > second ? second : first;
    return (lighter + 0.05) / (darker + 0.05);
}

/** HSL lightness — (max + min) / 2 of the sRGB components, the quantity the design tabulates as the
    scheme's single tint. Not the same measure as relativeLuminance(), and the two must not be
    quoted for each other: ground is 91.2 % here and 75.0 % there. */
inline double hslLightness(std::uint32_t argb) noexcept
{
    const auto at = [argb](unsigned shift) noexcept
    { return static_cast<double>((argb >> shift) & 0xFFu) / 255.0; };

    const double r = at(16);
    const double g = at(8);
    const double b = at(0);
    const double highest = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const double lowest = r < g ? (r < b ? r : b) : (g < b ? g : b);
    return (highest + lowest) * 0.5;
}
}
