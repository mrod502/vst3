#pragma once

#include <PieGeometry.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
/** A `track` face with a `fill` wedge, no needle, no ring, no ticks and no outline.

    A rotary `juce::Slider` — the drag, the wheel, the arrow keys, the attachment and the automation come
    with it, and `Theme::drawRotarySlider` replaces everything JUCE would otherwise draw (`02` §0 A). The
    two modes are the two laws the product needs a dial for: `proportional` is the mockup's, one full turn
    from a 6 o'clock datum; `datumAtZero` is the blend's, growing clockwise for a positive request and
    counter-clockwise for a negative one from a 12 o'clock datum, with the single `ink` radial that is the
    only dark mark the design permits on a control — because the wedge's size alone cannot say where
    "wet only" is.

    The angles handed to `setRotaryParameters` come from `PieGeometry`, so the drag and the painting agree
    on where a value lives; the widget does arithmetic with nothing else. */
class PieDial : public juce::Slider
{
public:
    explicit PieDial (PieMode dialMode = PieMode::proportional, juce::String componentName = {});

    [[nodiscard]] PieMode pieMode () const noexcept { return mode; }

    /** The wedge as it stands: face, value and mode turned into angles by the light half. Not const for the
        same reason `Bar::geometry` is not — JUCE's normalisation is a non-const call. */
    [[nodiscard]] WedgeAngles wedge();

    /** The face is a circle, so its corners are not part of the control: a pointer that lands in one is
        outside the dial and belongs to whatever is behind it. */
    [[nodiscard]] bool hitTest (int x, int y) override;

private:
    PieMode mode;
};
}
