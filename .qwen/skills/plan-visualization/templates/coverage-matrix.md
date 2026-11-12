# Coverage and status tables — the templates the diagram mirrors

The dependency chart is a **view** of these tables, so they come first and the chart is generated from them in the
same edit. Column rules are stated under each table because a column without a rule becomes a prose column, and a
prose column cannot be checked.

## 1. Item map (the authority the diagram is drawn from)

```markdown
| # | Item | Covers | Depends on | Status | Owner |
|---|---|---|---|---|---|
| 01 | <file-backed title> | §<n>[, §<n>] | — | Not started (plan written, <date>) | — |
| 02 | <title> | §<n> | 01 | Not started | — |
| 03 | <title> | §<n>, §<n> | — (parallel to 02) | Not started | — |
| 04 | <title> | §<n> | 01, 02, D-0xx | Blocked — <who or what clears it> | — |
| 06 | <title> | §<n> | 01 | In progress — <one clause of what is being done> | <agent/session> |
| 09 | <title> | §<n> | all above | Not started | — |
```

Column rules:

- **#** — the item number, and it *is* the file's `NN-` prefix. A row with no file is a theme, not an item.
- **Covers** — the milestone-spec sections the row closes. Empty means the row is not scope; more than one row may
  cover a section, and where the mapping is not one-to-one, say so in a line under the table.
- **Depends on** — item numbers and `D-0xx` decision ids only, never prose. Where the honest answer is "nothing, but
  it should read X first", write `— (parallel to 02)` and put the read-first reason on the diagram as a dotted edge.
- **Status** — one of the five, with its basis **in the cell**. `Complete (measured)` is legal only once the item's
  evidence row exists; `Blocked` must name the clearant.
- **Owner** — blank until claimed. `In progress` with a blank owner is the state that causes two agents to edit one
  file.

## 2. Milestone-criterion coverage (what the milestone promised vs who closes it)

```markdown
| Milestone § | Criterion, in decidable form | Owner item(s) | What makes it decidable | Basis once closed |
|---|---|---|---|---|
| 7.1 | <claim> at <rung>, decided by <named leg> | 02 | <the leg or record that exists> | <command · build type · date · figure> |
| 7.2 | <claim> | — | **open: no owner** | — |
| 7.3 | <claim> | 05, 07 | <which of the two closes which half> | <…> |
| ~~7.4~~ | ~~<criterion struck because it lost its domain>~~ | — | struck: F-0xx — <one clause> | — |
```

Column rules:

- Restate the criterion in its decidable form here; a link back to the milestone's adjectives is not a criterion
  (`plan-acceptance-criteria`).
- **A row with no owner is a finding**, written `open: no owner` and not left blank — blank reads as "somebody's
  problem" and survives close-out.
- **A struck criterion stays visible with strikethrough, the finding id, and why.** Deleting it is how a milestone
  quietly stops promising something.
- **Basis once closed** carries the four things: command, build type, date, figure or named leg. `green` is not a
  basis. `As expected` is not a basis.

## 3. Parked items (open on purpose, with a way back)

```markdown
| Parked | Why it stays parked | What already supports it | Reopens when |
|---|---|---|---|
| <thing> | <decision nobody took / host nobody has / listening test nobody ran> | <the measurement, leg, or row that exists today> | <the specific event, and the milestone or person who acts on it> |
```

Column rules: an entry without a **reopens when** is a deletion candidate, not a parked item; an entry whose support
column is empty is a guess; anything here that also appears as an item row is a contradiction — one of the two is
wrong and the index has to say which.

## 4. Reading rules that go under the diagram

Copy these five and mean them:

```markdown
- **Shape is kind**: rectangle = a work item, an artefact, a milestone or the gate; diamond = a decision from the
  decisions table; the grey class's dashed stroke = outside this milestone's writable surface.
- **Fill and glyph are status** (`✓` measured · `▣` closed on record, and `▣` on purple = a measured record ·
  `▶` in progress · `✕` blocked · `○` not started · `▢` context). The item map above is authoritative; this diagram
  is redrawn from it.
- **Every edge label is a reason**, and a dotted edge means "reads it first", not "waits for it".
- **A node with no colour has no meaning here** — every node is classified, including last milestone's work.
- **Update the diagram in the same change that updates the table**, node for node.
```

## 5. Consistency checks before committing an index edit

1. Node count equals item-map row count, and node ids map to item numbers.
2. Every status cell's colour class matches the glyph prefix's status.
3. Every `Depends on` entry has an edge; every edge's reason appears in the sequencing section or on the edge itself.
4. Every `Blocked` row names its clearant in the table **and** in the node label.
5. Every parked entry appears on the diagram as a grey node or is explicitly out-of-frame in the reading rules.
6. No `Complete` row lacks an evidence row; no evidence row lacks a figure or named leg.
7. Decision ids cited on edges exist in the decision log, and the ledger's next-free line has moved past them.
