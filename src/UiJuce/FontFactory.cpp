#include "FontFactory.hpp"

#include <EntangleFonts.h>

#include <array>

namespace ui
{
namespace
{
/* The design's face, read out of the binary rather than out of the host: a family-name request is a question the
   host answers, and JUCE 9 resolves no substitute for a family it cannot match (F-022). The bytes are the tracked
   drop's, unmodified per FFL §02/§05, licence at public/ClashDisplay_Complete/License/FFL.txt (D-025, D-037). */
[[nodiscard]] const void* faceBytes (TypeWeight weight) noexcept
{
    return weight == TypeWeight::bold ? static_cast<const void*> (ui::ClashDisplayBold_otf)
                                      : static_cast<const void*> (ui::ClashDisplayLight_otf);
}

[[nodiscard]] size_t faceByteCount (TypeWeight weight) noexcept
{
    return weight == TypeWeight::bold ? ui::ClashDisplayBold_otfSize : ui::ClashDisplayLight_otfSize;
}

[[nodiscard]] const juce::Typeface::Ptr& embeddedFace (TypeWeight weight)
{
    static const juce::Typeface::Ptr lightFace
        = juce::Typeface::createSystemTypefaceFor (faceBytes (TypeWeight::light),
                                                   faceByteCount (TypeWeight::light));
    static const juce::Typeface::Ptr boldFace
        = juce::Typeface::createSystemTypefaceFor (faceBytes (TypeWeight::bold),
                                                   faceByteCount (TypeWeight::bold));

    return weight == TypeWeight::bold ? boldFace : lightFace;
}

/** The options every font on the sheet is built from: the face's own pointer and the ramp's declared line
    box. The override is per em (JUCE multiplies it by the point size), so the pair in `TypeRamp.hpp` means
    the same thing at every size. */
[[nodiscard]] juce::FontOptions declaredFaceOptions (TypeWeight weight)
{
    const juce::Typeface::Ptr face = embeddedFace (weight);
    jassert (face != nullptr);

    const juce::FontOptions options
        = face != nullptr ? juce::FontOptions{}.withTypeface (face) : juce::FontOptions{};

    return options.withAscentOverride (static_cast<float> (kDeclaredAscentPerEm))
                  .withDescentOverride (static_cast<float> (kDeclaredDescentPerEm));
}

/** The em every face figure is measured at, so the px figures *are* the per-em ratios. */
constexpr float kFigureEm = 1000.0f;

/** An instrument font whose height *is* its em.

    `FontOptions::withHeight` sizes the line box, and the box the sheet uses is the declared 0.714 + 0.184 pair —
    0.898 em — so an outline or advance read off a height-sized font comes back 1/0.898 too large unless the pair
    is pinned to 1/0 (D-037). */
[[nodiscard]] juce::Font figureFont (TypeWeight weight)
{
    return juce::Font{declaredFaceOptions (weight)
                          .withAscentOverride (1.0f)
                          .withDescentOverride (0.0f)
                          .withHeight (kFigureEm)};
}

/** The outline height of one character as a ratio of the em. Empty for a face that yields no outline. */
[[nodiscard]] float outlineHeightPerEm (const juce::Font& font, juce::juce_wchar character)
{
    juce::GlyphArrangement layout;
    /* charToString, never String (character): String has an int constructor, which a char32_t prefers, so the
       codepoint is drawn as its decimal number and a digit's height becomes two glyphs' bounding box. */
    layout.addLineOfText (font, juce::String::charToString (character), 0.0f, 0.0f);

    if (layout.getNumGlyphs () == 0)
        return 0.0f;

    juce::Path outline;
    layout.getGlyph (0).createPath (outline);

    return outline.getBounds().getHeight () / kFigureEm;
}
}

std::span<const std::byte> embeddedFaceData (TypeWeight weight) noexcept
{
    return {static_cast<const std::byte *> (faceBytes (weight)), faceByteCount (weight)};
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

    /* The pointer, never a name: with one already set, FontOptions::withTypeface would be ignored, and a
       FontOptions built from a null Typeface::Ptr is the crash F-022 measured. A null here means the embedded
       data did not load — a broken build, so the assertion fires and JUCE's default face draws. */
    const juce::FontOptions options = declaredFaceOptions (spec.weight);

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

FaceFigures faceFiguresFor (TypeWeight weight)
{
    /* A 1000 px em so the px figures are the per-em ratios the entry records, with the squeeze at 1 — the
       face's own proportions, not a role's fitted ones — and with the line box pinned to 1/0 so the height the
       instrument is given really is the em. Fallback off: a figure measured from a substituted face is somebody
       else's figure. */
    const juce::Font font = figureFont (weight);

    FaceFigures figures;
    figures.capPerEm = outlineHeightPerEm (font, 'H');
    figures.xPerEm = outlineHeightPerEm (font, 'x');

    /* The line-box pairs are fractions of the height the layout is given, so they are measured on a font sized
       the way the sheet sizes its fonts: emPx as height. */
    const juce::Font laidOut{declaredFaceOptions (weight).withHeight (kFigureEm)};
    figures.lineAscentFraction = laidOut.getAscent () / kFigureEm;
    figures.lineDescentFraction = laidOut.getDescent () / kFigureEm;

    const juce::Font faceOnly{juce::FontOptions{}.withTypeface (embeddedFace (weight))
                                  .withHeight (kFigureEm)};
    figures.faceAscentFraction = faceOnly.getAscent () / kFigureEm;
    figures.faceDescentFraction = faceOnly.getDescent () / kFigureEm;

    for (int digit = 0; digit < 10; ++digit)
        figures.digitAdvancePerEm[static_cast<std::size_t> (digit)]
            = juce::GlyphArrangement::getStringWidth (
                  font, juce::String::charToString (static_cast<juce::juce_wchar> ('0' + digit)))
              / kFigureEm;

    return figures;
}

bool faceHasGlyph (TypeWeight weight, char32_t codepoint)
{
    /* Fallbacks off, and the glyph index rather than the advance: the no-substitution rule that made
       embedding necessary also means a codepoint the face lacks draws nothing at all, and a fallback face
       would answer here with a glyph the sheet can never actually draw. */
    const juce::Font font{declaredFaceOptions (weight).withHeight (kFigureEm).withFallbackEnabled (false)};

    juce::GlyphArrangement layout;
    layout.addLineOfText (font, juce::String::charToString (static_cast<juce::juce_wchar> (codepoint)),
                          0.0f, 0.0f);

    return layout.getNumGlyphs () > 0 && layout.getGlyph (0).getGlyphIndex () != 0;
}
}
