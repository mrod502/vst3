#include "Bar.hpp"

#include "Theme.hpp"

namespace ui
{
Bar::Bar (juce::String componentName)
    : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
{
    setName (std::move (componentName));

    /* The two JUCE additions a bar in this design has no use for: the editable box (the value's words are
       painted in the body) and the drag-time tooltip (a level bar's value is already on screen). Both are
       off at construction rather than left for `03` to remember. */
    setTextBoxIsEditable (false);
    setPopupDisplayEnabled (false, false, nullptr);
}

void Bar::setReadOnly (bool nowReadOnly)
{
    readOnlyBar = nowReadOnly;

    /* A meter must not take focus, and it must keep its enabled colour: the inert state belongs to the
       engine's decision to pin a value, not to this flag (§2.4's two states are not this one). */
    setWantsKeyboardFocus (!nowReadOnly);
    repaint();
}

void Bar::setLabelWords (const juce::String& words)
{
    label = words;

    /* The label column comes out of the body, so the drag region the Slider cached on the last resize is
       now wrong by exactly that width. resized() is the Slider's own route back through
       LookAndFeel::getSliderLayout. */
    resized();
    repaint();
}

void Bar::setValueWords (const juce::String& words)
{
    value = words;
    repaint();
}

juce::Rectangle<int> Bar::body() const
{
    auto bounds = getLocalBounds();

    if (!label.isEmpty())
        bounds.removeFromLeft (BarGeometry::labelColumnAt (kitScaleFor (*this)));

    return bounds;
}

BarGeometry Bar::geometry()
{
    const auto bounds = body();
    const double span = getRange().getLength();
    /* JUCE's own normalisation, because a skewed range is the one case where the fill's fraction is not
       (value − start) / span. valueToProportionOfLength is not const-qualified in JUCE 9.0.4, which is why
       this method is not const either. */
    const double normalised = span > 0.0 ? valueToProportionOfLength (getValue()) : 0.0;

    return BarGeometry::forBody ({bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()},
                                 kitScaleFor (*this), normalised, readOnlyBar);
}

bool Bar::isActive() const
{
    return !readOnlyBar && isEnabled() && (hasKeyboardFocus (true) || isMouseOver (true));
}

bool Bar::hitTest (int x, int y)
{
    /* A read-only bar refuses the pointer entirely rather than taking the click and doing nothing, so the
       event falls through to whatever is behind it; and the label column is not part of the control, so a
       click there is not a value change (§2.2, §2.4). */
    return !readOnlyBar && body().contains (x, y);
}
}
