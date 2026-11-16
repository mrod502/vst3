#pragma once

#include "Grid.hpp"

#include <algorithm>
#include <cmath>

namespace ui
{
/** The ink display panel's content inset. The mockup draws its black panels as fields with type set in
    from the edge and gives no figure for the gap, so this takes the sheet's own content unit — one
    gutter — rather than a new number, and both panels and every future one read it from here.
    Re-pointing it is this line and nothing else. */
inline constexpr double kInkPanelContentInsetMm = kGutterMm;

/** The inset in px at window scale k. */
[[nodiscard]] inline int inkPanelInsetAt(double k) noexcept
{
    return static_cast<int>(std::lround(kInkPanelContentInsetMm * k));
}

/** What is drawn inside an ink panel: the panel's rect, inset once, symmetric on both axes. Everything
    inside is a tint, because the panel inverts the sheet's rule (spec §1 rule 1) and a tint on ink is the
    only other mode the palette has. */
[[nodiscard]] inline Rect inkContentRect(Rect panel, double k) noexcept
{
    const int inset = inkPanelInsetAt(k);
    return {panel.x + inset, panel.y + inset, std::max(0, panel.w - 2 * inset),
            std::max(0, panel.h - 2 * inset)};
}
}
