# Status palette — the measured figures

Authoritative numbers for the plan-diagram palette in `plan/M5-impl-plan/INDEX.md` §2.1 — the reference instance, and
so far the only diagram in `plan/`. `SKILL.md` deliberately does not repeat these figures; this file is the only place
they are stated, and every one of them is regenerable with the two commands below.

## Regenerating

```bash
# 1. hue / lightness / contrast, and the greyscale-collapse list
python3 scripts/figures.py                       # from this skill's directory
python3 scripts/figures.py '#3f9d5c,#d0821f'     # any candidate set, same table

# 2. the dataviz gate: lightness bands, surface contrast, colourblind DeltaE.
#    <dataviz-skill-base> is the base directory the bundled dataviz skill reports when it loads.
node "<dataviz-skill-base>/scripts/validate_palette.js" \
     '#3f9d5c,#d0821f,#d05a52,#4f86c6,#8f6fc4,#7b8794' --mode dark
node "<dataviz-skill-base>/scripts/validate_palette.js" \
     '#3f9d5c,#d0821f,#d05a52,#4f86c6,#8f6fc4,#7b8794' --mode light
```

Run 1 prints the OKLCH, luminance and contrast columns of the table below; *Extra cue* is chart convention, not a
computed figure. Run 2 prints `WARN` and exits 0 in both modes; its full warning list is reproduced further down so
that a future edit can tell an accepted warning from a new one.

## The set

| Status | Fill | Glyph | OKLCH L | OKLCH C | Luminance Y | vs `#0b1220` | vs `#ffffff` | Extra cue |
|---|---|---|---|---|---|---|---|---|
| `done` — Complete (measured) | `#3f9d5c` | `✓` | 0.623 | 0.132 | 0.259 | 5.52:1 | 3.39:1 | — |
| `record` — a measured fact, or a decision answered by measurement | `#8f6fc4` | `▣` | 0.609 | 0.129 | 0.212 | 4.67:1 | 4.01:1 | — |
| `wip` — In progress | `#d0821f` | `▶` | 0.674 | 0.141 | 0.295 | 6.15:1 | 3.05:1 | lightest mark in the set |
| `blocked` — Blocked | `#d05a52` | `✕` | 0.615 | 0.151 | 0.213 | 4.70:1 | 3.99:1 | `stroke-width:2px` |
| `todo` — Not started | `#4f86c6` | `○` | 0.611 | 0.114 | 0.228 | 4.96:1 | 3.78:1 | — |
| `context` — context, parked, out of scope | `#7b8794` | `▢` | 0.618 | 0.024 | 0.237 | 5.11:1 | 3.66:1 | `stroke-dasharray:5 4` |
| Label text, every node | `#0b1220` | — | — | — | — | — | — | never white |

`▣` is the one glyph with two readings, separated by the fill: green + `▣` = *Complete (closed on record)*, purple +
`▣` = *a measured record*. No glyph encodes a node's **kind** — shape does that (rectangle = item, artefact,
milestone or gate; diamond = decision), which is why the decision nodes in §2.1 carry a status glyph and nothing else.

Exact `classDef` block — byte-identical between `templates/dependency-chart.mmd` and
`plan/M5-impl-plan/INDEX.md` §2.1; copy it, do not retype it:

```
    classDef done fill:#3f9d5c,color:#0b1220,stroke:#1d5c33;
    classDef record fill:#8f6fc4,color:#0b1220,stroke:#5b3f8c;
    classDef wip fill:#d0821f,color:#0b1220,stroke:#96590f;
    classDef blocked fill:#d05a52,color:#0b1220,stroke:#963a34,stroke-width:2px;
    classDef todo fill:#4f86c6,color:#0b1220,stroke:#365f92;
    classDef context fill:#7b8794,color:#0b1220,stroke:#5b6673,stroke-dasharray:5 4;
```

`legendbox` (`fill:none,stroke:none,color:#0b1220`) is template-only — §2.1 puts its legend in the prose under the
fence instead, and the two are not to be combined.

The strokes are hue-matched darker variants of their own fill, so a node's edge reads against either surface. The
validator governs the **fills** (the marks); the borders are not marks and were not put through the mark bands.

## Greyscale collapse (why the glyph is load-bearing)

`scripts/figures.py` flags pairs whose luminance ratio is under 1.15:1. **Nine of the fifteen pairs are flagged:**

| Pair | Luminance ratio |
|---|---|
| `record` / `blocked` | 1.01:1 — identical in greyscale |
| `todo` / `context` | 1.03:1 |
| `blocked` / `todo` | 1.06:1 |
| `record` / `todo` | 1.06:1 |
| `done` / `context` | 1.08:1 |
| `blocked` / `context` | 1.09:1 |
| `record` / `context` | 1.09:1 |
| `done` / `todo` | 1.11:1 |
| `done` / `wip` | 1.11:1 |

Widest gaps: `record`/`wip` 1.32:1, `wip`/`blocked` 1.31:1, `wip`/`todo` 1.24:1, `wip`/`context` 1.20:1. Two
consequences that are design rules, not observations:

1. **The fills are status labels, never a scale.** No ordering may be read off them; a real scale needs a sequential
   ramp from the dataviz skill.
2. **Every node carries its glyph**, even when the fill looks sufficient, because most pairs are one channel deep
   rather than two.

## Colourblind close pairs (validator output, accepted knowingly)

`--mode dark` prints 19 WARN lines and no FAIL. The DeltaE values below are CIE76 under the validator's simulations
and are identical in both modes; only the four light-surface contrast lines are light-mode additions, and `--mode
light` therefore prints 23.

| Pair | protanopia | deuteranopia | tritanopia |
|---|---|---|---|
| `wip` / `blocked` | 23.682 | **5.859** | **4.044** |
| `done` / `context` | 26.718 | 9.704 | 14.300 |
| `record` / `context` | 26.325 | 14.545 | 8.578 |
| `record` / `todo` | 16.712 | 27.266 | 19.598 |
| `done` / `record` | — | 20.633 | 19.855 |
| `done` / `blocked` | 36.877 | — | — |
| `done` / `todo` | — | — | 20.043 |
| `todo` / `context` | — | 39.578 | 22.741 |

Also warned, and intentional: `#7b8794 has low OKLCH chroma 0.024 and may read as gray` — reading as grey is the
point of that entry.

Light mode adds four accepted warnings where `wip` and `blocked` drop under the 2.5:1 surface floor *under
simulation*: `wip` 2.232 (protanopia) and 2.014 (deuteranopia); `blocked` 2.469 (protanopia) and 2.154
(deuteranopia). Node fills are large areas, and the status is carried by glyph plus stroke, so these are accepted
rather than chased.

**Mitigation that is already in place for the worst pair** (`wip`/`blocked`, ΔE 4.0–5.9): 1.31:1 luminance gap, the
`stroke-width:2px` that only `blocked` gets, and `◈` vs `✕`. A pair this close in hue is survivable **only** because
of those two non-colour cues — do not remove either to "tidy" the chart.

## Thresholds the validator enforces (learned from a rejected candidate)

`FAIL` (exit 1) is raised for a mark whose OKLCH lightness falls outside the mode's band, or whose **unsimulated**
contrast against the chart surface is under 2.5:1. Colourblind-simulated contrast and close pairs are `WARN`.

The rejected candidate `#166534,#b45309,#b91c1c,#475569,#0e7490,#334155` failed the dark run on three of six marks:

| Fill | Why it failed |
|---|---|
| `#166534` | L 0.448, outside the dark band 0.45–0.92; contrast 2.488 |
| `#475569` | L 0.446, outside the band; contrast 2.341 |
| `#334155` | L 0.372, outside the band; contrast 1.713 |

The lesson is not "avoid dark colours" but: **dark greens and slate greys are the usual casualties**, and a set that
looks calm in a colour picker can be under the floor for three of its six marks. Saturated mid-tones in the 0.6–0.68
OKLCH L range clear both bands.

## Adding a status

1. Pick a hue that is not adjacent to an existing one, then run `python3 scripts/figures.py '<candidate set>'`.
2. If the new entry lands within 1.15:1 of a neighbour it must not be a colour-only distinction either — give it a
   glyph **and** a stroke treatment, and prefer to merge it into an existing status instead.
3. Run the validator in both modes. `FAIL` is a stop; a new WARN close-pair needs a written mitigation in this file,
   in the same change.
4. Add the `classDef`, the glyph row above, and the legend entry — in the same change as the first diagram that uses
   it. A status that exists in the palette but not in the legend is a decoding error for the reader.
