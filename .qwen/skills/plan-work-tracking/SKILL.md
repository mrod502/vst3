---
name: plan-work-tracking
description: Keep a multi-item plan honest over many sessions — status vocabulary with a basis rule, claim-before-code, append-only corrections, evidence tables, a decision-number ledger, and a close-out pass that says what actually shipped. Use when maintaining a milestone index, marking work done, handing off between sessions, or closing a milestone.
source: learned
---

# Work tracking: a status line worth its basis

Distilled from `plan/M3-impl-plan/INDEX.md` §3 and §10, `plan/M4-impl-plan/INDEX.md` §2/§3/§10 (eight items,
several agents, several weeks) and `plan/M5-impl-plan/INDEX.md`. The failure this prevents is specific: a tracker
that says "done" three times, where the third means "planned", and the next agent builds on it.

## When to Use

- Maintaining an index or tracker for a multi-item plan.
- Marking an item complete, or reopening one.
- Two sessions have touched the same plan and you cannot tell which claims are current.
- Closing a milestone and writing the handoff.
- Deciding whether something discovered now is a row, a new item, or a parked entry.

## The Contract (read first)

**Five statuses, and each has a basis condition.** A status without its basis is an opinion:

| Status | You may write it only if |
|---|---|
| `Not started` | Nothing has been edited for it. A written plan is still `Not started` — say "plan written, <date>" in the row, not a new status |
| `In progress` | You have claimed it **before your first edit**, in one clause saying what you are doing |
| `Blocked` | You can name the blocker and the person or decision or environment that would clear it |
| `Complete (measured)` | The item's evidence table is filled: command, build type, date, and the number or named leg that carries the guarantee |
| `Complete (closed on record)` | A leg an earlier session ran and recorded. Name whose record. **Do not re-run it** — re-running is how a settled figure gets silently re-derived differently and argued about later |

Anything else — "mostly done", "should work", "green" — is `In progress`.

**The index is the authority; items cite it.** Rules, contracts, environment facts, decisions, coverage, failed
premises, parked items and sequencing live in the index. Item files hold their own design, criteria, arms,
evidence and corrections. A rule that exists only in an item file will be missed by the next item.

**Append, never tidy.** Item files are permanent: never delete evidence, never rewrite another item's status,
never smooth over a claim that has since become wrong. If your change invalidates a claim, add a dated line under
that item's evidence saying so. If a premise failed, it goes to the index's failed-premises section **with the
measurement**, and the document that misled gets edited so the next reader is not misled by the same sentence.

## Procedure

1. **Claim before you code** — one edit to your item's `Status:` line and to your row in the index's item map,
   before the first line of implementation. This is also what tells a parallel agent to pick a different file.
2. **One owner per file.** You edit your own item file and your own row. Cross-item statements are dated appends,
   not edits.
3. **Keep a decision-number ledger in the index** ("next free pair is D-024 / F-019"), and correct it **in the same
   change** that spends a number. Reserving a number in an item's §0 is fine and is how two planned items avoid
   colliding on one entry — but the reservation has to be visible in the ledger, and when one item takes a number
   another had assumed, the other's reservation moves and the index says so.
4. **Fill the evidence table as you go, not at the end.** Per criterion: the command, the build type and host, the
   date, and the figure or named leg. A row that says "as expected" or "green" is not evidence — say what was
   counted. Include the runs that did **not** behave as predicted; those are the interesting rows.
5. **Mirror the diagram in the same edit as the table** (see `plan-visualization`). The table is authoritative and
   the diagram is a view; a diagram that disagrees with its table is worse than no diagram because it gets read
   first.
6. **Keep an environment-facts section with dates on it.** Facts about the machine (core count, `/tmp` size, a
   missing tool, a filter that exits 0 while selecting nothing) decay and are re-verified cheaply. "Verified
   <date>" on each cluster of them.
7. **Record three kinds of open honestly, and never conflate them:**
   * *unstarted work* — a row in the item map;
   * *blocked work* — a named blocker, in the item and on the diagram;
   * *unknowable-here work* — a host row, a listening judgement. This is **an instrumented open question**, not a
     failure to finish: name the instrument that exists, what a good answer looks like, and who can give it.
     A milestone that closes host research as "tested" has just created a green row that stands for nothing.
8. **New scope gets a new file plus a row**, never a paragraph inside the item you are already inside. Small,
   reviewable units; a theme is not an item.
9. **Parked table with reopen triggers.** Anything gated on a decision, a host, or a listening test sits in §9 with
   *why it stays parked*, *what already supports it*, and *what reopens it*. Reopen triggers are how a parked
   decision gets picked up by the right milestone instead of being re-argued by the wrong one.
10. **Close-out is its own pass**, not a side effect of the last code edit:
    1. Re-apply the milestone-criteria coverage table; every row names what it closed on.
    2. Full suite green, including the harnesses, and the shipped artefact linking at the documented parallelism.
    3. Decision entries plus their index rows; the repo's top-level context file's index rows (targets, doc table,
       gaps).
    4. The milestone doc annotated with the measured basis, and its wrong wording corrected.
    5. The handoff written **against what actually shipped**, not against what was planned.
11. **Write the handoff with the caveats the next milestone would otherwise rediscover**: what stayed parked, which
    figures are console-only, which rows are open and instrumented, which criteria were struck and why.

## Pitfalls

- **"Complete" without a basis.** The five-status vocabulary exists because a milestone once carried a status that
  meant "the plan exists" and a later agent read it as "the code exists".
- **Editing a completed item's file.** Append a dated correction. The old claim is evidence about what was believed
  and measured at the time.
- **Re-deriving a recorded result.** `Complete (closed on record)` means someone ran it and wrote it down. Trust the
  record unless you have a specific reason to doubt it, and say which.
- **Silent decision-number collisions.** Two items taking D-024 is discovered when the log has two entries with one
  id. The ledger line and the same-change rule are the whole defence.
- **Letting the diagram drift.** Statuses change in the table and the graph keeps last week's colours.
- **Closing an item because the code merged.** The criteria, the arms and the docs are the item; the diff is a part.
- **Reporting a filter that selected nothing as a pass**, or a verdict on binaries you did not build
  (`plan-test-validation`'s traps).
- **Upgrading an open question to a pass at close-out.** "Every host row is open, and here is the line a host has to
  paste" is a complete and honest close; the same row recoloured green is a lie that outlives the milestone.
- **A tracker that only tracks code.** Docs, decision entries and index rows are work items with evidence too.

## Verifying a Change

1. Every item row states a status from the five, with its basis visible in the row or in that item's evidence table.
2. Every `Complete (measured)` row quotes a command, a build type, a date and a number or named leg.
3. The decision ledger's "next free" line matches the decision log's actual last numbers.
4. The diagram and the item map agree, node for node.
5. The parked table has a reopen trigger per entry, and no item contradicts it.
6. Corrections are dated appends; nothing has been deleted from a completed item file.
7. The close-out pass has run, and the handoff describes the tree that exists rather than the plan that was written.
