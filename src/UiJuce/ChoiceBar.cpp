#include "ChoiceBar.hpp"

namespace ui
{
ChoiceBar::ChoiceBar (juce::String componentName)
    : juce::ComboBox (std::move (componentName))
{
    /* The two settings that are not painting: nothing to type and no wheel-stepping through the list. The
       missing arrow is not a setting — JUCE 9.0.4 has no setArrowSize, so it is the absence of a drawing in
       `Theme::drawComboBox` that removes it. */
    setEditableText (false);
    setScrollWheelEnabled (false);
}

void ChoiceBar::addChoice (const juce::String& text, int itemId)
{
    addItem (text, itemId);
}
}
