---
name: juce-custom-ui-painting
description: Paint and theme a custom JUCE UI — LookAndFeel_V4 ColourScheme + colour-id overrides, paint()/Graphics with Path and AffineTransform, SVG-derived Drawables — without breaking colour lookup, dirty-rectangle repaint, or the message-thread boundary.
source: learned
---

# Rich Custom UI: Painting + Theming (JUCE)

Distilled from the vendored `juce/` submodule (git describe `9.0.4-1-g454fe891ac`):
`juce_gui_basics/lookandfeel/` (`LookAndFeel`, `LookAndFeel_V4` + `ColourScheme`),
`juce_gui_basics/components/juce_Component.h` (paint/repaint/colour/LAF plumbing),
`juce_gui_basics/buttons/juce_DrawableButton.h`,
`juce_gui_basics/misc/juce_DrawableComponent.h`,
`juce_graphics/geometry/` (`Path`, `AffineTransform`, `PathStrokeType`),
`juce_graphics/contexts/juce_GraphicsContext.h`,
`juce_graphics/drawables/` (`Drawable` + SVG parsing, `DrawableShape`, `DrawableText`),
`juce_graphics/placement/juce_RectanglePlacement.h`.

## When to Use

- Giving a plugin editor a look of its own: rounded panels, custom sliders/knobs, meters,
  graph/spectrum backgrounds.
- Theming: switching a whole UI between dark/midnight/grey/light or a brand palette.
- Drawing non-rectangular or vector shapes — arcs, needles, envelopes, bezier curves —
  instead of stacking `fillRect` calls.
- Shipping icons/art as SVG, recolouring the same art per theme, or scaling art into a
  control without distortion.
- Debugging: "my colour change does nothing", black-on-black text, a widget that paints
  only after a resize, blurry icons, art clipped or offset when scaled, a theme switch that
  survives some widgets but not others.

## The three theming layers (know which one you are fighting)

| Layer | API | Who reads it |
|---|---|---|
| Global default | `LookAndFeel::setDefaultLookAndFeel (&laf)` — caller keeps ownership, `nullptr` reverts to JUCE's own `LookAndFeel_V4` | every component with no LAF of its own |
| Sub-tree | `Component::setLookAndFeel (&laf)` — not deleted by the component, inherits down to children that have no explicit LAF | that component + children |
| Colour ids | `LookAndFeel::setColour (int colourId, Colour)` / `Component::setColour (int colourId, Colour)` (per-widget override) | widget `paint()` via `findColour` |

Lookup order used by widgets: `Component::findColour (id)` → own override →
`LookAndFeel::findColour (id)` → **`Colours::black`** (`LookAndFeel.h:162`, `Component.h:2334`).
An unset id is black, not transparent — that is why "transparent" defaults must be set
explicitly as `0x00000000`.

Colour ids are per-widget enums: `Slider::ColourIds` (`thumbColourId` 0x1001300,
`trackColourId` 0x1001310, `rotarySliderFillColourId` 0x1001311, `textBoxTextColourId`
0x1001400), `Label::ColourIds`, `TextEditor::ColourIds`, `TextButton::buttonColourId` /
`buttonOnColourId`, `ComboBox::…`, `PopupMenu::…`, `ScrollBar::…`, `TreeView::ColourIds`.

`LookAndFeel_V4::ColourScheme` is a **separate 9-slot palette** feeding V4's own drawing,
with a hard arity check in its constructor:

```cpp
enum UIColour { windowBackground, widgetBackground, menuBackground, outline, defaultText,
                defaultFill, highlightedText, highlightedFill, menuText, numColours };
// static_assert (sizeof... (coloursToUse) == numColours) — exactly 9, in this order
```

Statics: `getDarkColourScheme()`, `getMidnightColourScheme()`, `getGreyColourScheme()`,
`getLightColourScheme()`. Per-slot access: `getUIColour (UIColour)` /
`setUIColour (UIColour, Colour)`; schemes compare with `operator==`.

## Procedure

1. **Install the look and feel once, on the message thread, before/at editor construction.**
   Keep the `LookAndFeel` alive as a member (or a static) of the editor or plugin wrapper —
   `setDefaultLookAndFeel` does **not** take ownership and JUCE keeps using it until you
   replace it. `Desktop::setDefaultLookAndFeel` asserts
   `JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED` and pushes `sendLookAndFeelChange()` to every
   top-level component, so do it in `AudioProcessorEditor`'s constructor, never lazily from
   `paint()` and never from the audio thread.

2. **Start from a V4 scheme, then override individual colour ids — in that order.**

   ```cpp
   setColourScheme (LookAndFeel_V4::getDarkColourScheme());   // re-derives the id palette
   setColour (Slider::trackColourId,        findColour (LookAndFeel_V4::ColourScheme::widgetBackground));
   setColour (Slider::rotarySliderFillColourId, accent);
   ```

   `setColourScheme()` stores the scheme **and calls `initialiseColours()`**, which
   overwrites dozens of widget ids (`TextButton::buttonColourId`, `TextEditor::*`,
   `Label::*`, `ComboBox::*`, `PopupMenu::*`, `ScrollBar::*`, …) from the scheme. Any id you
   set before the call is silently discarded. `getCurrentColourScheme()` returns a **mutable
   reference** and does *not* re-run `initialiseColours()` — mutate through
   `setUIColour()` only when you deliberately want to leave the id palette alone.

3. **Read colours through `findColour`, never through hard-coded literals, in custom
   widgets and in `LookAndFeel` overrides.**

   ```cpp
   void paint (Graphics& g) override
   {
       g.setColour (findColour (backgroundColourId));
       ...
   }
   ```

   Define your own `enum ColourIds { backgroundColourId = 0x7a10001, … }` on the custom
   component so a theme can restyle it without touching its code. V4 itself mixes both
   sources — some paths read `getCurrentColourScheme().getUIColour (…)`, others read ids —
   so half-theming (scheme only, or ids only) is the usual cause of a "partly themed" UI.

4. **Override the narrowest `LookAndFeel_V4` hook that covers the need** rather than
   replacing the whole look. The hooks you will actually use:
   `drawButtonBackground`, `drawToggleButton`, `drawTickBox`, `getTextButtonFont`,
   `drawLinearSlider`, `drawRotarySlider`, `getSliderThumbRadius`, `createSliderTextBox`,
   `drawComboBox` / `positionComboBoxText` / `getComboBoxFont`, `fillTextEditorBackground`,
   `drawTextEditorOutline`, `drawTooltip`, `drawScrollbar`, `drawProgressBar`,
   `drawPopupMenuItem`, `getIdealPopupMenuItemSize`, `drawCallOutBoxBackground`,
   `getTickShape` / `getCrossShape`, `drawAlertBox` + the alert fonts.
   Each receives the `Graphics&` already clipped and positioned, plus the widget, so read
   its state (`slider.getValue()`, `slider.isSliderOnScreen()`, `slider.isBeingDragged()`,
   `slider.isEnabled()`) rather than caching it.

5. **Draw in `paint()` with `Graphics`, in this order: fills → clipped details → strokes →
   text.** Set the colour immediately before each draw call; `setColour` also sets the
   context's opacity from the colour's alpha and **deselects any gradient/brush**
   (`GraphicsContext.h:75`). Common primitives:

   ```cpp
   g.fillAll (findColour (backgroundColourId));               // fills the CLIP region, not the bounds
   g.setColour (outline);
   g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);  // drawRect-family strokes grow INWARDS
   g.setColour (fill);
   g.fillRoundedRectangle (area.reduced (1.0f), 5.0f);
   g.setColour (text);
   g.drawFittedText (label, area.removeFromTop (18), Justification::left, 1, 1.0f);
   ```

   Use `fillRect` (or `drawHorizontalLine` / `drawVerticalLine`) for axis-aligned lines —
   `drawLine` is explicitly documented as the wrong tool unless the line is angled
   (`GraphicsContext.h:385`). For many rectangles use `fillRectList`.

6. **Build geometry once in a `Path`, cache it, and only rebuild it when its inputs change.**
   `Path` is float-based and holds multiple sub-paths:

   ```cpp
   Path needle;
   needle.startNewSubPath (0.0f, 0.0f);
   needle.lineTo (radius, -2.0f);
   needle.lineTo (radius, 2.0f);
   needle.closeSubPath();                    // closed => joins, not end-caps, when stroked
   g.fillPath (needle, AffineTransform::rotation (angle).translated (cx, cy));
   ```

   - Close shapes with `closeSubPath()`, never by `lineTo`-ing back to the start — the
     header is explicit that strokes need this to pick joins over end-caps.
   - `addRoundedRectangle`, `addEllipse`, `addArc`, `addCentredArc`, `arcTo`,
     `addPieSegment`, `addTriangle`, `addLineSegment` cover most control art; they start
     their own sub-path.
   - Rebuild on change, not every frame: `if (cachedArea != area) { rebuild(); cachedArea = area; }`
     `Path::clear()` first or the old geometry stays in the fill.
   - `getBounds()` / `getBoundsTransformed (t)` for measuring, `contains (x, y)` for hit
     tests (closed sub-paths only, honours the winding rule), `getLength()` /
     `getPointAlongPath()` / `getNearestPoint()` for scales and tick placement,
     `getTransformToScaleToFit (x, y, w, h, preserveProportions)` + `applyTransform()` to
     normalise hand-authored art to a control's area.
   - Strokes: `g.strokePath (p, PathStrokeType (thickness))`, or
     `PathStrokeType (thickness, miteringLimit, jointStyle, endCapStyle)` with
     `JointStyle` / `EndCapStyle`; `PathStrokeType::createDashedStroke` (or
     `g.drawDashedLine`) for dashed lines — dash-length count must be even.

7. **Compose `AffineTransform` explicitly; remember `sheared`/`rotated`/`scaled` members
   append.** The matrix is `(mat00 mat01 mat02 / mat10 mat11 mat12)`; `rotated (angle)` is
   **clockwise radians about (0, 0)** unless you use the pivot overload.

   ```cpp
   auto t = AffineTransform::translation (centreX, centreY)
                                .rotated (radians)
                                .scaled (scale);            // translate, then rotate, then scale
   auto aboutPoint = AffineTransform::rotation (radians, centreX, centreY);
   auto back = t.inverted();                                // screen coords -> art coords
   ```

   Fit art into a rectangle with `RectanglePlacement` (`centred`, `stretchToFit`,
   `fillDestination`, `onlyReduceInSize`, `onlyIncreaseInSize`; default flags are `centred`)
   via `getTransformToFit (source, dest)` / `appliedTo (source, dest)`.

8. **Ship art as SVG and theme it by colour substitution.** Parse once, keep the
   `Drawable` tree as a member, draw with opacity + transform:

   ```cpp
   icon = Drawable::createFromSVGString (String::fromUTF8 (MyIconSvg));   // -> unique_ptr<DrawableComposite>
   if (icon == nullptr) { /* fail loudly at load time, not in paint() */ }
   icon->replaceColour (Colours::black, findColour (iconColourId));       // recursive fill+stroke swap
   icon->drawWithin (g, bounds.toFloat (), RectanglePlacement::centred, enabled ? 1.0f : 0.4f);
   ```

   - `createFromSVGFile` resolves external references relative to the file; parse at load
     (`BinaryData` strings avoid a file dependency at runtime).
   - `Drawable::getName()` carries the SVG `id` attribute, so a composite can be searched
     for a specific layer to recolour or hide.
   - `getDrawableBoundsUntransformed()` is the art's own extents; `getContentBounds()` is
     the SVG `viewBox` — use the viewBox for layout, since that is the designer's intent.
   - Sub-elements: `DrawablePath` (via `DrawableShape`) `setPath` + `setFill` /
     `setStrokeFill` / `setStrokeType`; `DrawableText` `setText` / `setFontHeight` /
     `setFontHorizontalScale` / `setBoundingBox (Parallelogram<float>)` / `setJustification`.
   - `Drawable::setClipPath` clips by another drawable; `Listener::drawableBoundsChanged`
     notifies when art changes size.

9. **For icons in buttons, use `DrawableButton`** (`DrawableButton (name, style)` then
   `setImages (normal, over, down, disabled, normalOn, overOn, downOn, disabledImageOn)`).
   It keeps **internal copies** of every drawable, so mutating your source afterwards does
   nothing. Styles: `ImageFitted`, `ImageRaw` (you guarantee the size),
   `ImageAboveTextLabel`, `ImageOnButtonBackground` (stretched to the button's proportions
   and coloured by `TextButton::buttonColourId` / `buttonOnColourId`),
   `ImageOnButtonBackgroundOriginalSize`, `ImageStretched`. `setEdgeIndent` controls the
   ~3 px default gap; missing `…On` images fall back to their "off" counterparts.

10. **Keep repaint cheap and honest.** `repaint()` marks the whole component dirty;
    `repaint (Rectangle<int>)` marks a region — use it for a moving thumb or a scrolling
    meter. Neither paints immediately and neither guarantees timing.
    - Guard expensive work: `if (! g.clipRegionIntersects (expensiveArea)) return;`
    - `setBufferedToImage (true)` for a component whose `paint()` is expensive and whose
      children change more often than it does.
    - `setOpaque (true)` only if `paint()` really covers every pixel, otherwise components
      behind it leak through.
    - `beginTransparencyLayer (opacity)` / `endTransparencyLayer()` for group-alpha (so
      overlapping semi-transparent strokes do not double-darken). It saves state too —
      every begin must be matched, on every path out of the function.
    - `saveState()` / `restoreState()` (or `Graphics::ScopedSaveState`) around any
      `addTransform`, `setOrigin`, `reduceClipRegion`, colour or font change you make inside
      a loop or helper.
    - `createComponentSnapshot` when you need the composited result as an `Image`.

11. **Drive animation from the message thread.** A meter/spectrum display reads DSP values
    that the processor publishes through atomics or a FIFO and `repaint()`s from a
    `Timer`/`ValueListener`. Nothing on the audio thread may call `repaint()`, touch a
    `Component`, or hold a reference to a UI object (AGENTS.md §7).

## Pitfalls

1. **Setting the scheme after your colour-id overrides wipes them.** `setColourScheme()`
   runs `initialiseColours()`, which rewrites the id palette from the scheme. Scheme first,
   overrides second; re-apply overrides after every scheme switch.
2. **Mutating `getCurrentColourScheme()` (a mutable `ColourScheme&`) changes V4's own
   drawing but not the id palette** — widgets whose `draw*` reads ids keep the old colours.
   Call `setColourScheme (modifiedScheme)` when you want both.
3. **`ColourScheme`'s constructor `static_assert`s on exactly 9 colours in enum order.**
   A 10th argument or a reordered list is a compile error or a subtly wrong theme — build
   from one of the four static schemes and `setUIColour` the slots you care about.
4. **A V4 special case compares whole schemes:** in the slider path,
   `getCurrentColourScheme() == getGreyColourScheme()` selects different linear-bar drawing.
   If your palette happens to equal Grey you inherit its slider look; tweak one slot if
   that is not intended.
5. **An unset colour id is opaque black**, not transparent — the classic invisible-label /
   black-panel result. Set `0x00000000` explicitly where you want nothing drawn.
6. **`Component::setColour` does not repaint** (it fires `colourChanged()` only);
   `LookAndFeel::setColour` notifies nobody. After retheming live UI, call
   `sendLookAndFeelChange()` on the editor — it runs `lookAndFeelChanged()`, `repaint()` and
   `colourChanged()` recursively.
7. **`setDefaultLookAndFeel` borrows.** Pass a member/static, and hand back `nullptr` (or
   install a new one) before the object dies, or components will keep using freed memory.
8. **`fillAll()` fills the clip region, not `getLocalBounds()`.** Combined with a partial
   `repaint (area)`, assuming the whole component was cleared leaves stale pixels; either
   repaint the affected area's dependencies or `setBufferedToImage (true)`.
9. **`setColour` cancels gradients.** Order matters: `setFillType` / `setGradientFill` must
   come *after* the last `setColour`. And `setOpacity` has no effect on a gradient — bake
   alpha into the gradient colours instead.
10. **`drawRect` / `drawRoundedRectangle` strokes are drawn inside the given rectangle** and
    thicker lines extend inwards, while `strokePath` straddles the outline. Mixing them at
    the same bounds shifts edges by half a thickness.
11. **Sub-pixel blurring:** a 1 px border drawn on float bounds with a fractional coordinate
    anti-aliases into two grey rows. Use `Rectangle<int>` (or `reduced (0.5f)` for a crisp
    hairline on a float area) and integer `drawRect`.
12. **Rebuilding `Path`s, fonts or parsed SVG on every `paint()` call** is the top cause of
    a UI that pegs the CPU during drag/animation: `Font` construction, `Typeface` lookup,
    `Path::getLength()` and SVG parsing all allocate. Cache them; `Path::precalculateSpans()`
    is not a substitute for caching the path itself.
13. **`Path::contains` on an open sub-path gives false results** (documented), and it
    respects the winding rule — a self-overlapping shape with the default non-zero winding
    fills solid where you expected a hole. Call `closeSubPath()` and, for holes,
    `setUsingNonZeroWinding (false)` or reverse the inner sub-path's direction.
14. **Rotation direction and pivot:** `AffineTransform::rotated` is clockwise radians
    (screen Y is down) about the origin unless you pass a pivot; `rotation (a)` followed by
    `translation (x, y)` is not the same as `translation (x, y)` followed by `rotated (a)`.
    Members append to the current transform; statics start from identity.
15. **`Drawable::setDrawableTransform()` and `Component::setTransform()` are mutually
    exclusive on the same object** (documented: "using both will lead to unpredictable
    behaviour") — a Drawable's origin is not `Component::getPosition()`. Use
    `setDrawableTransform` / `setDrawableTransformToFit`, or `DrawableComponent`'s
    `setTransformToFit`, and pass `BoundsToEnclose::contentBounds` for SVG so the `viewBox`
    drives fitting (`drawableBounds` is the pre-JUCE-9 behaviour; `DrawableComponent` itself
    is a compatibility shim).
16. **Scaling a non-integer-sized SVG with `stretchToFit` distorts strokes.** Use
    `centred` (aspect-preserving) and pick source art at a convenient em size, or accept
    `fillDestination` and clip. Set `g.setImageResamplingQuality` only for raster `Image`
    draws — it does not affect vector `Drawable` rendering.
17. **`createFromSVGString` / `createFromSVGFile` can return null** (bad markup, unsupported
    construct, missing file) and SVG support is a subset of the spec. Check for null at load
    time and assert; a null `Drawable` dereferenced inside `paint()` crashes the editor, not
    the DSP.
18. **`DrawableButton` copies its images** — recolouring the source `Drawable` afterwards
    changes nothing. Re-call `setImages`, or theme the art before handing it over.
19. **Theme switches must also reach `Font`s.** V4 sizes fonts from the LAF
    (`getTextButtonFont`, `getComboBoxFont`, alert fonts); overriding colours only leaves
    JUCE's default typeface in place. Set `setDefaultSansSerifTypeface` (or override
    `getTypefaceForFont`) once on the LAF — and note `getDefaultMetricsKind()` defaults to
    `TypefaceMetricsKind::portable`, so overriding it reflows every label.
20. **Do not paint from the audio thread or hold UI pointers in DSP.** Publishing sample
    data means a lock-free FIFO or atomics read on the message thread; `repaint()` from a
    `Timer`, not from `processBlock`. This is the AGENTS.md §7 boundary, and a violated one
    shows up as drop-outs, not as a rendering bug.
21. **JUCE stays at the plugin boundary.** Nothing under a JUCE-free library (`src/Dsp`,
    `src/Container`) may include `juce_graphics`/`juce_gui_basics` headers just to share a
    colour or a shape — pass plain numbers and convert at the editor seam (AGENTS.md §9).
