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
| D-006 | Transform backend | accepted (M2), both engines measured |
| D-007 | Every build tree lives in `build/<config>` at the repo root | accepted (M2) |
| D-008 | Grain-length set, bounds on the (L, H) pair, and its measured cost | accepted (M2) |
| D-009 | Interim unity dry + wet sum | accepted (M2), superseded by D-010; its peaks and the −9.3 dB figure stay live |
| D-010 | The mix law: equal-power blend, dry B, endpoint snapping, unity, the no-sidechain pin | accepted (M3) |
| D-011 | Blend / gain / bypass parameters, and the ramp living in the sample domain | accepted (M3) |
| D-012 | Output safety stage: memoryless tanh clip, knee 0 dBFS, ceiling +6 dB | accepted (M3) |
| D-013 | DC blocker on the convolved path: 5 Hz, second-order Butterworth, wet path only | accepted (M3) |
| D-014 | Level detector: measurement only, two-stage envelope, exact silence | accepted (M3); its revisit trigger fired 2026-10-30 — D-015 takes the target and the driving reading, the class and its measured figures stand |
| D-015 | Normalisation target: geometric-mean RMS of A and B, smoothed envelope as driver, UI floor with unity below it | accepted (M3); implemented by **D-016** (2026-10-31) — measured, and every figure it owed is in that entry |
| D-016 | Level normalizer: where the stage sits in the chain, the gate, the derived values, the non-finite rule | accepted (M3), implemented and measured |
| D-017 | The M4 channel rule: broadcast from one channel only, asymmetric rejected, ceiling 8 | accepted (M4), implemented and measured; **consumed since 2026-11-04 by `HopScheduler`** (item 02 resolves it into pointers) and, through `engine::gateLayout`, by `isBusesLayoutSupported` (**D-021**, same day). **Its `chA == 1 && chB >= 2` row is superseded by D-018**, which landed 2026-11-02 — the helper folds that family now and its `static_assert`s pin the folded shape |
| D-018 | Mono main consolidates the sidechain to a mono mean; every other layout needs main ≥ sidechain | accepted (M4) 2026-11-02, map half implemented by item 09 the same day, **mean computed and level measured 2026-11-04** by item 02 in `HopScheduler`. The −3 dB level consequence stated here is **corrected by that measurement**: the fold is −2.985 dB, the applied correction moves **+1.558 dB**; audibility stays M9 row 2's. **Host-reachable since 2026-11-04** — D-021's gate admits `(1, chB 2…8, 1)`, so a layout that folds is one a host can select, and §5.18 renders it |
| D-019 | Shared-spectrum optimisation: not in the engine API now; the seam already fits M7 | accepted (M4) 2026-11-02 — closes M4 §4 item 6 as "not now" |
| D-020 | Independent per-channel normalisation stays; linking moves to M9 behind a measurement | accepted (M4) 2026-11-02 — no code change, row stays open on the user's host |
| D-021 | The layout gate: two rules of its own above `resolve`, main bus mono/stereo, sidechain to 8 | accepted (M4) 2026-11-04 with the user (A widths, B wide-main deferral, C reasons), **implemented and measured the same day** as `engine::gateLayout` + `LayoutGateTests` (9 cases / 9 125 assertions) and `OfflineRender` §5.18. Closes the "silence is not a refusal" gap and makes the D-018 fold host-reachable; wide mains deferred behind item 04 + a cost curve + M4 §6 — **item 04's figures landed 2026-11-05 (D-022 C), so two of those three triggers remain** |
| D-022 | Prepare-time sizing: a floor with a name, a re-prepare keyed on seven fields, a footprint measured rather than bounded | accepted (M4) 2026-11-04 with the user (A floor, B conditional, C recorded), **implemented and measured 2026-11-05** as `engine::ReconfigureKey` + `ReconfigureTests` (14 cases / 182 assertions) + `EntangleAudioProcessor::maxHostBlockFor` + `OfflineRender` §5.19 + `tools/PrepareFootprint`. `ctest` 175 → 189. An unchanged arrival no longer disturbs a running transport (0 bytes, bit-identical); the wide-main cost is on record at 1.00 MiB stereo / 1.02 MiB for the 8-wide fold |
| D-023 | The layout report: one line naming what the plugin was *told*, built at the tail of a real prepare and delivered twice | accepted (M4) 2026-11-06 as `08` §0 B/C/D, **implemented and measured 2026-11-08** as `EntangleAudioProcessor::layoutReport()` + the editor's `layoutReadout` + `OfflineRender` §5.21 (28 lines, 0 FAIL) and `docs/host-layout-matrix.md`. Counts, `getDescription()` names, gate words, `processed`, `R`, hop, both `L` figures, block/floor, `sideActive`, `blendEff`, and `prepares=`/`skipped=` — the last pair formatted at read time, because a declined arrival rebuilds nothing and D-022 B is otherwise invisible. **The host rows it exists to answer are all open** (INDEX §3 rule 4): the instrument is measured, the answers are not |
| F-001 | JUCE 9 latency plumbing | finding |
| F-002 | `/tmp` was noexec and broke juceaide | finding |
| F-003 | JUCE real-only FFT: per-engine scaling, locking fallback, untestable slot clearing, macOS vDSP measured | finding |
| F-004 | A spectral product needs no extra factor under D-006's convention | finding |
| F-005 | The OLA order *is* the latency model: emit, zero, advance, then add | finding |
| F-006 | `HopScheduler` cannot drive a reported latency of 0 at `hostBlock < H` | finding |
| F-007 | Grain-engine per-callback cost: hop-boundary callbacks are 68–266× idle callbacks | finding, absolute µs superseded by F-008 |
| F-008 | The default configure emits no `-O` flag: everything including the shipped plugin builds `-O0` | finding, fixed 2026-10-28 — single-config configures default to `Release` |
| F-009 | `libFftJuce.a` holds the whole JUCE module closure, and non-PIC until a shared object links it | finding, fixed 2026-10-28 |
| F-010 | A JUCE float parameter with a non-dyadic interval cannot express its own default or its own endpoints | finding, worked around in D-011 |
| F-011 | The delta anchor is a DC-heavy signal, so a filter in front of §5.3 un-pins the engine's gain | finding |
| F-012 | A modulo channel map is invisible to the tests, and three other mutation predictions that did not hold | finding |
| F-013 | A rule keyed to the main bus shape leaves commutativity with no domain to be tested over | finding |
| F-014 | A B-side hop buffer sized by one count and indexed by another: a live overflow in a supported layout, invisible to 158 value-level legs | finding, fixed 2026-11-04 by M4 item 02 — the memory-safety leg is now part of the item's bar |
| F-015 | JUCE asks the gate two different questions: `setBusesLayout` applies the layout verbatim or nothing, the per-bus setters substitute through `getNextBestLayout` and can move a bus the host never touched | finding, measured 2026-11-04 in `OfflineRender` §5.18 — no code change; drive layouts with `setBusesLayout`, and read any "the sidechain takes 8" claim as conditional on the main bus (advertisement: 8 beside mono, 2 beside stereo) |
| F-016 | A footprint figure built by re-evaluating the sizing expression cannot notice the expression changing, and the documented bound understated the engine by 1.5F | finding, measured 2026-11-05 — `EntangleProcessor.h` said `9F + 6` plus the transform's `2F` where `GrainEngine.cpp:49`/`:60-65` + `Window.cpp:13` + `JuceFft.cpp:29` are `10.5F + 6` mono and `12.5F + 6` stereo (96 KiB at the default grain length, 768 KiB at the cap); §5.18 printed rings + hop scratch from its own copy of `maxHostBlockSamples + hop`, and two harness `ChainConfig`s copied the floor's `std::max`. Fixed: the floor has one home (`maxHostBlockFor`, called by the harness), §5.18's figure is labelled a restatement, and the measured tables come from `tools/PrepareFootprint` |
| F-017 | A leg whose expected output is produced by the component under test cannot witness a mutation inside that component, and a harness fill sized by the applied layout crashes instead of reporting | finding, measured 2026-11-06 in the M4 item 06 arms — arm 6 (`hopA[slot] = slot`) left `OfflineRender` §5.20 green at 175 `ok` / 0 `FAIL` and was caught only by `EngineTests` #61, and arm 2 (a broadcast B slot resolved to silence) left both broadcast cells green, because every §5.20 model builds its own `engine::HopScheduler` and moves with the mutant. Fixed in the same item: mapped slots must carry a sample in absolute terms as well as match their model (`deadMapped` in M4/M5), which turns arm 2 into 37 red matrix lines, and M1's fills are bounded by the cell's streams with the five sample legs gated on M1, which turns the layout-misapplying arm 7b from a SIGSEGV inside `checkChannelMatrix` into 9 M1 FAIL lines |
| F-018 | JUCE claims a message thread for whoever constructs one, calls a disabled bus `Discrete #0`, and a prepare-snapshot report can never name a refusal | finding, measured 2026-11-08 building the M4 item 08 instrument — `MessageManager`'s constructor records its own thread (juce_MessageManager.cpp:38), so `initialiseJuce_GUI()` makes a console harness's `main()` the message thread and the report guard passes there for every prepare, while `setCurrentThreadAsMessageThread()` is called only from the plugin client (juce_audio_plugin_client_VST3.cpp:280); `AudioChannelSet::disabled().getDescription()` is `Discrete #0`, not `Disabled`, because `isDiscreteLayout()` is tested first (juce_AudioChannelSet.cpp:424) — with the pinned naming table for D-021 B's third trigger (`6:5.1 Surround`, `8:7.1 Surround`, `4:1st Order Ambisonics`, `9:2nd Order Ambisonics`, `4:Quadraphonic`); there is **no `AudioChannelSet::getName()`**, only `getDescription()`; a report built from a prepare can never carry `mainWidth`/`channelRule`, because §5.18 measured 0 of 192 probes landing on a refused layout; and `OfflineRender` under LeakSanitizer reports 14.48 MB in 297 records, **every one** through `juce::dsp::FFT`'s FFTW backend (`FFTWImpl::create`), 274 of them (12.0 MB) from legs that predate item 08 — a third-party at-exit report, not product leakage |

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

**Amended 2026-10-28, item 6.** The first clause of that evidence is dead. `OfflineRender` used to
assert `firstNonZeroIndex(impulse render) − n0 == getLatencySamples()`, which is a false premise for
any engine with a wet path: the first non-zero sample sits at the *physical* delay, so with the
interim dry add (D-009) it measures R and says nothing about the wet stream, and without one it
measures L. The pre-fill model itself is unchanged and still holds — what the harness measures now is
that the declaration and the pre-fill agree, by two independent means: the dry component of an
impulse lands at exactly `n0 + R`, and `getLatencySamples() == hop` for all 28 (hop, L) pairs the UI
can express, unchanged as L moves with H fixed (`M2-item-6-impl-plan.md` §5.3a, §5.4). The wet
response is asserted separately, at `n0 + R + L`. `latencySamples` is still the `hop` placeholder —
`configureRings` sets it and `setLatencySamples` remains the only route to JUCE — because the
representation itself is still parked (F-006, and §6 of the item-6 plan).

## D-005 — Test framework and how it is obtained

**Options.**
- JUCE `UnitTest` — needs a JUCE-linked console app, wrong for JUCE-light libraries.
- Catch2 via `FetchContent_MakeAvailable` — the obvious reading of AGENTS §10.
- Catch2 fetched and compiled under our own target.
- doctest.

**Choice.** Catch2 pinned to tag `v3.7.1` via `FetchContent_Populate`, compiled from
`extras/catch_amalgamated.cpp` into a project-local target `EntangleCatch2`, with
`catch_discover_tests` taken from the fetched `extras/` CMake modules.

## D-006 — Transform backend

**Context.** M2 item 1 needs a transform seam (§9): forward and inverse behind an interface,
so the installed engine can be replaced without touching convolution or grain code. Two
facts constrain it. JUCE has no `IFft` — `juce::dsp::FFT` is a concrete class
(`grep IFft juce/modules/juce_dsp/` returns nothing), so the interface is ours. And the
engine is per-platform by design: vDSP on macOS, FFTW elsewhere. JUCE picks it at **runtime**
— `createBestEngineForPlatform` returns the highest-priority engine whose `create()`
succeeds (`juce_FFT.cpp:60-66`) — and the FFTW engine only exists if `dlopen` resolves the
soname, with a miss returning null and falling through **with no diagnostic** (`:615-623`).

**Options.**
- Use `juce::dsp::FFT` directly at call sites. Rejected: no seam, and its scaling is
  per-engine (F-003), so the difference ships as a level change rather than a compile error.
- `IFft` plus a gain constant keyed on the compile target. Rejected: the engine is a runtime
  property, so `#if JUCE_MAC` is wrong the moment a Mac has IPP/MKL, or a Linux host has
  neither FFTW nor vDSP and silently gets the fallback.
- `IFft` plus one adapter over `juce::dsp::FFT` that **measures** its engine's gain once in
  `prepare()` and divides it out.
- Call FFTW directly behind `IFft`, bypassing JUCE's dispatch. Rejected for M2: it forfeits
  the native Accelerate path that makes macOS the optimisation target.
- Always the fallback engine, avoiding FFTW's GPL entirely. Rejected: it locks per
  `perform()` (F-003), which is an AGENTS §7 violation on the hop path.

**Choice.** `fft::IFft` (`src/Fft/Fft.hpp`) with two implementations: `fft::Dft`, a naive
`O(F²)` oracle that defines what correct means, and `fft::JuceFft`, one adapter over
`juce::dsp::FFT`. The header owns the contract — bins `[0, F/2]` interleaved re/im as
`F + 2` floats, forward unnormalised, inverse carrying `1/F` — and the adapter imposes it on
whatever engine it got by calibrating in its constructor: transform a unit impulse for
`forwardGain`, feed the resulting convention back for `inverseGain`, divide both out, and
expose them so a test reads them instead of inferring them. `JUCE_DSP_USE_SHARED_FFTW=1` is
set on non-Apple platforms only and **PUBLIC**, because `juce_add_module` attaches the
module `.cpp` as INTERFACE sources — a PRIVATE define would compile `FFTWImpl` with two
layouts in one link.

The calibration is what turns a silent engine downgrade into a red test: an unexpected gain
value is a number a test can assert, where 6 dB of level drift is not.

**Licence exposure (recorded per the M2 item-2/4 plan §7).** This choice pulls in **FFTW on every
non-Apple build**, and FFTW is **GPLv2-or-later** — a licence that propagates to a combined work,
which a linked plugin is. The option that avoided it (always the fallback engine) was rejected above
for a real-time reason, so the exposure is accepted, not overlooked. It is inert while the work is
unreleased, and the engine is a *runtime* dependency (`dlopen`, not a link-time `NEEDED` entry) only
in the sense that JUCE resolves it lazily — a redistributed build that relies on FFTW for its
performance still needs either a compatible licence for the whole plugin or a purchased FFTW
commercial licence. `JUCE_DSP_USE_SHARED_FFTW=1` is set unconditionally for `NOT APPLE` in
`src/FftJuce/CMakeLists.txt`, so the flag is a one-line change; the Apple path never touches FFTW and
carries no such question. **Blocking before first distribution, not before M3:** decide between GPL
release, FFTW commercial licence, or shipping a fallback-only binary behind a build flag.

**Revisit trigger.** M7 adds or swaps an engine, or a host reports a `forwardGain()` other than
the 1.0 recorded below — both shipped engines measure 1.0, so a different value means a third
engine (IPP/MKL) or one whose scaling JUCE does not already compensate. A non-finite gain or a
failing round-trip is a real break but is **not** evidence about which engine won: the fallback
reports 1.0 too (F-003), so on macOS engine identity comes from the closed engine list, never
from the number.

**Evidence — FFTW leg.** Measured in-container (Linux/aarch64, FFTW 3.3.10,
`JUCE_DSP_USE_SHARED_FFTW` on): `forwardGain == inverseGain == 1.0` at
`F ∈ {64, 256, 1024, 4096}`; `JuceFft` matches `Dft` bin for bin; round-trip within `1e-5`;
DC/Nyquist imaginary exactly zero; zero allocations inside `forward`/`inverse`; and a
`RTLD_NOLOAD` probe proves `libfftw3f.so` is mapped, so FFTW — not the locking fallback — won.
Discovery proven live: tightening the bin tolerance to `1e-12` turned exactly one test red.

**Evidence — vDSP leg (measured on host, 2026-10-26).** macOS 26.5.2 (25F84), arm64, Apple
clang 16.0.0: `forwardGain == inverseGain == 1.0` at all four sizes — **numerically identical to
the FFTW leg**. `JuceFft` matches `Dft` bin for bin, so the layout is confirmed the same on the
real engine, not just by reading `mirrorResult`; round-trip, zeroed DC/Nyquist imaginary slots
and zero allocations in `forward`/`inverse` all hold. 32/32 ctest green on the host,
`vstplug_VST3` links.

**The predicted 0.5 was wrong, and the reason matters more than the number.** This decision
recorded "`forwardGain()` should report ≈ 0.5 on a Mac" from `forwardNormalisation = 0.5f`
(`:456`, applied `:491`). That read the normalisation in isolation: vDSP's `zrip` forward carries
an intrinsic 2× of its own — measured directly against Accelerate, an impulse returns
`(2.000, 2.000)` in DC, `2.0` in bin 1 — and JUCE's `0.5f` divides **exactly that** out. Net
engine gain through JUCE is therefore 1.0, the same as FFTW's. The two plan docs disagreed on
this (`M2-item1-ifft-plan` §2 anticipated the internal 2×; the verification item's table did
not); the host settled it in favour of the 2× reading.

**Consequence: on macOS the gain does not name the engine.** Every candidate — vDSP, FFTW, the
fallback — reports 1.0, so §"the calibration turns a silent downgrade into a red test" holds only
for a gain-shifting engine, not for the downgrade that actually threatens macOS. What excludes
the fallback there is that the engine list is **closed**: `src/FftJuce/CMakeLists.txt` sets
`JUCE_DSP_USE_SHARED_FFTW` under `if(NOT APPLE)`, MKL and IPP default off, so only
`AppleFFT` (priority 5) and `FFTFallback` (−1) are registered; `nm` on the test binary shows
`EngineImpl<AppleFFT>` and `AppleFFT::create` linked and no FFTW/MKL/IPP engine symbol, and
`AppleFFT::create()` has no failure path — so priority 5 always wins. Asserted by
`JuceFft on macOS has no FFTW engine to fall through to`. A mapped Accelerate is necessary but
**not** sufficient evidence, since `juce_dsp` calls vDSP outside the FFT engines too.

**Do not run the standalone `RTLD_NOLOAD` probe on macOS.** `dlopen(..., RTLD_NOLOAD)` from a
process that never linked Accelerate reports "no" even on a host where vDSP is certainly the
engine; run verbatim here it printed `Accelerate mapped: no` for a correctly-linked binary.
Residency questions must be asked from inside the process that built the transform — which is
what the in-test assertion does.

**Reading the number on a host.** `ctest --output-on-failure` prints **nothing** for a test
that passes, so it cannot report a gain. Either run the program directly —
`./build/release/src/FftJuce/tests/FftJuceTests "JuceFft calibrated gains*"` — or
`ctest --test-dir build/release -R "calibrated gains" -V`. Both print one
`JuceFft F=<n> forwardGain=<g> inverseGain=<h>` line per size, pass or fail.

---

## D-007 — Every build tree lives in `build/<config>` at the repo root

**Context.** The container mounts `.:/workspace` and shields exactly one path name,
`build:/workspace/build`. A tree configured anywhere else — the `build-rel` tree F-008's CPU
comparison used, the top-level `CMakeFiles/` an in-source configure leaves behind — reaches
the host through the bind mount carrying the container's numeric uid. Docker passes uid/gid
through numerically, so anything written by a root-context invocation (`exec -u root`,
`compose cp`) lands root-owned on APFS, and unlink needs write+execute on the *containing*
directory: one directory the host cannot unlink aborts everything under it, and `rm -rf
build` fails partway with `permission denied`. Separately, the shielded name is a *named
volume*, which lives in the VM's filesystem: a host `build/` and the container's
`/workspace/build` are two different trees under one name, so cleaning one silently leaves
the other.

**Options.** Pin the container user with `user:` so writes match the host uid; give each
configuration its own volume mount; or put every build tree under the one path the volume
already shields.

**Choice.** `build/<config>` — `build/debug`, `build/release` — as the only build-tree
locations on both sides, always spelled relative to the repo root. The same command then
works on the host and in the container, and in the container the volume intercepts the path
before it reaches APFS, so configurations cannot clobber each other or the host tree.
`cmd/install [debug|release]` owns the directory→`CMAKE_BUILD_TYPE` mapping; `make` exposes
`BUILD_TYPE` (canonical CMake spelling, default `Release`) and derives the directory name
from it, so `make build` yields a Release tree exactly as before. `.gitignore` keeps
`/build-rel` so a stale command cannot dirty the tree.

**Consequence.** Host and container trees of the same name are different storage: `rm -rf
build` on the host clears host trees only, and a container tree goes with
`docker compose exec vst3-dev rm -rf /workspace/build/<config>`. `configure_file` stays one
bind-mount write by design — `src/Entangle/Entangle.h` is generated into the source tree and
is tracked; it is a single file, not a tree, so it is not the deletion hazard.

**Revisit trigger:** a build tree that must live outside `build/` — an external harness
insisting on its own output directory — needs its own volume mount, not the bind mount.

**Evidence.** 2026-10-28, macOS arm64: `find . ! -user mike` over the tree is empty, so the
incident left no surviving un-mappable file and the fix is structural rather than a cleanup;
`git check-ignore build-rel` matched nothing before this change (`.gitignore:37` was `/build`,
which does not match it) while `build-rel/` sat untracked in the working tree. **Not
verified:** which invocation wrote the un-deletable tree — the container-side checks
(`docker inspect vst3-dev`, `docker exec vst3-dev id`) had not been run when this was
written. The uid pass-through and the named volume living in the VM are Docker Desktop
semantics, not observations of this repo's files.

---

## D-008 — Grain-length set, the bounds on the (L, H) pair, and its measured cost

**Context.** Item 6 puts a grain-length control on the shell, and `L` is the one figure that sets
what the plugin can do musically (grain duration in mode 1, IR length in mode 2) *and* what it
costs: cost per second is `F log F / H`, so a dial position is only affordable together with the
hop the other dial is on. Three constraints had to be squared: the engine's (`H | L`, and
`F = nextPow2(2L-1)` collapses to `2L` only for a power-of-two `L`), the COLA sum's, and the host's
callback deadline.

**Options.**
- A time-valued dial in ms, converted to samples at `prepareToPlay`.
- A free integer parameter in samples.
- A fixed power-of-two sample list, with the ms figure shown as a readout.

**Choice.** The sample list `{1024, 2048, 4096, 8192, 16384, 32768, 65536}`, default **8192** (index
3). Powers of two because `H | L` then holds for every pair in D-003's hop set without a second
rule, and because `F = 2L` exactly — a non-power-of-two `L` would round `F` up and pay for bins that
carry no grain. Rejected the ms dial: the numbers would move with the sample rate, so a preset saved
at 44.1 kHz would resolve to a different `L` at 96 kHz, and the choice-index a host stores would stop
meaning anything. The ms figure the user actually wants is derived and displayed (`EntangleEditor`'s
readout, `"8192 samples = 170.7 ms"`), never stored. A free integer was rejected for the same reason
as item 2's hop: an unbounded dial reaches configurations the engine cannot run.

**Bounds, one rule in `clampGrainLength`, shared by `configureRings` and the editor.**
`L ∈ [2H, min(65536, 128·H)]`, after which `L | H` holds by construction.

- **Floor `2H`.** `H | L` alone admits `H = L`, where Hann has no overlap to sum: the hop-class sum
  becomes `w[phase]` rather than the `C = L/(2H)` the engine's `1/C` gain assumes, so the engine's
  own normalisation stops describing the signal. Two hops of coverage is the minimum the gain model
  is true at.
- **Ceiling `128·H`** is the CPU gate, and it is a bound on the *pair* because that is what the cost
  law says. With the shipped sets it excludes exactly three combinations — L = 32768 and 65536 at
  H = 128, and 65536 at H = 256 — and admits every other pair the two dials can express. The editor
  readout prints the reduction rather than hiding it, because a control that reports the dial
  position instead of the running value is a control that lies.

**Measured cost — this is the data the bound sits on, not an estimate.** `tools/CpuBaseline`,
`Release` (`-O3`), 48 kHz, R = H, 3000 hops per row, `processHop` alone, `% ` of one core,
hostBlock 32 / 64, 12-core aarch64 Linux. Raw output: `docs/cpubaseline-m2-item6.log`.

| L | F | H = 128 mono | H = 128 stereo | H = 512 mono | H = 512 stereo | stereo spike µs |
|---|---|---|---|---|---|---|
| 1024 | 2048 | 0.37 / 0.30 | 0.62 / 0.61 | 0.08 / 0.08 | 0.15 / 0.15 | 47 |
| 2048 | 4096 | 0.66 / 0.65 | 1.35 / 1.32 | 0.16 / 0.16 | 0.32 / 0.32 | 65 |
| 4096 | 8192 | 1.34 / 1.34 | 2.69 / 2.68 | 0.33 / 0.33 | 0.66 / 0.66 | 155 |
| 8192 **(default)** | 16384 | 2.87 / 2.83 | 5.66 / 5.58 | 0.70 / 0.70 | 1.40 / 1.40 | 237 |
| 16384 | 32768 | 7.21 / 7.25 | 14.51 / 14.53 | 1.85 / 1.84 | 3.66 / 3.66 | 578 |
| 32768 | 65536 | 15.79 / 15.30 | **30.64 / 30.68** | 3.80 / 3.79 | 7.76 / 7.78 | 1035 |
| 65536 | 131072 | 40.91 / 40.37 | **80.76 / 79.39** | 10.17 / 10.16 | 20.22 / 20.23 | 3353 |

Four things the run settled:

- **Stereo is 2× mono, so every 2×-mono extrapolation on record was sound.** Measured ratio
  1.69–2.06 across all fourteen shapes, 1.94–2.06 above `L = 2048`. The `1.69×` at `L = 1024` is not
  a channel-sharing win — it is two channels' FFTs fitting in cache that one spills at larger sizes.
- **`F log F` holds up to `L = 8192` and breaks above it, in the pessimistic direction.** Measured
  step-ups at H = 512 are 2.13×, 2.06×, 2.12× against the 2.18×, 2.17×, 2.15× the model predicts,
  then **2.61×** at 8192→16384 and **2.60×** at 32768→65536 against 2.14× and 2.12×. The working-set
  risk the item-6 plan flagged at `L = 65536` is real and starts one step earlier than predicted, so
  the two longest presets are documented with their own numbers rather than interpolated past.
- **The gate at 25 % of one core admits every pair up to `L = 128·H`** — worst admitted case
  20.2 % (`L = 65536` at H = 512) — and excludes the three above it. The default pair (H = 128,
  L = 8192) costs 5.66 %.
- **Load is not the binding constraint at the top of the set; the spike is.** With `R` a declaration
  rather than a deferral (D-004), a whole hop's FFT must finish inside the callback that crosses the
  boundary. At `L = 65536` the stereo spike is 3.2–3.4 ms, which busts a 128-sample callback
  (2.67 ms at 48 kHz) even though its *mean* load is only 20 %; `L = 16384` at 578 µs still clears a
  32-sample callback. So the admitted `L = 65536` pair is real-time capable only on a host blocking
  at 256 or more, and that is a property of the pair no percentage in this table would have shown.

**Footprint, bounded next to the cap (AGENTS §8).** Stereo, at the 8192 default: history `2·L·N` +
accumulator `2·L·N` + scratch `≈ 5F` floats ≈ **0.7 MB**, plus the transform's own tables. At the
65536 cap ≈ **5.5 MB**, which is where the cache cliff in the table above lives. All of it
prepare-time; nothing on the audio path grows.

**Consequence.** Changing either dial re-prepares: `parameterChanged` fires `triggerAsyncUpdate()`,
and `handleAsyncUpdate` runs the D-003 `suspendProcessing` → re-prepare → resume path. Rings are
sized for the maximum configuration (`maxHostBlockSamples`, `maxGrainLengthSamples`) once, so a dial
move reallocates the engine and the transform, not the scheduler's rings.

**Revisit trigger:** the transform changes (D-006), which moves the constant in front of `F log F`
and with it the ratio the gate admits; or a host asks for `L > 65536`, which is a different memory
story, not a bigger number in this list.

**Evidence.** 2026-10-28, in-container `Release`, `docs/cpubaseline-m2-item6.log` (144 lines, 14
shapes × hostBlock {32, 64} × channels {1, 2}) and `tools/OfflineRender` §5.9 for the clamp over all
28 UI pairs. **Host caveat:** these are one machine's numbers, and it is roughly 1.3× faster per hop
than the Ryzen 5950X column and 2.2× faster than the Apple Silicon column in the item-6 plan's table
(at `L = 2048`, H = 512: 17.1 µs here, 22.0 there, 37.9 on Apple). `128` is therefore this host's
crossing point, and a slower host should lower `maxGrainToHopRatio` rather than trust the shape of
the curve — the ratio is the rule, the constant is a knob. **Decided 2026-10-28:** the constant
ships at this host's crossing point; re-measuring it on the release hardware is not wanted here,
and further perf work plus any parameter-constraint validation are deferred to live testing. The
caveat above is context for whoever does that testing, not an open task. **Not verified:** the
spike column is a `max` over 3000 hops on a shared container, so it includes scheduling noise; and
no measurement here was taken inside a real host, only through `processBlock` offline.

---

## D-009 — Interim unity dry + wet sum

**Context.** Item 6 replaces `PassthroughEngine` with `grain::GrainConvolutionEngine`. The shell
emits exactly what the scheduler drains, and with no sidechain bus B is silence (D-001, D-002), so
a pure-wet shell renders silence until a host routes a second bus — on a host that cannot offer an
extra bus that is forever, not a corner case. The passthrough made exactly that configuration
audible, which is why `OfflineRender`'s no-sidechain assertions existed at all.

**Options.**
- Ship pure wet and require a sidechain.
- Sum a dry A path at unity, blend dial still unconsumed.
- Invent the mix law now.

**Choice.** The unity sum, as `engine::DryWetEngine`: a decorator over any `Engine` that calls
`inner->processHop(a, b, scratch, n)` into its own hop scratch and writes `out = scratch + a`,
forwarding `prepare`, `reset`, `latencySamples()` and `wetDelaySamples()` untouched. Rejected the
third option because the mix law is M3's decision, and a gain figure invented here would enter a
signal path during review rather than a log.

Two commitments come with it:

- **The dry path is not a delay line.** The scheduler's pre-fill delays everything it drains by R
  (D-004), so an add made inside `processHop` reaches the host at the same `+R` as the wet stream —
  bit-exactly, with no allocation and no extra declared latency. Writing it in the shell would have
  been both redundant work and wrong: `processBlock`'s main output aliases its main input, so
  reading A after `scheduler.process()` has written the buffer reads the engine's output, not A.
- **Headroom is now a live number, and stays M3's.** Two unity streams can exceed full scale and A
  correlated with B — the musically interesting case — is where it happens. Measured through the
  plugin at the default configuration (H = 128, L = 8192, R = 128, stereo, ±0.4 input): peak
  **2.012** with independent A and B, **2.916** with A == B. At full-scale input, **11.23** and
  **16.77**. Nothing is scaled or clamped to make those fit. The trim that would have the default
  correlated case land at unity is **1/2.916, i.e. −9.3 dB on the summed output**, and it is
  recorded here rather than coded, per §0A of the item-6 plan. M3's law owns it, along with the
  safety clamp that `M2-grain-engine.md` §8 still asks for and item 2/4 deliberately did not invent.

**What M3 inherits.** The decorator is where the law goes, so M3 replaces the body of
`DryWetEngine::processHop` and keeps the harness legs that wrap it (§5.1 shell-identity and §5.5
dry-path, both in `tools/OfflineRender`) plus the peak figures above as their baseline. The Morph
knob still does nothing: `smoothedBlendValue` is ramped in `processBlock` and consumed by nobody,
and a control with no law behind it is a control that lies.

**Revisit trigger.** M3 writing the mix law. Earlier if a host reports clipping at levels where the
old passthrough did not clip — expected on the numbers above, and the reason this entry carries
them.

**Evidence.** 2026-10-28, in-container `Release` (`-O3`), `tools/OfflineRender`: "sidechain
disabled: A delayed by R … bit-exact" and "sidechain enabled and silent: same A delayed by R" at
both (H, L) = (512, 1024) and (128, 8192); "shell minus dry == pure-wet render" to 1.19e-07
absolute, against a `1e-6` tolerance set by the float rounding of the add itself; peaks as above.
Library level, `src/Engine/tests/DryWetEngineTests.cpp`: unity sum bit-exact against a stub inner
engine whose output depends on hop index and channel, a silent inner leaves A bit-exact, a
passthrough inner doubles it (unity on *both* streams), R and L forwarded unchanged, zero
allocations in `processHop`, and — under `HopScheduler` — the sum arriving at exactly the declared
R with no delay line anywhere.

---

## D-010 — The mix law: equal-power blend, dry B, endpoint snapping, unity, and the no-sidechain pin

**Context.** D-009 summed `wet + A` at unity and said outright that the law is M3's decision. With
the law installed the blend dial does something for the first time, and three things that were free
under the interim sum stop being free: what the weights are, what happens at the endpoints where
`sin`/`cos` do not produce 0 and 1, and what the plugin outputs when there is no B at all — because
the law makes `p = 0` *pure convolution*, which under D-002 is silence.

**Options.**
- Constant-gain pan (`w_dry = t`, `w_wet = 1 − t`): no level dip on uncorrelated material, and the
  milestone's §4 wording asks for equal power with constant gain noted as an alternative to consider.
- The milestone's equal-power law, `w_dry = sin(t·π/2)`, `w_wet = cos(t·π/2)`.
- Either of those with a trim baked in to keep the sum under full scale.
- Either of those trusting `sinf`/`cosf` at the endpoints.

**Choice.** Equal power, snapped at three positions, at unity:

| Dial | Dry signal | `w_dry` | `w_wet` |
|---|---|---|---|
| p = 0 | — | exactly `0.0f` | exactly `1.0f` |
| −1 < p < 0 | A | `sin(t·π/2)` | `cos(t·π/2)` |
| p = −1 | A | exactly `1.0f` | exactly `0.0f` |
| 0 < p < +1 | B | `sin(t·π/2)` | `cos(t·π/2)` |
| p = +1 | B | exactly `1.0f` | exactly `0.0f` |

with `t = |p|`, and the whole stage

```
out = g · [ β · ( w_dry · dry + w_wet · conv ) + (1 − β) · A ]
```

`g` the output-gain parameter (0 dB exactly `1.0f`, D-011), `β` the effect coefficient (1 in the
effect, 0 bypassed onto A). Five commitments come with it, and each is a consequence of something
measured rather than a preference:

- **The three exact rows are values, not a tolerance.** `cosf(pi/2f) == -4.371138829e-08`, so a
  literal law leaves `conv × −4.37e-08` in a sum that M3 §7 requires bit-exact. It was expected to
  show only on the loud correlated material of D-009; running the mutation said otherwise — with the
  snapping removed, all eight endpoint legs in `OfflineRender` §5.10 go red, at both ±0.4 and
  full-scale input and in both the independent and the `A == B` pairing, because what decides it is
  the size of the *sample* the residue lands on, not the level of the render. Snapping costs three
  comparisons once per hop and buys the criterion.
- **The dry bus swaps only at `p = 0`.** `w_dry(0) == 0`, so the hand-over from A to B drops no
  sample of the outgoing bus and takes no early sample of the incoming one. The sign is tracked on
  the raw ramp value before the crossing snap, or the sample after the landing reads as a second
  crossing and pins the dial at zero.
- **Unity, and no trim.** The −9.3 dB figure of D-009 stays in that entry as a number. What the dial
  actually costs, at the default configuration (H = 128, L = 8192, R = 128, stereo) and reported by
  `OfflineRender` §5.7 rather than gated: peak **1.68** independent / **2.54** with `A == B` at ±0.4
  input, **10.51** and **15.86** at full scale, with the worst point of the travel at the centre —
  pure convolution — where the correlated case sits ~0.5 dB above the independent one across the
  middle of the sweep and falls to the dry level at both ends. So the soft clipper of item 02 is
  engaged at default settings, and the level-flatness half of M3 §7 stays blocked with the
  normalizer: the milestone's own §3 says A and conv are partially correlated, which is exactly why a
  ±2 dB flatness target is unreachable at unity.
- **The law contributes no gain and no delay to the wet path.** `grain::GrainConvolutionEngine`'s
  1/C remains the whole wet gain table; `prepare`, `reset`, `latencySamples()` and
  `wetDelaySamples()` forward to the inner engine untouched. The `L − R` offset between dry and wet
  is D-004's, and is not this law's to close.
- **With the sidechain bus disabled the effective `p` is −1**, whatever the dial says. Without the
  pin the plugin is silent at its own default on any host that cannot offer a second bus, which is
  the property D-009 was created to keep; the law made `p = 0` pure convolution and so turned that
  property into a choice. The dial keeps its value and a prepare that sees a sidechain restores it —
  which means **a preset saved at `p = 0` with a sidechain renders A in a host without one**. The
  editor reports the effective position, not the dial's, on the same grounds as the grain-length
  readout: a control that lies about what is running is worse than no control.

**Revisit trigger.** Listening, on the user's own host, says the unity result is unusable — that is
D-009's own trigger and it reopens the trim, with the §5.7 sweep as the input. The blocked
normalizer's target decision may move these levels again. And if item 02's DC blocker breaks the
endpoint legs, the exactness requirement moves into 02's contract, which is why this entry states it
twice rather than leaving it to a comment.

**Evidence.** 2026-10-29, in-container `Release` (`-O3` + LTO), `ctest --test-dir build/release`
102/102 with `OfflineRender` green. `OfflineRender` §5.10: `p = −1` → A and `p = +1` → B bit-exact
delayed by R = 128, at 0.4 and 1.0 amplitude, independent and `A == B`; 257 dial positions with no
sidechain report pure A and every twentieth renders A delayed by R bit-exact; the dial returns after
the bus reappears. §5.3 delta anchor lands at `n0 + R + L + m` to a worst 3.69e-07 against a
1.0 peak, unchanged from before the law. §5.1 shell-identity bit-identical through the shared chain
builder. Library level, `src/Engine/tests/MixEngineTests` (12 cases, 125 003 assertions): the weight
table over the dial's own grid, the three exact rows, settled positions handing back dry A / the wet
stream / dry B bit-exactly, and both dry paths arriving at the declared R under `HopScheduler`.
Discriminators run, not asserted: removing the endpoint snapping reddens all eight §5.10 endpoint
legs; doubling the dry-B weight reddens the weight table and every `+1` leg; 1 % on the wet term
reddens §5.3, the centre-versus-pure-wet leg and every gain leg, while §5.1 stays green — the shared
builder is what keeps the harness comparing what ships.

---

## D-011 — Blend, output gain and bypass: the parameter layer and the sample-domain ramp

**Context.** Blend existed with range `[-1, 1]` and a one-pole `blendSmoothingPerCallback = 0.02f`
applied once per *callback* and consumed by nobody. Per callback is the wrong unit: τ is 49.5
callbacks, so the same gesture takes 66 ms at hostBlock 64, 132 ms at 128, 528 ms at 512 and 4.22 s
at 4096 — and 4096 is inside this shell's own `maxHostBlockSamples`. It also never lands exactly on
its target. Output gain and bypass did not exist, and the two choice parameters are indexed by
*position* in a saved preset, so anything added has to arrive at the end.

**Options.**
- Ramp in the shell with `juce::SmoothedValue`, keeping the stage free of time constants.
- Ramp inside the stage, which owns the output-sample clock.
- Bypass as routing (skip the wet engine, or swap the declared latency) vs bypass as a coefficient.

**Choice.** Inside the stage. `juce::SmoothedValue` lives in `juce_audio_basics/utilities/`, so using
it would put a JUCE header into a JUCE-light library (AGENTS §9); and a ramp is a function of output
samples, which only the class holding the sample counter knows. So the stage takes three *targets* —
`setBlendTarget` in dial units, `setOutputGainTarget` in dB, `setBypassTarget` as a bool — and ramps
each with a straight line in the stored domain per output sample: 20 ms blend, 20 ms output gain,
10 ms bypass. The final step *assigns* the target rather than evaluating the line at progress 1.0,
so every endpoint criterion holds once anything has settled. `prepare` and `reset` snap: both drop
the buffered stream, and a ramp would otherwise spend its time constant replaying a gesture the
transport has already discarded. The shell hands the targets down in `processBlock`, and again in
`configureRings` and in `reset()` *before* `scheduler.reset()` — that call is what snaps the stage's
ramps, so pushing afterwards opens a fresh transport on the previous setting and crossfades through
the first 20 ms of the new one.

- **Parameters.** Blend `[-1, 1]`, default 0, automatable, quantum `1/128` (F-010). Output gain
  `[-60, +12]` dB, default 0, automatable, quantum `0.125` dB, for the same reason. Bypass bool,
  default false, automatable. All three appended after the two choice parameters, because the choices
  are positional in a saved preset and the float and bool ones are looked up by id; a state file
  written before this item loads with the new parameters at their defaults and the old three
  unchanged, which `OfflineRender` §5.10 asserts by stripping the two children out of a real state
  and reading the remainder into an instance configured differently.
- **Bypass is a crossfade onto dry A**, on the stream the scheduler already drained. It switches no
  routing and touches no buffer, so it cannot move R — proved rather than assumed by the leg that
  reads `getLatencySamples()` before and across a bypass cycle, and by the mutation that makes bypass
  swap the declared latency instead, which reddens exactly that leg and nothing else. The dial
  survives a bypass cycle bit for bit.
- **Reads are `apvts.getRawParameterValue(id)->load()`** on the audio thread, never UI state, and the
  no-sidechain pin lives in one function that both the DSP path and the editor call, so the readout
  and the audio cannot disagree. None of the three triggers a re-prepare; hop and grain length keep
  their existing `AsyncUpdater` path.

**Revisit trigger.** The three ramp times are opening values to confirm by ear, which is the user's
job on his own host, not a number to defend on paper — 20 ms is far brisker than the per-callback
one-pole it replaces at any ordinary block size. A log-domain gain ramp is worth trying if the
bottom of the range feels abrupt. The normalisation-amount parameter stays blocked behind the
normalizer decision and is not part of this layer.

**Evidence.** 2026-10-29, in-container `Release`. `src/Engine/tests/MixEngineTests`: the ramp lands
on its target bit-exactly at a sample count taken from its own milliseconds; the automation curve is
bit-identical through hostBlock 128, 256 and 512 — the criterion the per-callback one-pole cannot
pass; the click metric `d2(x) = max |x[n] − 2x[n−1] + x[n−2]|`, reported as a ratio against the same
content rendered static, separates the shipped path from the stepped known-bad by 15×/20×/39.6×/35×/
21.7× against a limit of 8.0. `OfflineRender` §5.10: bypassed output bit-exact A delayed by R with
the wet and dry-B contributions reaching exactly zero, declared latency unchanged at 128, gain
matching `10^(dB/20)` bit-identically at −60, −12, 0, +6 and +12 dB, finite and monotone over all 73
values in range. Discriminators run: stepping the coefficients once per hop instead of per sample
reddens the settling and bypass legs (and *not* the click or block-size legs — see the item file's
§8), and removing the ramp entirely reddens the click metric, the block-size invariance, the
settling and the bypass legs.

---

## D-012 — Output safety stage: a memoryless tanh clip, knee at 0 dBFS, ceiling at +6 dB

**Context.** M3 §4 item 6 asks for output limiting. Under D-009/D-010's unity law a correlated pair
at full scale sums to 15.86 before anything else touches it (measured, §5.7 below), so the plugin can
put out fifteen times full scale on material a host can hand it today. The milestone is explicit that
the guarantee must not cost delay, and 02's decision A rules the whole question out of the latency
model rather than into it.

**Options.**
- `tanh` over the whole range — the textbook soft clip. Rejected: it is never the identity, so every
  signal a host already handles correctly gets rewritten and D-010's bit-exact `p = ±1` endpoints die
  with it.
- Hard clamp at unity. Rejected: the slope steps from 1 to 0 exactly where the input crosses 0 dBFS,
  which is the busiest place in an existing mix, and a slope step is a click.
- Lookahead or feedback limiting. Rejected by the milestone's own scope (§6): the delay it needs is
  real, and R is only honest for what the engine actually defers (D-004).
- Identity to a knee, then a `tanh` ramp onto a ceiling. Taken.

**Choice.** `dsp::SoftClipper`, in `src/Dsp`, JUCE-light, the last thing the output sees before the
host. `knee = 1.0f`, `ceiling = 2.0f`; above the knee
`y = sign(x) · (knee + (ceiling − knee) · tanh((|x| − knee) / (ceiling − knee)))`. Odd, monotone
non-decreasing, and C² across the knee because `tanh''(0) = 0`, so the ramp joins the identity line
without a curvature step.

- **Why the ceiling is above unity rather than at it.** Identity up to `|x| = 1` plus a continuous
  first derivative at the knee leaves no monotone shape bounded by 1: the only bounded monotone
  continuation from the identity approaches its bound asymptotically, so the bound has to sit above
  the value the curve leaves from. A ceiling at 1.0 would mean either a slope step at the knee (the
  rejected clamp) or a knee below unity (which breaks B1 and the endpoint criteria). +6 dB is the
  figure that keeps the stage invisible to anything a host could already handle and still catches the
  sum-overrun the law produces.
- **Memoryless is the contract, not an optimisation.** One sample in, one sample out, no state:
  nothing to latch, ring or drift subnormal (02 decision A); silence is `0.0f` on the sample it
  arrives; output sample *q* depends on input sample *q* alone, which is what lets M3 §4 item 7
  resolve to a proof that nothing moved rather than a compensation to design. It is also why the
  declared latency is untouched — see the leg in D-013's evidence.
- **Non-finite samples become `0.0f` and are counted.** A NaN that reaches the sum would otherwise
  propagate through every later hop through the wet state; replacing it bounds the damage to one
  sample, and the relaxed atomic counter gives a read-out on another thread without a lock. Nothing
  reads the count back into the signal.
- **The ceiling clamp is a class property, not a reachable branch.** `knee + (ceiling − knee) ·
  tanh(u) ≤ ceiling` holds for every finite `u` because IEEE float `tanh` saturates at exactly `1.0f`,
  so deleting the clamp changes no output. 02's §5 mutation table predicted "remove the ceiling clamp
  → B3 red"; running it left all 119 tests green. The clamp stays — a different shape function could
  approach from above, and a test that only ever feeds the curve it already has would never notice —
  and B3 asserts the bound rather than the branch. Recorded because it is the one row of that table
  that does not pay as written.

**Operating points (Release, 2026-10-30, §5.7).** Same input, same law, neither stage → shipped:

| Input | Peak unshaped → shipped | % above knee | d2 shipped / baseline |
|---|---|---|---|
| 0.4 independent | 1.68163 → 1.57266 | 1.57 % | 4.04159 / 4.06349 = 0.99461 |
| 0.4 A == B | 2.53687 → 1.90587 | 7.29 % | 5.4667 / 6.15304 = 0.888455 |
| 1.0 independent | 10.5102 → 2.0 | 45.4 % | 7.99857 / 25.3968 = 0.314944 |
| 1.0 A == B | 15.8554 → 2.0 | 51.4 % | 7.99998 / 38.4565 = 0.208027 |

Across the dial at amplitude 1.0 the post-clip peak is 2.0 at every position from −0.5 to +0.5 and
1.99883 at ±0.75, falling to 0.99997 at the endpoints, where the above-knee fraction is 0 %. No dial
position leaves the chain above the ceiling, and the dry endpoints are transparent.

**What the d2 column does not prove.** The ratio is against the *unshaped* render, and at amplitude
1.0 that baseline is itself the loudest thing in the comparison, so a ratio below 1 says the shipped
chain has less sample-to-sample curvature than a signal that is merely over full scale — it does not
independently demonstrate the absence of clipper clicks. The click-safety argument is structural
(memoryless, C² at the knee) plus the 0 % engagement at the endpoints; the ratio is the number to
watch if a future trim moves the operating point, which is why it is printed per dial position.

**Revisit trigger.** 45–51 % of samples shaped on full-scale correlated material is the §7 risk
realised, not a result to defend: it is item 04's decision input (the level curve in §5.7 exists for
it), and if the normalizer lands, this stage's operating point moves and B7 has to be re-run before
the table above is quoted again. A fixed output trim inside the law would move it too, which is why
02's decision A left the trim to the blocked normalizer.

**Evidence.** 2026-10-30, in-container `Release`. `src/Dsp/tests/SoftClipperTests` carries B1–B5 and
B8 (identity to the knee bit-for-bit, monotone and odd, the ceiling bound, the non-finite rule and its
counter, silence). B1's loops are parameterised on `SoftClipper::knee`, so the case also carries a
literal pin — `knee == 1.0f`, `shape(0.5f) == 0.5f`, `shape(-0.9f) == -0.9f` — added when the
discriminator showed the case as written could not see the constant move. `OfflineRender` §5.3 carries
B6 — the clipper-only render's arrival index equals the raw wet one at all three shapes (n24713, n3586,
n197186) — and §5.4 the unchanged declared latency. §5.10 shows the eight `p = ±1` endpoint legs and the
five gain legs still bit-exact through the stage. Discriminators run: letting NaN through reddens B5
(1 case, 75 assertions); moving the knee to 0.25 reddens B1 once the literal pin is in place, plus the
literal expectations in B4 and B5; removing the clamp reddens nothing, as above.

---

## D-013 — DC blocker on the convolved path: 5 Hz, second-order Butterworth, wet path only

**Context.** F-003's half-spectrum form cannot null DC, and D-008's record shows the anchor's DC
component riding up with L. Measured rather than quoted (02 criterion A10): with a 1 % offset on both
buses the wet path carries DC of **0.4096** with the stage out of the chain — a third of full scale,
on a signal whose AC content the plugin is being asked to blend.

**Options.**
- First-order one-pole HP: cheapest, and cannot meet A2 (0.05 dB of flatness at eight times the
  corner) — it sits 0.067 dB down there.
- Elliptic/Chebyshev for a sharper knee: buys transition width with passband ripple, which is exactly
  what A3 forbids and what the headroom table would then have to explain.
- Second-order Butterworth (Q = 1/√2): the highest Q that is monotone, −0.0011 dB at eight times the
  corner. Taken.

**Choice.** `dsp::DcBlocker`, transposed direct form II, `cornerHz = 5.0` as a constructor default so
a different anchor or engine can be probed without touching the class.

- **Coefficient form.** `b1 = −2·b0`, `b2 = b0`, written as relations rather than three evaluations,
  so `b0 + b1 + b2` is exactly zero in double. The DC null is then a property of the coefficients and
  the recursion only has to hold what they already promise. The measured consequence: the residue
  scales linearly with the offset it is holding back — 3.13e-8 of a full-scale offset, 3.13e-10 at
  0.01, 3.13e-11 at 0.001 — which is the shape A1's "≤ 1e-6 of the DC offset" is stated against.
- **State is double.** The pole radius is √a2 ≈ 0.99985 at the shipped corner. Rounding a float state
  that slow leaves a floor of order 1e-7 *of the signal*, where A1 asks for 1e-6 *of the offset*: with
  a small offset riding on a large signal the float floor is the binding term. Double state removes
  the class of failure instead of tightening a tolerance.
- **Wet path only.** A and B reach the sum unfiltered, so D-010's bit-exact `p = ±1` endpoints survive
  and §5.5's centre leg is the wet stream through the two stages and nothing else. Running the
  blocker over the dry buses instead reddens exactly 01's endpoint legs (A/B through the blocker are
  not bit-exact A/B) and nothing else; running it on nothing at all reddens A1, A4, A10 and the §5.5
  legs. That split is the proof of the placement.
- **Latency honesty.** Output sample *n* depends on input samples ≤ *n* and its own past: the stage
  defers nothing, so R is unchanged (§5.4 reports 128 with the stage in the chain) and nothing moves
  in the latency model — which is what M3 §4 item 7 asked for. What the stage does cost is *settling*:
  **34 283 / 37 315 / 74 630 / 149 260 samples** at 44.1 / 48 / 96 / 192 kHz, 0.777396 s at the
  reference rate, published through `settlingSamples()` rather than left implicit. A10's in-flight
  figure has to be read against that bound: where a 33 024-sample anchor render ends, the DC is
  −58.06 dB; once past the bound it is −232.43 dB (9.79e-13).

**The corner, and what it costs.** A9's probe runs the shipped transfer over the raw anchor at six
corners. Two of its columns matter for the choice: the response's peak (what the stage takes out of
the band) and `|y|` past the analytic support (what the stage's own ring leaves beyond the tail the
engine already declares). At H = 128, L = 8192, 48 kHz:

| Corner | worst \|Hann − filtered\| | filtered peak | max \|y\| past support |
|---|---|---|---|
| 0.5 Hz | 0.321216 | 0.833155 | 0.308315 |
| 1 Hz | 0.552593 | 0.703238 | 0.485678 |
| 2 Hz | 0.842764 | 0.612171 | 0.551344 |
| **5 Hz (shipped)** | **1.11535** | **0.414009** | **0.0827042** |
| 10 Hz | 1.11024 | 0.160964 | 0.110448 |
| 20 Hz | 1.03926 | 0.0427595 | 0.0392494 |

The window deviation cannot decide it — a Hann of peak 1.0 carries mean 0.5, so removing DC moves the
waveform a lot at every corner (F-011). Read against the other two columns the corner has a knee: 2 Hz
buys 3.40 dB of retained peak and costs 6.7× the ring past the support; 10 Hz buys no shorter ring
(0.110 against 0.083) and loses 8.21 dB of peak. 5 Hz sits at that knee.

What the corner costs is set by the grain length, because the anchor response *is* a Hann of length L
and a Hann's main lobe spans ±2 bins, ±2·sr/L. Against a 5 Hz corner that is ±93.8 Hz at L = 1024,
±11.7 Hz at L = 8192 and ±1.46 Hz at L = 65536, and the retained peak follows it down: 0.797657
(−1.96 dB) at L = 1024, 0.414009 at L = 8192, 0.0107268 (−39.4 dB) at L = 65536, where the difference
the stage makes, 1.0105, is itself the size of the raw response peak. That is a property of the probe,
not of program material — a delta convolved into a 1.4 s window is a subsonic object by construction —
and it is why the corner decision is taken against A10 as well as A9: on the program-material case the
stage takes the DC and almost nothing else (0.4096 → −232.43 dB settled, render peak 1.80055 →
1.70621 = −0.47 dB). The §7 risk this item carried — "a corner that is inaudible at L = 1024 can shape
the response at L = 65536" — is confirmed, and it is a reason to keep the corner low, not a reason to
drop the stage.

**Revisit trigger.** Item 04's threshold decision is the next input here: what the normalizer needs
from the low end may move the corner, and the probe is now a table the harness prints on every run, so
re-deciding is a read rather than an experiment. If a future engine's response carries less DC the
stage stays — the A10 case is program material, not the anchor.

**Evidence.** 2026-10-30, in-container `Release`. `src/Dsp/tests/DcBlockerTests` (14 cases, 227 924
assertions): −3 dB corner realised to 5.00028 / 4.99974 / 5.00013 / 5.00006 Hz across the four rates
(≤ 0.006 % off design); DC residue 3.13e-8 of a full-scale offset within the published bound at every
rate; gain at eight times the corner −0.00106 dB (the analytic 10·log10(4096/4097)) and worst passband
gain above unity +3.2e-8 dB, so no peak (A2, A3); plus chunking (A6), channel isolation (A7),
allocation and RT rules (A8). `OfflineRender` §5.11 carries A10, §5.3 the corner probe, §5.5 the
bit-identical centre leg (worst 0). Discriminators run: corner ×100 reddens A2 and the absolute-frequency
legs and moves the reported reshaping; deleting the stage reddens A1/A4/A10; blocking the dry buses
instead reddens 01's endpoint legs.

---

## D-014 — Level detector: measurement only, a two-stage envelope, exact silence

**Context.** M3 §4 item 1 asks for "level detector module (RMS/peak envelope, attack/release in ms) as
a standalone DSP class". Its only consumer — the §4.2 normalizer — is blocked behind a target decision
that has not been taken (`plan/M3-impl-plan/INDEX.md` §9), and M3 §3 leaves the *measurement scheme*
open too (per-grain energy versus a smoothed envelope, "recommend a smoothed envelope, test both"). So
the class has to deliver a measurement without delivering either decision, and be correct on its own
terms: with no consumer, nothing will break if it is wrong, which is exactly why every figure below is
a measured property of the reading rather than a comparison against a caller.

**Options.**
- A policy-bearing detector with a target and a silence gate, pointed at a named bus. Rejected: that is
  item 04's decision, made by whoever types the code first.
- **A single-state follower of the square** — the textbook RMS envelope. Rejected on measurement, not on
  taste. Placing its pole where criterion 3 demands (the published reading crossing 63.2 % of a step at
  τ_attack and 36.8 % at τ_release, which in the power domain means exponents −0.51012 on the rise and
  −2 on the fall) makes it read a stationary sine **+23.24 % high** — measured at 997 Hz and at 5 kHz
  with τ_attack = τ_release = 100 ms, where criterion 1 allows 0.5 %. The same variant passes criterion
  3 to +0.2 %, so the two criteria are not merely tight, they are in opposite directions. The cause is
  structural: a follower with two coefficients settles where α_a·E[(u−p)⁺] = α_r·E[(p−u)⁺], which is not
  the mean of u, and the ripple in its *input* is the whole of the error term.
- Feeding the follower from the boxcar windowed RMS instead of a dedicated stage. Rejected on
  arithmetic: a boxcar's area delay is W/3 samples, so the accuracy of the attack figure becomes a
  function of `integrationMs` — a measurement-window control, not a time constant.
- **Two stages, taken**: a linear mean-square smoother, then an amplitude-domain attack/release follower
  of its square root.

**Choice.** `dsp::LevelDetector`, three readings per channel all live at once, no target, floor, gate or
gain anywhere in the class.

- **The readings.** `windowedRms` is the square root of the mean square over `windowSamples()`;
  `windowedPeak` is the largest magnitude in the same window, kept in a monotone queue so it is O(1) per
  sample like the RMS; `envelope` is the follower. Both windowed readings zero-pad before the start, so
  they settle over exactly one window and a window holding one blip reports the blip.
- **The time constants live on the published reading**, both poles `1 − exp(−1/(τ·sr))` on the amplitude
  and not on the power. The consequence worth stating: with τ_attack = τ_release the follower is a plain
  one-pole, and so is unbiased by construction at any crest factor — the asymmetry, and any bias, exists
  only when the caller asks for one.
- **The inner stage** runs at `min(τ_attack, τ_release)/100` — derived, not a fourth control, and
  published through `innerTimeConstantMs()` because it bounds how low a tone the envelope can resolve.
  Measured envelope error against A/√2: 0.043 % at 997 Hz with a 100 ms constant.
- **What the structure costs.** The inner stage's lag moves the measured step crossing to +0.63 % of τ on
  the attack and +0.21 % on the release, against the single-stage variant's +0.2 %/+0.02 %. Both sit
  inside criterion 3's ±5 % and criterion 5's 1 %, and the tests print the figures on every run so the
  trade is visible rather than buried.
- **Silence is exact, not asymptotic** — this class's half of §7's "no denormal CPU spikes". The RMS sum
  is forced to `0.0` the moment a count says the window holds nothing but zeros: exact by construction
  rather than thresholded, which is why the windowed readings reach exact `0.0f` at exactly
  `windowSamples()` and not "within a tolerance of it". Both envelope stages flush at
  `envelopeFloor` = 1e-12 (−240 dB), and `silenceSamples()` publishes the bound —
  τ_release·ln(from/floor) + 2·τ_inner = 129 313 samples = 2.694 s at release 100 ms/48 kHz. No state of
  the object is ever subnormal, which is the point of flushing rather than waiting for the asymptote.
- **hostBlock never enters.** Every constant is milliseconds realised in `prepare` and the filtering is
  per sample: chunk 1 and chunk 4096 give a bit-identical reading series (criterion 4, 0 stops differing
  out of 4 286 at chunk 7 and 235 at chunk 128).
- **Not a latency question.** The class reads samples and never delays one; it reports nothing to the
  latency model and adds nothing to R or W.

**Revisit trigger.** Item 04's target decision. If it lands on per-grain energy, the windowed pair is the
measurement surface and the envelope is unused but harmless; if it lands on a smoothed envelope, ditto
the other way — that is the point of shipping both. The divisor 100 is the number to revisit if a
consumer needs `envelope()` to resolve content whose period approaches `innerTimeConstantMs()`; its cost
is the attack lag above, so it trades against criterion 3, and the 23.24 % figure above is what happens
when it is set to infinity.

**Evidence.** 2026-10-30, in-container `Release`. `src/Dsp/tests/LevelDetectorTests.cpp`: 10 cases,
264 026 assertions (`DspTests` as a whole 24 cases, 491 953). Criterion 1: windowed RMS worst 0.076 %
and envelope worst 0.043 % off A/√2 — against mean-rectifying's 9.97 %, which is the distinction the
tolerance exists to make. Criterion 2: peak deficit 5.96e-06 %. Criterion 3: attack 483 samples against
480, release 4 810 against 4 800, settled reading exactly 0.5 for a steady 0.5, monotone both ways, each
constant still nominal with the other pinned to instantaneous. Criterion 4: 0 differing stops across 7,
128, 4096 and a seeded random sequence. Criterion 5: attack 25.147/25.146/25.156/25.156 ms and release
75.510/75.5/75.5/75.5 ms at 44.1/48/96/192 kHz against 25 and 75 ms. Criterion 6: exact `0.0f` at the
window count and at the published bound, monotone throughout, no subnormal or non-finite reading
anywhere on the decay. Criterion 7: square wave, DC, full-scale noise and a 1e30 impulse train all
finite and inside the input's own peak, no growth between quarters. Criteria 8–10 in
`plan/M3-impl-plan/03-level-detector.md` §10, which also carries the seven §5 discriminators as run:
mean-rectifying reddens 1 (and 2, 10); swapped constants redden 3 in both directions (and 5, 7);
sample-count constants redden 5 alone; once-per-call filtering reddens 4 (and 6); removing the square
root reddens 1 and 2 (and 7, 10); a running sum with no correction term reddens 10 (reported 12.2474
against the 0.353868 recomputed from the samples); one channel's state reused for the next reddens 8
alone.

---

## D-015 — Normalisation target: geometric-mean RMS of A and B, smoothed envelope, UI floor with unity below it

**Status first.** The decision was taken by the project owner on 2026-10-30 and **implemented and
measured on 2026-10-31 as D-016**, which carries the figures it owed. Everything below was arithmetic
when it was written, and the arithmetic held: the geometric-mean target reproduces to 2.4e-07 dB on the
held level, the half-slope is 6 dB for a 12 dB change on one bus, and the arithmetic mean sits 1.9382 dB
off where the geometric one puts the held level. The two places measurement disagreed with expectation
are recorded in D-016, not here, because both are about wiring and program level rather than about this
target.

**Context.** Three questions gate M3 §4.2, and through it §4.5's normalisation-amount dial and §7's
±2 dB flatness gate: what level the gain drives the convolved path toward, which reading measures it,
and what happens when there is no level to measure. `INDEX.md` §9 held them as one block; M3 §3
recommends a geometric mean and a smoothed envelope without committing to either. Item 03 shipped
`dsp::LevelDetector` precisely so these could be answered from readings instead of by whoever typed the
normalizer first (D-014).

**The decision.**

* **Target** `target = √(envA · envB)` — the geometric mean of the two RMS readings. The milestone's
  recommended option, taken. Written as a word rather than a symbol because `t` is already the
  milestone's mix-law shorthand for `|p|`, and this file has no notation table of its own to overrule it.
* **Driving reading** — the smoothed envelope, `LevelDetector::envelope()`, on A, on B and on the
  convolved path. Not the windowed per-grain pair. This fires the measurement-scheme half of D-014's
  revisit trigger; the windowed pair stays exposed and nothing in this decision removes it.
* **Silence** — the floor is a **UI parameter**, not a constant, and below it the applied gain is
  **unity**: the stage stops correcting, rather than holding its last value, muting, or running to its
  range limit.

**Why the three hold each other up (arithmetic).**

1. *Zero propagates through a geometric mean.* If either bus reads zero, the target is zero, so a silent
   A or B would otherwise drive the correction as far as its range limit allows. That is the exact
   failure M3 §3's silence bullet exists to prevent, and it is worse for this target than for an
   arithmetic mean, which would sit at half. Unity-below-the-floor closes it: the gated case is the
   identity, so a silent bus leaves the wet path at its own level and §7's "no noise blow-up" holds by
   construction rather than by tuning. The two halves of the decision are interlocked, not independent.
2. *In the log domain the target is well-behaved everywhere except at exact silence.* The target is the
   arithmetic mean of the two readings' dB values, so its sensitivity to either bus is exactly ½ at
   every level — a +6 dB change on A alone moves the target +3 dB. Seen from the linear side the same
   fact is the unbounded derivative `∂target/∂envB = √envA / (2√envB)` as `envB → 0`. Implementation
   consequence: floor **each reading** before the mean, in the domain where the mean is additive;
   clamping the resulting product leaves a discontinuity at the clamp that the per-reading floor does not.
3. *The half-slope is a property of the target, not an error to correct.* Because the wet path is
   *defined* to track `√(envA · envB)`, a change that hits one bus alone must move the output by half
   that change. §7's ±2 dB criterion therefore reads as **normalization error against the target**, not
   as "output level constant while one bus is swept" — only the first is satisfiable. What survives
   along the dial travel is the mix law's own behaviour on partially-correlated content, which 01
   measured and reported (D-010) and the normalizer does not touch: at `p = ±1` the wet path carries a
   zero weight, so the endpoints stay bit-exact provided the applied gain is finite at every sample —
   which unity-below-the-floor also delivers, since nothing divides by a reading the floor has caught.

**The consequence that needs the most care: where conv is measured.** §4.2 names three detector inputs —
A, B *and conv* — and the gain needs conv's level to form `target / envConv`. Measured **upstream of the
applied gain**, on the engine's own output, that is a feed-forward computation and M3 §8's "strictly
feed-forward" holds. Measured **downstream** of it, the identical formula is an AGC loop wrapped around a
follower: stable only inside some region of (τ_a, τ_r, slew), winding up on a burst, with a pump period
set by time constants nobody chose for that purpose. This entry records it because the difference between
the two is one line of wiring order, and the wrong one is silent in every test that does not listen.

**What this decision does not decide** — none of it blocking. `plan/M3-impl-plan/04-level-normalizer.md`
§0 took all six as its decisions A–G on 2026-10-30; where its answer is more specific than the sketch
below, that file is the current word and this list stands as the question set as it was left. Note one
resolution worth keeping: gating on *any* of the three readings removes the need to clamp anything, so the
"floor on the reading or on the mean" question dissolves rather than being answered.

* **The gate condition.** Floor on *either* reading or on the mean, and whether the boundary wants
  hysteresis. A loud A over a silent B gives `target = √(envA · F)` once each reading is floored at `F`,
  so the clamp alone does not produce the unity behaviour: the gate is a separate mechanism from the
  floor and has to name its own condition.
* **The floor dial's range, quantum and default.** F-010 binds here as it did on the other two float
  dials — choose a dyadic step in dB so the default and both endpoints are exactly expressible
  (`1/128` and `0.125` dB are the precedent), and the value must stay above `LevelDetector::envelopeFloor`
  = 1e-12, which any sane UI floor does by many orders of magnitude.
* **Gain range and slew limits.** §4.2 requires both. With unity below the floor the upper bound is the
  live number, and because the envelope already delays its target by roughly its own time constant, a
  slew limit is a second independent bound on top of a lag rather than a substitute for one.
* **The three detector instances' attack/release defaults.** Choosing the envelope makes these the
  normalizer's response time — user-facing behaviour, not detector tuning. M3 §6's "tune attack/release
  against grain length" is the research question that sets them.
* **Channel law.** Per-channel `target = √(envA[c] · envB[c])` or a linked pair target. M4 §4 item 4 asks the
  same question and its §6 asks which is preferable for stereo material; the detector is per-channel
  either way, so nothing in M3 constrains the answer.
* **The amount dial's parameterisation.** `gain = (target / envConv)^amount` with amount ∈ [0, 1] is the
  natural monotone form: amount 0 is the identity, which is the same gain the gate produces, so the dial
  and the gate meet at unity instead of fighting over it. Suggested, not decided.

**Revisit trigger.** Listening. The geometric mean is the target that lets a quiet partner drag the whole
result toward the floor, and the half-slope means the wet level follows each bus at 6 dB per 12 dB of
that bus. If that reads as the effect being shoved around by the sidechain rather than sitting under it,
the alternatives M3 §3 listed — one input, or a fixed reference — are a two-line change in the target
computation, and nothing else here moves: the floor, the unity behaviour, the reading and the measurement
point are all independent of which mean is taken. Second trigger: if pumping appears at the edges of
silence, the gate condition and its hysteresis are the thing to change, not the target.

**Provenance.** Decided in session on 2026-10-30 in answer to the three questions `INDEX.md` §9 recorded.
The brief those questions were originally routed through — `01-agent-brief.md` §8 Q5 — does not exist in
the tree or in history, so the option list they were answered against is `INDEX.md` §9's paraphrase plus
M3 §3's own bullets, not a quotation (`INDEX.md` §8.11).

---

## D-016 — Level normalizer: where the stage sits, what gates, the derived values, the non-finite rule

**Status first.** Implemented and measured 2026-10-31: `dsp::LevelNormalizer` (`src/Dsp`), fed and
applied by `engine::MixEngine` between the DC blocker and the law, installed by
`entangle::makeChain` in `ChainVariant::Shipped`, driven by the two host controls
`normalisationAmount` and `silenceFloor`. D-015 owns the target and the driving reading; this entry owns
the wiring, the gate, the numbers D-015 left open, and what a bad reading does. Figures are from
`DspTests`, `MixEngineTests` and `tools/OfflineRender.cpp` on the tree as landed, in-container
aarch64 `Release` with LTO on the JUCE-linked targets.

**Context.** Six things had to be pinned before the target could run: which library owns the arithmetic,
where in the chain the stage sits, where `envConv` is measured, what the floor actually gates, the gain
range and slew, and what happens when a reading is not a number. `plan/M3-impl-plan/04-level-normalizer.md`
§0 took them as decisions A–G; the three that are architecture rather than tuning are recorded here
because they are the ones a later reader cannot recover from a diff.

**The decision.**

* **One primitive, three detectors, installed by a setter.** `dsp::LevelNormalizer` owns three
  `LevelDetector`s — A, B, conv — each with a one-hop window, and does the read/combine/gate/limit/ramp
  arithmetic. `MixEngine` still decides what runs and in what order (D-012's rule): it feeds the stage,
  scales `scratch`, and multiplies. The stage is installed by `installLevelNormalizer` rather than a
  fourth constructor argument because it runs *before* the last of the two stages the constructor already
  orders, and a positional `(blocker, clipper, normalizer)` would say the opposite of what the chain
  does. It asserts a not-yet-prepared engine: the hop it ramps over is sized in `prepare`.
* **Insertion point: after the blocker, before the law.** The order is grain engine → block conv →
  normalise conv → law → gain → clip. Both halves are load-bearing. Reading DC-carrying conv inflates
  `envConv` and therefore the correction, which is the same reason D-013 put the blocker first; and
  scaling before the law is what leaves the `p = ±1` endpoints bit-exact, because `coefWet` is exactly
  zero there and `0 × finite = 0`. §5.5's criterion-3 leg and §5.10's gain ladder both re-verify that
  identity with the stage installed (at amount 0, which is bit-exact unity) and both come back
  bit-identical.
* **Feed-forward, measured on the sample the stage has not yet scaled.** `processChannel` reads the hop
  before it scales it in place, so `envConv` is the engine's own output level and the loop is open. The
  alternative — the identical formula fed from the post-gain stream — is an AGC loop wrapped around a
  follower, and it is one line of ordering away. A5 asserts the stage's conv reading bit-equals a bare
  `LevelDetector` driven with the same stream while the correction runs, with a negative control: the
  same detector fed the scaled stream reads differently.
* **The gate runs before the arithmetic, on any of the three readings.** `gain = 1.0f` exactly when
  `envA`, `envB` or `envConv` is at or below the floor. Because the gate runs first, everything downstream
  works on readings strictly above it, so the geometric mean needs no per-reading clamp and the ratio no
  divide guard — D-015's open "floor on the reading or on the mean" question dissolves. The consequence
  to keep in view: the gate is a separate mechanism from the floor control, so a loud A over a floored B
  yields unity, not the `√(envA · F)` boost a clamp-only design would apply.
* **`amount` is an exponent, and the applied gain is a line across the hop.** `gain = (target/envConv)^amount`
  so amount 0 is bit-exact unity at every sample and the dial meets the gate at the same value instead of
  fighting it. The hop is then scaled along a straight line from the previous hop's gain with the **last
  sample assigned**, not evaluated at progress 1.0 — the same float fact D-011 recorded, and the reason
  an endpoint criterion can be met at all.
* **A non-finite reading degrades to unity and is counted, not sanitised.** `LevelDetector` has no
  non-finite rule and none was added: a NaN poisons its state, which is what its contract implies. The
  stage checks the *computed* gain once per hop per channel; not finite ⇒ that hop's gain is `1.0f` and
  `nonFiniteHops()` increments, mirroring `SoftClipper::nonFiniteSamples()` as a diagnostic nothing
  synchronises on. The degradation is permanent for that stream and that is deliberate: unity and
  uncorrected beats silence, NaN, or a per-sample `isfinite` on three streams. Nothing surfaces the
  counter in the UI.

**The values D-015 left open, and what each was derived against.**

| Control | Value | Basis |
|---|---|---|
| Gain range | `[−36, +36]` dB | D-008's measured wet response peaks span 37.45 dB across the shipped L range, so ±36 dB covers it to within 1.45 dB and still leaves the ±2 dB gate reachable |
| Slew limit | Stated as time: the full 72 dB range in **200 ms** (360 dB/s), realised as `360·H/sampleRate` dB per hop | A per-hop cap is INDEX §8.5's unit error in different clothing — it would be 8× slower in time at H = 512 than at H = 64, and the shipped hop set spans exactly that |
| Detector τ | attack 20 ms, release 100 ms, all three buses | 20 ms matches the blend and gain ramps D-011 shipped; 100 ms is M3 §6's anti-pumping lean |
| Amount dial | `[0, 1]` at `1/128`, **default 1.0** | The blend dial's dyadic quantum for the blend dial's reason (F-010). Default fully normalising, because the correction is the feature |
| Floor dial | `[−90, −20]` dBFS at `0.125` dB, default `−60` | Dyadic step with both endpoints exactly expressible; 180 dB above `LevelDetector::envelopeFloor` = 1e-12, so the two silence rules cannot interact. Measured: the legacy preset leg loads it at exactly `-60` |

**Measured.**

* Target: held level equals the analytic `√(envA · envB)` to −2.4e-07 dB from a wet level 14 dB below it;
  half-slope 0 / +6 / +6 / +12 dB for (baseline, A alone +12, B alone +12, both +12); amount 0.5 measured
  6.9897 dB where the power form says 6.9897 and the linear form would say 9.54242.
* Limits: cap 0.48 / 0.96 / 1.92 / 3.84 dB per hop at H = 64/128/256/512, with 72 dB crossed in
  200 / 200 / 202.7 / 202.7 ms; unity again 434.7 ms after the input stops, against the detector's
  published 2766 ms bound; NaN on any of the three streams counted every hop at unity while the clean
  channel next door keeps correcting.
* Chain: `latencySamples()` still one hop across the 28 (H, L) pairs; the delta anchor's arrival index
  unchanged with the stage installed; the dial travel's click metric never exceeds the parked render's
  (ratio 0.728512 … 1.0); automating either control every callback comes in at 0.575 and 0.906 of the
  static render's click; zero allocations across the hop path with both controls moving.
* Flatness: worst corrected deviation **0.376 dB** against the ±2 dB gate. The no-stage baseline spans
  **9.5997 dB** across both tables; on the `L` axis at H = 512 the bare levels run −24.7915 to
  −26.1815 dBFS, i.e. 1.39 dB, and the hop axis carries the work (9.16847 dB off at H = 64, L = 8192,
  corrected to 0.376085).

**Two findings that are documentation, not defects.**

1. *The follower's crest bias cancels in the applied ratio, and only when the buses share ripple
   content.* D-014's asymmetric follower reads a 1 kHz tone 7.8 % (+0.65 dB) high. All three buses are
   read by the same class, so the bias cancels between `target` and `envConv` and the held level is the
   analytic geometric mean — a rule column built from `envelope()` instead of from the amplitudes puts
   that 0.65 dB on every rung and reads as a gain error that is not there. It does **not** cancel when
   the three buses have different ripple content, which is what happens when one is much quieter than
   the others; there it shows up as a level offset.
2. *The L-spread on program material is not D-008's figure.* The 37.45 dB is a delta **response-peak**
   span. The settled RMS across the same L range at fixed H spans **1.39 dB** (bare −24.7915 to
   −26.1815 dBFS), and it is the hop half of `C = L/(2H)` that moves program level (9.17 dB at H = 64,
   L = 8192). So §7's flatness criterion is
   a much smaller ask than the response-peak number suggests, and the correction's visible work on
   stationary material is correspondingly modest — while the peak figures D-009 and D-012 quote do move:
   §5.7's shipped peaks are the with-the-stage numbers now, e.g. 2.53687 → 1.90372 for a correlated pair
   at 0.4 amplitude.

**Consequences worth naming.** Adding a third stage re-pointed two existing harness legs that modelled
the shipped chain by hand (§5.5, §5.10) — F-011's other half, and the reason both now park the amount
dial at 0 for the comparison rather than the harness growing a copy of the correction. Default amount 1
changes what a fresh instance does, so every pre-item peak figure in this file is a without-the-stage
number unless the leg says otherwise. And the floor gates on the *wet* reading too: at H = 64, L = 8192
with −40 dBFS in, amount 0 and amount 1 render identically at the shipped −60 dBFS floor, and the stage
asks for 26 dB once the floor moves to −90 — the dial decides where "nothing is amplified" is, which is
exactly decision D's intent and the reason it is a control.

**What the discriminator run proved, and what it did not.** All ten of the plan's §5 mutations were
applied, built, run and reverted byte-verified (plan §14). Seven behaved as predicted. Three did not, and
each of those three is worth more than the seven:

* **The gate carries D-010's endpoint criterion, not just the noise one.** Deleting it did not merely
  risk amplifying a floor: with exact silence the readings are finite (so the finiteness rule passes),
  `log10(0)` reaches `−inf`, the difference of two infinities is NaN, and `0 × NaN` is NaN even where the
  law's wet coefficient is exactly zero — so the clipper's non-finite rule turned the `p = −1` and
  sidechain-disabled renders to **silence**. That is why the gate sits before the arithmetic rather than
  as a clamp after it, and why the finiteness check is on the three readings rather than on the computed
  gain: they are not the same test.
* **The slew cap and the ±36 dB clamp have no chain-level witness.** Both mutations leave
  `OfflineRender` bit-untouched and passing — the harness never drives a gesture fast enough for the cap
  to bind, nor a wet level quiet enough to ask for the clamp. Those two figures are proved by the
  `DspTests` cases that name them and by nothing else. Don't cite C5 or C8 for them.
* **The blocker-before-the-stage ordering is untested, by anything in this repo.** Swapping the two
  stages leaves every leg green: the blocker still removes DC before the output, so the DC leg cannot see
  it, and the only consequence — a reading inflated by DC, hence a slightly wrong gain — sits far inside
  the flatness gate. Naming this is the point: the invariant is real (it is why the stage sits where it
  does) and it is held by one comment in `MixEngine::processHop` plus the builder's call order. Proving it
  needs either a chain variant that swaps the two stages or a read-out of the applied gain through the
  shell, and neither is worth building while the shell is being rewritten (AGENTS §13). The *other*
  ordering claim is well covered: moving the stage past the law reddens seven harness legs, including the
  bit-exact endpoints.
  * **Closed 2026-11-02 by M4 item 07** (`../plan/M4-impl-plan/07-ordering-invariant-leg.md`), and neither
    of the two escapes named above had to be built. The witness is a wet stub that puts a DC offset *under*
    a 1 kHz tone: with the blocker in front, the stage's own conv reading sees the tone alone and the applied
    gain is the tone ratio (**+7.95675 dB** measured against **7.9588 dB** analytic); with the loops swapped
    the DC is inside the reading and the gain inverts to **−7.12531 dB**, a **15.08 dB** separation. One
    `TEST_CASE`, "level stage runs after the DC blocker", reddens 662 assertions on the swap while
    `OfflineRender` stays green — the blind spot this bullet describes. It also compares the stage's reading
    hop-for-hop against a bare `LevelDetector` on the stage-free chain's output, which is what makes it an
    ordering witness rather than a presence one: taking the filter out on both sides leaves that comparison
    green and reddens the gain instead.

**Revisit trigger.** Listening, as D-015 says: the geometric mean lets a quiet partner drag a channel
toward the floor and out of correction, and the half-slope means the wet level follows each bus at 6 dB
per 12 dB of it. Second: if pumping appears at the edges of silence, change the gate and add hysteresis,
not the target. Third: if long-grain settings pump, scale `releaseMs` with L rather than touching the
target — at the default L = 8192 the wet response itself spans 171 ms against a 100 ms release. A
per-channel detector reset (the alternative to permanent-for-that-stream degradation) would change
D-014's contract, so it is not assumed here.

**Provenance.** Implemented 2026-10-31 from `plan/M3-impl-plan/04-level-normalizer.md`, whose §10–§12
carry the per-criterion evidence trail including the three premise corrections the harness forced. The
plan's §5 numbering and its §3 leg numbering conflicted; the assignment actually used is recorded in the
plan's §10. Plan §14 carries the ten-mutation scorecard with its three negative results, and the close-out
verification: `DspTests` 34 cases / 494 256 assertions (the normalizer's 10 cases, 2 303 of them),
`MixEngineTests` 16 / 126 428, `ctest` **140/140** with `OfflineRender` green, `vstplug_VST3` linking at
`-j4` in the same in-container `Release` tree.

---

## D-017 — The M4 channel rule: broadcast from one channel only, asymmetric rejected, ceiling 8

**Status first.** Implemented and measured 2026-11-01 as `engine::ChannelMap` (`src/Engine/ChannelMap.hpp`,
header-only) with `ChannelMapTests` — 12 cases, 1494 assertions, `ctest` **152/152** in the in-container
aarch64 `Release` tree with `vstplug_VST3` linking at `-j4`. Nothing consumes it yet: item 03 will gate
`isBusesLayoutSupported` on it and item 02 will turn its indices into pointers. Taken with the user in the
`plan/M4-impl-plan/INDEX.md` §6 pass, which **replaced two of the plan's provisional answers** — §0 H's
ceiling of 16 became 8, and §0 F's "output count must equal `max(chA, chB)`" became "at most".

**One row of this entry is superseded.** The mono-main family is D-018's now, implemented by
`plan/M4-impl-plan/09-map-consolidation.md` on 2026-11-02; that item re-ran the 729-triple sweep against the
amended oracle and every other family resolved exactly as measured here (17 cases / 1 470 assertions,
`ctest` 158/158). The 12 cases / 1 494 assertions below stay as the record of what D-017 shipped, with the
broadcast row among them.

**Context.** M4 §1 wants channels as independent convolutions with a mono side broadcast. Three of §3's
four questions are answerable in code — the channel rule, the ceiling, and what an output count smaller
than the source count means — and the fourth (the layout constraint, which hosts allow what) is item 08's.
M4 §3 leaves "define what happens when `chA` and `chB` are both > 1 but different" open with a
recommendation to restrict, and M4 §7's commutativity criterion reads as though the restriction follows
from it. It does not (F-012), so the restriction is justified here on the ground that survives measurement.

**The decision.**

* **`N = max(chA, chB)`, and a side broadcasts its channel 0 exactly when it has one channel.** A side with
  `ch > 1` maps output `c` to source `c`. No modulo, no wrap, no upmix.
* **`chA > 1`, `chB > 1` and `chA != chB` is rejected** — `ChannelMapStatus::asymmetric`, which becomes a
  `false` from `isBusesLayoutSupported` in item 03. The reason is **injectivity, not commutativity**: under
  a modulo rule a 2-channel A against a 6-channel sidechain would feed A's channel 1 into outputs 1, 3 and
  5, so M4 §7's independence criterion degenerates into "the same convolution, three times" and a source
  channel is silently reused rather than mapped. Commutativity does not discriminate between the two rules
  — measured, not reasoned: a modulo mapping swaps cleanly (`F-012`).
* **`channelsOut` may be at most `max(chA, chB)`.** Fewer outputs select the leading channels — index `c`
  of each side, or that side's channel 0 where it is mono — and drop the rest; more outputs is
  `countMismatch`. Selection is not the downmix M4 §2 excludes: no output ever mixes two source channels,
  each is still exactly one convolution of one pair. The accepted case is the milestone's own §6 row 4
  (a mono main out with a stereo sidechain), which now resolves rather than being refused.
  **Amended by D-018 for the mono-main family:** there the ceiling is 1, not `max(chA, chB)`, and what the
  one output reads is not a selected channel but the fold of the whole sidechain. Every other family keeps
  this rule exactly as written and item 09's sweep is measured against it.
* **A missing sidechain is silence, and silence is not channel 0.** `chB == 0` puts `kSilentSource`
  (`0xFFFF`) in every B slot. D-002's "no sidechain bus" and "mono sidechain" are different states and the
  map carries both; the sentinel sits outside every real index range so an unhandled one fails as an
  out-of-range index instead of aliasing the dry bus into the wet path. `chA == 0` has no such rescue: it
  is `empty`, because A is the mandatory side.
* **`kMaxChannels = 8`,** chosen over 16 in the §6 pass. Per-channel state below the seam is linear in `N`
  — `MixEngine`'s scratch is `hop × N` floats (`MixEngine.cpp:51`) and the level stage owns three
  window-length `LevelDetector`s per channel — so 16 doubles the footprint of *every* configuration,
  including the ones no host asks for. 8 covers 7.1 first-order ambisonics and every layout the shell's
  own buses can express. The map itself is 36 bytes at this ceiling and is asserted against the bound
  written beside the constant. `resolve`'s `capacity` argument lets a caller ask for less and a test drive
  the boundary without recompiling; a `capacity` above `kMaxChannels` is clamped to it rather than trusted.
* **Rejection is a typed status, never an exception, in a fixed precedence:** empty → asymmetric →
  exceedsCapacity → countMismatch. A layout gate needs the reason, not a bare `false`, and the precedence
  means a triple that breaks several rules reports the most specific one and never fills what it is about
  to reject. On every rejection the escaping map is value-initialised.
* **One function owns all of it.** `isBusesLayoutSupported` and the pointer resolution will call
  `ChannelMap::resolve`; neither may re-derive the rule (INDEX §3 rule 2). It lives in `Engine` because
  `Engine` already carries `EngineConfig::maxChannels` and the scheduler whose silence-filling this
  formalises, and because AGENTS §10 needs the exhaustive test to run as a console program — the shell is
  JUCE and could not host it.

**Finding that corrected the milestone's rationale (F-012).** M4 §3's implied argument — that a modulo rule
would make swapping A and B produce different mappings — is false, and so is the item plan's prediction
that a modulo implementation turns the commutativity leg red. Measured against the accepted set: with
asymmetric pairs rejected and `channelsOut ≤ max(chA, chB)`, every filled slot satisfies `c < ch` for a
multi-channel side, so `c % ch == c` and the modulo rule is *observationally identical* to the documented
one — that mutation leaves all 12 cases green. What actually turns the mapping legs red is accepting the
asymmetric case (criteria 1 and 2 and 4) and dropping the output-count check (1, 6 and 11). F-012 carries
the three predictions that did not survive the run.

**Revisit trigger.** A host that must be supported with an asymmetric layout (item 08's findings) does not
buy an `#ifdef` — it buys a second status and a documented rule, decided in its own entry. If item 05's
shared-spectrum path needs the broadcast signalled through the map, that is §6 row 5's entry, not an edit
to this one. And if a mono main out over a stereo sidechain ever has to *fold* B's channels rather than
drop B's channel 1, that is the downmix M4 §2 excludes and needs a decision of its own. **That trigger fired
2026-11-02: D-018 took the fold.** The correction rather than the exception — M4 §2 now names that fold as
its single exception, confined to one family, stated by a sentinel and computed above the seam, while the
matrix- and spatial-shaped downmix this entry was ruling out stays out.

**Evidence.** `plan/M4-impl-plan/01-channel-map-helper.md` §10 (per criterion, with command and date) and
§5's mutation table with its measured column — six planned mutations plus two added to isolate the ones the
predictions could not reach, each reverted byte-verified. The exhaustive leg sweeps all 729 triples of
`{0,1,2,3,4,6,8,16,17}³` against a table restated from the header's own row list, and the shipped mappings
(1,0), (2,0), (1,1), (2,1), (2,2), (1,2), (8,0), (8,8) and the past-the-ceiling (16,16) are pinned by
`static_assert`, which is why four of the eight mutations are caught by the compiler rather than by a test
run.

---

## D-018 — The mono-main case consolidates the sidechain to a mono mean; every other layout needs main ≥ sidechain

**Status first.** Decided with the user 2026-11-02, closing `plan/M4-impl-plan/INDEX.md` §6 row 2, and
**implemented the same day** by `plan/M4-impl-plan/09-map-consolidation.md`. `ChannelMap::resolve` folds the
`chA == 1 && chB > 1` family now: one processed channel, and `kConsolidatedSource` (`0xFFFE`) in its B slot.
The two things that pinned the retired rule pin this one — the `(1, 2)` `static_assert` in
`src/Engine/tests/ChannelMapTests.cpp` reads `channels == 1` with the marker, and the exhaustive leg's
oracle restates the fold. **The other half landed 2026-11-04**, when `plan/M4-impl-plan/02-broadcast-resolution.md`
put the resolver in `HopScheduler`: the mean is computed there, above the `processHop` seam, and the level
consequence recorded below is **measured** — and is not the figure this entry first asserted. Read the
amendment below the bullet list before quoting either number.

**Context.** The §6 question was which asymmetric bus shapes the layout gate allows, and M4 §3's reason for
asking is that a plugin on a mono track cannot emit stereo. The answer first taken was "allow a mono main
with a stereo sidechain and make the output stereo", which requires `mainOut != mainIn` — a shape the shell
rejects outright at `src/Entangle/EntangleProcessor.cpp:137` and one M4 §3 says most hosts will not route.
Shown that consequence, the user replaced the answer rather than defending it: the output stays mono and the
sidechain is folded to the mean of its channels. The rule is therefore keyed to the **main bus shape**, not
to symmetry between the two sides, and no exotic bus shape is ever requested — which is what makes the rule
implementable without item 08's host research being a precondition.

**The decision.**

* **`chA == 1 && chB > 1` → one processed channel, mono output, and the sidechain source is the arithmetic
  mean `(1 / chB) · Σ_c B[c]`.** Not channel 0 (that throws away `chB − 1` channels of the sidechain's
  content), and not D-017's broadcast to `chB` outputs (that asks the track for a wider output bus than it
  has). Cheaper than the rule it replaces: one convolution per hop instead of `chB`.
* **Every other accepted layout keeps `chB <= chA`, and the shell keeps `mainOut == mainIn`.** So the
  existing main-bus equality guard stays, `isBusesLayoutSupported` never has to admit unequal main buses,
  and item 08 shrinks from "can we emit stereo from a mono track" to "which hosts offer a sidechain bus
  wider than mono at all, and can it be enabled independently".
* **Arithmetic mean, not a power mean and not a −3 dB-compensated sum.** Figures for the record, from the
  definition: two equal-level uncorrelated channels fold to −3.01 dB, a coherent identical pair keeps its
  level exactly, and an antiphase pair folds to silence — which reaches `LevelNormalizer`'s floor gate and
  leaves the wet path at unity rather than boosting a folded-to-nothing sidechain.
* **The fold is the one documented exception to "channels never read each other's samples" (M4 §5), and it
  lives above the seam.** The resolver computes the mean into a mono hop and hands the engine one B pointer,
  so nothing in `Engine`, `Dsp`, `Fft` or `Grain` gains a cross-channel read and INDEX §3 rule 3 is intact.
  M4 §7's independence criterion is stated over mapped sources with this fold named as the exception.
* **What survives of D-017 unchanged:** the ceiling of 8, the asymmetric rejection, `kSilentSource` and
  D-002's "no sidechain is not mono sidechain", the typed status precedence, and the one-function ownership.
  Only the mono-main row changes shape: that family's allowed output count becomes exactly 1, so
  `channelsOut > 1` there must be a rejection rather than a map.
* **The amended contract cannot ride on the index arrays alone.** `resolve(1, 2, 1)` is byte-identical under
  both rules — broadcast-channel-0 and consolidate-to-mean both leave `a[0] = 0`, `b[0] = 0` — so a test
  aimed at the old shape would not notice the change. Item 09 carries the fold as something the map states:
  `kConsolidatedSource` in the affected B slot, a sentinel rather than a flag beside the arrays — a flag
  gives a caller two sources of truth to read in either order, and a `bool` member takes `ChannelMap` past
  the 36-byte bound its own `static_assert` pins (measured 2026-11-02: the field makes it 40 bytes and the
  build fails). The leg that owns the marker compares it against `0` *and* against `kSilentSource`, so it
  cannot co-mutate with the constant the way a leg that only compares against it does (F-012's failure,
  reproduced and this time caught: setting `kConsolidatedSource = kSilentSource` leaves the swept table
  green and reddens three legs that compare two resolves).
* **Level consequence, arithmetic from D-016's formula and not measured:** a −3 dB fold drops `envB`, and
  `wantedDb` weights `10 · log10(envB)` once, so the level stage's target moves −3 dB at `amount` = 1 for
  uncorrelated sidechain material. Written down so a later listening note does not attribute it to the
  follower's time constants. **This sentence is wrong in both its number and its sign — see the amendment
  below.**

**Amended 2026-11-04 by item 02, which measured the half this entry could only assert.** `HopScheduler` now
resolves the map into pointers and computes the fold: `HopScheduler::foldSidechain` sums the `chB` sidechain
sources once per hop into one pre-allocated mono buffer, the reciprocal applied once after the sum, every
consolidated slot pointing at that buffer. Value legs are bit-exact at `chB` ∈ {2, 3, 4, 8} over four input
patterns, an even-width antiphase pair folding to exact zeros, and the hop path does not allocate.

The measured level consequence is **not** the paragraph above. Through the shipped builder, mono main,
decorrelated stereo sidechain folded against channel 0 of the same pair, dial centred, `amount` = 1
(`OfflineRender` §5.17, Release, Linux aarch64, RMS over the last second of 3 s):

| Reading | Folded vs a mono sidechain |
|---|---|
| wet path with the level stage parked at `amount` = 0 | **−2.985 dB** — the −3.01 dB arithmetic, confirmed |
| the correction the stage applies | −0.117 dB on a mono side, **+1.441 dB** folded → **+1.558 dB** |
| corrected output | **−1.427 dB** |

So the fold does drop the side 3 dB, and the *target* moves **+1.56 dB in the opposite direction**, not
−3 dB. Two reasons, both structural: `wantedDb` consumes `√(envA · envB)`, so halving `envB`'s power is
−1.5 dB rather than −3; and `envConv` — the engine's own convolved output level, which the stage divides
against — falls with the fold, so part of what the side loses the output has already given back. The
sentence stands above unedited because this log records what was believed when the decision was taken; a
later reader should take the figure from this paragraph, and M9 row 2 should judge *this* delta's audibility
rather than the one that was predicted.

**Revisit trigger.** An item 08 finding that a host will not enable a wider-than-mono sidechain bus
independently of the main bus; the M9 listening pass deciding the fold is audible (antiphase cancellation,
the 3 dB drop, or a preference for a channel-0 or power-mean fold — all three are M9 rows, not edits here);
or a surround main bus, where per-channel broadcast and consolidation would compete for the same family.

**Evidence.** The map half is measured 2026-11-02 in `plan/M4-impl-plan/09-map-consolidation.md` §10:
`ChannelMapTests` 17 cases / 1 470 assertions, `ctest` **158/158**, the plugin relinked at `-j4`, and nine
mutation arms with their measured column in that file's §5. The mean half is measured 2026-11-04 in
`plan/M4-impl-plan/02-broadcast-resolution.md` §10 — bit-exact fold legs at four side widths, the pointer
resolution, its no-allocation leg, the ASan/UBSan sweep that **F-014** required, and the §5.17 figures in the
amendment above; `ctest` **166/166** from that change. What stays unmeasured is the half a console cannot
reach: whether any of it is audible (M9 row 2).
`plan/M4-impl-plan/01-channel-map-helper.md` §10 (729 triples, 1 494 assertions, `ctest` 152/152, 2026-11-01)
stands as the record of the rule that shipped before this amendment, not as evidence for this one.

---

## D-019 — Shared-spectrum optimisation: not in the engine API now, because the seam already fits M7

**Status first.** Decided with the user 2026-11-02, closing §6 row 5. No code change. M4 §4 item 6 closes as
"not now" and `plan/M4-impl-plan/INDEX.md`'s item 05 loses its reason to exist in this milestone; the row
moves to M7, which is where a cost figure for it would come from.

**Context.** M4 §3 asks this early so M7 does not have to break the seam to get the optimisation in. The two
signalling routes M4 §5 names are pointer equality and an explicit flag, and the §6 pass put a third shape on
the table — a prepare-time flag, which leaves `processHop` alone but puts layout-derived state below the
seam.

**The decision.**

* **Nothing is added to `EngineConfig` or `processHop`.** Both eventual routes are additive, which is the
  whole answer to "so M7 doesn't have to break it": pointer equality needs no signature at all, and
  `EngineConfig` is a plain aggregate taken by const reference with every engine built from source, so a new
  field is not a break at any call site. Neither route touches `processHop`'s four arguments.
* **The flag now was the option not taken.** It would put layout-derived state below the seam, against INDEX
  §3 rule 3, and every engine would carry a flag it does not use in exchange for an optimisation nobody has
  measured a need for. F-007's cost data is per-callback, not per-channel, so this repo has no figure for
  what an N× broadcast transform actually costs — M7 produces that curve and re-opens this with a number
  rather than a hunch.
* **Pointer equality is the route M7 should expect to take**, and its failure mode is the reason it is safe
  to leave unimplemented: a shared spectrum must be bit-identical to computing the same hop twice, so a
  caller whose buffers stop aliasing loses the optimisation and nothing else. Wrong audio is not reachable
  through it. The cost it does carry is that performance becomes dependent on the caller's buffer policy, so
  whoever implements it has to assert the aliasing rather than assume it.
* **What is being paid for the deferral, stated plainly:** a broadcast configuration does `N` transforms per
  hop where a shared spectrum would do one, and a consolidated mono main pays one cross-channel fold per hop
  (D-018) above the seam. No harness here measures either figure.

**Revisit trigger.** M7's per-channel cost curve; or any implementation in which a shared spectrum is not
bit-identical to the recomputed one, which would make it a behaviour change needing its own entry.

**Evidence.** Decision only. The seam facts it rests on are in the tree: `Engine::processHop` takes
`const float* const*` for both sides (`src/Engine/Engine.hpp:49-50`), and `EngineConfig`
(`src/Engine/Engine.hpp:6-13`) is an aggregate with no private state.

---

## D-020 — Independent per-channel normalisation stays; the linking question moves to M9 behind a measurement

**Status first.** Decided with the user 2026-11-02, §6 row 6. No code change — M3's per-channel behaviour
ships. The row does **not** close: it is a listening call, and INDEX §3 rule 4 says a host-only criterion
never counts as measured. It moves to `plan/M9-preference-tweaks.md` with a measurement placed in front of it.

**Context.** `dsp::LevelNormalizer` decides per channel: `gain = (√(envA · envB) / envConv) ^ amount`, with
the floor gate evaluated per channel too. So a hard-panned or level-asymmetric stereo pair receives different
gain on each channel, and a channel that falls below the −60 dB floor sits at unity while its partner moves by
up to ±36 dB. That is image steering, and it is the whole reason M4 §6 asks for a linked option.

**The decision.** No linked mode, no new parameter, and no rule written from the mechanism alone. What ships
is what M3 measured. Before any future linking decision there is to be a figure: the inter-channel gain delta
the level stage produces on decorrelated stereo, per shipped hop, so the choice is made against a measurement
rather than against a plausible-sounding failure mode. A console harness can produce that delta — it cannot
decide whether it is audible, which is the part that stays with the user's host and a named DAW and version.

**Revisit trigger.** M9's measurement, or the user's listen. Either way the entry that changes this one has
to name the delta it was decided against.

**Evidence.** Decision only. The per-channel mechanism is in `src/Dsp/LevelNormalizer.hpp` (`processChannel`
takes a channel index and touches that channel's state alone) and its measured settling behaviour is D-016's.

---

## D-021 — The layout gate: two rules of its own above `resolve`, main bus mono/stereo, sidechain to 8

**Status first.** Decided with the user 2026-11-04, in three parts (A the widths, B the wide-main deferral,
C the reason carried by every refusal), and **implemented and measured the same day** by
`plan/M4-impl-plan/03-layout-negotiation.md` as `engine::gateLayout` (`src/Engine/LayoutGate.hpp`,
header-only, `constexpr`, JUCE-light) with `LayoutGateTests` — 9 cases / 9 125 assertions, its own binary for
the allocation leg. `ctest` **166 → 175**, green; `vstplug_VST3` relinked; `OfflineRender` §5.18 added (192
layouts probed through the public `checkBusesLayoutSupported`, 0 host/gate disagreements). This closes the gap
`AGENTS.md` §13 carried — "a layout the rule rejects is currently met with silence rather than a clean
refusal" — and makes the D-018 fold reachable from a host for the first time.

**Context.** Until now `isBusesLayoutSupported` was three hand-written guards — `mainIn == mainOut`, main
mono-or-stereo, sidechain mono-or-stereo — that never cross-checked the two buses and never called
`ChannelMap::resolve`. The rule therefore lived twice: once as prose-shaped code in the shell, once in the map.
Measured against the 12 shapes the old gate could see, its answers were already rule-correct, so this is a
derivation change plus a widening, not a bug fix. The widening is what makes it a decision: `resolve` has
folded a mono main over a wide sidechain since D-018 and `HopScheduler` has computed that mean since item 02,
but **the plugin itself refused a wider-than-stereo sidechain**, so the design's one cross-channel read was
unreachable from any host — not because a host declined to offer it, but because our own gate did.

**The decision.**

* **A — the widths (the user's call).** The main bus stays **mono or stereo**; the sidechain bus may be **up to
  `kMaxChannels` (8)**. A wide main is refused with a verdict of its own (`mainWidth`) so it is never confused
  with a channel-rule rejection. Measured consequence: the accepted set gains `(1, chB 2…8, 1)` — §5.18 finds
  8 newly reachable layout combinations among 192 probed — and the refusals get real cases: a stereo main with
  a 5.1 send is now refused `asymmetric`, a nameable refusal where the old code had a rule that did not know
  to look. Processed count stays ≤ 2, so nothing below the seam grows.
* **B — the wide main is deferred, not rejected on principle (the user's call).** 8-wide mains (quad, 5.1, 7.1
  — all of which `resolve` takes today) wait on three things this item does not own: item 04's prepare-time
  allocation figures, M7's per-channel cost curve (F-007 is per-callback, and D-019 deferred on exactly that
  absence), and M4 §6's open question about how JUCE reports surround and ambisonic buses. It is a named
  revisit trigger rather than an implicit line, and `kMaxMainBusChannels` is the single constant that changes
  when those figures exist — so a later item does not read the 2 as a rule of the design.
* **C — a refusal names its reason (the user's call).** `LayoutDecision` carries `verdict` (accepted /
  mainWidth / mainCountMismatch / channelRule), `rule` (the map's `ChannelMapStatus` **kept verbatim on every
  path**, including refusals the map did not cause) and `processed`. No fifth status was invented for the
  wide-main case — item 09 §0 E's reasoning extends: a status that fires in one family is where precedence
  bugs hide, and the wide-main refusal is not the map's to make.
* **One derivation, and `resolve` is consulted on every path** so a refusal always carries what the map
  thought, even when the verdict came from the width or the count: `(9, 9, 9)` is `mainWidth` whose `rule`
  still reads `exceedsCapacity`, and a leg asserts exactly that. The gate is compared against `resolve` and
  never against a sentence about it, because the sentence — `chB <= chA`, except the mono-main family — is a
  *necessary condition* that over-admits by **22 gate-visible triples**: 21 `asymmetric` pairs (`(3, 2, 3)`
  through `(8, 7, 8)`) plus `(1, 9, 1)` past the ceiling. D-018's own bullet says `chB <= chA` is what accepted
  layouts keep; it was never a decision procedure, and two index rows stated it as one.
* **Two callers of `resolve`, one decider.** `gateLayout` and `entangle::makeSchedulerConfig`
  (`src/Entangle/EngineChain.h`) both call the pure function, as does `tools/CpuBaseline.cpp`. Rule 2 forbids a
  second place that *decides*, not a second call; collapsing them into one call would thread the host-facing
  check into `prepareToPlay`, where the live buses are authoritative and can differ from the layout the host
  last asked about. §5.18 pins the two answers equal on the live triple (gate's `processed` 1, map's
  `channels` 1, one output channel offered).
* **`configureRings` takes `activeChannels` from the gate's `processed`**, not from a bare `resolve`. Its
  early return on `activeChannels <= 0` stays — `processBlock` must be total (§7) — but it is now unreachable
  from a layout a host can apply, which §5.18 measures by driving a callback whose buffers start as NaN: 0
  unwritten output samples on every accepted layout. That is the difference between "silence is the honest
  response for now" and "a refusal is what a host gets".

**Revisit trigger.** Wide mains: item 04's allocation figures **and** a per-channel cost curve **and** an
answer to how JUCE reports surround buses (M4 §6, and 08's host answers). The `asymmetric` rejection is
D-017's revisit trigger, not this entry's — admitting that family is a `resolve` change with an entry of its
own, never a gate edit. If no target DAW exposes a wider-than-mono sidechain (08's question), A buys a
reachable refusal and no reachable fold; that changes nothing about the rule, which should still be able to
say *why*.

**Evidence.** `src/Engine/LayoutGate.hpp` (the table, the ladder order, the 12-byte pin and the eight verdict
pins — six legacy shapes plus the two endpoints of the new family; item 09's F-012 lesson kept the pin set
small so the swept legs stay load-bearing). `LayoutGateTests`: the 12 legacy shapes with their exact verdict,
`rule` and `processed`; the `[0,12]³` derivation sweep (2 197 triples, `rule == resolve`'s status on every
path, 12 accepted); `mainOut == mainIn` including the leading selections `resolve` takes; the newly reachable
family; one leg per refusal family; the paraphrase's 22 over-admissions asserted as failures. `OfflineRender`
§5.18: the host-vs-gate sweep; 10 refusals applying nothing; the per-bus substitution table (F-015); the
advertisement (main 2 unchanged; sidechain 8 beside a mono main, 2 beside a stereo one); the naming sweep (94
`AudioChannelSet`s, 0 split by naming — the gate-level answer to M4 §6's ambisonic bullet); the inventory (2
in / 1 out, a third bus refused before the gate is asked); the table identical before and after
`prepareToPlay` and after a mid-session layout change; the footprint of the new shape (B-side rings plus hop
scratch: 65 KiB at `chB` = 2 → **260 KiB** at `chB` = 8, H = 128, `maxHostBlockSamples` 4096 — recorded, not
bounded; item 04 owns the ceiling); and the newly reachable family rendering as the fold — shell bit-identical
to the shared builder, differing from both single-channel references (peak Δ 0.183 and 0.177), and −2.985 dB /
−3.020 dB against them with the stage parked: §5.17's arithmetic, now reached through a layout a host can
actually select. **Twelve §5 mutation arms were run** (the eleven planned plus a 4b that disables the two family
pins so the runtime legs they shadow are exercised): results in that plan's §10, and the eight places the plan's
own predictions needed correcting in its §11 — including one arm (5) that is an equivalent mutant, with the
structural reason.

**Disclosed, not fixed.** D-018's antiphase cancellation is now field-reachable: a wide sidechain fed an
antiphase pair folds to exact zeros, falls through `LevelNormalizer`'s floor gate and leaves the wet path at
unity — quiet, uncorrected, no error anywhere. Before this item a host could not select it; after, it is a
layout choice with no control and no console answer. Judged by M9 row 2, per M4 §8.

**Annotated 2026-11-05, at B's first dependency.** Item 04 landed, so the wide-main deferral has its allocation
figures: D-022 C's tables put a stereo layout at 1.02 MiB and the 8-wide fold at 1.02 MiB with the default grain
length, and name the axis that actually costs — a wide **main**, where `processed == chA` multiplies every engine
buffer, which is exactly the family this entry refuses. A per-channel cost curve (M7) and M4 §6's surround-bus
question are the two triggers still outstanding, so the deferral stands.

---

## D-022 — Prepare-time sizing: a floor with a name, a re-prepare keyed on seven fields, and a footprint measured rather than bounded

**Status first.** Decided with the user 2026-11-04 in three parts (A the floor, B the conditional re-prepare, C the
recorded footprint) and **implemented and measured 2026-11-05** by `plan/M4-impl-plan/04-allocation-and-reprepare.md`
as `engine::ReconfigureKey` (`src/Engine/ReconfigureKey.hpp`, header-only, `constexpr` comparable, JUCE-light) with
`ReconfigureTests` — 14 cases / 182 assertions, its own binary for the allocation leg — plus
`EntangleAudioProcessor::maxHostBlockFor`, `OfflineRender` §5.19 and `tools/PrepareFootprint`. `ctest`
**175 → 189**, green; `vstplug_VST3` and `vstplug_Standalone` relinked; ASan/UBSan clean on the new suite. This
discharges the first of D-021 B's three dependencies and retires M4 §8's allocation-risk row.

**Context.** Two half-said things. `configureRings` sized every ring against `std::max(samplesPerBlock,
maxHostBlockSamples)` — a floor in fact, with nothing at the site saying so, and two more copies of the same
`std::max` loose in the harness — and it re-prepared on **every** arrival: `prepareToPlay`, `processorLayoutsChanged`,
and the hop and grain dials through `handleAsyncUpdate`. A host that re-applies an unchanged layout therefore cleared
the rings, re-prepared the engine — which rebuilds the JUCE transform — and re-applied the latency pre-fill, dropping
R samples of a running transport. F-001 records that some hosts re-query and re-prepare without meaning anything by
it. Meanwhile item 03's §5.18 printed the footprint of the shape it had just made reachable by re-evaluating the
scheduler's own expression (F-016), and the number that reached `AGENTS.md` §13 — 65 KiB at `chB` = 2 → 260 KiB at
`chB` = 8 — was the B-side slice of a total nobody had measured.

**The decision.**

* **A — `maxHostBlockSamples = 4096` stays a floor, and becomes a named function (the user's call).**
  `maxHostBlockFor(hostBlock)` is the one place the policy lives: the shell's `configureRings` calls it, §5.19 sweeps
  it, and the harness's two `ChainConfig`s call it instead of recomputing it. Measured consequence
  (`PrepareFootprint` [1], §5.19's sweep through the same function): every ring is `nextPow2(4096 + H)` =
  **8192 floats = 32 KiB at every hop the UI can select**, for every host block up to 4096 — a host running 64-sample
  callbacks pays the same 32 KiB as one running 4096, and the ceiling triple pays ~320 KiB of rings where the delay
  lines it exists to serve need ~13 KiB. Above the floor the host's number reaches the sizing and the rounding step
  shows: 4097 → 8192 f, 8192 → 16 384 f, 65 536 → 131 072 f. What the floor buys is that the sizing does not depend
  on host behaviour at all, so a callback larger than the prepared block trips
  `assert(numSamples <= config.maxHostBlock)` in an assert build instead of degrading — as it does in a Release host
  — to a short `CircularBuffer::write` and a hole in the output. Recorded with the trade, not as a preference:
  sizing to the prepared block would cut a 512-sample host's rings to 4 KiB each and trade that tripwire away. One
  consequence nobody had stated: `nextPow2(hostBlock + hop) > hostBlock` for every hop ≥ 1, so **the assert cannot
  fire on host behaviour as the sizing stands** — it is a regression tripwire on the sizing expression, which is
  precisely what §5 arm 8 reproduces.
* **B — the re-prepare is conditional on a seven-field configuration key (the user's call).** `sampleRate`,
  `hostBlock` (as prepared, before A), `chA`, `chB` (`0` = the sidechain bus is absent, so it carries D-002's enabled
  bit), `chOut`, `hop`, and `grainLength` **after the clamp**. Consequence of the change: a host that re-applies the
  unchanged layout stops dropping the tail and stops rebuilding the transform, and a dial move the clamp swallows
  stops re-preparing at all. Measured: the second prepare with an unchanged key allocates **0 bytes** (printed for
  five layouts in `PrepareFootprint` [5]) and a mid-render `setBusesLayout` + `processorLayoutsChanged` +
  `prepareToPlay` carrying the layout already in force leaves a running transport bit-identical (§5.19). Consequence
  of *how*: the key must be exactly right, so all seven fields are swept one at a time and each must still produce a
  re-prepare — a missed dimension is a real change silently ignored. Post-clamp `L` is a field, which is what makes
  32768 → 65536 at H = 256 a no-op (ceiling 32768, render bit-identical) and the same move at H = 512 a real
  re-prepare (peak Δ 0.212).
* **C — the footprint is measured, documented, and never asserted (the user's call).** No test holds a byte limit.
  `tools/PrepareFootprint` counts live bytes through a replaced global `operator new` over regions that build the
  real thing, prints host, compiler, build type and assert state on its first line, and is deliberately not in CTest
  on `CpuBaseline`'s stated grounds. Consequence accepted: a later sizing change drifts the documented figure
  instead of reddening a test, and running that tool is what re-establishes it. What *is* asserted is behaviour, not
  bytes: a narrowing gives back exactly the ring storage it no longer needs (C2) and the first callback after a
  re-prepare allocates nothing (C8).

**The figures** (2026-11-05; Linux aarch64, gcc 13.3, Release with `-flto`, `ENTANGLE_ASSERTS=ON`, `fft::JuceFft`):

| triple | scheduler alone, H = 128 | whole chain, default L = 8192 | whole chain, L = 65536 (H = 512 only) |
|---|---|---|---|
| `(1, 0, 1)` mono | 81.6 KiB | 760.7 KiB | 5 488.7 KiB |
| `(2, 2, 2)` stereo | 211.9 KiB | 1 024.4 KiB | 6 669.4 KiB |
| `(1, 8, 1)` the gate's ceiling | 342.1 KiB | 1 021.2 KiB | 5 761.2 KiB |

Four things item 03's B-side slice could not say. **The wet path dwarfs the scheduler**: 10.5F + 6 floats at one
processed channel and 12.5F + 6 at two, measured 7.1 KiB mono / 12.5 KiB stereo above those figures at the default
grain length and 26.6 / 47.0 KiB at the cap — `juce::dsp::FFT`'s own tables, named as excluded rather than folded in,
because the backend is chosen at runtime (vDSP, IPP, MKL, FFTW, or the fallback). **An 8-wide sidechain is not the
expensive shape**: consolidation processes one channel, so the ceiling triple pays +130 KiB of scheduler and saves
133 KiB of engine beside stereo — 1021.2 against 1024.4 KiB, a net wash. The axis that costs is the one the gate
still refuses, where `processed == chA` multiplies every engine buffer. And the shell is 265 KiB before any prepare,
so what the guard of B skips per redundant arrival is 761–1024 KiB of churn plus a transform rebuild, against a real
hop change's 8–15 KiB net.

**Revisit trigger.** Wide mains, per D-021 B — the table above is the input that was missing, and the two triggers
that remain are M7's per-channel cost curve and M4 §6's surround-bus answer. Separately: **memory follows the widest
configuration a session has seen**, because `assign` and `clear` keep capacity — measured at the ceiling and H = 256,
a narrowing gives back exactly 224 KiB of ring storage and keeps 7.0 KiB of hop scratch plus 0.4 KiB of ring-object
array. That is the deliberate price of a callback that never grows anything; it becomes a problem only against a host
that flaps between wide and narrow layouts, and the fix then is to release capacity at prepare, never a second
decider (INDEX §3 rule 2).

**Evidence.** `src/Engine/ReconfigureKey.hpp` (the seven fields, the 32-byte size pin, the never-matches-a-real-
configuration pin). `ReconfigureTests`: seven one-field sweeps each with the control that proves the comparison is
not vacuous, the equality and initial-key legs, a refusal clearing the key, prepare sizing by source in and processed
out, the narrowing's exact release, hop scratch keeping its capacity, and a zero-allocation callback after a
re-prepare — all against a byte model that matched the allocator exactly on the first run. 14 cases / 182 assertions,
clean under ASan/UBSan; `EngineTests` was run there by case-name subset (9 cases / 90 365 assertions) rather than
whole, the whole suite exceeding the sanitizer run's time budget at `-O0`. `OfflineRender` §5.19: the
C12 floor sweep through `maxHostBlockFor`, latency == hop over 20 layout × hop preparations plus a dial walk, a fresh
instance's first callback (0 unwritten samples, peak 1.36), mid-session `(2,2,2) ⇄ (1,8,1)` bit-exact against an
instance prepared at the destination, the unchanged-layout re-apply bit-exact against an undisturbed control, the
clamp-swallowed dial move, 28 `(H, L)` pairs through the engine's own guard band, and a refused quadraphonic request
applying nothing across two re-configures. `tools/PrepareFootprint`: the five tables quoted above. **The eleven §5
mutation arms ran**: ten caught, arm 10 the predicted equivalent mutant, arm 8 caught under the sanitizer tree only
(`heap-buffer-overflow` in `CircularBuffer::copyOut` at `(1, 8, 1)` — F-014's signature reproduced on demand), and
arm 11's tripwire finding a real bug in the guard this decision ships. Outcomes, and the eleven places this plan's own
premises needed correcting, are in that plan's §10 and §11.

---

## D-023 — The layout report: one line naming what the plugin was told, built at the tail of a real prepare

**Status first.** Decided 2026-11-06 in `plan/M4-impl-plan/08-host-layout-matrix.md` §0 B/C/D and **implemented and
measured 2026-11-08** as `EntangleAudioProcessor::layoutReport()` with `refreshLayoutReport` behind it, the editor's
`layoutReadout` label, and `OfflineRender` **§5.21** (28 lines, 0 FAIL; 26 report lines reach the installed logger).
`ctest` stays **189/189** — §5.21 lives inside `OfflineRender`, so no test is registered — and `vstplug_VST3` plus
`vstplug_Standalone` relinked at `-j4`. **The rows this instrument exists to answer are open**: H0–H8 in
`docs/host-layout-matrix.md` are all unanswered, because no host has loaded this plugin and INDEX §3 rule 4 will not
let a container answer one. What landed is the ability to answer them with a pasted line rather than an impression.

**Context.** Every layout claim in D-017, D-018 and D-021 was measured in a console against JUCE's own
`checkBusesLayoutSupported`/`setBusesLayout` paths, and item 03 read the VST3 wrapper rather than running it. M4 §6's
questions — which hosts expose a sidechain send wider than mono, whether it can be enabled independently of the main
bus, what a refused layout does in the wild, how JUCE reports surround and ambisonic buses — cannot be answered that
way, and F-015 already showed why the host's own labels are not evidence: this plugin advertises 8 sidechain channels
beside a mono main and 2 beside a stereo one, because the advertisement is computed *from the gate*. A report exists
so the answer is what the plugin was **told**, at the moment it was told.

**The decision.**

* **A — one function, one place, describing a configuration that was actually built.** `layoutReport()` returns a
  `juce::String` assembled by `refreshLayoutReport`, which is called from the tail of `configureRings`: after the
  `gateLayout` call, after the cached `activeChannels`/`sidechainActive` members, and **after** the D-022 B guard's
  early return. Ordering is the decision, not the style — an instrument fed by an arrival that was skipped describes
  a rebuild that did not happen, and §5 arm 2 (moving the call above the guard) reddens twelve C1 lines, C2's
  first-prepare leg, three C3 refusal legs and C4's one-line leg because a prepare counter that counts pokes cannot
  distinguish those cases any more.
* **B — the field set, and the two counters.** Bus inventory (`inBuses`/`outBuses`), live counts and
  `AudioChannelSet::getDescription()` verbatim for the three buses plus each bus's `isEnabled()` flag, the gate's
  `verdict/rule`, `processed`, `getTotalNumOutputChannels()`, rate, hop, requested and running grain length, the
  declared latency, `hostBlock/maxHostBlockFor(hostBlock)`, `sidechainActive`, `effectiveBlendPosition()`, and
  `prepares=`/`skipped=`. The names are the point of P5 and of D-021 B's third trigger, which is why arm 1 (names
  replaced by an empty string) reddens three fields on every one of the twelve accepted triples rather than one
  leg. **Deviation from the plan, and it matters:** the two counters are formatted where the line is *read*, not
  where its body was built. A declined arrival never reaches `refreshLayoutReport`, so counters frozen into the body
  could never show a skip — and "a skipped arrival proves the guard ran" (D-022 B's only outside view, P6's whole
  question) would have been invisible until the next real prepare. Configuration fields still describe the last real
  prepare; only the prefix moves.
* **C — two deliveries, both message-thread, neither on the audio path.** The editor's `layoutReadout` label, fed
  from the existing 100 ms tick beside `blendReadout` and `grainReadout`, with the whole line in the label's tooltip
  because the label cannot hold it and the editor is mid-rewrite (AGENTS §12). And `juce::Logger::writeToLog` once per
  real prepare, behind `MessageManager::existsAndIsCurrentThread()`. The guard is not tidiness: `writeToLog` reaches
  stdout or `NSLog`, which can block, and hosts call `prepareToPlay` from threads of their own choosing. Measured
  consequence, and the reason the console legs are written the way they are: `MessageManager`'s constructor records
  *its own* thread (F-018), so a harness that calls `initialiseJuce_GUI()` satisfies the guard on `main()` and logs
  every prepare; only a genuinely different thread is silenced, which is what §5.21 C5 tests by forking one. In a real
  VST3 host the wrapper designates its own thread, so whether the line ever appears in a host's log is H0, and if it
  never does, the readout becomes the delivery and this entry is amended.
* **D — a lock, and a value, because prepare has no fixed thread.** The body and counters sit behind a
  `mutable juce::CriticalSection` and the getter returns by value. `prepareToPlay` may run on a thread the editor is
  not on, and a `juce::String` assigned under a reader is a crash, not a cosmetic race. Nothing here is reachable
  from `processBlock`: the callback reads no logger, no lock and no string, and C7's grep still returns nothing.

**Rejected.** Writing a file from the plugin (a host-specific user-data path, a privacy question nobody asked it, and
a console harness that would need an env var to reach it — splitting the code path the tests run from the one the host
runs); an env-var-gated diagnostic (same split, plus a mechanism with one user); the host's own meter as the
instrument (P8 keeps one audible probe precisely because it needs no code, but it answers one question, qualitatively).

**Revisit trigger.** The log delivery goes away and the readout is the only carrier if a named host never shows the
line (H0). The whole instrument relocates with the editor rewrite AGENTS §12 anticipates — it follows an existing
readout pattern rather than inventing one, so it moves with the pattern. If a future path changes the configuration
without going through `configureRings`, the line becomes a lie; the counters are the tripwire and C1's
field-by-field comparison against the accessors is the leg that reddens it.

**Evidence.** `OfflineRender` §5.21, all 28 lines green: 12 triples × 24 fields against accessors and
`gateLayout` (0 wrong each); the pinned naming table for eleven shapes; the counter legs
(`1/0` fresh, `1/1` after an unchanged layout re-applied, `1/2` after a repeated `prepareToPlay`, `2/2` after the hop
dial, `3/2` after a layout the gate takes); three refusals moving no configuration field and leaving the render
unchanged (§5.19's method) while `skipped` reaches 2; a hop-only reconfigure whose diff set is exactly `hop,R`; one
log line per real prepare and none per declined arrival; an off-message-thread prepare that logs nothing, refreshes
the report, and leaves the next message-thread prepare free to log. ASan/UBSan (`build/asan`, Debug, asserts ON):
204 `ok`, 0 FAIL, no invalid access and no UB — the at-exit leak report is JUCE's FFTW backend and predates this item
(F-018). C6's footprint, printed not asserted: **+64 B at construction and +287…+294 B at the first prepare** —
761.04 KiB against the published 760.75 mono, 1024.65 against 1024.37 stereo, 1021.48 against 1021.19 at
`(1, 8, 1)` — which is the report's own `juce::String`, and a second prepare with an unchanged key still costs
**0 KiB**. C7: §5.1…§5.20's output byte-identical to the pre-report baseline apart from its own wall-clock lines, all
175 earlier `ok` lines unchanged, `ctest` 189/189. Six of the eight §5 arms red as designed (1, 2, 3, 4, 5, 6), the
reformat control and the editor blind spot green, every restore `cmp`-verified and the no-mutation control rebuilt
and green. Per-arm results and the five places the plan needed correcting are in `08` §10 and §11.

---

## D-023 — The layout report: one line naming what the plugin was *told*, assembled at the tail of a real prepare

**Status first.** Decided 2026-11-06 as `plan/M4-impl-plan/08-host-layout-matrix.md` §0 B/C/D and **implemented and
measured 2026-11-08**: `EntangleAudioProcessor::layoutReport()` over a body written by `refreshLayoutReport` at the
tail of `configureRings`, the editor's `layoutReadout` label beside the two readouts that already ride the 100 ms
tick, and `OfflineRender` **§5.21** — 28 lines decided, 0 FAIL, 26 report lines reaching an installed `juce::Logger`.
`ctest` stays **189/189** (no new registered test: §5.21 lives inside `OfflineRender`, which stays test #189),
`vstplug_VST3` and `vstplug_Standalone` relinked at `-j4`, ASan/UBSan clean on the new legs. **What it exists to
answer is still open**: every row H0–H8 in `docs/host-layout-matrix.md` is unanswered, because no host has loaded
this plugin and §3 rule 4 will not let a container answer one. The instrument is measured; the answers are not.

**Context.** Nothing in this repo's evidence has ever loaded the plugin in a host, so M4 §6's questions — which hosts
expose a sidechain send wider than mono, whether it can be enabled independently of the main bus, what a refused
layout does in the wild, how JUCE reports surround and ambisonic buses — have no answer path that does not go through
a DAW. Reading the host's labels is not a path: F-015 measured this plugin's advertisement (8 sidechain channels
beside a mono main, 2 beside a stereo one) being computed from the plugin's own gate. So the deliverable is an
instrument that records what the plugin was *told*, plus a document whose rows count only when they carry that line
verbatim.

**The decision.**

* **A — one function, one place: the tail of `configureRings`.** After the `gateLayout` call, after the
  `activeChannels`/`sidechainActive` members, and **after** D-022 B's guard returns early — so the line describes a
  configuration that was actually built. The ordering is the decision: arm 2 moves the call above the guard and
  reddens twelve C1 lines plus C2's first-prepare leg, three C3 refusal legs and C4's one-line leg, because a report
  rebuilt by an arrival that changed nothing stops distinguishing "we rebuilt" from "the host re-applied and we left
  the transport alone".
* **B — the fields, and where the counters are formatted.** Bus inventory (`inBuses`/`outBuses`), the live count and
  `AudioChannelSet::getDescription()` verbatim for each of the three buses plus each bus's `isEnabled()` flag, the
  gate's `verdict/rule` and `processed`, `getTotalNumOutputChannels()`, the rate, the hop, the requested and running
  grain length, the declared latency, `hostBlock/maxHostBlockFor(hostBlock)`, `sidechainActive`,
  `effectiveBlendPosition()`, and the two counters. Two deviations from the plan, both measured into existence. The
  method is `getDescription()` — **there is no `getName()`** (F-018). And the counters are formatted *at read time*,
  not when the body is built: a declined arrival never reaches `refreshLayoutReport`, so counters frozen into the
  body could never show a skip, which is the one thing `skipped=` exists to say. Configuration fields still describe
  the last real prepare; only the prefix moves, and C2's "the configuration fields did not move" leg compares those
  fields alone.
* **C — two deliveries, both message-thread, neither blocking the audio path.** The editor label (whole line in its
  tooltip, because a label cannot hold it and the editor is mid-rewrite — AGENTS §12), and one
  `juce::Logger::writeToLog` per real prepare behind `MessageManager::existsAndIsCurrentThread()`. The guard is not
  decoration: `writeToLog` reaches stdout or `NSLog` and can block, and hosts call `prepareToPlay` from threads of
  their own choosing. Measured sharpness (F-018): `MessageManager`'s constructor records *its own* thread, so any
  harness that initialises JUCE's GUI passes the guard on `main()` and logs every prepare — only a genuinely other
  thread is silenced, which is what C5 tests by forking one. A VST3 host designates its own wrapper thread, so
  whether the line ever appears in a host log is H0's question, and if it never does, the readout becomes the
  delivery and this entry gets amended.
* **D — the counters, and the lock.** `prepares=` counts arrivals that rebuilt, `skipped=` counts arrivals the
  seven-field key declined (D-022 C). Both are prepare-time state, reachable from nothing in `processBlock`, and both
  live under a `mutable juce::CriticalSection` with the getter returning by value — `prepareToPlay` has no fixed
  thread and a `juce::String` assigned under a reader is a crash, not a cosmetic race.

**Rejected (the plan's list, unchanged).** Writing a file from the plugin — a host-specific user-data path, a privacy
question nobody asked the plugin to answer, and a console harness needing an env var, which splits the tested code
path from the shipped one. An env-var-gated diagnostic — same split, plus a mechanism with one user. The host's own
meter — P8 keeps one audible probe precisely because it needs no code, but it answers one question and answers it
qualitatively.

**Revisit trigger.** If a named host never shows the log line, the delivery is the readout alone and C is amended
(H0). The whole instrument relocates with the editor rewrite AGENTS §12 anticipates; it follows an existing readout
pattern precisely so that it moves *with* the pattern rather than being designed twice.

**Evidence.** `OfflineRender` §5.21: C1's 24 fields × 12 accepted triples, 0 wrong each; C1b's eleven pinned channel
names; the counter walk `1/0 → 1/1 → 1/2 → 2/2 → 3/2`; three refusals moving no configuration field, `skipped`
reaching 2, and the render unchanged through §5.19's method; a hop-only reconfigure whose diff set is exactly
`hop,R`; C4's one line per real prepare and none per declined arrival; C5's off-thread prepare logging nothing and the
next message-thread prepare logging. ASan/UBSan (`build/asan`, Debug, asserts on): 204 `ok`, 0 FAIL, no invalid
access, no UB. C6, printed not asserted: **+64 B at construction, +287…+294 B at the first prepare** — 761.04 KiB
against D-022 C's published 760.75 mono, 1024.65 against 1024.37 stereo, 1021.48 against 1021.19 at `(1, 8, 1)` —
which is the report's own `juce::String`, and **0 KiB** for a second prepare with an unchanged key. C7: §5.1…§5.20's
output byte-identical to the pre-report baseline apart from the harness's own wall-clock lines, all 175 pre-existing
`ok` lines unchanged, `ctest` 189/189, and `processBlock` still matching none of `Logger`/`DBG`/allocation/lock.
Six of the eight §5 arms reddened as designed and the two that should stay green did; per-arm results and the five
places the plan needed correcting are in `08` §10 and §11.

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

## F-003 — JUCE real-only FFT: per-engine scaling, locking fallback

All line numbers are `juce/modules/juce_dsp/frequency/juce_FFT.cpp` at the pinned
`454fe891ac`, read rather than inferred.

**Real-only scaling is per-engine internally, and reading one constant gives the wrong
answer.** FFTW's `performRealOnlyForwardTransform` (`:696-710`) is a raw
`fftwf_execute_dft_r2c` with **no scaling**, and its inverse (`:712-718`) multiplies by `1/n`
because FFTW's c2r is unnormalised. Apple's (`:483-494`) runs `vDSP_fft_zrip` and then
`vsmul`s by `forwardNormalisation`, which its ctor sets to `0.5f` (`:456`, applied `:491`).
That `0.5` is a **correction, not a net scale**: `vDSP_fft_zrip`'s own forward gain is 2.0
(measured against Accelerate on a unit impulse — DC returns `(2.000, 2.000)`), so the two
cancel and JUCE's real-only forward gain is **1.0 on both engines** — `forwardGain == 1.0`
measured on FFTW in-container and on vDSP on the host. Inferring the net from
`forwardNormalisation` in isolation predicts `0.5` and is wrong by 2×; that is exactly the
mistake this entry previously carried, corrected 2026-10-26 by the host run.
`JuceFft::forwardGain()`/`inverseGain()` exist so scaling is a measured number in a test
rather than a constant read off one engine — and so the next engine added (IPP, MKL, static
FFTW) cannot ship a silent 6 dB change.

**The two engines also pack the half-spectrum differently, but JUCE hides it.** vDSP's `zrip`
stores the Nyquist real part in the DC bin's imaginary slot. `AppleFFT::mirrorResult`
(`:515-529`) undoes that on the forward — `out[F/2] = {out[0].imag(), 0}`, `out[0] =
{out[0].real(), 0}` — and `:506` restores it on the inverse. The net effect is that **both
engines present the layout `Fft.hpp` specifies**, which is why the adapter's only correction
is a scalar and why a `Dft`-vs-`JuceFft` bin-for-bin test catches layout as well as scale.

**The fallback engine is numerically fine and structurally unsafe.** It takes a `SpinLock` on
**every** `perform()` (`:115`) and `alloca`s its scratch (`:146`). That matters for us
specifically because the real-only entry points route through `perform()` (`:136 → :176-181`),
so the lock sits on the hop path — an AGENTS §7 violation. It is also **indistinguishable by
result or gain**: its forward is unnormalised and its inverse scales by `1/size`, the same 1.0
as FFTW — and, measured on the host, the same 1.0 as vDSP. Only which engine the process
actually resolved proves anything, and the way to prove it is **platform-specific** (D-006): on
Linux the FFTW engine is `dlopen`ed, so `RTLD_NOLOAD` residency of `libfftw3f.so` is decisive;
on macOS vDSP is linked at build time and Accelerate is mapped by `juce_dsp` regardless of
which engine wins, so residency is necessary but not sufficient and the closed engine list does
the excluding. **A passing numeric suite is never evidence the fallback is not running** — on
either platform.

**FFTW is safe on the hop path**: `FFTWImpl` locks only in its ctor (`:660`) and dtor (`:674`)
to guard the non-thread-safe `plan_*`/`destroy_*`; the `execute_*` path takes no lock. Note
`prepare()` therefore serialises against other FFTW users — `prepareToPlay`, never the
callback.

**Engine priorities, so a silent downgrade is predictable:** static FFTW 10 (`:570`), IPP 9
(`:831`), MKL 8 (`:745`), **vDSP 5** (`:446`, compiled for `(JUCE_MAC||JUCE_IOS) &&
JUCE_USE_VDSP_FRAMEWORK`, default on), **shared FFTW 3** (`:572`), **fallback −1** (`:92`).
`createBestEngineForPlatform` (`:60-66`) takes the highest that `create()`s. All three
`JUCE_DSP_USE_*` flags default to 0, so without `D-006`'s define even a host with FFTW gets
the locking fallback. There is no separate SIMD engine in this file (`NEON|SSE2|AVX` → 0
matches).

**Missing FFTW is silent.** The soname is chosen per OS — `libfftw3f.dylib` / `.dll` / `.so`
(`:619-623`) — and a failed `lib.open` returns null from `create()`, falling through to the
next engine **with no diagnostic**. On macOS that name is not on dyld's default path, so a
Mac with FFTW installed may still not use it. Design against this: measure, never assume.

**The FFTW c2r ignores exactly the two slots `JuceFft::inverse` clears, so that clearing is
untestable in this container.** Measured 2026-10-26 by mutation: deleting
`scratch[1] = 0.0f` and `scratch[F + 1] = 0.0f` from `JuceFft::inverse`, then feeding it a
product spectrum with `NaN` in `bins[1]` and `Inf` in `bins[F + 1]`, leaves every output sample
**bit-identical** at `F ∈ {64, 256, 1024, 4096}`. Even-`n` c2r discards the imaginary parts of
bin 0 and bin `F/2` — the same two slots `Fft.hpp` declares never-read, which is presumably why
the contract chose them. Consequences: the clearing is engine-portable hygiene for vDSP's
packing, not a live guard on FFTW; **no container test can distinguish its presence from its
absence**, so a test comment claiming to exercise it is wrong, and a future reader must not
"fix" the redundancy by deleting it on the strength of a green suite.

**macOS leg measured 2026-10-26, not inferred.** Host: macOS 26.5.2 (25F84) arm64, Apple clang,
CMake 4.2.3, Ninja, JUCE `9.0.4` — `ctest -R "^Dft"` 6/6, `ctest -R Spectral` 13/13, full suite
45/45. Two things this settles that the container could not:

- **The installed engine is vDSP, proven the platform-specific way** (see the residency
  paragraph above): `JuceFft on macOS has no FFTW engine to fall through to` passes, and
  `otool -L` on `FftJuceTests` shows `Accelerate.framework` linked with `nm -u` reporting
  `_vDSP_create_fftsetup`, `_vDSP_fft_zrip`, `_vDSP_fft_zop` and no FFTW symbol. The gain
  contributes nothing here — `forwardNormalisation = 0.5f` cancels `zrip`'s 2.0, so vDSP, FFTW
  and the fallback all report 1.0 and no gain figure can name an engine on Apple.
- **A caller-authored spectrum survives the vDSP packing.** `Spectral multiply through JuceFft
  matches the Dft circular convolution` runs `forward · multiplyHalfSpectrum · inverse` on both
  the `Dft` oracle and the vDSP-backed adapter and compares in the time domain at peak-relative
  `1e-4`, green at `F ∈ {64, 256, 1024, 4096}`. Under this entry's 1.0/1.0 calibration a
  spurious half-scale or a stray `1/F` is a 6 dB / `F`-fold error that bound cannot hide, so
  the adapter neither invents nor loses scale on a spectrum it did not produce — which is
  precisely what M2 item 6 hands it. **`F = 4096` was clean on both legs.**

**A failure mode this run was briefed to expect has no referent.** The macOS brief said a red
at `F = 4096` alone would be "the known vDSP split-radix defect from D-006". Neither D-006 nor
this entry records any split-radix defect, and nothing in the tree measures one — `JuceFft`
matches `Dft` bin for bin at 4096 (#37). So a future `4096`-only red on Apple is an
unexplained new finding, not a known issue to wave through, and the green runs above are the
baseline it will be judged against.

## F-004 — A spectral product needs no extra factor under D-006's convention

`fft::multiplyHalfSpectrum` (`src/Fft/Spectral.hpp`) applies no scale, and nothing downstream
should add one on the grounds that FFT normalisation is owed.

With forward unnormalised and inverse carrying `1/F` — the convention `Fft.hpp` pins and D-006
calibrates to on both shipped engines — `IDFT(X·Y)[n] = Σ_m x[m]·y[(n−m) mod F]`. Circular
convolution, nothing left to apply. The `1/F` a product "needs" is the one the inverse already
does.

**Evidence.** Probe run 2026-10-26 in-container (`g++ -std=c++23`, `fft::Dft`, `F = 8`,
`a = [1,2,3,4,0,0,0,0]`, `b = [0,0,1,5,0,0,0,0]`): forward both, multiply bins with no factor,
`Dft::inverse`, against a `double` direct circular convolution.

```text
product -> inverse:  0  0  1  7 13 19 20  0
naive circular conv: 0  0  1  7 13 19 20  0
Dft forward of delta: bin0=(1.0000,0.0000) bin4=(1.0000,0.0000)
```

Equal at every sample, and a delta's spectrum is `(1,0)` in every bin, so the delta identity holds
with no factor. Read off the pinned source, same conclusion: `Dft::inverse` (`Dft.hpp:65`) applies
`scale = 1.0 / F` (`:68`) and nothing on the forward path does. Now pinned by `SpectralTests`, not
by this probe.

**Why it is worth a finding rather than a footnote.** `Fft.hpp` used to state, in the same sentence
that pinned the convention, that "the `1/F` a spectral product needs is the caller's to fold into
its own gain table". An implementer obeying that divides by `F` a *second* time: output
`20·log10(F)` dB quiet — `−72 dB` at `F = 4096`, effectively silence — and having "already handled
the scaling", they hunt the bug in the window or the OLA. Loud failure, expensive diagnosis,
because the doc said the factor was owed. Corrected in `Fft.hpp` and `src/Fft/README.md`
§Normalisation alongside this entry.

**Consequence for the engine's gain table (M2 item 5):** it carries the window/OLA correction
**only**. There is no FFT normalisation term waiting for it.

**Not a scale change anywhere.** `Dft` and `JuceFft` were correct as written and both legs of D-006
stand; this is a documentation defect and the primitive's deliberate absence of a factor.

## F-005 — The OLA order *is* the latency model: emit, zero, advance, then add

`grain::OlaAccumulator::emitAndAdd` (`src/Grain/OlaAccumulator.hpp`) emits the H samples at the head
and zeroes them, advances the head by H, then adds the arriving grain at the new head. The reverse
order — add, then emit — is the natural reading of "overlap-add" and it is wrong in three ways at
once, which is why the order is pinned in the header rather than left to the implementation.

Adding before emitting shifts the whole wet stream one hop early:

- the delta anchor moves from `out[n0+L+m] = w(m)` to `out[n0+L−H+m]`, so the physical delay reads
  `L−H` rather than `L`;
- output sample q then draws on input at and after q, breaking the strict causality that lets a
  reported R of 0 be honest;
- the live span at emit time is `2L−1+H`, which no power-of-two `F = nextPow2(2L−1)` satisfies — the
  ring bound holds only because the emitted H are removed before the new grain lands.

**Evidence.** Derived 2026-10-27 while implementing M2 items 2 and 4, and pinned by tests rather than
prose: `expectDeltaIsWindow` asserts `out[n0+L+m] == w(m)` to `1e-5` with silence either side
(`GrainEngineChecks.hpp`), `expectCausalAtZeroLatency` asserts output q is unchanged when every input
at and after `q` is perturbed, and `OlaAccumulatorTests` compares against a naive overlap-add
bit-exactly. Green on the `Dft` oracle (33 cases / 1,340,073 assertions) and on `fft::JuceFft`
(12 cases / 48,383 assertions), 2026-10-27.

**Why it is a finding.** The plan's §4 pseudo-code and §6 anchors disagreed on this order, so the
ambiguity is in the spec and will be re-read by whoever implements M3. Resolve it against the delta
anchor: it is the only statement of the three that fixes delay, causality and ring size
simultaneously.

## F-006 — `HopScheduler` cannot drive a reported latency of 0 at `hostBlock < H`

`HopScheduler::prepare` pre-fills the output ring with exactly `latencySamples` and then requires
every callback to drain what it asked for. With `R = 0` and a host block smaller than the hop the ring
is empty on the first callback, so the drain assert fires before any audio is produced.

Consequence for the tests: §6.5 block-size invariance runs at `R = H` (see the comment on
`expectBlockSizeInvariance`), and `R = 0` is proven at the engine seam only — where it belongs, since
`processHop` never sees the reported latency. The two are consistent: `R` shifts where the host cuts
the stream, not its shape.

**Revisit trigger.** The latency wiring, which `M2-grain-engine.md` §4 lists as **item 7** (the shell
swap is item 6 and landed 2026-10-28, deliberately leaving `R` at the `hop` placeholder, so this
remains open). It has to settle between pre-filling `max(R, H−1)` and changing the scheduler's drain
assert to tolerate a short first callback; the choice is a latency-policy question (D-004) and should
land there, not in the plugin.

**Evidence.** Read from `src/Engine/HopScheduler.cpp` and reproduced by driving the scheduler at
`R = 0, hostBlock = 32, H = 128` during M2 item 2/4 integration, 2026-10-27. Reproduced again
2026-10-28 at `R = 1` on the same H and hostBlock, in `DryWetEngineTests`: what fires is
`assert(delivered == arrived)` on the *first* callback. That second point sharpens the bound — the
failure is not "`R` is too small" in the abstract but "the ring held less than the pre-hop callbacks
drain", which at `H = 128, hostBlock = 32` is the 3 × 32 = 96 samples consumed before the input
first reaches a full hop. The test now sits on that boundary rather than guessing at it.

## F-007 — Grain-engine per-callback cost: hop-boundary callbacks are 68–266× idle callbacks

The §6.7 CPU baseline for M7's flattening work. `tools/CpuBaseline.cpp` (target `CpuBaseline`, not
registered with CTest — wall-clock numbers on a shared machine are a measurement, not a pass/fail
signal) drives the real `GrainConvolutionEngine` through `HopScheduler` at 48 kHz, mono, 3000 hops,
`R = H`, and buckets callbacks by whether `hopCount()` advanced. Measured in-container 2026-10-27 on
whichever engine `juce::dsp::FFT` installed (forward/inverse gain both 1.000, i.e. FFTW or the
fallback — this host has no vDSP):

| L | H | hostBlock | hop callback mean | p99 | max | idle mean | max | `processHop` alone mean | core |
|---|---|---|---|---|---|---|---|---|---|
| 512 | 128 | 32 | 19.74 µs | 25.83 | 34.58 | 0.29 µs | 4.92 | 18.90 µs | 0.71 % |
| 512 | 128 | 64 | 20.65 µs | 31.62 | 37.25 | 0.58 µs | 5.00 | 18.61 µs | 0.70 % |
| 1024 | 256 | 32 | 38.65 µs | 48.42 | 61.08 | 0.29 µs | 7.75 | 35.86 µs | 0.67 % |
| 1024 | 256 | 64 | 38.61 µs | 48.71 | 54.62 | 0.57 µs | 8.21 | 36.09 µs | 0.68 % |
| 2048 | 512 | 32 | 77.11 µs | 99.96 | 111.71 | 0.29 µs | 14.88 | 73.67 µs | 0.69 % |
| 2048 | 512 | 64 | 77.37 µs | 97.42 | 105.62 | 0.57 µs | 10.17 | 73.37 µs | 0.69 % |

**What the numbers say.** Total cost is ~0.7 % of one core at every size — hop work scales with
`F log F` while hops/s scales with `1/H`, and `H ∝ L`, so the two cancel. The problem is not load,
it is distribution: **the hop-boundary callback costs 68× the idle callback at L = 512 and 266× at
L = 2048.** With `R = 0` the whole FFT must finish inside the callback that crosses the boundary (§0
of the M2 item-2/4 plan), so this is a 77 µs spike repeating every `H` samples, not a smooth 0.7 %.

**Why hostBlock does not help.** At `H = 512`, `hostBlock = 32` gives one spike per 16 callbacks and
`hostBlock = 64` one per 8; the hop mean is flat (77.11 vs 77.37 µs) because the same work moves into
a different number of buckets. A larger host block lowers spike *frequency*, never spike *cost* —
so M7's lever is partitioning the FFT across hops (M6's partitioned-convolution machinery), not
tuning the host's buffer size.

**Two caveats for anyone re-running this.** `processHop alone` shows an occasional outlier (156 µs at
L = 512, 224 µs at L = 2048) far above its own p99, and it appears in only some rows — a one-off
scheduling hiccup rather than a recurring cost, cause not measured here; the scheduler-driven column
is the one to quote. And vDSP on Apple hardware is unmeasured: the ratio is the finding, the absolute
µs are host-specific.

**Correction 2026-10-28 — every µs above is an `-O0` number (F-008), so they are all pessimistic.**
Re-measured on three configurations, same source, `processHop alone` means:

| L | H | Linux container `-O0` (the table above) | Apple Silicon `-O0` | Apple Silicon `-O3` | Ubuntu x86-64, Ryzen 5950X `-O3` |
|---|---|---|---|---|---|
| 512 | 128 | 18.90 | 25.4 | 8.2 | 5.1 |
| 1024 | 256 | 35.86 | 48.9 | 16.6 | 10.8 |
| 2048 | 512 | 73.67 | 99.7 | 37.9 | 22.0 |

Optimisation is worth 2.6–3.1× on the Apple Silicon machine at these sizes, and the two `-O0` columns
differ from each other by ~35% on machine grounds alone — so **the absolute µs in the original table
carry neither a build type nor a comparable host and must not be quoted as a cost figure.** What
survives unchanged is the shape: total load stays flat across the three sizes, hop work scales with
`F log2 F` (measured step-ups 2.02× and 2.28× against the 2.2× and 2.18× the model predicts), and the
hop-boundary callback dwarfs the idle one — at `-O3` the ratio is 202× on Apple Silicon and 104× on
the 5950X, still inside the 68–266× band recorded above. The `-O3` rows were measured by the user on
his own hosts; the container rows by me.

**Addendum 2026-10-28 (M2 item 6) — stereo is 2× mono, and the spike is what bounds the preset set.**
`CpuBaseline` now takes channel count and the full D-008 `L` set; run `Release`, stereo, hostBlock 32
and 64, over `L ∈ {1024 … 65536}` at H = 128 and H = 512 (`docs/cpubaseline-m2-item6.log`, full table
in D-008). Two things this entry did not know:

- The **stereo/mono ratio is 1.94–2.06× above `L = 2048`** (1.69× at 1024, where two channels still
  fit in cache), so the per-channel loop is linear and every 2×-mono extrapolation in the tables
  above was arithmetically sound. It had never been measured; now it has.
- The **68–266× idle/hop ratio understates the host's problem at the top of the set.** Ratio is about
  distribution, but a host experiences an absolute deadline: at `L = 65536` stereo the hop callback
  spikes to 3.2–3.4 ms, which busts a 128-sample callback at 48 kHz (2.67 ms) while the *mean* load
  sits at 20 % of a core. `L = 16384` spikes to 578 µs and still clears a 32-sample callback. So the
  real-time ceiling on the preset list is the spike against `hostBlock`, and the percentage column is
  only a proxy for it — which is why D-008 bounds the `(L, H)` pair and records the spike beside it.

Load itself stays cheap and flat in the way this entry originally found: the default pair (H = 128,
L = 8192) costs 5.66 % of one core in stereo at `-O3` on the container host, and the whole admitted
set stays under 25 % except the corner D-008 excludes.

## F-008 — The default configure emits no `-O` flag: everything including the shipped plugin builds `-O0`

`CMAKE_BUILD_TYPE` is empty in the cache and `CMAKE_CXX_FLAGS` is empty, so **no translation unit in
this project receives an optimisation level and clang compiles them all at `-O0`** — the default when
no `-O` is given. Read off ninja's own command graph:

```
/usr/bin/c++ -I…/src/Grain -I…/src/Engine -I…/src/Container -I…/src/Fft -std=gnu++2b -arch arm64 -c GrainEngine.cpp
```

`FftJuce` and the plugin target pick up `-DNDEBUG=1` from JUCE's interface definitions and still get
no `-O`, so the FFT wrapper and the engine are unoptimised too. `cmd/install` passes no build type and
`cmd/build` does not set one, so **this is the configuration a plugin binary is actually built and
shipped in**, not merely a test-harness artefact. `CMAKE_CXX_FLAGS_RELEASE` is `-O3 -DNDEBUG` and sits
unused.

Measured consequence on the grain engine (Apple Silicon, same source both ways): 2.6–3.1× at the sizes
in F-007's correction table above — 99.7 µs → 37.9 µs at L = 2048.

**Why it matters beyond a benchmark.** Any CPU figure quoted without its build type is meaningless, and
the first one anyone reaches for is F-007's, which was recorded before this was noticed. It also makes
the shipped plugin several times more expensive than it needs to be.

**Fixed 2026-10-28.** The root `CMakeLists.txt` now defaults a single-config configure to `Release`:

```cmake
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()
```

Verified on the idiom in an isolated project: empty build type → `Release` with `-O3`; an explicit
`Debug` or `RelWithDebInfo` is respected; a multi-config generator is left alone (`CMAKE_CONFIGURATION_TYPES`
guards it). Because the test is on the *value* and the set is `FORCE`d, a tree cached with an empty
`CMAKE_BUILD_TYPE` — every tree in this project to date — self-heals on its next reconfigure.

**The consequence, decided 2026-10-28: `Release` adds `-DNDEBUG`, which compiles out all 26 `assert()`s
in `src/`, and the test programs inherit that.** That removes the scheduler's short-delivery tripwire
(`assert(delivered == arrived)`, `HopScheduler.cpp:121`, D-004's forbidden-pad-to-look-complete guard T2)
and the `L % H` / `F >= 2L−1` / power-of-two prepare invariants in `GrainHistory`, `OlaAccumulator` and
`RingSpan`. `ctest` at the new default therefore runs without them.

They are kept. `option(ENTANGLE_ASSERTS … ON)` in the root `CMakeLists.txt` carries `-UNDEBUG` on an
`EntangleAsserts` interface target, linked `PRIVATE` into `Engine`, `Grain` and `Fft` — the JUCE-light
libraries that own the tripwires — and into `GrainTests` and `FftTests`, whose own translation units are
the ones that instantiate the header-inline checks in `Window.hpp`, `RingSpan.hpp`, `Spectral.hpp` and
`Dft.hpp`.

**Why per-target and why not everywhere.** Two measurements, both in isolated projects at
`-DCMAKE_BUILD_TYPE=Release`:

| Wiring | Assert inside a static library's `.cpp` |
|---|---|
| no flag | `exit=0` — compiled out |
| `-UNDEBUG` as `INTERFACE` on the library, consumed by the **test executable** | `exit=0` — **still compiled out** |
| `-UNDEBUG` reaching the **library's own** compile lines | `exit=134` — live, at `-O3` |

A consumer's flags do not rebuild a static library's objects, so "add it to the test targets" — the
shape recommended above — does nothing for the asserts that live in `HopScheduler.cpp` and `Grain/*.cpp`.
It has to be on the owning target. Equally it must **not** be global: `juce_TargetPlatform.h:145-146`
turns `JUCE_DEBUG` on when neither `NDEBUG` nor `DEBUG` is defined, so a project-wide `-UNDEBUG` would
put JUCE itself into debug mode; and applied to a TU that includes JUCE headers (`FftJuce`, the plugin,
`OfflineRender`, `GrainEngineJuceTests`, `FftJuceTests`) it changes JUCE's class layout in that TU while
the JUCE modules were built with `NDEBUG` — an ODR break. Those targets keep `NDEBUG`.

**What that means for the shipped plugin, stated plainly.** `vstplug` links `Engine` and `Grain`, so the
objects it links now carry live tripwires: an invariant violation in a host is `abort()`, not a silent
wrong number. There is no configuration that keeps the tripwires in `ctest` and not in the plugin,
because both link the same objects. `-DENTANGLE_ASSERTS=OFF` is the knob for a build that must not abort,
and it turns them off for the tests too. Measured both ways: `ON` → `exit=134`, `OFF` → clean exit, with
`-O3` present in both (`-O3 -DNDEBUG -UNDEBUG` on the owning target's command line).

**Still open:** every CPU figure recorded before this fix is `-O0` and needs re-measuring at `Release`,
and the whole suite has not been run at `Release` at all. `JuceFft.cpp:31`'s power-of-two guard stays
compiled out at `Release` — it is in a JUCE-including TU; converting it to `jassert` would be the fix and
has not been done.

**Evidence.** `build/CMakeCache.txt` (`CMAKE_BUILD_TYPE:STRING=`, `CMAKE_CXX_FLAGS:STRING=`,
`CMAKE_CXX_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`) and `ninja -C build -t commands <obj>` for the Grain,
FftJuce and plugin targets, 2026-10-28. The speed-up is `CpuBaseline` at default versus
`cmake -B build-rel -DCMAKE_BUILD_TYPE=Release`, run by the user on his hosts.

**Not verified here:** whether JUCE's own modules are affected in a way that matters beyond the above —
they are compiled per-TU by the same flags, so they are `-O0` as well. Whether a Release build changes
any numeric output (it should not: no fast-math flags are in play) is untested, and `OfflineRender`'s
bit-exact assertions are the check that would catch it if it did.

---

## F-009 — `libFftJuce.a` holds the whole JUCE module closure, and non-PIC objects only fail when a shared object links them

**Symptom.** M2 item 6's first `vstplug_VST3` link — the first time anything in this project linked the
static libraries into a shared object — failed on a symbol nobody had edited:

```
ld: relocation R_AARCH64_ADR_PREL_PG_HI21 against symbol
    `_ZN4juce10FlacReader14errorCallback_EPvPKci' which may bind externally
    can not be used when making a shared object; recompile with -fPIC
    src/FftJuce/libFftJuce.a(juce_audio_formats.cpp.o)
```

Adding `-fPIC` to `FftJuce` alone moved the failure twice rather than fixing it: next
`libGrain.a(GrainEngine.cpp.o)` against `_ZTVN5grain22GrainConvolutionEngineE` (a vtable), then
`libEngine.a(DryWetEngine.cpp.o)` against `_ZN6engine12DryWetEngine5resetEv`.

**Two facts behind it.**

- **The archives are bigger than their sources.** `ar t build/release/src/FftJuce/libFftJuce.a` returns
  12 objects for one `JuceFft.cpp`: nine of them `juce_core`, `juce_audio_formats` and FLAC (`juce_audio_formats_flac_1.c.o`,
  `…_2.c.o`). `juce_add_module` attaches a module's `.cpp` as `INTERFACE` sources, so the library that
  first pulls in `juce_dsp` absorbs the closure — which is why a FLAC error handler is the first thing
  the linker complains about when the change under test was a plugin engine.
- **Non-PIC static objects fail only at the shared link, and only for addresses.** CMake defaults
  `POSITION_INDEPENDENT_CODE` off for a static library, and everything the plugin had linked so far was
  reachable through calls, which a non-PIC `BL` resolves fine. What breaks it is a TU taking *the
  address* of something with external linkage — a vtable reference or a virtual call — because that
  needs an `ADRP`+`ADD` of the symbol's page, and an `ADRP` that could bind externally cannot be
  relaxed by the linker. Installing a polymorphic `Engine` under a `std::unique_ptr` is exactly that.

**Fix, and why it is project-wide.** `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` in the root
`CMakeLists.txt`, before any `add_subdirectory`. Per-target `-fPIC` was tried first and turned a single
error into a whack-a-mole across three archives in a row — the third failure appeared only after the
first two were fixed, so each fix hid the next. Every static library here is reachable from
`Entangle.so`, and the plugin is the only shared object in the project, so there is no target that
pays for PIC and gets nothing. Full rebuild after the change: clean, suite green — 95/95 where the
baseline was 90/90, the five extra being `DryWetEngineTests` rather than anything to do with PIC.

**Revisit trigger:** a second shared object with different flags, or an embedded/older toolchain where
PIC costs measurable registers on a hot loop — neither exists here today.

**Evidence.** 2026-10-28, Linux aarch64, GCC, `Release`. The three linker errors above are verbatim
from the three successive link attempts; the `ar t` counts are from the built tree after the fix, so
they describe the archive shape the flag had to cover. **Not verified:** the cost of PIC on this
workload — `CpuBaseline` was re-run only after the fix, so no before/after pair exists, and aarch64
indirect-addressing through the GOT is not expected to show at these sizes.

---

## F-010 — A JUCE float parameter with a non-dyadic interval cannot express its own default or its own endpoints

**The fact.** A `juce::AudioParameterFloat` does not hold the value it was given. It holds a
normalised float, and a `NormalisableRange` with an interval snaps on the way through, in two places
that both lose the last bits:

- `convertTo0to1(v)` then `convertFrom0to1(n)` — so a value whose proportion of the range is not
  dyadic does not come back as itself. For a `[-60, +12]` dB range with no interval, the default 0
  is `60/72` in the normalised domain and returns as **−1.96e-06 dB**, i.e. a stage running at
  **0.99999977** instead of unity.
- `snapToLegalValue(v)` = `start + interval * floor((v − start) / interval + 0.5)`. For `[-1, 1]`
  with the `0.01` interval the blend parameter shipped with, that is `-1 + 0.01f * 100`, and this
  Release build evaluates it to **−2.23517418e-08** — so the parameter's *own default* is not zero.
  The topmost position fares worse: `-1 + 0.01f * 200` returns **0.99999994**, one step of the float
  grid inside 1.0.

Both figures were observed in the shell rather than derived on paper, but they were observed
differently and the record should say so: `-2.23517418e-08` was read straight off
`apvts.getRawParameterValue(id)->load()` at the parameter's default, while the topmost-position
`0.99999994` is inferred from the same `snapToLegalValue` expression — its direct evidence is the four
`p = +1` legs, which were red by a few ULP and went green when the step became dyadic. Together they
are the whole explanation for two legs that looked like DSP bugs: a
`3.5e-08 × A` leak of dry signal in a render that had to be pure convolution — `9.26354e-09` at the
first sample of §5.1's, which is `sin(2.2e-08 · π/2) × a[0]` — and a `p = +1` render that kept
`7.05e-08` of the wet stream because a p that stops short of 1.0 never reaches a law's endpoint
snap.

**Why it bites here and not usually.** A UI reading the parameter formats to two decimals and sees
`0.00` either way, and nothing downstream in ordinary plugin work compares two floats for equality.
This project's M3 §7 criteria are bit-exactness at `p = ±1` and at unity gain, so a residue in the
*parameter* is indistinguishable from a residue in the law — and it is invisible in the UI.

**The rule that falls out.** Any parameter whose value a test compares bit-exactly needs a dyadic
quantum, and the test belongs on the round trip rather than on the arithmetic: `-1 + 256 × (1/128)`
is exactly `1.0f` and `-60 + 480 × 0.125` is exactly `0`, in every evaluation order, because there is
nothing left to round. So the blend runs on a `1/128` step (257 positions, finer than the 0.01 it
replaced) and the output gain on `0.125` dB, both recorded in D-011. A decimal step can be repaired
on the read side instead — `roundf(v * 100) / 100` — but that leaves the UI, the parameter and the
DSP holding three slightly different numbers, which is the situation this finding exists to avoid.

**Evidence.** 2026-10-29, in-container `Release` (`-O3` + LTO), GCC. Before the quantum change:
`OfflineRender` §5.1 "shell adds nothing" red at `ch0 n128 got -9.26354e-09 want 0` with the shell
holding `-2.23517418e-08` at its own default, all four `dial +1 … B by R, bit-exact` legs red by a few
ULP at the small samples where a `7e-08` addend is still visible, and §5.5 "sidechain connected and
silent" red because the centre is no longer pure convolution. After: §5.1 bit-identical, §5.10 green,
and the state-compatibility leg showing the blend loading as `0.367188` — `0.37` poked onto the
`1/128` grid, which is the honest comparison and why that leg compares the writer's held value rather
than the literal.

---

## F-011 — The delta anchor is a DC-heavy signal, so a filter in front of §5.3 un-pins the engine's gain

**The trap.** §5.3's three physics legs — nothing before `n0 + R + L`, a Hann of peak 1.0 inside the
window, nothing after `n0 + R + 2L − 1` — are the only assertions in the harness that pin the engine's
*absolute* gain. Everything else in §5 compares one render against another. They ran against whatever
the shell emits, which was fine while the shell added nothing but delay and sum, and stops meaning
anything the moment the shell contains a filter: the legs then measure the filter, and an engine change
that alters the gain can hide behind it.

**Why this anchor in particular.** The response is a Hann of length L, so its mean is 0.5 against a
peak of 1.0 and its main lobe spans ±2 bins (±2·sr/L — ±11.7 Hz at L = 8192, 48 kHz). It is dominated by
exactly the band a DC blocker exists to remove: at the shipped corner the shipped chain's response peak
is 0.414009 where the leg demands 1.0, and `|y|` past the analytic support is 0.0827042 where the leg
demands ≤ 1e-5.

**Measured, not argued** (2026-10-30, in-container `Release`, by pointing the legs at the shipped
chain): all three shapes go red, but not evenly. The post-support residue fails by four orders of
magnitude — 0.0827042 at L = 8192, 0.364114 at L = 1024, 0.0104987 at L = 65536, against the leg's
1e-5 — while the window deviation fails by 2 % (1.0176e-5 at m = 61, 1.193e-5 at m = 16, 1.0007e-5 at
m = 244). So the support leg is what catches the filter; the window leg catches it only by luck of the
tolerance, and widening the tolerance to accommodate a filter would quietly delete the gain pin as a
side effect.

**The rule that falls out.** Engine physics runs on a chain variant that contains no safety stage
(`entangle::ChainVariant::RawWet`); the shipped chain is asserted against the same chain built without
the shell, which is where a stage that moves the arrival index or the reported latency is caught; and
the arrival-index leg (criterion B6) runs on `ClipperOnly`, because across `Shipped` and `RawWet` it
would measure the filter's phase rather than a delay. A stage added to the output path gets a variant
in that enumeration, not a wider tolerance on an existing leg.

**The other half of the same trap.** §5.5's centre leg and §5.10's gain legs compare the shipped chain
against a reference staged from the very classes the chain uses, so they pin wiring and order and
cannot see a defect *inside* a stage — both sides of the comparison make the same mistake together.
Unit tests own stage behaviour; the same-stage reference owns composition. Any "the harness is green"
claim about a stage's numbers has to name which of the two it is resting on.

---

## F-012 — Four channel-map mutation predictions that did not survive the run

**Found 2026-11-01** running `plan/M4-impl-plan/01-channel-map-helper.md` §5's mutations against
`ChannelMapTests`. Recorded because each one would otherwise be re-derived by the next person who mutates
this header — and because two of them were load-bearing in a milestone rationale (D-017).

* **`a[c] = c % chA` is not a mutation.** With asymmetric pairs rejected and `channelsOut ≤ max(chA, chB)`,
  every filled slot satisfies `c < ch` for a multi-channel side, so `c % ch == c`; and for a mono side
  `c % 1 == 0`, which *is* the broadcast rule. The modulo alternative is observationally identical on the
  whole accepted set — the mutation left all 12 cases green, where the plan predicted four red. The leg
  that discriminates the index rule is the one that *accepts* the asymmetric case and resolves it with
  modulo (criteria 1, 2 and 4 went red).
* **Commutativity does not argue for rejecting asymmetric.** Under modulo, `resolve(2, 6, 6)` gives
  `a = [0,1,0,1,0,1]`, `b = [0,1,2,3,4,5]` and the swap gives exactly those two arrays exchanged — the map
  commutes. The measured result was green on the commutativity leg, which the plan had predicted red. What
  a modulo rule really costs is injectivity: two outputs reading one source, so an impulse lands in three
  outputs and the independence criterion stops meaning "mapped to it".
* **"Nothing half-written" cannot be mutated into view through a value-returning `resolve`.** Moving the
  capacity check after the fill loop writes indices into a local that is then discarded and a fresh
  value-initialised map returned, so the pristine check on the rejected result stays green — only the
  table leg went red, and it went red on *status precedence*, not on the fill. With this API shape the
  property is structural: a rejected call cannot publish indices. Testing it as if it were behavioural
  needs an out-parameter, and that is a worse API for a map that is copied by value onto the audio path.
* **`ctest` runs whatever binary is on disk.** Four of the eight mutations are caught by the criterion-8
  `static_assert`s, so the compile fails and `ctest` reports the *previous* mutant's failures — which read
  as a genuine new result until the build status was checked. A mutation run has to gate the test run on
  the build's exit status, and a green-looking list is not evidence of anything.

**The rule that falls out.** Write mutation predictions as predictions, run them, and let the measured
column overrule the plan — including when the overruled claim is the one a milestone used to justify a
restriction. And when a criterion's discrimination is structural rather than behavioural, say so in the
criterion rather than letting a later reader conclude the test is doing work it cannot do.

---

## F-013 — A rule keyed to the main bus shape leaves commutativity with no domain to be tested over

**Found 2026-11-02** while recording D-018, which was itself found by working out what the decision does to
`ChannelMapTests`'s commutativity leg rather than by assuming the leg would survive it.

D-017's accepted map was symmetric in (`chA`, `chB`) — every rule was stated over the pair without saying
which side was the plugin's own signal — which is what let that item's commutativity leg swap every `ok`
triple that has a sidechain and check the two index arrays exchange bit-exactly (601 assertions,
`plan/M4-impl-plan/01-channel-map-helper.md` §10 row 3). D-018 keys the rule to the **main** bus: a mono main
with a wide sidechain consolidates to one channel, a wide main with a mono sidechain broadcasts to `chA`
channels. Those two are different channel counts, so the swap partner for the whole (`1`, `> 1`) family is a
layout with a different shape.

Worked through, the `ok` families after D-018 are:

| Family | Swap partner | Non-trivial? |
|---|---|---|
| `chB == 0` | none — swapping moves to `empty` (A is the mandatory side) | n/a |
| `chA == chB` (including `1, 1`) | itself; both sides map by index | no — identity both ways |
| `chA > 1`, `chB == 1` | (`1`, `chA`) → consolidated, one channel | no — different channel count, no comparison to make |
| `chA == 1`, `chB > 1` | (`chB`, `1`) → broadcast, `chB` channels | no — same asymmetry seen from the other side |

So the only pairs still swappable are the diagonal ones, where both sides map `c → c` and the assertion is
true by construction. **A leg written as "swap and compare" becomes a tautology.** The concrete reds when
item 09 landed (2026-11-02, both line numbers pre-amendment): `ChannelMapTests.cpp:474`'s `(1, 2) → 2 outputs`
pin and the leg's own domain guard at `:292` (`resolve(1, 4, 4).status == ok`), whose companion at `:293`
stayed green — which is exactly how a half-dead leg looks on the console.

**What follows for the milestone.** M4 §1's "A-mono with B-stereo equals B-stereo with A-mono" is superseded,
and the reason is that A and B are not interchangeable roles: A is the track's own signal, B is the sidechain,
and a rule may legitimately key on that distinction. M4 §7's commutativity row has to be restated as either a
structural property of the equal-count case — stated as trivial, so nobody reads it as a pass — or dropped,
leaving render-level symmetry out of the acceptance set entirely. Do not re-add commutativity as a
discriminator for anything in this file's history: F-012 already measured that a modulo map commutes cleanly,
so it discriminated nothing even when its domain was wide.

**Landed 2026-11-02 as the restatement, not the deletion** (item 09, §11 records the choice). The leg is
named `"commutativity survives only on the equal-count diagonal, where it is trivial (F-013)"`, checks the 24
diagonal triples that fit under the ceiling, and asserts per slot that both sides read index `c` — which is
the proof that the swap it then performs is a tautology rather than a finding. It also asserts the two
families that lost their partner (`(4, 1)` → 4 channels, `(1, 4)` → 1) side by side, so the shape difference
that kills the relation is visible in the test. The old leg's wide-domain guard, `resolve(1, 4, 4)` was `ok`,
is inverted into the positive claim that such a request is now `countMismatch`.

**The general rule.** A property that holds only while the rule is symmetric in its two arguments cannot
survive as an acceptance criterion once the rule is keyed to which argument plays which role. When a decision
breaks that symmetry, check which criteria still have a *domain* before carrying any of them into the next
item — a criterion whose test set has quietly become the diagonal still reports green, and green is what makes
it dangerous.

## F-014 — A buffer sized by the processed count and indexed by the source count: a live overflow that 158 green legs could not see

**Found 2026-11-03** while writing `plan/M4-impl-plan/02-broadcast-resolution.md`, and found by sanitizer
rather than by reading: `HopScheduler::prepare` allocated `hopBufferB` with `hop * config.channels` — the A-side
width — while `process` wrote it with `hop * config.sidechainChannels`, one slice per B ring. Any layout where
the sidechain is wider than the main input wrote past the end of that allocation, into whatever the heap handed
out next.

```text
WRITE of size 4 at 0x511000000280 thread T0
    #1 container::CircularBuffer<float>::read(float*, unsigned long)
    #2 engine::HopScheduler::process(...) src/Engine/HopScheduler.cpp:149
```

The layout is not exotic. `isBusesLayoutSupported` gated on `mainIn == mainOut` and on capacity, never on the
relation between the two input buses, so a mono main with a stereo sidechain — a normal thing to route — was
accepted, configured, and run. Nothing in 158 test cases touched it, because every leg that drives a sidechain
used matching widths: the value tests were asserting arithmetic on a buffer whose *extent* nothing asserted.

**Why it survived, and what that says about the suite.** Every leg in the file checked a *number*. A wrong
extent is not a wrong number — the samples are read and written correctly inside the slice, and the overrun
lands in slack that `std::vector` growth or the allocator's rounding may have supplied anyway, at a size where
the run stays silent and the next `free` decides. Two consequences worth keeping:

* **A value-level suite gives no evidence about memory safety, and no number of green cases buys any.** The
  M4 rule that a component's extents are derived in `prepare` from one place is what makes this checkable at
  all; the check is a sanitizer run over the shapes the rule permits, not another assertion on the samples.
* **Derive the extent and the index from the same source value.** The fix is not a bound — bounding the write
  would have hidden a wrong answer — it is that sizing is per *source* on the way in (`chA` A rings, `chB` B
  rings) and per *processed* channel on the way out, with `prepare` throwing on a map whose index names a ring
  that does not exist. The consolidation family is exactly the case where the two counts differ, which is why
  a rule about *levels* (D-018) turned into a memory-safety finding.

**Reproduced and then closed by item 02** (2026-11-04): `src/Engine/HopScheduler.cpp` sizes by source and points
by processed count, and the ASan/UBSan repro over `(1,2,1)`, `(1,8,1)`, `(2,1,2)`, `(2,2,2)`, `(2,0,2)` exits 0
with nothing on stderr. Re-running the arm with the old sizing reproduces the fault above. The plan's §5 arm 7
predicted "no value leg can see it"; measured, glibc aborts four legs on this host — but that is a crash
falling out of allocator state, not a test that would fail, and the sanitizer leg is the one that names the
buffer and the line.

---

## F-015 — JUCE asks the gate two different questions, and only one of them is the layout the host asked for

**Found 2026-11-04** measuring `OfflineRender` §5.18 for M4 item 03, while checking whether a refusal leaves the
plugin where it started. It does — but only on the path a host is supposed to use. The two entry points into
`isBusesLayoutSupported` behave differently, and the difference is not in the plugin.

**The clean path.** `AudioProcessor::setBusesLayout` → `canApplyBusesLayout`, which defaults to
`isBusesLayoutSupported(layouts, /*allowReplacing=*/false)` (`juce_AudioProcessor.cpp:118-131`; the virtual is
declared const at `juce_AudioProcessor.h:1465`). Our three counts reach the gate exactly as the host wrote them,
and a `false` applies nothing: measured over 10 refusal shapes — wide main, unequal mains, `chB` 3…9 on a stereo
main, `chB` 9 on a mono main, a 0-channel main — 0 layouts applied, 0 buses moved, the layout identical after
each attempt. Bus *count* changes are checked before the virtual (`checkBusesLayoutSupported`, `:222-227`), and
`canAddBus`/`canRemoveBus` default to false, so the inventory is fixed at 2 in / 1 out and a third bus never
reaches the gate.

**The substituting path.** `AudioProcessor::Bus::setLayout` (`:196-206`) never asks the whole question. It calls
`getBusesLayoutForLayoutChangeOfBus` → `getNextBestLayout` (`:231-330`), whose ladder is: the request as-is; the
*opposite* bus set to the requested width; the opposite bus set to its default; all buses set to the request;
then whichever of default and current is nearer. Whatever that returns is applied — **without
`canApplyBusesLayout` being consulted again**, because the rung that produced it is JUCE's own. So a per-bus
probe can move a bus the host never mentioned. Measured from a mono origin: setting the main input to stereo is
"honoured" and lands on `(2, 0, 2)` — the main output followed it. Setting the main output to stereo does the
same. Setting the sidechain to stereo lands on `(1, 2, 1)`, which the gate genuinely accepts. 3 of 12 probes were
honoured; 0 landed on a layout the gate refuses — the rungs stay inside the accepted family because every rung
keeps `mainIn == mainOut` — but two of the three honoured probes moved a bus nobody asked about. From a stereo
origin, all 9 wide requests are refused and nothing moves.

**The advertisement depends on where you start.** `Bus::getMaxSupportedChannels` (`:1082-1090`) and
`supportedLayoutWithChannels` (`:1140-1159`) walk widths and ask the gate *with the current layout in place*, so
the same plugin advertises a sidechain maximum of **8 beside a mono main** and **2 beside a stereo main**
(measured on three setups). That is a correct consequence of a rule that keys on the pair (D-018), not a defect.
What is wrong is any statement of the form "the sidechain takes up to 8 channels" with no main-bus qualifier —
including the one in item 03's acceptance criterion 7, corrected in that plan's §11.

**Why it matters here, and the rule that follows.** A gate can only be asked about whole layouts, so it cannot
prevent substitution; it can only guarantee that whatever JUCE lands on is a layout it serves. That guarantee is
now measured (§5.18's substitution table, 12 probes over three origins). For everything downstream — this repo's
harness, any preset loader, any host-facing doc — **drive layouts with `setBusesLayout`**; use the per-bus probe
only to learn what JUCE would substitute, and expect that answer to depend on the layout you are standing in.


## F-016 — A footprint figure that re-evaluates the sizing expression cannot notice the expression changing

**Found 2026-11-05** writing the C1 measurement for M4 item 04, in two places that had been reading as one.

**The documented bound understated the engine by 1.5F.** `EntangleProcessor.h` stated the cap's footprint as
"stereo, 9F + 6 floats of engine buffers plus the transform's 2F scratch, so ~704 KB at the default 8192 and ~5.5 MB
at this cap". The allocation sites say otherwise: two history rings and one accumulator ring per processed channel
totalling 2F (`GrainHistory.cpp:19-21`, `OlaAccumulator.cpp:18-20`), a shared scratch set of six vectors at 6F + 6
(`GrainEngine.cpp:60-65`), the Hann window table at L = F/2 (`Window.cpp:13`), and `JuceFft`'s 2F of in-place scratch
(`JuceFft.cpp:29`) — **10.5F + 6 floats at one processed channel, 12.5F + 6 at two**. At the default L = 8192
(F = 16 384) that is 800 KiB against the comment's 704; at the cap L = 65 536 (F = 131 072) it is 6.25 MiB against
5.5 MB. Measured (`tools/PrepareFootprint` [4]): 812.5 KiB and 6 447 KiB for the stereo engine. The residue over the
*corrected* bound — 7.1 KiB mono / 12.5 KiB stereo at the default, 26.6 / 47.0 KiB at the cap — is `juce::dsp::FFT`'s
own tables plus the three safety stages, and it grows with F, which is the half of the story the comment left out: the
transform's tables are not in the bound and cannot be, because the backend is chosen at runtime (vDSP, IPP, MKL, FFTW
or the fallback). Corrected in the comment, dated, with the exclusion named.

**The harness was printing a copy of the scheduler's arithmetic, not a figure about the scheduler.** §5.18 built
`container::CircularBuffer<float>(EntangleAudioProcessor::maxHostBlockSamples + hop)` and read `capacity()` — a second
copy of `HopScheduler.cpp:53-55` — so changing the scheduler's sizing expression leaves the leg printing the old
number with the suite green. It was not an isolated instance: two harness `ChainConfig`s recomputed
`std::max(kMaxBlock, maxHostBlockSamples)` and a third set `maxHostBlock` to the bare constant where the shell applies
the floor. Four copies of a policy expression, none of them the policy. §5 arm 8 is the mutation that proves the
difference: reverting the hop-scratch sizing to F-014's shape makes `OfflineRender` die of heap corruption inside
`[5.15]` — before the footprint block runs, and without a single leg failing — so the printed figure stays a figure
about the expression, not about what the scheduler allocates. Only the sanitizer tree names the overrun
(`CircularBuffer::copyOut`, `(1, 8, 1)`), and only a byte-counting program that drives the production sizing says
what the layout actually costs.

**The rule that follows.** A printed figure about a production expression either **calls the production function** or
says out loud that it is a restatement and points at the measurement. Measuring instead of restating costs one binary
with a replaced global `operator new`, and that is also why it cannot be folded into anything that exists: one
`operator new` per binary, so `PrepareFootprint` is its own target and `OfflineRender` stays free of the hook. The
policy needed a name before it could have a leg — `maxHostBlockFor` is why §5.19 can sweep the floor without
allocating a plugin, and why §5.18's block now routes through the shell and calls itself restated.

## F-017 — A leg whose expected output is produced by the component under test cannot witness a mutation inside it

**Found 2026-11-06** running the discriminator arms of M4 item 06 (`OfflineRender` §5.20, the channel matrix), and
it is a harness finding rather than a product one — which is why it reads F-016's shape from the other direction.

**Two arms that should have been caught at render level and were not.** Arm 6 replaces the scheduler's
`hopA[slot] = hopBufferA + map.a[slot] * hopSpan` with `slot`; the matrix printed **175 `ok` and 0 FAIL**, and the
only red in the whole suite was `EngineTests` #61 (261 assertions). Arm 2 resolves a broadcast B slot to silence
instead of its source, and the two broadcast cells — `(1,1,1)` and `(2,1,2)`, the layouts the same item was written
to hear for the first time — stayed green while the consolidated cells reddened. The reason is the same in both
cases and it is structural: every model §5.20 compares against (`renderOf`, `slotModel`, `monoPairModel`) reaches
the chain through `renderReference`, which builds **its own `engine::HopScheduler`**. A mutation inside the
scheduler moves the render and its expectation together, so `max |got − want|` stays `0.000e+00` and a bit-exact
verdict prints next to wrong audio. The leg is not weak because its tolerance is loose; it is weak because the
thing it measures and the thing it predicts share the code under test.

**The fix is a second, model-free half on the same leg.** M4 and M5 now require a mapped slot to carry a sample in
absolute terms (`deadMapped`, counted as `!(slotPeak > 0.0)` and printed as `%d dead`) *before* the comparison to
its model decides anything. With it, arm 2 reddens **37 lines of §5.20** including both broadcast cells. Silence is
already asserted absolutely — an unmapped output is required to be exactly `0.0f` — and that half caught arm 4 (the
fold dropping its last source) at 10 lines without any help from the new requirement, which is the sign the two
halves are doing different jobs.

**The harness crash that was really the same finding.** Arm 7 (`applyLayout` ignores `bus.sideSet`) and its
matrix-local twin 7b (every cell applies `kStereoWithB` while keeping its own literal expectations) both died with
SIGSEGV, 7b inside `checkChannelMatrix` — gdb frame #0, with the log ending mid-§5.19 only because stdout is
block-buffered and that is where the last flush landed. The cause is the mirror of F-014's: M1 filled its callback
from `getChannelCountOfBus`, the *applied* counts, while its streams are sized by the cell's, so `(1,0,1)` read
`b[0]` on an empty `Stream`. A leg whose whole job is to *report* a layout that is not the one it declares must not
be the first thing to dereference it. M1's fills are now bounded by the streams and the five sample legs run only
after M1 passes; the same mutant prints **9 M1 FAIL lines**, `(2,2,2)` — whose declared layout is the injected one —
runs all six legs green, and nothing crashes.

**The rule that follows.** Any leg whose expectation is computed rather than written down has to say which of its
assertions survive the expectation being wrong, and at least one must be absolute: a non-zero sample, an exact
`0.0f`, a byte count, a name. A comparison against a model is a statement about the difference between two paths,
and a mutation the two paths share is invisible to it at every tolerance — the more shared machinery the harness
reuses for convenience, the more of the product it has quietly placed above test. Reusing the chain is still right
(the alternative is a second implementation to keep in sync, which is what F-016's restated arithmetic was);
what was wrong was treating the comparison as the whole leg.

---

## F-018 — JUCE claims a message thread for whoever constructs one, calls a disabled bus `Discrete #0`, and a prepare-snapshot report can never name a refusal

Measured 2026-11-08 building the item 08 instrument — three facts that each changed how the code or a leg had to be
written, plus one that only changes what a future reader will otherwise mistake for a bug.

* **`MessageManager`'s constructor records the thread that built it** (`messageThreadId (Thread::getCurrentThreadId())`,
  juce_MessageManager.cpp:38), so `initialiseJuce_GUI()` — and therefore `ScopedJuceInitialiser_GUI` — makes that
  thread the message thread without any call to `setCurrentThreadAsMessageThread()`, which exists in exactly one place
  in JUCE: the plugin client (juce_audio_plugin_client_VST3.cpp:280). Two consequences, both paid for. (i) A console
  harness that initialises JUCE's GUI satisfies D-023 C's guard on `main()`, so the report line went out on **every**
  prepare and interleaved itself into all twenty of `OfflineRender`'s earlier sections — C7's byte comparison needed a
  logger installed in `main()` that drops report lines, replaced inside §5.21 by one that counts them. (ii) The
  negative branch cannot be rehearsed by "doing nothing"; it takes a real second thread, which is what C5 now forks.
  For a host this cuts the other way: the wrapper designates its own thread, so a host that prepares on the audio
  thread shows no log line anywhere — H0's whole question, and the reason the readout is a delivery rather than a
  convenience.
* **`AudioChannelSet` has no `getName()`** — the plan's word — and `disabled().getDescription()` returns
  **`Discrete #0`**, because `getDescription()` tests `isDiscreteLayout()` before comparing against `disabled()`
  (juce_AudioChannelSet.cpp:424). A report line saying `side=0 sideName=[Discrete #0] sideEn=0` is therefore JUCE
  saying *there is no send*, and the count with the enable flag is what a reader must trust, not the words. Pinned as
  §5.21 C1b so the fidelity survives: the tempting "fix" is to print `Disabled`, which loses the verbatim property P5
  depends on.
* **The naming table for the eleven shapes the report can carry**, verbatim from this JUCE:
  `0:Discrete #0 1:Mono 2:Stereo 4:Quadraphonic 4:1st Order Ambisonics 6:5.1 Surround 8:7.1 Surround 8:Octagonal
  8:Discrete #8 9:2nd Order Ambisonics 9:Discrete #9`. Width 4 is shared by quadraphonic and first-order ambisonic and
  width 8 by three shapes, so the name distinguishes them while `processed` does not — which is exactly the treatment
  question M4 §6 leaves open, now with the words attached. What a host still owes is whether it routes those shapes to
  a plugin at all.
* **A report assembled at prepare can never carry a refusal word.** `configureRings` runs only for a layout that was
  applied, and §5.18 measured 0 of 192 probes landing on a refused one, because `isBusesLayoutSupported` refuses
  before JUCE builds anything. So `gate=mainWidth` and `gate=channelRule` are unreachable in a pasted line, plan `08`
  C1's "refusal sample" clause is unachievable as written, and the vocabulary is pinned where it *is* observable:
  §5.18's probe table and C3's `gateLayout` comparisons, with the counters as the evidence that the attempt happened.
  Same reason for §5 arm 2 being half an equivalent mutant: "publish a processed count for a refused triple" cannot be
  reached from a console; "rebuild on an arrival that changed nothing" can, and that is what reddens it.

**And one number to stop re-investigating.** `OfflineRender` under LeakSanitizer exits 1 reporting **14.48 MB across
161 540 allocations in 297 records, every one** rooted in `juce::dsp::FFT`'s FFTW backend — `FFTWImpl::create` at
juce_FFT.cpp:84, under `fft::JuceFft::Impl`, reached from `configureRings`. 274 records (12.0 MB) come from legs that
predate item 08 and 23 (1.68 MB) from its own fresh-instance prepares: one cached plan set per transform, never
returned to the OS. No invalid access, no UB, and the report's own allocation appears nowhere in the report. Read it
as JUCE-plus-FFTW global planning state on Linux, not as product leakage; whether the vDSP backend on macOS behaves
the same is unmeasured here.
