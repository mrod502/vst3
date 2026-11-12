---
name: plan-acceptance-criteria
description: Write acceptance criteria a run can decide — decidable wording, an explicit exactness rung, a named leg or command per criterion, and a domain check so a criterion cannot pass while asserting nothing. Use when a spec's criteria read as adjectives ("robust", "click-free"), when a green suite would not prove the feature works, or before signing off a milestone.
source: learned
---

# Acceptance criteria that a run can decide

Distilled from `plan/M3-impl-plan` items 01–04 and their index §7, `plan/M4-impl-plan` §7, `plan/M5-test-suite.md`
§7, and the findings that were earned by a criterion being wrong rather than by the code being wrong — F-010 (a
dial that could not express its own default), F-013 (a criterion that lost its domain), F-017 (a leg that could
not see the bug it was written for).

## When to Use

- Drafting or reviewing a plan's acceptance section.
- A criterion reads as an adjective: robust, clean, click-free, efficient, correct for all layouts.
- Two criteria will be closed by the same measurement, or none will close one of them.
- A criterion was inherited and later decisions may have made it unfalsifiable.
- Someone has said "the suite passes, so the change is done".

## The Contract (read first)

**A criterion is a proposition plus the run that decides it.** If no run decides it, it is a design goal and
belongs in a different section. The grammar that survives review:

> **<Claim about an observable>.** Decided by <the named leg, command or record>, at <exactness rung>, on
> <configuration and host/build-type qualifiers>.

**The exactness ladder, and every criterion picks a rung.** An unstated rung is a future argument about rounding:

| Rung | Use it when | Example |
|---|---|---|
| **Bit-exact** | Deterministic by construction: block-size invariance, dial endpoints, `reset()` freshness, shell-vs-reference identity | "`p = −1` yields `A` delayed by exactly `R`, bit-exact" |
| **Exactly `0.0f`** | Zero is the mathematically exact answer, so anything else is a leak, not rounding | a silent mapped source's output — an FFT of zeros is exactly zero, so is its product and its inverse |
| **`1e-5 · peak`** | Against a reference computed differently | engine output against a naive windowed-convolve reference |
| **`1e-6` absolute** | On float sums where the peak is not the useful scale | a summed-stream error |
| **A named figure with a bound** | A measured quantity with a stated budget | "worst hop-level error 0.376 dB against a ±2 dB gate" |
| **Recorded, not gated** | A number worth having and not worth a red run (cost, footprint) | `CpuBaseline` prints; it does not fail |

**Three classes of decidable, and they must be labelled** because they close on different evidence:

1. **Console-measured** — a run here decides it. This is the only class that may be quoted as *measured*.
2. **Host-recorded** — needs a named host, version, OS and a pasted line. It closes on a recorded answer or stays
   open. A console run may stand in for neither half of it.
3. **Human-judged** — audible or visual quality. Route it to the milestone that owns judgement, behind the
   measurement that would inform it, and write the criterion so it cannot be closed by a green run.

**Every criterion needs a domain.** Before carrying one forward, ask what cases it can actually be evaluated on
after the decisions taken since it was written. "Swapping A and B yields identical output" survived D-018 only for
the configurations where it is true by construction — so a green run reported a property that no longer had any
non-trivial instance. Strike it, name the finding, and replace it with the relation that does discriminate.

## Procedure

1. **Rewrite each criterion as a proposition about an observable**, then attach the run. "No leakage between
   channels" becomes "an impulse in one A channel leaves every output whose mapping does not select it exactly
   `0.0f`", decided by leg M4 of the matrix, per cell.
2. **State the rung at the assertion, not in a preamble.** The tolerance belongs in the test and in the criterion
   sentence. A criterion that says "within tolerance" has delegated its own acceptance to whoever writes the code.
3. **Split a criterion the moment two runs decide it.** "Tests are fast and complete" is two criteria; "the
   cross product passes and a mutation proves it bites" is also two, and separating them is what makes the
   mutation half impossible to skip.
4. **Add the falsification half to every "it works" criterion.** The pairing is always *pass the good case* +
   *redden on the specific bad change*. M5 §7's shape: "a behaviour change made to depend on the callback size
   must redden at least one triple at one block size, and the check must be run, not asserted." A criterion with
   no bad-change half is satisfied by a test that does nothing.
5. **Check each criterion for an unreachable configuration before committing to it.** `R = 0` at a host block
   below the hop trips an assert by design (F-006); a wide main bus is refused by the gate; a layout a host cannot
   route is not a test case. Write the qualifier into the criterion (`hostBlock ≥ H`, "gate-accepted triples only")
   rather than discovering it in the run.
6. **Put the measurement before the threshold.** For a threshold you cannot yet measure, either block the criterion
   on the measurement or state it as a reported figure with no gate. Never pick a number to be safe and let the
   implementation aim at it.
7. **Anything that multiplies a sample needs a decision-log entry before it appears in a signal path** — the
   criterion is written that way so the figure cannot arrive with only a test behind it. Same idea for any policy
   the criteria would silently change: a latency test asserting `measured == declared` *is* a latency-policy
   change in disguise.
8. **Write the coverage table** in the index: milestone criterion → owning item(s) → note that makes it decidable →
   basis once closed. A criterion with no owner is scope that will be discovered at the end; a criterion with four
   owners is a criterion nobody closes.
9. **For criteria about performance or memory, choose record-or-gate explicitly** and say who sets the budget. Two
   tools that print without failing are better than one that fails on a shared machine and gets disabled.
10. **Close-out re-applies the table** (see `plan-work-tracking`): every row names what it closed on, no row says
    "as expected", and rows that stayed open are named as open with their reason — not deleted, and not upgraded to
    measured because the milestone needed to look finished.

## Pitfalls

- **Adjective criteria.** "Rejected cleanly" is not decidable until it becomes "`isBusesLayoutSupported` returns
  false and nothing is ever asked to process; an assert firing in a host is not clean."
- **The self-fulfilling criterion.** If the expected value is produced by the component under test, the criterion
  cannot fail (F-017). Require an absolute expectation alongside any delta-against-a-model.
- **A criterion that only the current implementation satisfies.** Overfitting to today's bytes turns the next
  legitimate improvement into a regression. Prefer an invariant; pin the bytes only where bit-exactness *is* the
  requirement.
- **A backend-sensitive criterion stated as absolute.** It passes until the transform changes; say which rungs are
  backend-dependent and make them parametrised.
- **Counting tests instead of counting criteria.** 189 passing cases is not acceptance; the question is which
  propositions have a red-capable leg.
- **Letting a green suite close a criterion whose mutation was never run.** That is the single most common way a
  milestone lies to itself.
- **Closing a host-only criterion with a console run**, or a listening criterion with a metric. Label the class and
  keep the evidence type matched to it.
- **Editing the criterion after the fact to make it pass.** Correct the criterion in a dated line with the
  measurement; do not silently restate it (`plan-work-tracking`'s append rule).

## Verifying a Change

1. Every criterion names a run — a leg, a command, or a record — and the run exists or is scheduled in an item.
2. Every criterion states its exactness rung at the assertion.
3. Every "it works" criterion has a matching bad-change half, and the mutation campaign table has a row for it.
4. Every inherited criterion has passed the domain check, and the struck ones are visibly struck with a reason.
5. Host-only and human-judged criteria are labelled as such, and none of them is quoted as measured.
6. The index's coverage table has an owner for every milestone criterion and no owner for anything the milestone
   did not promise.
7. No numeric threshold exists without a measurement behind it or an explicit "recorded, not gated".
