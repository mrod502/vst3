# Ui

The interface's JUCE-light half: colour values, the type ramp, the sheet's geometry — pure data and
arithmetic, no JUCE in the include set or the link line. `src/UiJuce` is the skin that consumes them.
The split exists so every number a test could check is reachable from a console program with no
display.

## Headers

| Header | Holds |
|---|---|
| `Palette.hpp` | five `std::uint32_t` tokens in `0xAARRGGBB`, plus `relativeLuminance`, `contrastRatio` (WCAG 2.1) and `hslLightness` |
| `TypeRamp.hpp` | seven roles, two weights, one em each, plus the horizontal squeeze for the two roles that have one. `resolve(role, k)` is the only route from the table to a pixel figure |
| `Grid.hpp` | the sheet's millimetres (the only place they are named), `kOf(widthPx)`, and a `Grid` answering `box(1…12)`, `span(col, row, cols)`, `marginPx`, `gutterPx`, `cellPx`. Also the window range the editor offers: `kMinimumWindowWidthPx` 595, `kMaximumWindowWidthPx` 2380, their heights, and `kSheetAspectRatio` |
| `BarGeometry.hpp` | a bar's body (17.7 mm), the `round(0.8 mm × k)` inset applied **once** in `insetAt`, the value fill (`fillRect()`, and `fillRect(true)` for the active state), the 19.8 mm label column, `valueForX` / `xForValue`, and `hitRect()` — empty for a `readOnly` meter |
| `PieGeometry.hpp` | `wedgeAngles(value, mode)` for a full turn and a symmetric half turn, the face's centre and inscribed radius, `forEachWedgePoint` (the walk a caller turns into a path), `radialQuad` and `datumRadialWidthAt` for the datum's ink mark, `hitTestPie`, and `discRectOf(face)` — the disc inscribed in the face |
| `PanelGeometry.hpp` | the ink display panel's content inset — one gutter — and `inkContentRect(panel, k)`, inset once and symmetric; the trace contract below it |
| `TextGeometry.hpp` | `kRotationRadians` (−90°), `kRotatedCaptionInsetMm` (2.0), the `TextOrientation` / `TextAlignment` / `TextSurface` vocabularies, `kInkFieldWordToken`, and the rotated caption's placement |
| `snapToQuantum` | the design's one quantisation expression |

## The two rules these headers exist to keep

**Five tokens, no sixth.** One file holds every colour in the product. No `juce::Colour`, `Colours::`
name or hex literal appears in `src/UiJuce` or `src/Entangle`; `ui::Theme::toColour()` is the single
conversion. Any two tints contrast by 1.03–1.15, so **a tint can never carry a state** — state is ink,
or geometry, or nothing.

**`k` is the only size input.** `k = windowWidthPx / 315.0`, everything rests in millimetres, and no
widget carries a pixel figure of its own. Rounding is one-time and absolute: an origin is
`round(mm × k)` from the millimetres, never a rounded origin plus a rounded offset, because
accumulated rounding drifts the gutter. A reserve built from a *font* figure is still a grid figure,
so it rounds once as a multiple of the em — `round(n × em)`, not `n` times as a stepped `round(em)`.

## State vocabulary

| State | A bar | Any control |
|---|---|---|
| Rest | fill inset | — |
| Active | fill flush, nothing else differs | — |
| Inert (`!isEnabled()`) | body traded for `ground`, words kept | never a hue, never a stroke |
| Read-only meter | enabled, no active state, empty `hitRect()` | keeps reporting while refusing the pointer |

## The trace contract (`PanelGeometry.hpp`)

Both display panels draw from these, and every figure a leg checks is here rather than inside a `paint()`.

| Name | Contract |
|---|---|
| `kTraceWindowSeconds` (2.0) | the scrolling window; the publication's ring is sized against it, so a panel never asks for history the writer cannot hold |
| `kMaxTraceColumns` (1030) | the widest ink field the sheet can produce — a two-cell span at the resize ceiling less one content inset — rounded up to the next ten. Every column buffer is this size and none grows |
| `kMaxTraceHistory` (6000) | 2 s at the fastest hop and rate the shell configures (H = 64 at 192 kHz). Past it a trace stops short of the left edge rather than a buffer growing |
| `traceThicknessAt(k)` | one column at the reference width, two at the ceiling, never zero |
| `tracePixelsPerSecond(width, seconds)` | the panel's width over the window; 0 for a degenerate rect |
| `CorrectionAxis` / `EnvelopeAxis`, `correctionYAt` / `envelopeYAt` | value → y on an axis **handed in** — the shell gives `gainRangeDb` and the gate parameter's own bound. A panel invents no range, and a degenerate axis answers the middle line instead of dividing by zero |
| `amplitudeToDb` | the log, with silence at `lowest()` so it lands on the axis's bottom edge rather than on a floor invented here |
| `HoldPolicy`, `decimateToColumns` | readings → columns, **peak-hold or valley-hold, never an average**. Oldest first with the newest pinned to the last column; columns before the returned index are untouched so the caller draws nothing there, and a column that falls between two readings steps to the earlier one rather than gapping |
| `Spread`, `spreadOf` | the min and max of one hop's per-channel figures. One channel gives two equal figures, which is what mono actually looks like |
| `kInkFieldRuleToken` | the one rule inside an ink field is `ground`, not ink: inside an ink field the tints are the ink, and ink on ink is invisible |

**No arithmetic in `paint()`.** A panel maps values into paths in its own `refresh()` — on a data change and
on a resize — and `paint()` fills what is already built. A resize re-decimates from the held history rather
than stretching columns measured against the old width.

## Threading

Nothing here is thread-aware: every function is pure arithmetic over its arguments and `Grid` is
immutable after construction. All of it is safe on the paint path and nothing allocates. The mutable
half of the theme — the font cache — lives in `src/UiJuce`, is confined to the message thread, and is
sized at construction rather than grown.

## Build and test

```bash
cmake --build build/release --target UiTests -j4
ctest --test-dir build/release -R "ui:" --output-on-failure          # the whole UI suite
ctest --test-dir build/release -R "ui: policy:" --output-on-failure  # source-text rules
```

Tests live in `tests/`. `UiTests` links `Ui EntangleCatch2 EntangleAsserts` and nothing else — that
link line *is* the evidence that the light half has no JUCE, so adding a JUCE include ends the
criterion. `SourcePolicyTests.cpp` reads the text of `src/UiJuce` and `src/Entangle` and fails on a
colour name, a hex literal, a numeric `setFont`, or an em figure from outside the ramp table; it
cannot see a colour computed at run time, which is what `UiProbe` covers. Every case in the four UI
binaries carries the `ui: ` prefix. `-R` is a case-name filter, and a filter matching nothing exits 0,
so quote the count with the status.

## Decisions

D-024 (tokens, ramp, state vocabulary), D-025 (the shipped face), D-026 (window range), D-027 (the trace
window and the axis-is-the-bound / rule-is-the-value split), F-022, F-023,
F-024 — `docs/decisions.md`.
