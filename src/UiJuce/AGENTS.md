# UiJuce

The skin and the kit: the one `LookAndFeel_V4` that renders the design, the one place a face is
obtained, and the five controls plus the two display panels the editor uses. Every rect, inset, angle,
colour and em figure comes from `src/Ui` — **no widget computes geometry or names a size of its own.**

| File | Role |
|---|---|
| `Theme.hpp/.cpp` | the skin: five tokens as a `ColourScheme`, seven roles as `Font`s, the kit's flat paint overrides, `drawCapAlignedText`, `capHeightOf` |
| `FontFactory.hpp/.cpp` | the only place a face is obtained — from binary data, never by family name |
| `Panel` | a sheet or ink field |
| `Bar`, `PieDial` | `Slider` subclasses (bar control, dial control) |
| `ChoiceBar` | `ComboBox` |
| `Text` | `Label` |
| `ResponsePanel` | boxes 7–8: the level stage's applied correction, min and max across the processed channels, on a `±gainRangeDb` axis with one unity rule |
| `EnvelopePanel` | boxes 11–12: the three buses the level stage follows (`track` / `fill` / `trace`), on a dBFS axis handed in, with one rule at the gate's live value |

## Rules the skin keeps

- **Colour:** `Theme::toColour(token)` is the only conversion from `src/Ui`'s five tokens. No
  `Colours::` name, hex literal, `findColour` or raw `ColourId` appears in this directory or in
  `src/Entangle` — `ui: policy:` cases fail the build on any of them. `ColourScheme` has nine slots for
  five tokens, so four collisions are unavoidable and the per-id overrides go in **after**
  `setColourScheme`, because that call discards earlier overrides. The inherited slot setters are
  shadowed private so the palette stays write-once.
- **Type:** faces arrive as embedded binary data resolved with `createSystemTypefaceFor` +
  `FontOptions{}.withTypeface` (D-025). A family name is never passed to JUCE: JUCE 9 resolves no
  typeface for a family it cannot match and the null kills the process later. A missing face is a
  broken build, not a substituted font.
- **Two scales, one division.** The grid's `k` is px per mm; the ramp's `k` is 1.0 at the shipped
  sheet. `Theme::fontScaleFor` is the only place the kit divides them. Passing a window scale straight
  to `fontFor` gives 3.778×-too-big type, silently.
- **Placement is by the cap,** not the em box: `drawCapAlignedText` positions from the cap top, and
  `positionComboBoxText` anchors `topLeft` because `ComboBox` centres its child label's em box.
- **Suppression, not recolour.** JUCE's default chrome paints colours the five tokens never named, so
  the widget hooks are overridden at the leaf: `drawLinearSlider`, `…Background`, `…Outline`,
  `…Thumb`, `getSliderThumbRadius` → 0, `getSliderLayout` (carving the label column so the drag region
  *is* the painted body), `drawRotarySlider`, `drawComboBox`, `getComboBoxFont`,
  `drawButtonBackground`. Overriding only the top-level hook leaves a track, a thumb and an outline
  drawable by another path.
- **The dial fills `discRectOf(face)`,** not its widget rect, so the shape painted and the circle
  `hitTestPie` answers with are one shape.
- **Text is measured, not assumed** — a caption's box comes from the ramp and the grid; the kit clips
  at the box edge.
- **The display panels draw, they do not decide.** A panel is handed arrays of readings and the axis figures,
  holds the window it was last given, and builds its paths in `refresh()`. It reads no processor and borrows
  nothing: what it plots is a copy. Its axis and its rule are **separate** inputs (`setAxisDbRange`,
  `setGateDb`) so the rule can move inside a fixed field; `setAxisDbRange` re-plots the held history, because
  a trace drawn on the old scale under labels naming the new one is two claims about one signal. With nothing
  advancing a panel says `idle`, holds its traces, and decimates, rebuilds and repaints nothing — a flat line
  drawn at zero would read as a held signal.
- **Anything read out of a control is set after its attachment exists.** A kit control holds its own default
  (0.0) until its `SliderAttachment` is built, so a constructor that feeds a widget's value into a panel must
  run below the attachment block — F-025, which is what a §5.23 leg reddened.

## Threading

Message thread only. The font cache that keeps `paint()` from building a `Font` is confined to it,
sized at construction rather than grown; `prepareFontsFor(k)` fills it ahead of a resize. Nothing here
is reachable from the audio thread, and nothing in it holds a pointer to DSP state.

## Build and test

```bash
cmake --build build/release --target UiJuce UiJuceTests -j4
ctest --test-dir build/release -R "ui: skin:"  --output-on-failure
ctest --test-dir build/release -R "ui: probe:" --output-on-failure
ctest --test-dir build/release -R "ui: alloc:" --output-on-failure
```

`UiJuceTests` renders offscreen with the software rasteriser (no peer, no display). `UiProbe` asserts
on sampled pixels from `Component::paintEntireComponent`, which paints in local coordinates and so
needs an explicit `g.setOrigin`. `UiAllocTests` replaces global `operator new` and counts allocations
inside one paint: JUCE's own rasteriser allocates before any kit code runs, so the verdicts are
equalities against the same drawing without the kit, with absolute figures printed. Those three
programs own `main()` for the GUI initialiser and logger teardown, so they compile Catch2's
amalgamated source with `CATCH_AMALGAMATED_CUSTOM_MAIN=1`.

Links `Ui`, `EntangleFonts` and `juce::juce_gui_basics`, propagating `JUCE_WEB_BROWSER=0
JUCE_USE_CURL=0` PUBLIC — drop either and the link fails with `undefined reference to curl_easy_init`.

## Decisions

D-024, D-025, F-022 (the nine slots, the derived widget table, the null typeface), F-023 (hooks live on
the widget, the leaf-hook reachability, the rasteriser's allocations), F-024 (the metrics the embedded
face moved) — `docs/decisions.md`.
