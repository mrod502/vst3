#pragma once

#include <TextGeometry.hpp>
#include <TypeRamp.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
/** Words on the sheet: one of the seven roles, horizontal or turned −90°, in one of the design's two
    text modes.

    A `juce::Label` rather than a drawn string, because `LabelAttachment` needs a `Label&` and because the
    product's readouts change from the audio thread — which is the one thing a `Label` already does
    correctly. What is overridden is the drawing: `Label` places a line from its ascent inside its box, and
    the sheet places every line by its **cap top**, which is the difference the design says makes it look
    right (`Theme::drawCapAlignedText`, item 01 §0 G).

    The role is fixed at construction. That is the whole point of the ramp: a component that could pick a
    size would reproduce the mockup's hand-dragged scatter in code, and the policy case in `UiTests` is what
    stops the size being named anywhere else instead. */
class Text final : public juce::Label
{
public:
    explicit Text (juce::String componentName = {},
                   juce::String initialWords = {},
                   TypeRole role = TypeRole::caption,
                   TextAlignment alignment = TextAlignment::left,
                   TextSurface surface = TextSurface::tinted);

    [[nodiscard]] TypeRole role () const noexcept { return textRole; }

    void setAlignment (TextAlignment newAlignment);
    [[nodiscard]] TextAlignment alignment () const noexcept { return horizontalAlignment; }

    void setSurface (TextSurface newSurface);
    [[nodiscard]] TextSurface surface () const noexcept { return wordsOn; }

    /** Horizontal, or −90° reading bottom-to-top anchored on the cell's bottom-left.
        The mockup's +91° "Vibes" is a measurement of one hand-drawn element, not a second rule (§2.5). */
    void setOrientation (TextOrientation newOrientation);
    [[nodiscard]] TextOrientation orientation () const noexcept { return rotated; }

    void paint (juce::Graphics&) override;

private:
    TypeRole textRole;
    TextAlignment horizontalAlignment;
    TextSurface wordsOn;
    TextOrientation rotated = TextOrientation::horizontal;
};
}
