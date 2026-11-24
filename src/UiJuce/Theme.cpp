#include "Theme.hpp"

#include "Bar.hpp"
#include "FontFactory.hpp"
#include "PieDial.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace ui
{
namespace
{
/* Every kit number arrives from `src/Ui` as a ui::Rect and these hooks fill with juce::Rectangle<int>;
   writing the conversion at each call site is where a w gets swapped for an h. */
[[nodiscard]] juce::Rectangle<int> toJuce (const Rect& area) noexcept
{
    return {area.x, area.y, area.w, area.h};
}

/** How a wedge is tessellated. Not a design figure — the mockup draws a sector, not a polygon — so this is
    the skin's resolution and lives here; the light half's default segment count is the same number, so the
    reference polygon its tests measure and the drawn outline agree. */
constexpr int kWedgeSegments = 64;

/** The bar's body and its value fill: the whole of what a bar looks like, in one place, so the top-level
    hook and the background leaf can never give two answers. */
void drawBarBody (juce::Graphics& g, const juce::Slider& slider, const BarGeometry& geometry)
{
    const auto* bar = dynamic_cast<const Bar*> (&slider);
    /* Inert outranks active: a control that lost its enabled state while holding focus shows nothing but
       its words (the state table's third row, arm 6). */
    const bool inert = ! slider.isEnabled ();
    const bool active = bar != nullptr ? bar->isActive ()
                                       : (! inert && (slider.hasKeyboardFocus (true) || slider.isMouseOver (true)));

    g.setColour (toColour (inert ? kGround : kTrack));
    g.fillRect (toJuce (geometry.body));

    if (inert)
        return;

    g.setColour (toColour (kFill));
    g.fillRect (toJuce (geometry.fillRect (active)));
}

/** The bar's two word slots. A plain `juce::Slider` carries no words — a `Bar` does — so this draws nothing
    for any other slider the skin is asked to paint. */
void drawBarWords (Theme& theme, juce::Graphics& g, const juce::Slider& slider, const BarGeometry& geometry)
{
    const auto* bar = dynamic_cast<const Bar*> (&slider);

    if (bar == nullptr)
        return;

    const double k = theme.scale ();
    const float rampScale = Theme::fontScaleFor (k);
    g.setColour (toColour (wordToken (TextSurface::tinted)));

    /* The label's room is the column the layout carved — which is `body.x`, the distance from the widget's
       own left edge to where the track starts. One number, so the words, the body and the drag region
       cannot disagree. */
    if (! bar->labelWords ().isEmpty ())
        theme.drawCapAlignedText (g, bar->labelWords (), TypeRole::barLabel, rampScale,
                                  {slider.getLocalBounds ().getX (), geometry.body.y},
                                  geometry.body.x, juce::Justification::left);

    if (! bar->valueWords ().isEmpty ())
    {
        const Rect words = geometry.valueTextRect (k);
        theme.drawCapAlignedText (g, bar->valueWords (), TypeRole::barValue, rampScale,
                                  {words.x, words.y}, words.w, juce::Justification::left);
    }
}

/** The geometry for the bounds a paint hook was handed. The fill is driven by the slider's own position in
    its range, never by the `sliderPos` arguments: measured 2026-11-14, a single-value `Slider` arrives with
    `sliderPos == minSliderPos == maxSliderPos == the track's start`, so a fill driven by those three floats
    would never move. F-023. */
[[nodiscard]] BarGeometry barGeometryFor (juce::Slider& slider, const Theme& theme,
                                          const Rect& widgetBounds)
{
    const auto* bar = dynamic_cast<const Bar*> (&slider);
    const double span = slider.getRange ().getLength ();
    const double value = span > 0.0 ? slider.valueToProportionOfLength (slider.getValue ()) : 0.0;

    return BarGeometry::forBody (widgetBounds, theme.scale (), value, bar != nullptr && bar->isReadOnly ());
}

/** The pie mode of the widget being drawn: a plain rotary `Slider` under this skin is proportional, and only
    a `PieDial` built in `datumAtZero` says otherwise. */
[[nodiscard]] PieMode pieModeOf (const juce::Slider& slider) noexcept
{
    const auto* dial = dynamic_cast<const PieDial*> (&slider);
    return dial != nullptr ? dial->pieMode () : PieMode::proportional;
}

/** The wedge as a Path, walked from the light half's outline into the caller's reusable scratch — so a
    repaint allocates nothing once the scratch has grown to the segment count (criterion 10). */
void addWedgePath (juce::Path& path, const Rect& face, WedgeAngles angles)
{
    const PointF centre = centreOf (face);
    const double radius = radiusFor (face);
    bool first = true;

    forEachWedgePoint (centre, radius, angles, kWedgeSegments,
                       [&path, &first] (PointF point)
                       {
                           const float px = static_cast<float> (point.x);
                           const float py = static_cast<float> (point.y);

                           if (first)
                           {
                               first = false;
                               path.startNewSubPath (px, py);
                           }
                           else
                               path.lineTo (px, py);
                       });

    path.closeSubPath ();
}

/** The dial's face as a Path the caller owns — the disc `discRectOf` inscribes in the widget's rect, drawn
    through reusable storage so a repaint allocates nothing (criterion 10). */
void addDiscPath (juce::Path& path, const Rect& disc)
{
    path.addEllipse (static_cast<float> (disc.x), static_cast<float> (disc.y),
                     static_cast<float> (disc.w), static_cast<float> (disc.h));
}

/** The datum's one ink mark, from the light half's four corners. */
void addRadialPath (juce::Path& path, const Rect& face, double angle, double k)
{
    const auto quad = radialQuad (centreOf (face), radiusFor (face), angle,
                                  static_cast<double> (datumRadialWidthAt (k)));

    path.startNewSubPath (static_cast<float> (quad[0].x), static_cast<float> (quad[0].y));

    for (std::size_t i = 1; i < quad.size (); ++i)
        path.lineTo (static_cast<float> (quad[i].x), static_cast<float> (quad[i].y));

    path.closeSubPath ();
}
} // namespace

juce::Colour toColour (std::uint32_t token) noexcept
{
    return juce::Colour{token};
}

double kitScaleFor (const juce::Component& component) noexcept
{
    const auto* theme = dynamic_cast<const Theme*> (&component.getLookAndFeel ());

    /* The installed skin, when it is ours. A component under somebody else's look and feel has no sheet
       scale to read, and draws at the size the mockup was measured at rather than at nothing. */
    return theme != nullptr ? theme->scale () : kReferenceScale;
}

int lineHeightFor (const juce::Component& component, TypeRole role)
{
    auto* theme = dynamic_cast<Theme*> (&component.getLookAndFeel ());

    /* fontFor builds and caches on first ask, so this is not a const read; a component under somebody
       else's skin has no ramp to resolve and gets 0, which places no row rather than an undeclared one. */
    if (theme == nullptr)
        return 0;

    return static_cast<int> (theme->fontFor (role, Theme::fontScaleFor (theme->scale ())).getHeight ());
}

juce::LookAndFeel_V4::ColourScheme Theme::makeScheme()
{
    /* Five tokens into nine slots: ground once, track twice, fill twice, ink four times, trace not at all.
       The arity is a hard nine — ColourScheme's variadic constructor static_asserts it — so the repeated
       values are the design, not an omission (D-024's table holds each collision). */
    return ColourScheme{toColour(kGround), toColour(kTrack), toColour(kTrack),
                        toColour(kInk), toColour(kInk), toColour(kFill), toColour(kInk),
                        toColour(kFill), toColour(kInk)};
}

Theme::Theme()
    : schemeInForce(makeScheme())
{
    setColourScheme(schemeInForce);

    /* After the scheme, never before it: initialiseColours() reassigns six of these nine ids from the scheme,
       so an override applied earlier is silently discarded. Each line contradicts what the base class would
       have produced, down to the tooltip ground the sheet needs an edge against the host desktop with. */
    setColour(juce::ResizableWindow::backgroundColourId, toColour(kGround));
    setColour(juce::Slider::trackColourId, toColour(kTrack));
    setColour(juce::Slider::thumbColourId, toColour(kInk));
    setColour(juce::Slider::textBoxHighlightColourId, toColour(kFill));
    setColour(juce::TextEditor::highlightColourId, toColour(kFill));
    setColour(juce::ToggleButton::tickDisabledColourId, toColour(kInk));
    setColour(juce::TooltipWindow::backgroundColourId, toColour(kTrack));
    setColour(juce::TooltipWindow::textColourId, toColour(kInk));
    setColour(juce::TooltipWindow::outlineColourId, toColour(kInk));

    floorAtConstruction = squeezeFloor();
}

juce::Font Theme::fontFor(TypeRole role, float k)
{
    return entryFor(role, k).font;
}

void Theme::prepareFontsFor(float k)
{
    for (const TypeRole role : kAllRoles)
        (void) entryFor(role, k);
}

void Theme::layoutRun (juce::GlyphArrangement& into, const Entry& entry, const juce::String& text,
                       double left, double baselineFirstLine, int maxWidthPx,
                       juce::Justification horizontal, double& advanceOut, int& lineCountOut)
{
    /* One shaping pass for the whole run. Breaking it into per-line `addLineOfText` calls would shape
       once per line, which is more allocation than `drawMultiLineText` does, and the kit's paint path is
       measured against exactly that (criterion 10). Everything below moves glyphs the shaper already
       produced: digits to the role's pitch, a line that would pass the box down one em at its last space,
       and the finished line across to wherever the justification wanted it. */
    into.clear();
    into.addLineOfText (entry.font, text, 0.0f, 0.0f);

    const int limit = juce::jmax (1, maxWidthPx);
    const double linePitch = entry.font.getHeight ();
    const int glyphCount = into.getNumGlyphs ();

    double penX = left;
    double widest = 0.0;
    int line = 0;
    int lineStart = 0;
    int breakAt = -1;
    double penAtBreak = left;

    const auto justifyLine = [left, limit, horizontal, &into] (int start, int count, double width)
    {
        if (count <= 0 || horizontal.getOnlyHorizontalFlags() == juce::Justification::left)
            return;

        const double slack = static_cast<double> (limit) - width;
        const double offset = horizontal.getOnlyHorizontalFlags() & juce::Justification::horizontallyCentred
                                ? slack * 0.5
                                : slack;

        if (offset != 0.0)
            into.moveRangeOfGlyphs (start, count, static_cast<float> (offset), 0.0f);
    };

    for (int i = 0; i < glyphCount; ++i)
    {
        juce::PositionedGlyph& glyph = into.getGlyph (i);
        const double natural = glyph.getRight() - glyph.getLeft();
        const double advance = ui::isFixedPitchCharacter (glyph.getCharacter())
                                 ? static_cast<double> (entry.digitPitch)
                                 : natural;

        if (i > lineStart && penX + advance > left + limit && breakAt >= lineStart)
        {
            const double width = penAtBreak - left;
            widest = std::max (widest, width);
            justifyLine (lineStart, breakAt - lineStart + 1, width);

            const int moved = i - (breakAt + 1);
            if (moved > 0)
                into.moveRangeOfGlyphs (breakAt + 1, moved, static_cast<float> (left - penAtBreak),
                                        static_cast<float> (linePitch));

            ++line;
            lineStart = breakAt + 1;
            penX = left;
            breakAt = -1;
        }

        glyph.moveBy (static_cast<float> (penX - glyph.getLeft ()),
                      static_cast<float> (baselineFirstLine + line * linePitch - glyph.getBaselineY ()));

        penX += advance;

        if (glyph.isWhitespace ())
        {
            breakAt = i;
            penAtBreak = penX;
        }
    }

    widest = std::max (widest, penX - left);
    justifyLine (lineStart, glyphCount - lineStart, penX - left);

    advanceOut = widest;
    lineCountOut = line + 1;
}

Theme::MeasuredRun Theme::measureText (const juce::String& text, TypeRole role, float k)
{
    return measureRun (text, role, k, std::numeric_limits<int>::max ());
}

Theme::MeasuredRun Theme::measureRun (const juce::String& text, TypeRole role, float k, int maxWidthPx)
{
    for (const std::optional<MeasureEntry>& slot : measureCache)
        if (slot.has_value() && slot->role == role && slot->k == k && slot->text == text
            && slot->maxWidthPx == maxWidthPx)
            return slot->run;

    ++measures;

    const Entry& entry = entryFor (role, k);
    MeasuredRun run;
    run.lineHeightPx = entry.font.getHeight ();

    double advance = 0.0;
    int lines = 1;
    layoutRun (scratch, entry, text, 0.0, 0.0, maxWidthPx,
               juce::Justification::left, advance, lines);

    run.extent.advancePx = advance;
    run.lineCount = lines;
    run.extent.hasBreakPoint = text.containsChar (' ');

    for (int i = 0; i < scratch.getNumGlyphs(); ++i)
    {
        const juce::PositionedGlyph& glyph = scratch.getGlyph (i);
        if (glyph.isWhitespace ())
            continue;

        juce::Path outline;
        scratch.getGlyph (i).createPath (outline);
        const juce::Rectangle<float> bounds = outline.getBounds();

        if (! bounds.isEmpty())
        {
            const double baselineY = glyph.getBaselineY();
            run.extent.capPx = std::max (run.extent.capPx, baselineY - static_cast<double> (bounds.getY()));
            run.extent.descentPx = std::max (run.extent.descentPx,
                                             static_cast<double> (bounds.getBottom()) - baselineY);
        }
    }

    MeasureEntry built{text, role, k, maxWidthPx, run};
    const std::size_t victim = nextMeasureSlot < kMeasureSlots ? nextMeasureSlot++ : 0;
    measureCache[victim] = built;
    return run;
}

void Theme::drawCapAlignedText(juce::Graphics& g, const juce::String& text, TypeRole role, float k,
                               juce::Point<int> capTop, int maxWidthPx,
                               juce::Justification horizontalLayout)
{
    const Entry& entry = entryFor(role, k);
    const double baseline = static_cast<double> (capTop.getY()) + entry.capHeight;

    double advance = 0.0;
    int lines = 1;
    layoutRun (scratch, entry, text, static_cast<double> (capTop.getX()), baseline, maxWidthPx,
               horizontalLayout, advance, lines);

    g.setFont(entry.font);
    scratch.draw (g);
}

float Theme::capHeightOf (TypeRole role, float k)
{
    return entryFor (role, k).capHeight;
}

void Theme::setScale (double pxPerMm)
{
    kitScale = pxPerMm;
    prepareFontsFor (fontScaleFor (pxPerMm));
}

float Theme::fontScaleFor (double pxPerMm) noexcept
{
    return static_cast<float> (pxPerMm / kReferenceScale);
}

void Theme::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float, float, float,
                              juce::Slider::SliderStyle, juce::Slider& slider)
{
    const BarGeometry geometry = barGeometryFor (slider, *this, Rect {x, y, width, height});
    drawBarBody (g, slider, geometry);
    drawBarWords (*this, g, slider, geometry);
}

void Theme::drawLinearSliderBackground (juce::Graphics& g, int x, int y, int width, int height, float, float,
                                        float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    drawBarBody (g, slider, barGeometryFor (slider, *this, Rect {x, y, width, height}));
}

void Theme::drawLinearSliderOutline (juce::Graphics&, int, int, int, int, juce::Slider::SliderStyle,
                                     juce::Slider&)
{
}

void Theme::drawLinearSliderThumb (juce::Graphics&, int, int, int, int, float, float, float,
                                   juce::Slider::SliderStyle, juce::Slider&)
{
}

int Theme::getSliderThumbRadius (juce::Slider&)
{
    return 0;
}

juce::Slider::SliderLayout Theme::getSliderLayout (juce::Slider& slider)
{
    /* The whole widget as the track region, less a labelled bar's label column: `Slider` caches this layout on
       resize and uses it for both the paint hooks and the pointer mapping, so the drag region and the painted
       body cannot drift. A slider with an editable text box would need that carved out too — the kit has none. */
    auto trackRegion = slider.getLocalBounds ();

    if (const auto* bar = dynamic_cast<const Bar*> (&slider); bar != nullptr && ! bar->labelWords ().isEmpty ())
        trackRegion.removeFromLeft (labelColumnPx (kitScale));

    return {trackRegion, {}};
}

void Theme::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosProportional, float, float, juce::Slider& slider)
{
    const Rect face {x, y, width, height};

    /* The disc, not the rect: `PieGeometry::discRectOf` is the same circle `hitTestPie` accepts, so the shape a
       dial shows and the shape it takes a click on are one answer. Filling the widget's rect instead paints
       `track` into four corners the design leaves ground and the pointer refuses. */
    discPath.clear ();
    addDiscPath (discPath, discRectOf (face));

    if (! slider.isEnabled ())
    {
        g.setColour (toColour (kGround));
        g.fillPath (discPath);
        return;
    }

    const PieMode mode = pieModeOf (slider);
    /* JUCE's sliderPosProportional is always 0…1 across the widget's range. A bipolar pie needs the signed
       distance from its datum, which is what the light half's datumAtZero overload takes; the linear map
       from one to the other is the whole of it and is here because it is a unit conversion, not geometry. */
    const double position = static_cast<double> (sliderPosProportional);
    const WedgeAngles angles = wedgeAngles (mode == PieMode::datumAtZero ? position * 2.0 - 1.0 : position,
                                           mode);

    g.setColour (toColour (kTrack));
    g.fillPath (discPath);

    wedgePath.clear ();
    addWedgePath (wedgePath, face, angles);
    g.setColour (toColour (kFill));
    g.fillPath (wedgePath);

    if (mode != PieMode::datumAtZero)
        return;

    radialPath.clear ();
    addRadialPath (radialPath, face, datumRadians (mode), kitScale);
    g.setColour (toColour (kInk));
    g.fillPath (radialPath);
}

void Theme::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown, int, int, int, int,
                          juce::ComboBox& box)
{
    /* The body alone: no arrow — the mockup's choice bar has none — no outline, and no gradient, which is
       what the base class draws over a box of its own. The pressed state is `fill`, which is also how a bar
       says it is being interacted with. */
    const auto bounds = juce::Rectangle<int> {0, 0, width, height};
    g.setColour (toColour (isButtonDown ? kFill : (box.isEnabled () ? kTrack : kGround)));
    g.fillRect (bounds);
}

void Theme::positionComboBoxText (juce::ComboBox& box, juce::Label& labelToPosition)
{
    /* The value inset from the left edge, with the cap top on the box's top edge. The base class centres the
       label and offsets by its own font's ascent, which is em-top placement; the Label holding the words is
       JUCE's own and painted by JUCE, so this hook is where the cap rule reaches it. */
    const double k = kitScale;
    const float rampScale = fontScaleFor (k);
    const auto bounds = box.getLocalBounds ();
    const int inset = BarGeometry::valueInsetAt (k);
    const juce::Font valueFont = fontFor (TypeRole::barValue, rampScale);

    /* The label paints text centred in its own bounds, offset by the font's ascent. Lifting the bounds by
       (ascent − capHeight) puts the caps on the body's top line only once the text is anchored top-left: a
       vertically-centred line sits at centre + (ascent − descent) / 2, which is a property of the face. */
    const int lift = static_cast<int> (std::lround (capHeightOf (TypeRole::barValue, rampScale)
                                                    - valueFont.getAscent ()));

    labelToPosition.setFont (valueFont);
    labelToPosition.setColour (juce::Label::textColourId, toColour (wordToken (TextSurface::tinted)));
    labelToPosition.setJustificationType (juce::Justification::topLeft);
    labelToPosition.setBounds (bounds.getX () + inset, bounds.getY () + lift,
                               juce::jmax (1, bounds.getWidth () - inset * 2), bounds.getHeight ());
}

juce::Font Theme::getComboBoxFont (juce::ComboBox&)
{
    return fontFor (TypeRole::barValue, fontScaleFor (kitScale));
}

void Theme::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                  bool, bool shouldDrawButtonAsDown)
{
    /* The base class re-dyes the colour it is handed with withMultipliedSaturation() and contrasting() and
       fills a rounded rect with a gradient, so a bypass drawn without this override is neither of the five
       tokens — measured, and the reason the override is the mechanism rather than the colour table (§0 F). */
    const bool engaged = button.getToggleState () || shouldDrawButtonAsDown;
    g.fillAll (toColour (engaged ? kFill : (button.isEnabled () ? kTrack : kGround)));
}

juce::Font Theme::getTextButtonFont (juce::TextButton&, int)
{
    return fontFor (TypeRole::barValue, fontScaleFor (kitScale));
}

const Theme::Entry& Theme::entryFor(TypeRole role, float k)
{
    for (const std::optional<Entry>& slot : fontCache)
        if (slot.has_value() && slot->role == role && slot->k == k)
            return *slot;

    ++builds;
    Entry built{role, k, makeFont(role, k), 0.0f, 0.0f};
    built.capHeight = capHeightFor(built.font);

    /* The widest digit at this role's size and squeeze is the pitch every digit takes, so a readout that
       changes value keeps its width — measured once here, because the paint must shape nothing. And with
       charToString: the int constructor formats the codepoint, so a pitch measured off "48" is twice too wide. */
    for (int digit = 0; digit < 10; ++digit)
        built.digitPitch = std::max (built.digitPitch,
                                     juce::GlyphArrangement::getStringWidth (
                                         built.font, juce::String::charToString (
                                             static_cast<juce::juce_wchar> ('0' + digit))));

    const std::size_t victim = nextSlot < kCacheSlots ? nextSlot++ : 0;
    fontCache[victim] = built;
    return *fontCache[victim];
}
}
