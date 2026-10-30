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
| D-015 | Normalisation target: geometric-mean RMS of A and B, smoothed envelope as driver, UI floor with unity below it | accepted (M3) — **decided, not yet implemented** |
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

**Status first.** The decision was taken by the project owner on 2026-10-30. **Nothing in `src/`
implements it yet** — `plan/M3-impl-plan/04-level-normalizer.md` is written against it and its §2 row is
`Not started`. Every consequence below is arithmetic, not measurement: no figure in this entry was
measured, and the implementation item owes the numbers.

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
