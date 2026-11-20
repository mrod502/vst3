#pragma once

#include <PanelGeometry.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
/** The sheet's surface, and the only component in the kit that owns no value.

    Two modes and one inset. `sheet` is the page every other component sits on; `ink` is spec §1 rule 1's
    inverted surface, whose contents must therefore be tints, and whose fixed content inset is
    `kInkPanelContentInsetMm` — declared once in the light half so the two display panels and any future one
    cannot each pick their own. Flat fills only in both modes: no stroke, radius, gradient, shadow or
    opacity, because the mockup contains none of them and `LookAndFeel_V4` wants all of them (spec §5).

    It is the thinnest of the five, and that is the point: what keeps it a component rather than a painting
    rule is that three call sites share the inset and the inverted mode (`02` §0 C). */
class Panel : public juce::Component
{
public:
    enum class Mode
    {
        sheet,  ///< filled ground — the page.
        ink,    ///< filled ink — the inverted mode, whose content is a tint.
    };

    explicit Panel (juce::String componentName = {}, Mode initialMode = Mode::sheet);

    void setMode (Mode newMode) noexcept;
    [[nodiscard]] Mode mode () const noexcept { return panelMode; }

    /** The area inside the ink field's content inset, in this component's own bounds.

        The inset applies to a sheet-mode panel too — the same furniture on both modes, so contents placed
        against a panel's edge land in the same place whichever surface they are on. */
    [[nodiscard]] juce::Rectangle<int> contentArea () const;

    void paint (juce::Graphics&) override;

private:
    Mode panelMode;
};

/** Appends one trace to `path` as a filled ribbon: a flat band `thickness` px wide following `columnYs`
    from `first`, `count` columns on, with column 0 at `left`. Filled rather than stroked because the design
    contains no stroke (spec §1), and one closed polygon per trace is what lets a curve be a flat fill of a
    single token.

    `path` is the caller's and is cleared before it is built, so a panel that repaints without new data
    reuses the storage it already grew. */
void appendTraceRibbon (juce::Path& path, const int* columnYs, std::size_t first, std::size_t count,
                        int thickness, int left);
}
