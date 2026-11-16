#pragma once

#include <BarGeometry.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
/** A `Bar` body with no fill and the current choice as its words, opening a menu on press.

    A stock `juce::ComboBox` wearing the flat hooks in `Theme`: the popup, the keyboard, the
    `ComboBoxAttachment` and the item list are the reason it is a `ComboBox`, and the mockup's bar has no
    arrow, no box outline and no gradient, which is what the suppressions are for (`02` §2.3). Item ids are
    1-based because that is what `ComboBox` requires of a `ChoiceAttachment`, and it treats id 0 as "nothing
    selected" — `02` §5 arm 11 runs a 0-based list to show what that costs.

    Pressed is `fill` behind the ink words, which is the state vocabulary's own selection rule rather than a
    new one: the body has no value fill to go flush with, so the only thing that can change is what is
    behind the words. */
class ChoiceBar final : public juce::ComboBox
{
public:
    explicit ChoiceBar (juce::String componentName = {});

    /** Appends one menu entry with the id the APVTS attachment expects — `ChoiceAttachment`'s
        `itemIdOffset` is 1 by default, so a choice parameter's index 0 is this list's first id. */
    void addChoice (const juce::String& text, int itemId);
};
}
