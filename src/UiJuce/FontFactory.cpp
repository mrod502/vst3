#include "FontFactory.hpp"

#include <EntangleFonts.h>

namespace ui
{
namespace
{
/* The design's face, read out of the binary rather than out of the host. A family-name request is a question
   the host gets to answer, and JUCE 9 resolves no substitute for a family it cannot match (F-022); embedding
   the two faces the ramp binds to is the only route on which the answer is always the design's face (D-025). */
[[nodiscard]] const juce::Typeface::Ptr& embeddedFace (TypeWeight weight)
{
    static const juce::Typeface::Ptr boldFace
        = juce::Typeface::createSystemTypefaceFor (HelveticaNeueBold_otf, HelveticaNeueBold_otfSize);
    static const juce::Typeface::Ptr thinFace
        = juce::Typeface::createSystemTypefaceFor (HelveticaNeueThin_otf, HelveticaNeueThin_otfSize);

    return weight == TypeWeight::bold ? boldFace : thinFace;
}
}

juce::Typeface::Ptr typefaceFor (TypeWeight weight)
{
    return embeddedFace (weight);
}

juce::String typefaceDescriptionFor (TypeWeight weight)
{
    const juce::Typeface::Ptr face = embeddedFace (weight);
    if (face == nullptr)
        return "no embedded typeface";

    return face->getName () + " / " + face->getStyle ();
}

juce::Font makeFont (TypeRole role, float k)
{
    const TypeSpec spec = typeSpec (role);
    const juce::Typeface::Ptr face = embeddedFace (spec.weight);

    /* The pointer, never a name: with one already set, FontOptions::withTypeface would be ignored, and a
       FontOptions built from a null Typeface::Ptr is the crash F-022 measured. A null here means the embedded
       data did not load — a broken build, so the assertion fires and JUCE's default face draws. */
    jassert (face != nullptr);

    const juce::FontOptions options
        = face != nullptr ? juce::FontOptions{}.withTypeface (face) : juce::FontOptions{};

    /* The squeeze is the font's own horizontal scale, not a Graphics transform, which would smear the glyphs
       instead of re-hinting them. */
    return juce::Font{options.withHeight (spec.emPx * k).withHorizontalScale (spec.horizontalScale)};
}

float capHeightFor (const juce::Font& font)
{
    juce::GlyphArrangement layout;
    layout.addLineOfText (font, "H", 0.0f, 0.0f);

    if (layout.getNumGlyphs () > 0)
    {
        juce::Path outline;
        layout.getGlyph (0).createPath (outline);
        const juce::Rectangle<float> bounds = outline.getBounds ();

        if (bounds.getHeight () > 0.0f)
            return bounds.getHeight ();
    }

    return font.getAscent ();
}

float squeezeFloor () noexcept
{
    return juce::Font::getDefaultMinimumHorizontalScaleFactor ();
}
}
