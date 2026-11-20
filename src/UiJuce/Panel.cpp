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

void appendTraceRibbon (juce::Path& path, const int* columnYs, std::size_t first, std::size_t count,
                        int thickness, int left)
{
    path.clear();

    if (count == 0)
        return;

    const int half = thickness / 2;
    const int last = static_cast<int> (first + count - 1);

    path.startNewSubPath (left + static_cast<int> (first), columnYs[first] - half);
    for (int column = static_cast<int> (first) + 1; column <= last; ++column)
        path.lineTo (left + column, columnYs[column] - half);
    for (int column = last; column >= static_cast<int> (first); --column)
        path.lineTo (left + column, columnYs[column] - half + thickness);
    path.closeSubPath();
}
}
