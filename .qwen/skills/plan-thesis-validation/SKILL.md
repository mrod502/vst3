---
name: plan-thesis-validation
description: Validate a feature or milestone thesis before implementing it — state it falsifiably, inventory its premises, run discovery proofs against the tree, and build a known-bad reference a metric must flag. Use when a plan's premise might be wrong, when a metric needs calibrating, or before a milestone's first item is written.
source: learned
---

# Thesis and premise validation for feature plans

Distilled from the Entangle milestone plans (`plan/M3-impl-plan/INDEX.md` §8, `plan/M4-impl-plan/INDEX.md` §8,
`plan/M5-impl-plan/INDEX.md` §1 and §8) and the decision log's findings (`docs/decisions.md` F-005, F-006, F-013,
F-017). The pattern in every one: a plan asserted something about the code or about a number, the assertion was
wrong, and the cost of the error was hours of implementation aimed at a target that did not exist.

## When to Use

- Writing a milestone or feature plan, before its first item is written.
- A plan claims the code already does, or does not do, something (`"generalize the normalizer to N channels"`).
- A criterion depends on a quantity nobody has measured (a gain, a delay, a CPU figure, a threshold).
- A metric is being invented and §7 wants it to *discriminate* — flag the bad case, pass the good one.
- A decision was taken earlier and a later decision may have destroyed a criterion's domain.

## The Contract (read first)

**A thesis is a claim about the world with a run attached to it.** Three parts, and a plan that has only the
first is not a thesis:

1. **The claim**, in one paragraph, in the indicative: *"M1–M4 established real invariants and guarded them with a
   program nobody can run a subset of."*
2. **The evidence it stands on**, each item locatable: a file and line, a command and its output, a date.
3. **What would falsify it, and why that outcome is worth as much as confirmation.** For the example above:
   *"if a mutation that reddens an existing leg fails to redden any extracted leg, a guard was load-bearing in a
   way the structure hid — and §8 records which."*

**Premises are the cheap failures and they are the common ones.** Four classes, each with a real instance:

| Class | What it looks like in a plan | The instance |
|---|---|---|
| **Already-done** | "implement X across N channels" | M4 §4 items 2 and 4: `MixEngine::processHop` already looped per channel, so the work was the *mapping*, not the generalisation. Four of seven items shrank |
| **Wrong direction** | "frequency at least 20× below the envelope's time constant" | M3 item 03 stated the physics backwards; the regime described is one where an RMS cannot be read at all. Legs ran at `f·τ ≈ 100` instead, and the plan was corrected rather than the code |
| **Missing control** | "assert the measured delay equals the configured `R`" | M5 §4 row 2: the shell never configures `R` — it is the hop. Half the criterion had no knob to test |
| **Unavailable tool** | "choose among RTSan, a shim, or both" | clang 18.1.3 answers `unsupported argument 'realtime'`. Writing it as a free choice sets the item up to plan an instrument it cannot install |

**Findings are not decisions.** A `D-xxx` is a choice between live options with a revisit trigger; an `F-xxx` is
a fact that cost hours to establish and is cheap to lose. "The toolchain has no RTSan" is an `F`. Numbering:
take the next free number of the right kind, never reuse one, and add the row to the log's index table.

## Procedure

1. **Write the thesis before the work breakdown**, at the top of the index, with its falsification clause. If you
   cannot write what would prove it wrong, the milestone is a list of tasks, not a hypothesis, and its acceptance
   criteria will be unfalsifiable too.
2. **Inventory every premise the plan rests on** in one line each, with the source it came from — the milestone
   doc, an earlier item, a memory, your own inference. Untagged inferences are where the failures live.
3. **Verify each against the tree or by measurement, in that order of cheapness.** Read the function; grep the
   call sites; run the command. Do not accept a plan's prose about its own code, and do not accept an earlier
   plan's premise after a later decision — D-018 keyed the channel rule to the main bus and that silently killed
   the commutativity criterion (F-013).
4. **Where a premise fails, stop and fix the document, not the code.** The repo convention is a numbered
   "premises that failed verification" section in the index, holding the measurement, plus an edit to the
   milestone doc whose wording misled you, so the next reader is not misled by the text that misled this one.
   Coding around a false premise is how a wrong assumption becomes permanent and invisible.
5. **For a quantity a criterion depends on, measure it before you set the threshold.** If you cannot measure it
   yet, the criterion is not decidable and must say so and name who settles it — the user's host for anything
   audible or CPU-shaped, a named console run for everything else. Never let a guessed threshold become an
   acceptance bar.
6. **For a new metric, build the known-bad case first.** A threshold lifted from the literature discriminates
   nothing. The recipe: (a) construct the bad configuration *in the harness* rather than excavating it from
   history — a wrong order in a documented function (F-005's add-then-emit) or a variant one line away in the
   builder; (b) run the metric on good and bad in **one output**, both numbers printed; (c) accept the metric only
   on the ratio between them, and record the ratio. If the bad case is not constructible, say what is missing and
   let the criterion be blocked rather than decorative.
7. **Record a domain check for every inherited criterion.** Ask: after the decisions taken since this criterion
   was written, does it still have cases that are not tautologies? F-013's shape — the only remaining swap
   partners were the diagonal ones, where the assertion holds by construction — reports green while testing
   nothing. When a criterion loses its domain, strike it visibly (strikethrough plus the reason plus the finding
   number), do not quietly narrow it.
8. **Distinguish "answered by measurement" from "needs a decision"** in the decisions table, and colour or mark
   them differently in the diagram. A row that has been settled by a measurement should be dated; a row waiting
   on a person should name what a good answer looks like.
9. **Leave a research row per investigation, with the answer in it once known.** A research bullet that has been
   answered stays visible with its date and answer ("clang 18.1.3 does not have it, 2026-11-09") and a reopen
   trigger, so the next reader does not re-run a dead question.

## Pitfalls

- **Trusting the plan's description of the code.** One M2 plan's §4 loop was not alias-safe despite its own
  contract promising aliasing, and its own alias test would have failed it.
- **Trusting a premise about what a test exercises.** A plan claimed the poison test exercised zeroing in the
  inverse transform; deleting the zeroing left the test green, because the backend ignores exactly those slots.
  Only the mutation tells you.
- **Treating a necessary condition as a decision procedure.** `chB <= chA` holds of every accepted layout and
  still admits 22 triples the rule rejects. A gate written from the prose is wrong 22 ways, and a leg that only
  checks agreement with the prose proves nothing.
- **Coding around a false premise because the fix is bigger than the session.** That is the moment to write the §8
  entry and let the work be blocked, not to invent a workaround the next reader cannot undo.
- **Measuring once on a shared machine and quoting it as a property.** A binary measured 25.6 s alone and
  38.67 s after the others; a container measured 2.2× the release host. Name the host, the build type, and the
  run order, or do not name the number.
- **Letting a validation run be its own evidence.** If the check is a hand-written script re-deriving a verdict a
  real tool already produces, the script is the thing being tested. Prefer the tool's own exit status.
- **Assuming a claim in a document means the document exists.** One index cited `01-agent-brief.md` five times;
  it was never in the tree or in history. Every "the brief says" in that directory was the index's own paraphrase.

## Verifying a Change

A thesis-validation pass is done when:

1. The index opens with a claim and a falsification clause, both in the indicative.
2. Every premise line has a location — file and line, or command and date — or is marked as an inference.
3. Each failed premise has a numbered entry containing the measurement, an edit to the doc that was wrong, and no
   code that works around it.
4. Every criterion that a decision has since voided is struck visibly with its finding number, and nothing
   re-derived it in a narrower form.
5. Every metric has a known-bad configuration that the harness can build, and one run that prints the bad number
   beside the good one.
6. No quantity appears in a threshold without either a measurement or a named owner who will settle it on named
   hardware.
