#pragma once

#include <TypeRamp.hpp>

#include <juce_graphics/juce_graphics.h>

namespace ui
{
/** The one place a font is chosen.

    Whether the answer is Helvetica Neue, a bundled metric-compatible face, or Neue-where-it-exists with a
    fallback, it changes this file and nothing else — no Font anywhere in the product is constructed from a
    family name or a typeface pointer anywhere else. The shipped answer is the design's own face, embedded:
    the user answered the open question on 2026-11-15 with Helvetica Neue and nothing else, and supplied the
    files, so the two weights the ramp binds to are compiled into the binary and resolved from memory
    (D-025). Asking fontconfig — which is what this file did until then — drew the sheet in whatever the host
    had, because JUCE 9 substitutes for no unmatched family and returns null instead (F-022); embedding is
    what closes that, on every host, with no substitute anywhere in the build. */
[[nodiscard]] juce::Typeface::Ptr typefaceFor (TypeWeight weight);

/** What was actually resolved — "family / style" — so a host run can see whether the named face is the one
    being drawn. With the face embedded there is nothing left for a host to refuse: the answer is
    "Helvetica Neue / Thin" and "Helvetica Neue / Bold" on every platform, and the `ui: skin:` face legs assert
    exactly that rather than reporting what a host happened to have. A null face means the embedded data did not
    load — a broken build, not a design state — and this returns "no embedded typeface" for it. */
[[nodiscard]] juce::String typefaceDescriptionFor (TypeWeight weight);

/** The Font for a role at scale k: the weight's typeface, the role's em height times k, and the
    role's horizontal squeeze applied with Font::withHorizontalScale.

    The squeeze goes through the font API rather than a non-uniform Graphics transform because a
    scaled CTM anti-aliases glyphs sideways and moves them, while withHorizontalScale re-hints
    (§0 F). */
[[nodiscard]] juce::Font makeFont (TypeRole role, float k);

/** Cap height in user units for a font, from the typeface's own outline of "H".

    JUCE 9 exposes no cap-height accessor — Typeface has getMetrics (), whose ascent and descent sum
    to 1 and say nothing about caps — so the ratio comes from the glyph. The fallback is the ascent,
    which is the ascent-placement bug this function exists to avoid; it can only trigger on a face
    that yields no outline at all. */
[[nodiscard]] float capHeightFor (const juce::Font& font);

/** JUCE's global floor for horizontal squeezing, read from the runtime rather than assumed: it is
    FontValues::minimumHorizontalScale, an internal mutable global with no published constant.

    Measured 2026-11-13 on the pinned submodule: 0.7f, set in juce_FontOptions.cpp:51. It is a bound
    on *further* squeezing inside fitted-text layout — createFittedText multiplies it by the font's
    own horizontal scale — so it cannot raise a 0.65 font to 0.7. What it does mean is recorded in
    F-022: text drawn through drawFittedText with a 0.65 face may be squeezed to 0.455. */
[[nodiscard]] float squeezeFloor () noexcept;
}
