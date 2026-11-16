#include "Text.hpp"

#include "Theme.hpp"

namespace ui
{
Text::Text (juce::String componentName, juce::String initialWords, TypeRole role,
            TextAlignment alignment, TextSurface surface)
    : juce::Label (std::move (componentName), std::move (initialWords)),
      textRole (role),
      horizontalAlignment (alignment),
      wordsOn (surface)
{
    /* The Label's own painting is replaced wholesale, so its colour and font settings would be decoration;
       turning editing off is not decoration — a kit word is never a text field, and a `LabelAttachment`
       only ever writes it. */
    setEditable (false);
}

void Text::setAlignment (TextAlignment newAlignment)
{
    horizontalAlignment = newAlignment;
    repaint();
}

void Text::setSurface (TextSurface newSurface)
{
    wordsOn = newSurface;
    repaint();
}

void Text::setOrientation (TextOrientation newOrientation)
{
    rotated = newOrientation;
    repaint();
}

void Text::paint (juce::Graphics& g)
{
    auto* theme = dynamic_cast<Theme*> (&getLookAndFeel());

    /* Every word on the sheet is drawn by the skin that owns the ramp and the cap metric, and the kit is
       installed with that skin. Under any other look and feel there is no role to resolve, so there is no
       word — a blank sheet is a louder failure than a size nobody declared. */
    if (theme == nullptr)
        return;

    const double scale = kitScaleFor (*this);
    const float rampScale = Theme::fontScaleFor (scale);
    const auto bounds = getLocalBounds();
    const Rect box {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};

    g.setColour (toColour (wordToken (wordsOn)));

    if (rotated == TextOrientation::rotated)
    {
        const RotatedPlacement placed = rotatedPlacementFor (box, scale);
        const float pivotX = static_cast<float> (placed.pivot.x);
        const float pivotY = static_cast<float> (placed.pivot.y);

        g.addTransform (juce::AffineTransform::rotation (static_cast<float> (kRotationRadians),
                                                         pivotX, pivotY));

        theme->drawCapAlignedText (g, getText(), textRole, rampScale,
                                   {static_cast<int> (placed.pivot.x), static_cast<int> (placed.pivot.y)},
                                   juce::Justification::left);
        return;
    }

    /* The justification goes to the skin's helper, which lays the line out from capTop.x to the right edge of
       the current clip: measuring the string here would put an allocation in paint(), and a centred or
       right-aligned word is justified against the clip, not against a private measurement of itself. */
    const auto layout = horizontalAlignment == TextAlignment::right    ? juce::Justification::right
                        : horizontalAlignment == TextAlignment::centre ? juce::Justification::horizontallyCentred
                                                                       : juce::Justification::left;

    theme->drawCapAlignedText (g, getText(), textRole, rampScale, {box.x, box.y}, layout);
}
}
