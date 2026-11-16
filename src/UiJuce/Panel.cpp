#include "Panel.hpp"

#include "Theme.hpp"

namespace ui
{
Panel::Panel (juce::String componentName, Mode initialMode)
    : juce::Component (std::move (componentName)), panelMode (initialMode)
{
}

void Panel::setMode (Mode newMode) noexcept
{
    panelMode = newMode;
    repaint();
}

juce::Rectangle<int> Panel::contentArea () const
{
    const Rect inset = inkContentRect ({0, 0, getWidth(), getHeight()}, kitScaleFor (*this));
    return {inset.x, inset.y, inset.w, inset.h};
}

void Panel::paint (juce::Graphics& g)
{
    g.fillAll (toColour (panelMode == Mode::sheet ? kGround : kInk));
}
}
