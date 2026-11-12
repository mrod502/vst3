---
name: plan-visualization
description: Draw plan diagrams that stay true — a dependency chart whose colour and glyph encode status, edges that carry reasons not preferences, shape=kind, and a validated status palette with templates. Use when an index needs a flow diagram, when a status chart drifts from its table, or when a decision's consequences need a readable graph.
source: learned
---

# Plan visualisation: diagrams that agree with their table

Written down from the chart in `plan/M5-impl-plan/INDEX.md` §2.1 — the only mermaid diagram in `plan/` so far; the
M3 and M4 indexes carry the item-map *table* that it is generated from, and no graph. The colour rules come from the
bundled `dataviz` skill, which this one defers to for real *data* charts (step 10 below).

## When to Use

- An index needs a picture of what blocks what.
- A diagram exists and its table changed, or nobody trusts the diagram any more.
- A decision's knock-on effects need to be shown to someone who will not read nine item files.
- A status chart's colours need picking — and colourblind-safety, dark mode, or "why is everything green?" is in scope.

## The Contract (read first)

**The table is the authority; the diagram is a view of it.** Every node maps to a row, and the diagram is updated
**in the same edit** as the row. A diagram that drifts is worse than none, because it is read first and quoted
afterwards.

**Two channels encode status, never one.** Colour alone fails greyscale print, colourblind readers (~8 % of men), and
a terminal that strips the fill. So: **fill = status**, and **a leading glyph in the label = the same status**.
Readers who cannot see the fill still get it. Shape carries **kind** and never status — one channel each, or the
legend becomes a decoder ring.

**Every edge label is a reason.** `A --> B` says nothing; `A -. "D-0xx B settles which side feeds the detector" .-> B`
says what B cannot do without A. If you cannot write the reason, the edge is a preference and the ordering is
arbitrary — say so with an undotted line only where the dependency is a shared file, and name the file.

**Solid vs dotted is a real distinction:** solid = this cannot start until that finishes; dotted = it *should* read
that first, or that output feeds a threshold, but nothing breaks if it runs in parallel. Collapsing the two is how a
plan gains a fake critical path.

## The palette (validated, reuse verbatim)

Templates, in this skill's directory:

| File | What it is |
|---|---|
| `templates/dependency-chart.mmd` | Drop-in graph skeleton in the reference shape: one node of each kind, the six `classDef` lines, the legend node. Comment-free, because `mermaid.parse()` rejects a `%%` line before the diagram type |
| `templates/palette.md` | The authoritative figures — OKLCH L/C, luminance, contrast per fill, the greyscale-collapse list, the colourblind ΔE table, the rejected candidate, and how to add a status |
| `templates/coverage-matrix.md` | The item map, criterion-coverage and parked tables the diagram is drawn from, with the rule for every column, plus the reading rules and the consistency checks |
| `scripts/figures.py` | Regenerates every colour figure in `templates/palette.md`, for the shipped set or a candidate one |
| `scripts/check_diagrams.mjs` | Parses every mermaid fence in a markdown file (or a whole `.mmd`) with mermaid 12 and reports PASS/FAIL per block |

The short version of the palette — six statuses, one fill and one glyph each:

| Status | Fill | Glyph | Why this hue |
|---|---|---|---|
| `done` — Complete (measured) | `#3f9d5c` | `✓` | mid green, inside both theme bands |
| `record` — a decision answered by measurement, or any measured fact | `#8f6fc4` | `▣` | deliberately **not** orange: orange already means in-progress |
| `wip` — In progress | `#d0821f` | `▶` | the lightest mark in the set, and the only one that is |
| `blocked` — Blocked | `#d05a52` | `✕` | plus `stroke-width:2px` — a cue that survives greyscale |
| `todo` — Not started | `#4f86c6` | `○` | |
| `context` — context, parked, out of scope | `#7b8794` | `▢` | grey = no active work here; the dashed stroke = not this milestone's writable surface |
| Label text, every node | `#0b1220` | — | near-black: the fills sit 3.05–4.01:1 on white and 4.67–6.15:1 on near-black |

`▣` does double duty and the fill disambiguates it: on a green node it reads *Complete (closed on record)* — §2.1's
`M4` node — and on a purple node it reads *a measured record*. `✓` on green is *Complete (measured)*. Those are the
two `Complete` flavours of `plan-work-tracking`'s five-status vocabulary, and no glyph stands for any other status.

`templates/palette.md` carries the measured OKLCH L/C and luminance per fill, the pair-by-pair colourblind ΔE list,
and the command that regenerates every figure in it. Do not restate those numbers here — they would drift.

**Three facts from those figures decide the design of the palette**, and together they are why the glyph is not
decoration:

- The pair that collapses in **hue** — `wip` vs `blocked`, ΔE 5.9 deuteranopia / 4.0 tritanopia — carries a 1.31:1
  luminance gap (only `record`/`wip` is wider, at 1.32:1), so it still separates in greyscale and under print.
- The pairs that collapse in **luminance** — `record`/`blocked` 1.01:1, `todo`/`context` 1.03:1, `blocked`/`todo`
  1.06:1 — are separated in hue under all three simulations; none of them appears in the validator's close-pair list.
- **Nine of the fifteen pairs sit within 1.15:1 of each other in luminance.** No ordering or ranking may be read off
  these fills: they are status labels, never a scale. A chart that needs a scale uses a sequential ramp, from the
  dataviz skill, not this set.

Anything added to this set must be checked on both axes: a new status that differs from a neighbour only marginally in
hue **and** only marginally in luminance is unreadable by every route except the glyph, and that is a design accident,
not a palette.

**Validate any change to these hexes**, in both modes, before it goes in a document. The gate ships with the bundled
`dataviz` skill, so resolve its base directory rather than hard-coding one (if the skill path is unknown to you,
`find / -path '*bundled/dataviz/scripts/validate_palette.js' 2>/dev/null` locates it):

```bash
node "<dataviz-skill-base>/scripts/validate_palette.js" \
     '#3f9d5c,#d0821f,#d05a52,#4f86c6,#8f6fc4,#7b8794' --mode dark
node "<dataviz-skill-base>/scripts/validate_palette.js" \
     '#3f9d5c,#d0821f,#d05a52,#4f86c6,#8f6fc4,#7b8794' --mode light
```

Both runs print the status word `WARN` and exit 0. That is the accepted state for this set: `FAIL` (exit 1) is
reserved for a mark outside the lightness band or under 2.5:1 against the chart surface **unsimulated**, and the
WARN lines are the colourblind close-pairs and the grey's low chroma — exactly what the glyph, the dashed border and
the heavier `blocked` stroke exist to cover. The light run adds four accepted WARNs where `wip` and `blocked` fall
under 2.5:1 against the light surface *under simulation*. The full list is in `templates/palette.md`; do not
re-derive it.

## Procedure

1. **Write the table first**, then generate the graph from it — node per row, in row order, with the status column
   deciding the class. Never let the diagram be where you discover the structure.
2. **Start from `templates/dependency-chart.mmd`.** Copy it, keep the six `classDef` lines byte-identical to it and
   to `plan/M5-impl-plan/INDEX.md` §2.1, replace the nodes and edges. The template carries no comments on purpose —
   see the last pitfall.
3. **Give every node `glyph + number + name + the one clause of substance`.** A node that says only `06 · Cross
   product` is a table lookup away from being useful; a node that says `06 · cross product — 12 triples × 7 plans,
   ~83 s` carries its own cost. Keep it under ~110 characters or the box dominates the layout.
4. **Classify every node.** An unclassified node renders with mermaid's default theme and silently looks like a
   status that has no meaning. Add the `context` class to anything outside this milestone, including "M4, as
   delivered" and the next milestone's gate.
5. **Shape vocabulary — three things, and status is none of them:** rectangle = a work item, an artefact, a milestone
   or the gate; `{"…"}` diamond = a decision from the decisions table, because those are the nodes a reader can act
   on; and the `context` class's **dashed stroke** = outside this milestone's writable surface. A measurement is a
   `record`-classed node, not a fourth shape.
6. **Label edges with the reason and cite the decision** when a decision created the edge (`D-024 A`), and label the
   *absence* of an edge where readers will assume one: "02 does not gate 03, and that is not an oversight — …".
7. **Keep a legend, in one of the two places that get read.** Either a borderless node inside the graph (the
   template's `legendbox` class) or the first reading-rule bullet under the fence, which is what §2.1 does. Not both.
8. **Put the reading rules under the diagram**, three to five bullets: shape=kind, fill+glyph=status, edges=reasons,
   dotted=not blocking, and **the maintenance rule** ("update this diagram in the same change that updates the
   table"). Without the last one it rots within a session.
9. **Parse it before you commit** — `node scripts/check_diagrams.mjs <file.md>` checks every fenced block in the
   document, and it catches the class of error a preview glance does not: a label that ends the node early, a
   `classDef` with a stray character, an edge form the target renderer will not take. Then check the *content*: for
   every edge, what breaks if the two swap order; for every blocked node, is the blocker named in the label and not
   only in the table.
10. **For anything that is actually data** — cost over configurations, error in dB per cell, a time-series — use the
    **dataviz skill** instead: its own palettes, its own encode/mark selection, and its own validator. This skill
    covers state and structure diagrams only.

## Pitfalls

- **Colour-only status.** The most common failure and the one that survives review, because the author can see it.
- **A green chart as a morale object.** If most nodes are green and none of them is measurable, the palette is
  telling the truth about a lie upstream. The chart must be able to look bad: keep `blocked` reachable and use it.
- **Encoding status in shape as well** (red octagon for blocked, green tick shape for done). Then shape stops meaning
  kind and the legend becomes a decoder ring.
- **Edges that mean "mentioned in the same document".** Nine nodes and twenty edges of nothing, and the critical path
  disappears. Delete every edge whose reason you cannot say aloud.
- **Editing the diagram to fix a disagreement with the table.** Fix the table (or the plan) and let the diagram
  follow. The diagram is not where a claim gets corrected.
- **Reusing the status palette for data.** A bar chart of CPU cost by channel count is not a status chart: that is
  `dataviz`'s categorical palette and its hue-normalise step-6 rule, not `#3f9d5c`.
- **Picking "nice" hexes.** A darker, "professional-looking" set (`#166534,#b45309,#b91c1c,#475569,#0e7490,#334155`)
  fails the dark-mode run on three of its six marks — `#166534` at OKLCH L 0.448 / 2.488:1, `#475569` at 0.446 /
  2.341:1, `#334155` at 0.372 / 1.713:1 — and exits 1. Deep greens and slate greys are the usual casualties; the
  saturated mid-tones in the table above are what survives both bands. Run the validator, do not eyeball.
- **White text on these fills.** Contrast drops to 3.05–4.01:1 (`#d0821f` worst at 3.05:1) where 4.5:1 is the bar for
  body text. `color:#0b1220` in every `classDef`.
- **Mermaid syntax that parses in one version and not in another.** Measured against mermaid 12.3's `parse()`:
  a `%%` comment line before `flowchart TD` is **not** stripped and the file fails to parse, so templates stay
  comment-free; `classDef` is one line with single spaces; every edge label is quoted, in either position
  (`A -->|"reason"| B`, which is what §2.1 uses, or `A -- "reason" --> B`); the `A & B --> C` chain parses on 12.3,
  and where a document may meet an older viewer, spell the edges out — §2.1 does. Run the checker in step 9 rather
  than trying to remember the list.

## Verifying a Change

1. Node count == row count in the table it mirrors, and each node names its row.
2. Every node carries a class; every class in use has a `classDef`; every `classDef` colour passes the validator in
   **both** modes.
3. Every node's label carries its glyph, and the glyph↔status mapping matches `templates/palette.md`.
4. Every edge label states a reason; every blocked node names its blocker in the label.
5. The legend exists, and the reading rules under the diagram include the maintenance rule.
6. Every block parses — `node scripts/check_diagrams.mjs <the file>` exits 0 — **and** the page renders it in a
   markdown preview, which is the only check that catches a graph so wide it pushes the page; a wide one sits inside
   an `overflow-x:auto` container.
7. Greyscale test: convert to greyscale or print it — status still readable from glyphs, borders, and shape-of-kind.
