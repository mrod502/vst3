#include "PieDial.hpp"

#include "Theme.hpp"

namespace ui
{
PieDial::PieDial (PieMode dialMode, juce::String componentName)
    : juce::Slider (juce::Slider::Rotary, juce::Slider::NoTextBox),
      mode (dialMode)
{
    setName (std::move (componentName));

    /* The angles come from the light half and go straight to JUCE, so the drag limits and the drawn wedge
       are the same two numbers. The third argument is the snap-to-zero flag, on because a bipolar morph has
       to be parkable at exactly no correction (`02` §2.5). */
    const RotaryAngles angles = rotaryAnglesFor (mode);
    setRotaryParameters (angles.startRadians, angles.endRadians, true);
    setTextBoxIsEditable (false);
    setPopupDisplayEnabled (false, false, nullptr);
}

WedgeAngles PieDial::wedge()
{
    const double proportion = valueToProportionOfLength (getValue());

    /* A datumAtZero dial is signed whatever its endpoints are, so the signed value is recovered from the
       proportion rather than assumed to be the raw parameter. */
    return mode == PieMode::datumAtZero ? wedgeAngles (proportion * 2.0 - 1.0, mode)
                                        : wedgeAngles (proportion, mode);
}

bool PieDial::hitTest (int x, int y)
{
    const auto bounds = getLocalBounds();
    const Rect face {bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()};

    return hitTestPie ({static_cast<double> (x), static_cast<double> (y)}, centreOf (face),
                       radiusFor (face));
}
}
