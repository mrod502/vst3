# Decision log

One entry per decision that constrains future milestones. Format: Context / Options /
Choice / Revisit trigger / Evidence. Findings (F-xxx) are cheap-to-lose facts that are not
decisions but cost hours to rediscover.

| Id | Subject | Status |
|---|---|---|
| D-001 | Bus layout | accepted (M1) |
| D-002 | Missing-B behaviour | accepted (M1) |
| D-003 | Hop size set and default | accepted (M1) |
| D-004 | Latency policy and where the delay is realised | accepted (M1) |
| D-005 | Test framework and how it is obtained | accepted (M1) |
| F-001 | JUCE 9 latency plumbing | finding |
| F-002 | `/tmp` was noexec and broke juceaide | finding |

---

## D-001 — Bus layout

**Context.** A is the signal being transformed, B is the second signal. The engine needs
both, and the shell must not guess which channels carry B.

**Options.**
- Split one stereo input: left = A, right = B.
- Second input bus (sidechain): main input = A, bus 1 = B.

**Choice.** Second input bus. `BusesProperties().withInput("Input", …, true)
.withInput("Sidechain", …, false).withOutput("Output", …, true)`. A host that cannot offer
an extra bus simply never presents B; the plugin degrades to silence (D-002) rather than
falling back to channel-splitting, because a silently reinterpreted channel pair is worse
than an obviously absent B.

**Revisit trigger.** M4 multichannel generalisation, or a host that reports a sidechain bus
it never activates (then the "B not connected" UI state needs a distinct reason code).

**Evidence.** `AudioProcessor::getBusBuffer` is pure pointer arithmetic from
`getChannelIndexInProcessBlockBuffer` (`juce_AudioProcessor.h:672`), so reading bus 1 when
the host did not allocate it reads outside the bus region — the enabled check in D-002 is
load-bearing, not defensive.

## D-002 — Missing-B behaviour

**Context.** The sidechain bus can be absent (host without extra-bus support), disabled by
the host, or connected and silent. The hop clock must not care which.

**Options.**
- Treat B as silence.
- Treat B as a copy of A (self-transform).
- Refuse to run / bypass.

**Choice.** Silence, filled at scheduler ingest, and the state is exposed so the UI can say
"B not connected". The processor caches the enabled state of bus 1 off the audio thread
(`prepareToPlay`, `layoutChanged`); `processBlock` never reads the bus-1 buffer when that
cache says inactive, because `getBusBuffer` on an inactive bus hands back pointers computed
from an offset the host never filled.

**Revisit trigger.** M2, when a self-transform mode becomes musically interesting — that is
a user-selected mode, not a fallback. Also if a host enables the bus but leaves it silent
and the UI state needs to distinguish them.

**Evidence.** The scheduler advances its hop clock from the A ring only after B has been
filled (silence counts), so a disabled bus cannot stall it — tripwire T3. Covered by
`Engine/tests/HopSchedulerTests.cpp` ("sidechain absent").

## D-003 — Hop size set and default

**Context.** The internal step `H` is independent of hostBlock. It must be user-visible but
must not be automatable, because changing it re-prepares every ring.

**Options.** Free integer parameter; a fixed allowed set.

**Choice.** Non-automatable choice parameter over `{64, 128, 256, 512}`, default 128. A
change is handled on the message thread inside `suspendProcessing(true/false)` and
re-prepares the scheduler and the reported latency; nothing is reallocated on the audio
thread.

**Revisit trigger.** M2 fixes the relation between `H`, grain length `L` and FFT size `F`;
if that relation makes some values illegal the set narrows. Also if a host automates it
anyway and the suspend round-trip proves too jittery.

**Evidence.** `AudioProcessor::suspendProcessing` (`juce_AudioProcessor.h:932`) is the
documented message-thread way to keep `processBlock` out of a slow reconfiguration window.

## D-004 — Latency policy and where the delay is realised

**Context.** `Engine::processHop` is hop-aligned: it produces exactly one output hop per
input hop, so cumulative output samples can never exceed cumulative input samples. Any
positive latency therefore requires output samples that precede the input that produced
them.

**Options.**
- Delay line inside the engine, scheduler drains whatever is there.
- Scheduler pre-fills its output ring with `engine.latencySamples()` zeros.
- Pad the output with fresh zeros when short.

**Choice.** Scheduler pre-fill. `HopScheduler::prepare` writes `latencySamples` zeros into
each output ring before the first callback, which is what makes "samples out == samples in"
hold for every callback including `numSamples == 0`. Padding to make a short read look
complete is forbidden (tripwire T2): it changes latency silently.

The consequence for engines: **`processHop` must not add delay beyond what
`latencySamples()` declares.** `PassthroughEngine` is therefore a bit-exact copy whose
whole L-sample delay is the pre-fill, which keeps the end-to-end delay exactly L for any L,
including L that is not a multiple of H.

`EngineConfig::latencySamples` is the single source of the number; for M1 it equals `H`.
`setLatencySamples(config.latencySamples)` is called from `prepareToPlay` and from the
hop-parameter callback — never from `processBlock`, and never a literal.

**Revisit trigger.** M2: if the partitioned convolver's latency is not a pure integer pre-fill
(e.g. it needs look-ahead that outlives one hop) the pre-fill model is re-examined. M1 §8
also keeps latency constant per session as the safe default; a host that ignores a
mid-session latency change is grounds to make the hop parameter disabled during transport.

**Evidence.** F-001. Verified end to end by `tools/OfflineRender` (reported latency ==
measured impulse delay, output == input shifted, exact float equality).

## D-005 — Test framework and how it is obtained

**Options.**
- JUCE `UnitTest` — needs a JUCE-linked console app, wrong for JUCE-light libraries.
- Catch2 via `FetchContent_MakeAvailable` — the obvious reading of AGENTS §10.
- Catch2 fetched and compiled under our own target.
- doctest.

**Choice.** Catch2 pinned to tag `v3.7.1` via `FetchContent_Populate`, compiled from
`extras/catch_amalgamated.cpp` into a project-local target `EntangleCatch2`, with
`catch_discover_tests` taken from the fetched `extras/` CMake modules.

---

## F-001 — JUCE 9 latency plumbing

`AudioProcessor` lives in `juce_audio_processors_headless/processors/` in JUCE 9.
`setLatencySamples` is byte-identical to 8.0.7: it calls
`updateHostDisplay(ChangeDetails().withLatencyChanged(true))` itself, so calling it is
enough to notify the host. **`updateReportedLatency()` does not exist in either version** —
searching for it wastes time. Whether a host actually re-queries mid-session is host-specific
and belongs to the §3 hand-off, not to this tree.

## F-002 — `/tmp` was noexec and broke juceaide

A configure into `/tmp` died at "Testing juceaide" because `compose.yml` mounted `/tmp` with
the `type: tmpfs` short form, which always mounts `noexec`; CMake compiles a probe into
`TMPDIR` and then *runs* it. Fixed 2026-10-21 by the named tmpfs volume
`vst3-tmp` with `o: "mode=1777,size=512m,exec"`, re-checked from inside the recreated
container (compile+run in `/tmp` → exit 0, `cmake -B /tmp/probe-cfg` → exit 0).

Accepted caveat: an explicit `o:` string **replaces** Docker's tmpfs defaults instead of
merging, so `nosuid` and `nodev` were dropped along with `noexec`. Restate every flag you
want (`o: "mode=1777,size=512m,exec,nosuid,nodev"`) the next time the container is
recreated — `compose.yml` is deliberately left as the string that is currently mounted.
