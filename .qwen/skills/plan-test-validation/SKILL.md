---
name: plan-test-validation
description: Run a validation campaign whose green actually means something — mutation proofs, equivalent-mutant bookkeeping, byte-identity baselines, determinism both-halves, allocation tripwires, sanitizer runs, and the four traps that print green over red. Use when proving a test catches a bug, when a guard's value is questioned, before trusting a green suite, or when an extraction must not change behaviour.
source: learned
---

# Test validation: making green mean something

Distilled from the mutation campaigns in `plan/M3-impl-plan` items 01–04, `plan/M4-impl-plan` items 02–08 (§5 of
each is the arm table, §10 the results, §11 the misses), and the traps those campaigns recorded in
`docs/decisions.md` (F-012, F-014, F-016, F-017) and in the indexes' environment sections.

## When to Use

- Claiming "this test catches that bug", or being asked whether it does.
- Landing a refactor or extraction where behaviour must not move.
- Adding a numeric leg and deciding what makes it fail for the right reason.
- A suite went green after a change that should have broken something.
- Choosing what to run under a sanitizer, and what a sanitizer failure is evidence of.

## The Contract (read first)

**A test is validated by reddening it, not by passing it.** The unit of work is an **arm**: a specific mutation, a
prediction, and the observed red count. Every arm is run — never asserted, never inferred from "obviously it would
fail". The prediction matters as much as the result: an arm that fails to redden is a coverage finding, and a
green suite with no prediction attached to its arms teaches nothing.

**Four traps that print green over red.** All four have happened in this repo, and all four are in the indexes as
environment rules because they were each discovered mid-campaign:

| Trap | What it looks like | The rule |
|---|---|---|
| **Empty filter** | `ctest -R <TargetName>` prints `No tests were found!!!` and **exits 0** | Filter on case-name text. A run whose test count is zero is a failed run, reported as one |
| **Stale binary** | The build failed, so the previous binary ran and printed its results | Gate every leg's output on the build's exit status, always |
| **mtime-preserving restore** | `cp -p`/`shutil.copy2` puts pristine content back with an older mtime, ninja keeps the mutant's objects, and a later arm prints an earlier arm's red leg — including inside its own control | Restore with a plain copy or `touch`, then re-run a no-mutation control you confirm actually recompiles |
| **Self-fulfilling expectation** | The expected stream is produced by the component under test, so a mutation inside it moves both sides of the delta and the leg stays green (F-017: arm 6 left the section green at 175 `ok`) | Every mapped slot carries an absolute expectation as well as its delta — must-be-non-zero, must-be-exactly-`0.0f` |

**Equivalent mutants are a legitimate result and must be recorded as one,** with the structural reason. M4 item 06's
arm 6 (`hopA[slot] = slot`) is unobservable because `chOut == chA` makes that identity hold for every accepted
configuration, and a different binary's leg is what reddens it. Recording "this arm cannot redden here, and here is
why" is worth more than quietly dropping the arm.

## Procedure

1. **Capture the baseline on the day you work, and hash it.** For a text-emitting harness: line count, the `ok`
   count, the exit status and a `sha256`. Then name the **permitted-diff set** — the lines that legitimately move
   (durations) — so a diff is a comparison rather than a shrug.
2. **Write the arms before you implement, or immediately after**, in a table: mutation · prediction · observed
   (filled by the run, never by hope). Keep them cheap and specific: an off-by-one in one predicate, a dropped
   stage, a swapped loop order, a tolerance widened, a seed ignored, a call deleted.
3. **Run each arm as its own campaign step**: apply → build (check the exit status) → run → record which named legs
   went red and how many assertions → revert with a content-changing copy → rebuild → re-run the control.
   Confirm the mutation landed before trusting the run: an edit can echo success and change nothing.
4. **Include the deletion arm in any extraction campaign.** Delete one measurement's call site; the run must go
   red. This is the only arm that catches the failure mode where a refactor silently stops running something while
   still exiting 0.
5. **Include the "two routes share one builder" arm** wherever a harness and a product build the same object
   graph. Replace the shared builder with a local copy minus one stage: both routes must redden at once. If only
   one does, they had already diverged and your comparison was comparing a copy against itself.
6. **Assert determinism in both halves.** Same seed twice is bit-identical **and** a different seed differs. The
   first half alone passes for a generator that ignores its argument.
7. **Match the tool to the claim.** Allocation on the audio path → a counting tripwire around the armed window,
   with a control that *does* allocate so you prove it is armed, and a control that does not so you prove it is not
   counting noise. Memory error / UB → ASan+UBSan, run on the binaries that can carry them (a harness with its own
   cost bound will fail that run on the bound and on third-party leak reports, not on a leg — say so or don't run
   it). Thread or timing claims → they are not testable this way; write them as code-structure criteria or park
   them.
8. **A replaced global `operator new` is once per program.** Give each binary that wants a tripwire its own target
   and never a second shim in the same program. The mechanism is a header with inline state plus one `.cpp` per
   opted-in target; a mistake should fail at link time, which is a check the linker already performs.
9. **Bound a duration on the clock that can measure it.** A section's own wall clock, not the suite's: the same
   binary measures 25.6 s alone and 38.67 s after 188 others. Print the figure in the run so the bound is
   self-evidencing, and quote the host, build type and run order with it.
10. **Fail loudly on a run that did not run.** Zero tests selected, a build that failed, a filter that matched
    nothing, a leg skipped by an unsatisfied precondition — each must be a nonzero exit or a printed FAIL, never an
    absence of complaint.
11. **Report the campaign honestly**: arms caught, arms that missed (with what the miss means), equivalent mutants
    (with their reason), arms whose prediction was wrong (with the correction to the plan, not just to the code).
    A campaign reported as "all arms caught" is either luck or a report that omitted something.

## Pitfalls

- **Trusting a green run to prove a guard guards something.** The M2 items 2/4 lesson: a claim about what the poison
  test exercised was false until the mutation was run — deleting the zeroing left the test green because the backend
  ignores exactly those slots.
- **Trusting a verdict on binaries you did not build.** A baseline run reported five failures that existed only in
  on-disk binaries older than the tree; rebuilding returned green with no source touched.
- **Re-implementing the check in a script.** A hand-written validator that recomputes what the build system, the
  linker or `ctest` already decides becomes the thing under test, and it usually agrees by construction. Use the
  tool's exit status.
- **A test that passes because the run was too short.** Assert that the render length exceeds the support being
  measured (2L after an impulse, the follower's settling bound, the ring's fill) and print both numbers. An
  assertion landing mid-transient is not evidence.
- **Comparing across a dimension that legitimately changes the answer** — two `L` values, two backends, two sample
  rates — and calling the difference a bug or the agreement a proof.
- **Silently loosening a tolerance to get green.** If the bound is wrong, restate it in a dated line with the
  measured figure beside it; do not edit the constant until the number appears.
- **Assuming a sanitizer verdict is a leg verdict.** Debug/ASan builds change cost, timing and teardown asserts;
  a Debug-only abort (`Logger::~Logger` asserting on the installed logger) is a real bug found by a different
  instrument, not a flaky leg.
- **Testing the vendored code.** Nothing under `juce/`. Test the seam you wrote against it.

## Verifying a Change

1. Every new or changed leg has at least one arm; every arm has a prediction and an observed count.
2. The deletion arm exists and reddens, for any extraction or move.
3. The permitted-diff set for the byte-identity comparison is named, and the diff is empty outside it.
4. Both halves of every determinism claim are asserted.
5. Every quoted duration carries host, build type and whether it ran alone or after the rest.
6. Equivalent mutants and missed predictions are recorded in the item's corrections section with their reasons,
   and the plan or milestone doc that was wrong was amended, not reinterpreted.
7. The control run after the final revert recompiled and went green, and the binary's freshness is not in doubt.
