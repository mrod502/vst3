#pragma once

#include <BarGeometry.hpp>
#include <Grid.hpp>
#include <Palette.hpp>
#include <PieGeometry.hpp>
#include <TextGeometry.hpp>
#include <TypeRamp.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cstddef>
#include <optional>

namespace ui
{
/** The one conversion from a token to something JUCE can draw with. Nothing else in the product
    constructs a colour (INDEX §3 rule 2). */
[[nodiscard]] juce::Colour toColour (std::uint32_t token) noexcept;

/** The kit's window scale, read back off whichever skin is installed.

    One carrier, because one sheet has one scale: `03` sets it on the Theme when the editor lays out and
    every component reads it here rather than keeping a copy that can drift against it. The reference sheet
    is the answer when the installed look and feel is not ours, so a component with no skin draws at the
    size the mockup was measured at instead of at nothing. */
[[nodiscard]] double kitScaleFor (const juce::Component& component) noexcept;

/** The line height a role's face occupies for a component that lays its own type rows — `kitScaleFor` and
    `fontScaleFor` in one call, so a widget that places text does not re-derive the pair. JUCE lays a wrapped
    line one em below the last, so the em is the row pitch and not the cap height.

    Returns 0 under somebody else's look and feel, where there is no ramp to resolve — the same answer
    `Text::paint` gives for the word itself. */
[[nodiscard]] int lineHeightFor (const juce::Component& component, TypeRole role);

/** The skin: the five tokens as a ColourScheme, the seven roles as Fonts, the kit's flat paint overrides,
    and the grid's text placed by cap height. Constructed once by the editor and installed with
    setLookAndFeel (item 03).

    The nine slots and the four collisions they force are D-024; what LookAndFeel_V4 gives back — the
    alpha-derived ids its base class manufactures, the dark scheme it starts from, and the inherited
    setColour on the id table — is F-022.

    The kit's overrides live here and nowhere else. In JUCE 9.0.4 they are declared in the per-widget
    `LookAndFeelMethods` structs (`Slider::LookAndFeelMethods`, `ComboBox::LookAndFeelMethods`,
    `Button::LookAndFeelMethods`) rather than in `LookAndFeel`, and there is no `setSkin`, so there is no
    per-widget carrier: suppressing the thumb, the outline and the button gradient is global to whoever
    installs this class, which is why the list has to be complete (`02` §11). They contain no arithmetic —
    every rect, inset and angle is answered by `src/Ui` (`02` §0 D). */
class Theme final : public juce::LookAndFeel_V4
{
public:
    /** The nine-slot scheme, then the per-id overrides, in that order.

        The order is not style: setColourScheme () runs initialiseColours (), which reassigns the whole
        derived widget-id table from the scheme, so an override applied before it is silently
        discarded. Five of the nine overrides are ids that table owns, so getting the order wrong is
        invisible until a widget is drawn. */
    Theme ();

    /** The Font for a role at scale k, built at most once per(role, k). paint()) calls this and must
        not build a Font itself; prepareFontsFor () removes even the first-frame build. */
    [[nodiscard]] juce::Font fontFor (TypeRole role, float k);

    /** Builds every role's Font for one k, so no paint()) can be the first call. */
    void prepareFontsFor (float k);

    /** How many times a Font has actually been constructed by this instance — the leg that keeps
        "cached in the Theme" a fact rather than a comment (item 01 §5 arm 11). */
    [[nodiscard]] int fontBuildCount () const noexcept { return builds; }

    /** Places the cap top of the text on capTop, not its em box top.

        JUCE lays a baseline out from the ascent inside a Rectangle, and ascent ≠ cap height, so a
        faithful implementation that uses the em box puts every label low against the grid — which is
        the one thing spec §2 says will make this design look wrong. The ratio comes from the
        typeface's own "H", so a substituted face sits at its own offset; that difference is recorded
        per host, not tuned away (risk 5).

        Horizontal placement follows horizontalLayout's own flags, measured from capTop.x to the
        right edge of the current clip. Vertical flags are ignored: the caller has already said where
        the text goes. */
    void drawCapAlignedText (juce::Graphics& g, const juce::String& text, TypeRole role, float k,
                            juce::Point<int> capTop, juce::Justification horizontalLayout);

    /** JUCE's squeeze floor as this theme saw it, for the leg that prints it. */
    [[nodiscard]] float squeezeFloorAtConstruction () const noexcept { return floorAtConstruction; }

    /** The nine slots as they were applied.

        LookAndFeel_V4 keeps its scheme private and hands out the only read path as a non-const
        reference, which makes that path a slot setter; this const accessor is why the inherited one
        is shadowed below. What the scheme actually does to the widgets is verified against the ids
        initialiseColours () derives from it, not against this copy. */
    [[nodiscard]] const ColourScheme& colourScheme () const noexcept { return schemeInForce; }

    //==============================================================================
    /** The sheet's window scale for the kit's geometry: px per mm, so `kReferenceScale` at the shipped
        window and 1.0 on a 315 px sheet.

        Setting it warms every role's Font at the ramp scale that goes with it, so no first frame builds
        one. Note the two scales are not the same number: the ramp's em figures are px *at the shipped
        window* (spec §2 measures them there), so the value `fontFor` takes is this one divided by
        `kReferenceScale`. `fontScaleFor` does that division and is the only place the kit makes it. */
    void setScale (double pxPerMm);
    [[nodiscard]] double scale () const noexcept { return kitScale; }

    /** The ramp's scale for a window scale — 1.0 at the shipped window, where the ramp's px figures were
        measured. Passing a window scale straight to `fontFor` instead is 3.778×-too-big type, which is why
        the division has a name and lives next to the thing it feeds. */
    [[nodiscard]] static float fontScaleFor (double pxPerMm) noexcept;

    /** The cap height of a role's face at the ramp scale `k`, in px — the metric `drawCapAlignedText` uses
        internally and the one a caller needs to place a line by its caps inside a box of known height. */
    [[nodiscard]] float capHeightOf (TypeRole role, float k);

    //==============================================================================
    // The kit's painting. Every one of these is a flat fill of a rect or a wedge that `src/Ui` computed;
    // none of them computes anything, and the two that draw "nothing" are deliberate suppressions.

    /** The body, the value fill and whichever text slots the widget carries — the whole bar in one pass,
        which is why the three leaf hooks below are not part of the drawing path.

        `sliderPos` and friends are ignored: for a single-value `Slider` they are pixel positions on the
        track (`minSliderPos == maxSliderPos == the track's start`), not the value, so the fill's width comes
        from the slider's own range instead. F-023. */
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    /** The body and its fill, for whatever asks for a background on its own — the same answer, never a
        second one. */
    void drawLinearSliderBackground (juce::Graphics&, int x, int y, int width, int height,
                                     float sliderPos, float minSliderPos, float maxSliderPos,
                                     juce::Slider::SliderStyle, juce::Slider&) override;

    /** Nothing. The design has no outline; the mockup contains zero stroke properties. */
    void drawLinearSliderOutline (juce::Graphics&, int x, int y, int width, int height,
                                  juce::Slider::SliderStyle, juce::Slider&) override;

    /** Nothing. There is no thumb, which is also why the drag must be position-proportional rather than
        thumb-relative — see `02` §4 criterion 3. */
    void drawLinearSliderThumb (juce::Graphics&, int x, int y, int width, int height,
                                float sliderPos, float minSliderPos, float maxSliderPos,
                                juce::Slider::SliderStyle, juce::Slider&) override;

    /** Zero, which is what makes `LookAndFeel_V2::getSliderLayout` hand the `Slider` the whole body as its
        track region: the click at a bar's centre is then the range's midpoint and not the midpoint plus or
        minus half a thumb, and a `LinearHorizontal` bar's drag region is the body it paints. */
    int getSliderThumbRadius (juce::Slider&) override;

    /** The widget's track region, which `Slider` caches from here on every resize and uses for both the
        paint hooks and the drag mapping.

        For a `Bar` that carries label words it is the widget's bounds less the label column, so the two
        consumers cannot disagree: the body the skin fills is the body the pointer maps onto, and a click in
        the label column is not a click on the value. Everything else gets its bounds, because with the
        thumb radius at zero the base class's inset is nothing. */
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    /** The face and the wedge, and in `datumAtZero` the one `ink` radial the design permits on a control. */
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    /** The `track` body, or `fill` while pressed — and no arrow, no box outline and no gradient. */
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    /** The choice's words at the value inset, with their cap top where the body's top edge is rather than
        the ascent inside the box the base class positions by. */
    void positionComboBoxText (juce::ComboBox&, juce::Label& labelToPosition) override;

    /** The `barValue` role, which is what both the box's own line and its popup are drawn with. */
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    /** `track` at rest, `fill` while pressed or engaged, always flat.

        The scheme's derivation is not enough and this override is the evidence rather than a belt:
        `LookAndFeel_V4::drawButtonBackground` re-dyes the colour it is handed with
        `withMultipliedSaturation` and `contrasting()` before it fills a rounded rect with a gradient, so a
        bypass button drawn by the base class is neither of the five tokens (`02` §0 F measured it). */
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    /** The `barValue` role at the sheet's own scale, for the one control the kit has no type of its own.

        JUCE 9.0.4 has no `Button::setFont`: `LookAndFeel_V2::drawButtonText` asks `getTextButtonFont` and
        nowhere else, and `LookAndFeel_V4`'s answer is `jmin (16, buttonHeight * 0.6)` at the system face — a
        size and a family nobody declared, on a sheet whose whole point is that every size is a ramp role and
        every family comes from `FontFactory`. The same reason `getComboBoxFont` above exists. What the hook
        cannot reach is placement: `drawButtonText` centres its own line, so this is the one place on the
        sheet the cap rule does not govern — recorded, not tuned, because the alternative is a button with no
        words at all. */
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

private:
    /* No slot setter survives construction: setColourScheme() re-runs initialiseColours() and drops every
       override made since, and getCurrentColourScheme() returns the palette by mutable reference. Both are
       LookAndFeel_V4's public API, so shadowing them here is the only refusal available to a Theme& caller. */
    using juce::LookAndFeel_V4::setColourScheme;
    using juce::LookAndFeel_V4::getCurrentColourScheme;

    [[nodiscard]] static ColourScheme makeScheme ();

    struct Entry
    {
        TypeRole role;
        float k;
        juce::Font font;
        float capHeight;
    };

    [[nodiscard]] const Entry& entryFor (TypeRole role, float k);

    /* Pre-allocated and fixed: the callback rules in AGENTS §7 apply to paint()) too, so the cache
       never grows. Seven roles by four distinct k values; the design has one k, so it is warm after
       the first frame and the round-robin slot below is never reached in practice. */
    static constexpr std::size_t kCacheSlots = ui::kRoleCount * 4;

    ColourScheme schemeInForce;
    std::array<std::optional<Entry>, kCacheSlots> fontCache;
    std::size_t nextSlot{};
    int builds{};
    float floorAtConstruction{};

    /* The kit's one window scale, px per mm. Message-thread confined like the font cache above: paint()
       reads it and only setScale() writes it, from the editor's layout pass. */
    double kitScale = kReferenceScale;

    /* The disc, the wedge and the datum mark drawn into reusable storage, so a repaint allocates nothing:
       JUCE's Path::clear() keeps the element array's capacity (clearQuick, not shrink-to-fit), so these grow
       once. Confined to the message thread, like the font cache — a requirement of the paint path. */
    juce::Path discPath;
    juce::Path wedgePath;
    juce::Path radialPath;
};
}
