# Host layout matrix — what a real DAW hands the plugin, and the line that records it

**Status: instrument implemented and measured in-console 2026-11-08 (M4 item 08); no host row exists yet.**
Every probe below is OPEN. That is the honest state of this file, and it is the state the console legs are for:
the instrument that will answer M4 §6 cannot be allowed to rot between the day it was written and the afternoon
somebody sits down in a host.

**What this is for.** M4 §6 asked which hosts expose a sidechain send wider than mono, whether it can be enabled
independently of the main bus, what a refused layout does in the wild, and how JUCE reports surround and
ambisonic buses. Every answer in `docs/decisions.md` so far was measured in a console against JUCE's own
`checkBusesLayoutSupported` / `setBusesLayout` paths, and no host has ever loaded this plugin. A host's own labels
are not evidence — F-015 measured this plugin's labels and JUCE's behaviour disagreeing — so the plugin now
reports what it was told, and a row below counts only when it carries that line verbatim.

**Scope, stated once (D-023 F, plan `08` §0 F).** VST3 only. `src/Entangle/CMakeLists.txt` is
`FORMATS VST3 Standalone`: there is no AU, AUv3 or AAX, so M4 §3's "AU and AAX are the strictest" is unprobeable
with what ships and stays open here rather than being answered by analogy. The Standalone build cannot
contribute either — it has no sidechain bus to route.

---

## 1. The gate's answers, by pointer

Not restated as rules — the rules live in the decisions and their tests. This table exists so a person holding a
host's bus menu can find the plugin's verdict for the shape they are about to route.

| Shape the host may present | Verdict | Authority |
|---|---|---|
| `(1, 0, 1)` — mono main, no send | accepted, `processed = 1`, effect pinned off | D-017, D-011's pin; `OfflineRender` §5.20 |
| `(1, 1, 1)` | accepted, broadcast, `processed = 1` | D-017; §5.20 |
| `(1, 2…8, 1)` | accepted, **consolidated** — the send folds to its arithmetic mean above the seam | D-018 (fold), D-021 B's deferral admits it; §5.17's level figure, §5.20's geometry |
| `(1, 9, 1)` | refused `exceedsCapacity` | D-017's ceiling binds bus widths; §5.18 |
| `(2, 0…2, 2)` | accepted, `processed = 2` | D-017; §5.20 |
| `(2, 3…9, 2)` | refused `asymmetric` (`channelRule`) | D-017; §5.18 |
| any main wider than 2 | refused `mainWidth` | D-021 B's deferral, reason C |
| `chOut != chA` | refused `mainCountMismatch` | D-021 C; §5.18 |

`chB <= chA` is a necessary condition and **never** a decision procedure — measured over-admission of 22 triples
over `[0,12]³` (`LayoutGateTests`, D-021). Do not answer a host question with that inequality.

What a host can actually *reach* is narrower than what the rule accepts, and the console measured that too: 0 of
192 layout probes in §5.18 landed on a refused layout, because `isBusesLayoutSupported` refuses before anything is
built. So the report can never carry `gate=mainWidth` or `gate=channelRule` — a refused shape is not a state the
plugin runs in, and P4 records what the *host* does about it, not a report line.

## 2. The report line

One line, assembled at the tail of `EntangleAudioProcessor::configureRings` — after the gate call, after the
cached bus members, and **after** the D-022 B guard's early return, so it describes a configuration that was
actually built. Read it from `layoutReport()`; the editor shows it (`layoutReadout`, and the whole line in the
label's tooltip because the label cannot hold it); `juce::Logger::writeToLog` gets it once per real prepare when
`juce::MessageManager::existsAndIsCurrentThread()`.

Verbatim, from `OfflineRender` §5.21 on this tree:

```
entangle layout prepares=1 skipped=0 inBuses=2 outBuses=1 mainIn=1 mainInName=[Mono] mainInEn=1 side=3 sideName=[Discrete #3] sideEn=1 mainOut=1 mainOutName=[Mono] mainOutEn=1 gate=accepted/ok processed=1 outCh=1 rate=48000 hop=128 Lreq=8192 Lrun=8192 R=128 block=4096/4096 sideActive=1 blendEff=0.000
```

| Field | Source | What it rules in or out |
|---|---|---|
| `prepares` / `skipped` | counters in the shell | real prepares vs arrivals the seven-field key declined. **D-022 B's only outside view** — P6's question. Formatted at read time, so a declined arrival moves `skipped=` immediately; the configuration fields keep describing the last real prepare |
| `inBuses` / `outBuses` | `getBusCount` | fixed at 2 / 1: `canAddBus`/`canRemoveBus` are not overridden, so the inventory is not a host variable |
| `mainIn` / `side` / `mainOut` | live `Bus::getCurrentLayout().size()` | what the host routed, read at prepare — not the advertised maximum (F-015: the send advertises 8 beside a mono main and 2 beside a stereo one) |
| `mainInName` / `sideName` / `mainOutName` | `AudioChannelSet::getDescription()`, verbatim, in brackets | **P5's payload.** JUCE's words, not ours, and that is the point: `side=0 sideName=[Discrete #0]` is JUCE calling a disabled bus that — `isDiscreteLayout()` is tested before `disabled()` (juce_AudioChannelSet.cpp:424). Read the count and `sideEn`, not the name, for "is there a send" |
| `mainInEn` / `sideEn` / `mainOutEn` | `Bus::isEnabled()` | `side=0 sideEn=0` is D-002's absent send; `side=1 sideEn=1` is a mono send. Different states, and the plan's P1/P2 turn on telling them apart |
| `gate` | `engine::gateLayout` (D-021) | `verdict/rule`, e.g. `accepted/ok`. A refused triple never reaches the report (above), so this field is `accepted/*` in every row you can paste |
| `processed` | the same decision | `N`, the per-hop channel count — before consolidation, so `(1,5,1)` says 1 |
| `outCh` | `getTotalNumOutputChannels()` | catches a host that routed an output count the buses do not imply |
| `rate` | `getSampleRate()` | — |
| `hop` | the shell's `H` | — |
| `Lreq` / `Lrun` | dial position / `clampGrainLength` | the 2×H floor and 128×H CPU ceiling (D-008) as a *difference* rather than a silently moved dial |
| `R` | `getLatencySamples()` | what the host compensates (D-004). P7 compares this against the host's own display |
| `block` | `hostBlock / maxHostBlockFor(hostBlock)` | D-022 A's floor, visible: `2048/4096` says the host runs 2048-sample callbacks and the rings were sized for 4096 |
| `sideActive` | the shell's cached flag | the pin's input. `sideEn=1 sideActive=0` would be a bug — it means the bus is enabled but empty |
| `blendEff` | `effectiveBlendPosition()` | the blend the law is at (D-011): `-1.000` with no send is the pin, not the dial |

**Where the plan's format differs from this.** Plan `08` §2.1 shows `mainIn=1[Mono]`; the shipped line splits the
name into its own `mainInName=[Mono]` field. The reason is parsing, not looks: a bracketed value glued to a
number has to be split by a name-aware scanner, and the console legs (`reportField`) scan `key=value` at a token
boundary. §5.21 arm 7 is the control that keeps the legs attached to the fields rather than to this layout.

**How to get the log line.** `juce::Logger` defaults to stdout (and `NSLog` on macOS). A host's own log may or may
not carry it — that is H0, and if it never does, D-023's revisit trigger moves the delivery to the readout alone.
Note the measured fact that governs this: `MessageManager`'s constructor records the thread that built it
(juce_MessageManager.cpp:38), so *any* thread that initialises JUCE's message manager is "the message thread"
(F-018). A console harness that calls `initialiseJuce_GUI()` logs every prepare; a VST3 host that designates its
own wrapper thread and prepares on the audio thread logs nothing. Neither is a bug, and only a host can say which
one you have.

## 3. The probes — one person, one host, one afternoon

Order matters: P0 first, because it says whether the instrument works in this host at all; then the send
questions, which are what the open decisions are waiting on. Each probe records one line from the readout's
tooltip (or the log) — do not retype it, and do not summarise it. Record host, version, OS and format above every
group of lines; a row without them is an open row wearing an answer's clothes.

| # | Do this | Record | Feeds |
|---|---|---|---|
| **P0** | Open the plugin on a stereo track and read the readout | the line, and whether anything appeared in the host's log | whether the delivery works here (D-023 C). A missing log line is a *finding*, not a failure |
| **P1** | Open the bus/I-O list | the send's label as the host shows it, or "there is no send" | M4 §6's narrowed question; D-018's field reachability |
| **P2** | Enable the send — on a stereo track, and on a mono track if offered | the line before and after (`side=0 sideEn=0` → `side=…`) | D-021 A's premise; whether enabling is a host capability or a manual dance |
| **P3** | On a mono main try send widths 1, 2, 3, 5, 8; on a stereo main try 2 and 3 | the line per attempt, plus what the host's menu offered | the consolidated family's reachability, and F-015 in the wild: the stereo-main case should be refused — record what the host *does* about it |
| **P4** | Attempt a refusal: a 5.1 main, a 9-wide send, mono-in/stereo-out where offered | the host's response verbatim (greyed out / error text / silently routed something else) and the line after, which must be unchanged | M4 §7 row 5's "rejected cleanly" outside JUCE's own paths; amends F-015 with observed behaviour |
| **P5** | Put the plugin on a surround or ambisonic bus | the `*Name=[…]` fields verbatim | **D-021 B's third trigger.** The console answer is already on record (§5.21 C1b: `6:5.1 Surround`, `8:7.1 Surround`, `4:1st Order Ambisonics`, `9:2nd Order Ambisonics`); what a host adds is whether it will route those shapes to a plugin at all, and what it says about folding a surround send |
| **P6** | Save a session with a consolidated layout, quit, reload | `prepares=`/`skipped=` after load, and whether a stop/start kept the tail | D-022 B's real-world premise — a host that re-applies layouts on load and *was* skipped is the answer §5.19 can only simulate |
| **P7** | Read the latency the host displays; move the hop dial | the displayed compensation vs `R=`, and whether it follows | AGENTS §7's latency honesty at the host seam. Facts for the parked `R := W` question, no decision |
| **P8** | Feed L/R **antiphase** into an N-wide send on a mono main, blend to wet | does the wet path go quiet? host + version + date | confirms the delivered width audibly (D-018's fold sums to zero). The *judgement* about whether the fold sounds right is M9 row 2's — this row only says it happened on a named host |

A probe may legitimately close with **"this host cannot do that"** — say what it offered instead (P3 in
particular: "send is fixed mono, greyed out" is a useful answer, and D-021's revisit note says a negative changes
nothing about the rule).

## 4. Per-host template

```
### <host> <version> — <OS + version>, VST3, <date>, tested by <who>

Build: `git rev-parse --short HEAD`, `CMAKE_BUILD_TYPE=Release`, bundle path from `find build -name '*.vst3'`.

P0 readout:  <line verbatim>
P0 log line: visible in <where> / never seen in <what was checked>
P1 send label: "<as shown>" / none
P2 before: <line>
   after:  <line>
P3 mono main:  1 → <line>   2 → <line>   3 → <line>   5 → <line>   8 → <line>   | menu said: <...>
   stereo main: 2 → <line>  3 → <line>   (refusal expected)
P4 <shape>: host response "<verbatim>"; line after: <line>
P5 <bus shape>: mainInName=[<...>] sideName=[<...>] — verdict: <accepted/refused + what happened>
P6 after reload: prepares=<n> skipped=<n>; tail survived stop/start: yes/no
P7 host shows <n> samples vs R=<n>; moved with the hop dial: yes/no
P8 antiphase on a <n>-wide send: wet went quiet / did not change / send not available
Notes:
```

## 5. Results

**Nothing in this section is answered.** No host has loaded this plugin as of 2026-11-08, and per INDEX §3 rule 4
a host-only criterion does not close from a container.

| Row | Probe | Status | Host / version / OS / format / date | Report line |
|---|---|---|---|---|
| H0 | P0 | OPEN | — | — |
| H1 | P1 | OPEN | — | — |
| H2 | P2 | OPEN | — | — |
| H3 | P3 (mono-main widths and the stereo-main refusal) | OPEN | — | — |
| H4 | P4 | OPEN | — | — |
| H5 | P5 — the row D-021 B's third trigger waits on | OPEN | — | — |
| H6 | P6 | OPEN | — | — |
| H7 | P7 | OPEN | — | — |
| H8 | P8 | OPEN | — | — |

What the console half *has* settled, so a host run does not have to re-derive it:

* JUCE's own words for every bus shape this plugin could be handed, including surround and ambisonic (§5.21 C1b,
  and the `Discrete #0` quirk above). The half of D-021 B's third trigger that needs no host is answered; the half
  that needs one is whether a host will route those shapes to a plugin at all.
* The report's fields agree with the accessors they claim to mirror, for all twelve gate-accepted triples the
  harness can build (§5.21 C1, 24 fields each).
* The counters distinguish a re-applied layout from a rebuild, and a refused layout moves neither the report's
  configuration nor the running render (§5.21 C2/C3, with §5.19's bit-exactness behind the second half).
* One log line per real prepare, none per declined arrival, and none from a prepare made off the message thread
  (§5.21 C4/C5).

## 6. What this document will never answer by itself

* AU / AUv3 / AAX strictness — no such target ships (plan `08` §0 F). Adding one needs its own decision entry.
* Whether the fold or independent normalisation *sounds* right — M9 rows 1 and 2. This file records that a fold
  happened on a named host; the listening judgement stays there.
* The wide-main decision (D-021 B). This item supplies two of its three inputs — the surround-reporting answer and
  host reachability — and the third is M7's per-channel cost curve.
* Per-host behaviour inferred from a different host. VST3 answers do not transfer between hosts, and "hosts accept
  it" without naming one is the F-015 mistake one level up.
