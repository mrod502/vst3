---
name: plan-requirements-refinement
description: Turn a fuzzy milestone or feature brief into a scoped set of work items with named owners, inherited contracts and a decisions table — splitting scope in/out, merging items that share an edge, and checking whether the work already exists. Use when a spec is prose rather than items, when items cannot yet be ordered, or before writing an implementation plan for a milestone.
source: learned
---

# Requirements refinement: from milestone prose to work items

Distilled from how the Entangle milestones were planned and then corrected — `plan/M3-impl-plan/INDEX.md`,
`plan/M4-impl-plan/INDEX.md`, `plan/M5-impl-plan/INDEX.md` and the milestone docs they re-point
(`plan/M4-multichannel.md` lost two of its seven items to a measurement before a line of code was written).

## When to Use

- A milestone or feature brief exists as prose and §"work breakdown" is a wish list.
- Someone asks "what should be item 01" and the list has no edges between its entries.
- Two items would edit the same file, or one item's criterion depends on a decision nobody has taken.
- An inherited contract (threading rules, latency policy, a tolerance ladder) has to survive into every item.
- You suspect part of the scope is already implemented — the most common and most pleasant surprise.

## The Contract (read first)

**A work item is a unit of work one agent can claim, one run can close, and one file tracks.** Anything that
fails all three is a theme, not an item. The shape that has worked here:

```
NN-<slug>.md     one unit of work: §0 decisions · §1 current state, measured · §2 behaviour to deliver ·
                 §3 work breakdown · §4 acceptance criteria · §5 discriminator proofs · §6 out of scope ·
                 §7 risks · §8 docs · §9 how to verify · §10 evidence · §11 corrections this item had to make
```

The section list *is* the refinement method: §0 forces the decisions to be explicit, §1 forces the current state
to be measured rather than assumed, §6 forces scope discipline by writing down what is deliberately not being
done, §11 gives the item somewhere to record that the plan was wrong without pretending it never was.

**Refinement happens in the index, not in the items.** The index owns: the item map, the dependency edges, the
housekeeping rules, the environment facts, the contracts every item inherits, the decisions table, the milestone
acceptance coverage, the failed premises, the parked items, and the sequencing with reasons. If a rule exists in
both an item and the index, the index wins and the item cites it.

**Inherited contracts are requirements too.** Every milestone here begins §5 with "everything the previous index's
§5 says carries forward unchanged" and then lists additions. A contract that has to be re-derived per item is a
contract that will be broken per item.

## Procedure

1. **Re-verify the scope against the tree before you split it** (see `plan-thesis-validation`). M4 §4 item 2 asked
   for per-channel engine state and item 4 asked to generalise four stages to N channels; both were already
   written, so seven items became five and the milestone said so in its §8. Splitting first and discovering later
   is how a plan ends up with items nobody can finish because they are already done.
2. **Write the scope statement in three lists, not one**: *In* (with the milestone section each entry answers),
   *Out* (with the milestone that owns it instead), and *Out and standing* — the things that are not work now and
   must never be assumed later. A scope without the third list gets re-litigated every session.
3. **Split by the file or seam an item touches, not by the sentence it answers.** The collision surface is
   concrete — two items editing the same harness, the same chain, the same table. Where two items would edit one
   file, order them and say why in the sequencing section; M3's rule was "two agents editing `OfflineRender` or
   the chain at the same time is the one collision to avoid here".
4. **Give each item a `Covers` cell that points at the spec sections it closes**, and keep the mapping honest when
   it is not one-to-one. M4 recorded it plainly: "item 1 → `01` (and `09`), item 3 → `03`, item 5 → `04` … items 2
   and 4 have no file of their own — here is why", plus two items that exist which the milestone never listed
   because the work was discovered.
5. **Make every dependency edge a reason, not a preference.** If you cannot say what the later item cannot do
   without the earlier one, the edge is fake and the ordering is arbitrary. Edges that are real: shared file,
   shared generated artefact, a contract that must exist before it can be used, a measurement the later item's
   threshold depends on.
6. **Put a decisions table next to the item map** with four columns: decision, what it gates, recommendation, and
   what each item must carry as a consequence. Then mark each row *answered*, *needs an answer*, or *blocked*.
   An item whose decision is unanswered stays `Not started` with a named blocker; it does not start with a guess
   that becomes load-bearing. Take the decisions that change an acceptance criterion **before** any item codes —
   M3 took five up front precisely because each one changed a criterion somewhere, and one of them (snap the
   crossfade weights at the endpoints) was the only way its own bit-exactness criterion could be met.
7. **Name the contracts that bind every item, in the index** — thread boundaries, allocation rules, tolerance
   ladder, notation, citation convention, "no logic below this seam", "host-only criteria never count as
   measured". Then, when an item needs an exception, the exception is written down and cited, not silently taken.
8. **Write the parked table (§9) as deliberately as the item table.** Anything gated on a decision nobody has
   taken, on a host nobody has, or on a listening test nobody has run goes there with three cells: why it stays
   parked, what exists to support it, and what would reopen it. This is the mechanism that stops a milestone
   quietly re-opening last milestone's settled arguments.
9. **Every item gets an out-of-scope section, written as an assertion.** "Deliberately absent: any leg moved out
   of the monolith; any change to a generator's distribution; any runner script (`09`)." Naming what you will not
   do is cheaper than reviewing for it later, and it is the fastest way to discover that two items both think a
   third thing is theirs.
10. **New scope discovered mid-item becomes a new file plus a row**, never an extra paragraph in the item you are
    already inside. The rule is what keeps an item closable.

## Pitfalls

- **Items sized to the spec's paragraphs.** A milestone section that lists seven things rarely implies seven
  units of work; it implies seven *claims*, which may need two items and a decision.
- **Folding a discovered dependency into the current item.** "While I'm here" turns a closable item into an open
  one, and the next agent cannot tell what the item was for.
- **Ordering by size or by appeal.** Order by the edges. If the most valuable item is behind three others, the
  honest move is to say its value is why the earlier ones exist.
- **A decision table with no owner column in spirit.** If nobody can answer it, it is a blocker, and a blocked
  item labelled `Not started` without a named blocker reads as laziness rather than as waiting.
- **Assuming a control exists because a criterion mentions it.** "Assert the measured delay equals the configured
  `R`" — nothing configures `R`. Refinement is where you find that out; implementation is where it costs a day.
- **Letting a host-only or listening-only requirement become an item's acceptance criterion.** Mark it parked or
  route it to the milestone that owns human judgement; a console run may not stand in for it.
- **Re-deriving contracts per item.** They belong in the index, and the item cites the index.

## Verifying a Change

1. Each item maps to a file, and each file has the eleven sections.
2. Every `Depends on` edge has a sentence somewhere saying *why* — in the sequencing section or on the diagram edge.
3. No two items edit the same file without an explicit ordering and a stated reason.
4. The scope has three lists, and every "Out" entry names the owner that took it instead.
5. The decisions table has no row that is both unanswered and gating an item marked `In progress`.
6. §9 lists every parked item with a reopen trigger, and nothing in any item's work breakdown contradicts it.
7. A reader who has read only the index can tell what is in, what is out, what is blocked, what is settled, and in
   which order to start — without opening an item file.
