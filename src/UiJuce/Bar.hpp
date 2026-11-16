#pragma once

#include <BarGeometry.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
/** The design's dominant object: a `track` body with a `fill` value bar, and one of two text slots.

    A stock `juce::Slider` in `LinearHorizontal` with the text box off, wearing the flat hooks in `Theme`:
    the attachments, the drag capture that continues when the pointer leaves the body, the wheel, the arrow
    keys, focus and accessibility are the reason it is a `Slider` and not a rectangle (`02` §0 A), and the
    flatness is entirely in the painting. Nothing is computed here that `BarGeometry` does not answer.

    Two content slots, not two widgets (`02` §0 B): `labelWords` is the `barLabel` role drawn left of the
    body — taller than the bar it labels, by design — and `valueWords` the `barValue` role inset ~1 mm
    *inside* it. The widget's bounds are the label column plus the body when a label is set and the body
    alone otherwise, which is what lets one component be the row in box 6 and the bare bar in box 9;
    `getSliderLayout` on the skin's side carves the same column off, so the drag region and the painted
    body are the same rect.

    `readOnly` is what makes the same class the applied-correction meter: the pointer goes through it and
    the active state never comes. It is *not* `setEnabled (false)` — a meter keeps its `track` body and its
    fill, and only the truly inert state (disabled, or pinned by the engine) trades the body for `ground` so
    that the words are all that is left (`02` risk 6, and criterion 9's "still tells the truth"). */
class Bar : public juce::Slider
{
public:
    explicit Bar (juce::String componentName = {});

    /** The meter flag. A read-only bar refuses the pointer and never draws its active state; it stays
        enabled, because disabling it would repaint its body as `ground` and lose the value it exists to
        show. */
    void setReadOnly (bool nowReadOnly);
    [[nodiscard]] bool isReadOnly () const noexcept { return readOnlyBar; }

    /** The `barLabel` slot: the three-letter name left of the body. Setting it also takes the label column
        out of the body, so the drag region moves with the text. */
    void setLabelWords (const juce::String& words);
    [[nodiscard]] const juce::String& labelWords () const noexcept { return label; }

    /** The `barValue` slot: the words inside the body's left edge. */
    void setValueWords (const juce::String& words);
    [[nodiscard]] const juce::String& valueWords () const noexcept { return value; }

    /** The body rect in this component's own bounds: the bounds less the label column, when there is one.
        The skin's `getSliderLayout` and this function must agree, and both ask `BarGeometry` for the
        column's width — one number, two consumers. */
    [[nodiscard]] juce::Rectangle<int> body() const;

    /** The geometry for the body as it stands, at this component's scale. Not const: it asks the `Slider`
        for its normalised position and JUCE 9.0.4's `valueToProportionOfLength` is not const-qualified. */
    [[nodiscard]] BarGeometry geometry();

    /** Rest or active: focus or the pointer over the body. Virtual for one reason — a headless probe cannot
        hold focus or hover, and criterion 5 asks for the active state's pixels rather than for a comment
        saying they would differ. Production never overrides it. */
    [[nodiscard]] virtual bool isActive () const;

    [[nodiscard]] bool hitTest (int x, int y) override;

private:
    juce::String label;
    juce::String value;
    bool readOnlyBar{};
};
}
