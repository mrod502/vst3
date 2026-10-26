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
| F-001 | JUCE 9 latency plumbing | finding |
| F-002 | `/tmp` was noexec and broke juceaide | finding |
| F-003 | JUCE real-only FFT: per-engine scaling, locking fallback | finding |

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
`./build/src/FftJuce/tests/FftJuceTests "JuceFft calibrated gains*"` — or
`ctest --test-dir build -R "calibrated gains" -V`. Both print one
`JuceFft F=<n> forwardGain=<g> inverseGain=<h>` line per size, pass or fail.

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
