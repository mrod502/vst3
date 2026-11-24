# Decision log

One entry per decision that constrains future work, plus the findings that are not decisions but cost hours to
rediscover. Entry format: **Status**, then **Decision** (the rule as it stands today), the **Figures** that
constrain future work, **Revisit trigger**. Findings carry **Status**, the fact, and what to do about it. A
superseded rule is not restated: the surviving rule says what applies, and the Status line names what replaced it.

| Id | Subject | Status |
|---|---|---|
| D-001 | Bus layout: main + sidechain input buses | Accepted (M1) |
| D-002 | Missing-B behaviour: silence, state exposed | Accepted (M1) |
| D-003 | Hop size set and default | Accepted (M1) |
| D-004 | Latency policy: scheduler pre-fill | Accepted (M1) |
| D-005 | Test framework: pinned Catch2, own target | Accepted (M1) |
| D-006 | Transform backend: `IFft` + calibrated JUCE adapter | Accepted (M2); both engines measured |
| D-007 | A build tree is `$(BUILD_ROOT)/<config>`, `build/<config>` on the host | Accepted (M2); container location moved by D-028 |
| D-008 | Grain-length set, the (L, H) bounds, measured cost | Accepted (M2) |
| D-009 | Interim unity dry + wet sum | Superseded by D-010; peak figures live |
| D-010 | The mix law: equal power, endpoint snapping, unity, the pin | Accepted (M3); dry bus and dial domain amended by D-038 |
| D-011 | Parameters and the sample-domain ramp | Accepted (M3); blend's range and default amended by D-038 |
| D-012 | Output safety stage: memoryless tanh, knee 0 dBFS, +6 dB | Accepted (M3) |
| D-013 | DC blocker on the convolved path: 5 Hz Butterworth, wet only | Accepted (M3) |
| D-014 | Level detector: measurement only, two-stage envelope | Accepted (M3); class and figures stand |
| D-015 | Normalisation target: geometric mean, envelope, UI floor | Accepted (M3); implemented by D-016 |
| D-016 | Level normalizer: chain position, gate, values, non-finite rule | Accepted (M3); measured |
| D-017 | The channel rule: broadcast, asymmetric rejected, ceiling 8 | Accepted (M4); mono-main row superseded by D-018 |
| D-018 | Mono main consolidates the sidechain to a mono mean | Accepted (M4); fold measured at +1.558 dB |
| D-019 | Shared-spectrum optimisation: deferred, the seam already fits | Accepted (M4); no code change |
| D-020 | Independent per-channel normalisation stays | Accepted (M4); open behind a measurement |
| D-021 | The layout gate: two rules above `resolve`, refusals named | Accepted (M4); measured |
| D-022 | Prepare-time sizing: named floor, seven-field key, measured footprint | Accepted (M4); measured |
| D-023 | The layout report: one line naming what the plugin was told | Accepted (M4); host rows still open |
| D-024 | Five colour tokens, seven type roles, states by ink or geometry | Accepted (M10); measured |
| D-025 | The design's own face ships embedded; no family name in the product | Accepted (M10), amended M11 — face identity now D-037 |
| D-026 | The editor is resizable; the sheet scales as one sheet | Accepted (M10); measured |
| D-027 | Display publication: an SPSC ring injected at the engine seam | Accepted (M10); measured |
| D-028 | The container's build volume mounts outside the export | Accepted (M10); fixes F-028 |
| D-031 | Text fits by measured deficit: widen, squeeze or reword, per word | Accepted (M11); user rulings; closes A-1 |
| D-037 | Clash Display vendored under the FFL; ramp re-fit to the old cap | Accepted (M11); licence read, not answered by ITF |
| D-038 | Blend on `[0, 1]`: 0 dry A, 1 the convolution; dry B out of the sum; the datum off the sheet | Accepted (M11); measured, 16 arms run |
| F-001 | JUCE 9 latency plumbing | Finding |
| F-002 | `/tmp` was noexec and broke juceaide | Fixed; the `o:` caveat stands |
| F-003 | JUCE real-only FFT: per-engine scaling, locking fallback | Finding |
| F-004 | A spectral product needs no extra factor | Finding; documentation rule |
| F-005 | The OLA order *is* the latency model | Finding; pinned by tests |
| F-006 | `HopScheduler` cannot drive `R = 0` at `hostBlock < H` | Finding; open |
| F-007 | Per-callback cost: hop-boundary spikes, not load | Finding; absolute µs host-specific |
| F-008 | The default configure emitted no `-O` flag | Fixed; asserts kept per-target |
| F-009 | `libFftJuce.a` holds the JUCE closure; non-PIC fails at the shared link | Fixed project-wide |
| F-010 | A non-dyadic `NormalisableRange` cannot express its own default | Worked around in D-011 |
| F-011 | The delta anchor is DC-heavy, so a filter un-pins the gain legs | Finding; chain variants |
| F-012 | Four channel-map mutation predictions that did not survive the run | Finding |
| F-013 | A rule keyed to the main bus leaves commutativity no domain | Finding; leg restated |
| F-014 | Sized by the processed count, indexed by the source count: a live overflow | Fixed; sanitizer leg now mandatory |
| F-015 | JUCE asks the gate two questions; per-bus setters substitute | Finding; drive `setBusesLayout` |
| F-016 | A footprint figure that re-evaluates the sizing expression | Fixed; one named floor |
| F-017 | A leg whose expectation is produced by the component under test | Finding; absolute half mandatory |
| F-018 | Message thread claimed by construction; `Discrete #0`; no refusal in a report | Finding |
| F-019 | `-O3` deletes an untouched `new`/`delete`, so a counter can read zero | Finding; six shims unaudited |
| F-020 | A static archive lets a recompiled shell TU replace the plugin's | Finding; build-file rule |
| F-021 | A plan-invariance suite cannot be the length guard | Finding; binds the long run set |
| F-022 | `LookAndFeel_V4`'s nine slots; JUCE 9 resolves no typeface for a named family | Finding; font half ended by D-025 |
| F-023 | JUCE 9 paint hooks are on the widget; the rasteriser allocates in every paint | Finding |
| F-024 | Installing the design's own face moved four layout metrics | Finding |
| F-025 | Widget-before-attachment reads 0.0; a peerless repaint never paints | Finding |
| F-026 | Two display legs pass without their guard: the ring's mid-copy tag, the shell's frozen window | Finding |
| F-027 | A header-writing `operator new` shim aborts under Mach-O; the byte legs are Linux-only | Fixed same day; five pass-through shims unaffected |
| F-028 | A mountpoint inside the export carries `deny delete`, so the host cannot `rm build/` | Fixed by D-028 |
| F-030 | A contemporary family under JUCE: the name pair, no `tnum`, the line box is not the em | Finding; from D-037 |
| F-031 | Four UI legs gated box positions and printed a verdict on the words | Finding; closed by D-031 |
| F-032 | A quantum guard that reads a parameter through its own constant sees nothing | Finding; from D-038 |
| F-033 | A Bundle-only generator expression in POST_BUILD stops the project configuring on Linux | Finding; guarded `if(APPLE)` |

---

## D-001 — Bus layout

**Status:** Accepted (M1).

**Decision.** Two input buses — main = A, sidechain = B — and one output bus. No channel-splitting fallback: a
host that cannot offer an extra bus never presents B, and the plugin degrades to silence (D-002) rather than
silently reinterpret a channel pair.

**Why.** `getBusBuffer` is pure pointer arithmetic from `getChannelIndexInProcessBlockBuffer`
(`juce_AudioProcessor.h:672`), so reading bus 1 the host did not allocate reads outside the bus region — the
enabled check is load-bearing, not defensive.

**Revisit trigger.** A host that reports a sidechain bus it never activates: the "B not connected" state then needs
a distinct reason code.

## D-002 — Missing-B behaviour

**Status:** Accepted (M1).

**Decision.** An absent, disabled or silent sidechain is treated as silence, filled at scheduler ingest, and the
state is exposed so the UI can say "B not connected". The processor caches bus 1's enabled state off the audio
thread (`prepareToPlay`, `layoutChanged`) and `processBlock` never reads the bus-1 buffer when that cache says
inactive. The hop clock advances from the A ring once B has been filled, so a disabled bus cannot stall it.

**Revisit trigger.** A self-transform mode (a user-selected mode, never a fallback), or a host that enables the bus
and leaves it silent if the UI must tell the two apart.

## D-003 — Hop size set and default

**Status:** Accepted (M1).

**Decision.** Non-automatable choice parameter over `{64, 128, 256, 512}`, default 128. A change is handled on the
message thread inside `suspendProcessing(true/false)` and re-prepares the scheduler and the reported latency;
nothing reallocates on the audio thread.

**Revisit trigger.** A host that automates it anyway and for which the suspend round-trip proves too jittery — then
the parameter is disabled during transport.

## D-004 — Latency policy and where the delay is realised

**Status:** Accepted (M1).

**Decision.** The scheduler pre-fills each output ring with `latencySamples()` zeros in `HopScheduler::prepare`,
which is what makes "samples out == samples in" hold for every callback, including `numSamples == 0`. Padding a
short read to look complete is forbidden — it changes latency silently. Engines must not add delay beyond what
`latencySamples()` declares, so `PassthroughEngine`'s whole `L`-sample delay is the pre-fill and the end-to-end
delay is exactly `L` for any `L`, including one that is not a multiple of `H`. `EngineConfig::latencySamples` is
the single source of the number, applied through `setLatencySamples` from `prepareToPlay` and the hop-parameter
callback — never from `processBlock`, never a literal.

**Figures.** The declaration and the pre-fill are checked independently: the dry component of an impulse lands at
exactly `n0 + R`, and `getLatencySamples() == hop` for all 28 (`L`, `H`) pairs the UI can express. The wet response
is asserted at `n0 + R + L`. `latencySamples` is still the `hop` placeholder because the representation is parked
(F-006).

**Revisit trigger.** An engine whose latency is not a pure integer pre-fill (look-ahead outliving one hop). A first
non-zero sample measures the *physical* delay, not the declared latency, so it is never evidence about the wet
stream.

## D-005 — Test framework and how it is obtained

**Status:** Accepted (M1).

**Decision.** Catch2 pinned to tag `v3.7.1`, fetched and compiled from `extras/catch_amalgamated.cpp` into a
project-local target `EntangleCatch2`, with `catch_discover_tests` from the fetched `extras/` CMake modules. JUCE
`UnitTest` was rejected for the JUCE-light libraries; the submodule's `Catch2` target names are avoided (AGENTS §10).

## D-006 — Transform backend

**Status:** Accepted (M2); both shipped engines measured.

**Decision.** `fft::IFft` (`src/Fft/Fft.hpp`) with `fft::Dft`, a naive `O(F²)` oracle that defines what correct
means, and `fft::JuceFft`, one adapter over `juce::dsp::FFT`. The header owns the contract — bins `[0, F/2]`
interleaved re/im as `F + 2` floats, forward unnormalised, inverse carrying `1/F` — and the adapter imposes it on
whatever engine it got by calibrating in its constructor (`forwardGain`, `inverseGain`, both divided out and
exposed so a test reads them). `JUCE_DSP_USE_SHARED_FFTW=1` is set on non-Apple platforms only and **PUBLIC**,
because `juce_add_module` attaches the module `.cpp` as INTERFACE sources — a PRIVATE define would compile
`FFTWImpl` with two layouts in one link.

**Why.** The calibration turns a gain-shifting engine into a red test, where 6 dB of level drift is not. It does
**not** identify the engine: vDSP, FFTW and the fallback all measure 1.0, and JUCE picks the engine at runtime with
an FFTW `dlopen` miss falling through with no diagnostic (`juce_FFT.cpp:60-66`, `:615-623`). What excludes the
fallback on macOS is a closed engine list — `JUCE_DSP_USE_SHARED_FFTW` is set under `if(NOT APPLE)`, MKL and IPP
default off, and `AppleFFT::create()` has no failure path.

**Figures.** `forwardGain == inverseGain == 1.0` at `F ∈ {64, 256, 1024, 4096}` on FFTW and vDSP — legs numerically
identical. `JuceFft` matches `Dft` bin for bin; round-trip within `1e-5`; DC/Nyquist imaginary exactly zero; zero
allocations inside `forward`/`inverse`.

**Licence.** This pulls FFTW (GPLv2-or-later) into every non-Apple build, and a linked plugin is a combined work.
The fallback-only option that avoided it was rejected for a real-time reason (F-003), so the exposure is accepted,
not overlooked. **Blocking before first distribution:** choose between a GPL release, an FFTW commercial licence, or
a fallback-only binary behind a build flag — a one-line change in `src/FftJuce/CMakeLists.txt`. The Apple path never
touches FFTW.

**Revisit trigger.** A new or swapped engine, or a host reporting a `forwardGain()` other than the recorded 1.0. A
non-finite gain or failing round-trip is a real break but says nothing about which engine won (F-003). Engine
identity comes from the closed engine list, never from the gain; `RTLD_NOLOAD` run from a process that never linked
Accelerate reports "no" even where vDSP is the engine; `ctest --output-on-failure` prints nothing for a passing
test, so read a gain by running the program directly or with `-V`.

## D-007 — A build tree is `$(BUILD_ROOT)/<config>`, `build/<config>` on the host

**Status:** Accepted (M2); the container's location moved to D-028.

**Decision.** One spelling, `$(BUILD_ROOT)/<config>`, defaulting to the repo-relative `build/<config>` — so
`build/debug` and `build/release` on the host. `cmd/install [debug|release]` owns the directory→`CMAKE_BUILD_TYPE`
mapping; `make` exposes `BUILD_TYPE` (default `Release`) and `BUILD_ROOT`. Docker passes uid/gid through numerically,
so a tree the container writes inside `/workspace` lands on the host at the container's numeric uid — which is why the
container never runs with the default root (D-028).

**Consequence.** A host `build/release` and the container's `/ct/build/release` are two trees under one command;
cleaning one leaves the other. `configure_file` stays one bind-mount write by design
(`src/Entangle/Entangle.h` is generated into the source tree and tracked; one file is not the hazard).

**Revisit trigger:** a third tree location needs its own named variable, not a second path convention.

## D-008 — Grain-length set, the bounds on the (L, H) pair, and its measured cost

**Status:** Accepted (M2).

**Decision.** The sample list `{1024, 2048, 4096, 8192, 16384, 32768, 65536}`, default **8192** (index 3). Powers of
two because `H | L` then holds for every pair in D-003's set and `F = 2L` exactly; a millisecond dial was rejected
because the numbers move with the sample rate, so a stored index would stop meaning anything — the ms figure is
derived and displayed (`"8192 samples = 170.7 ms"`), never stored.

**Bounds — one rule in `clampGrainLength`, shared by `configureRings` and the editor.**
`L ∈ [2H, min(65536, 128·H)]`, after which `L | H` holds by construction. Floor `2H`: at `H = L` Hann has no overlap
to sum, so the hop-class sum becomes `w[phase]` instead of the `C = L/(2H)` the engine's `1/C` gain assumes.
Ceiling `128·H` is the CPU gate, a bound on the *pair*; with the shipped sets it excludes exactly three combinations
— `L` = 32768 and 65536 at `H` = 128, and 65536 at `H` = 256. The editor prints the reduction rather than hiding it,
because a control that reports the dial position instead of the running value is a control that lies.

**Measured cost — the data the bound sits on.** `tools/CpuBaseline`, `Release` (`-O3`), 48 kHz, R = H, 3000 hops per
row, `processHop` alone, % of one core, hostBlock 32 / 64, 12-core aarch64 Linux (`docs/cpubaseline-m2-item6.log`).

| L | F | H = 128 mono | H = 128 stereo | H = 512 mono | H = 512 stereo | stereo spike µs |
|---|---|---|---|---|---|---|
| 1024 | 2048 | 0.37 / 0.30 | 0.62 / 0.61 | 0.08 / 0.08 | 0.15 / 0.15 | 47 |
| 2048 | 4096 | 0.66 / 0.65 | 1.35 / 1.32 | 0.16 / 0.16 | 0.32 / 0.32 | 65 |
| 4096 | 8192 | 1.34 / 1.34 | 2.69 / 2.68 | 0.33 / 0.33 | 0.66 / 0.66 | 155 |
| 8192 **(default)** | 16384 | 2.87 / 2.83 | 5.66 / 5.58 | 0.70 / 0.70 | 1.40 / 1.40 | 237 |
| 16384 | 32768 | 7.21 / 7.25 | 14.51 / 14.53 | 1.85 / 1.84 | 3.66 / 3.66 | 578 |
| 32768 | 65536 | 15.79 / 15.30 | **30.64 / 30.68** | 3.80 / 3.79 | 7.76 / 7.78 | 1035 |
| 65536 | 131072 | 40.91 / 40.37 | **80.76 / 79.39** | 10.17 / 10.16 | 20.22 / 20.23 | 3353 |

- Stereo is 2× mono (1.94–2.06 above `L` = 2048; the 1.69× at 1024 is cache, not channel sharing), so the 2×
  extrapolations were sound.
- `F log F` holds to `L` = 8192 and breaks above it pessimistically: measured step-ups 2.13×, 2.06×, 2.12× against
  2.18×, 2.17×, 2.15× predicted, then **2.61×** at 8192→16384 and **2.60×** at 32768→65536. The cliff starts one step
  earlier than the model.
- The 25 %-of-one-core gate admits every pair up to `L = 128·H` — worst 20.2 % (`L` = 65536 at H = 512) — and
  excludes the three above it. The default pair costs 5.66 %.
- **The spike, not the mean, is the constraint at the top of the set.** With R a declaration rather than a deferral
  (D-004) a whole hop's FFT must finish inside the boundary callback: at `L` = 65536 the stereo spike is 3.2–3.4 ms,
  busting a 128-sample callback (2.67 ms at 48 kHz) at 20 % mean load; `L` = 16384 at 578 µs still clears a
  32-sample callback. So the admitted `L` = 65536 pair is real-time capable only on a host blocking at 256 or more.

**Footprint.** Stereo at 8192: history `2·L·N` + accumulator `2·L·N` + scratch `≈ 5F` floats ≈ **0.7 MB**, plus the
transform's tables; at the 65536 cap ≈ **5.5 MB**, where the cache cliff lives. All prepare-time.

**Consequence.** Either dial re-prepares through D-003's suspend → re-prepare → resume path. Rings are sized once for
the maximum configuration, so a dial move reallocates the engine and the transform, not the scheduler's rings.

**Revisit trigger:** the transform changes (D-006), moving the constant in front of `F log F`; or a host asks for
`L > 65536`, which is a different memory story. **Host caveat:** these are one machine's numbers, roughly 1.3×
faster per hop than a Ryzen 5950X and 2.2× faster than Apple Silicon at `L` = 2048/H = 512 (17.1 µs here, 22.0
there, 37.9 on Apple). `128` is *this* host's crossing point: the ratio is the rule, the constant a knob to re-measure
in live testing. The spike column is a `max` over 3000 hops on a shared container and carries scheduling noise.

## D-009 — Interim unity dry + wet sum

**Status:** Accepted (M2); superseded as the law by D-010. Its peak figures stay live as the baseline later stages
are compared against.

**Decision.** `engine::DryWetEngine`, a decorator over any `Engine` that calls `inner->processHop(a, b, scratch, n)`
into its own hop scratch and writes `out = scratch + a`, forwarding `prepare`, `reset`, `latencySamples()` and
`wetDelaySamples()` untouched. A pure-wet shell renders silence whenever there is no sidechain — forever on a host
that cannot offer a second bus — and the interim sum keeps that configuration audible without inventing the law.
- **The dry path is not a delay line.** The pre-fill already delays everything the scheduler drains by R (D-004), so
  an add inside `processHop` reaches the host at the same `+R`, bit-exactly, with no allocation and no extra declared
  latency. Writing it in the shell would be wrong as well as redundant: `processBlock`'s main output aliases its main
  input, so reading A after `scheduler.process()` reads the engine's output.
- **Headroom is a live number.** At the default configuration (H = 128, L = 8192, R = 128, stereo, ±0.4 input): peak
  **2.012** independent, **2.916** with `A == B`; at full scale **11.23** and **16.77**. Nothing is scaled or clamped
  to make those fit. The trim that would put the correlated case at unity is **1/2.916 = −9.3 dB on the summed
  output** — recorded, not coded.

**Revisit trigger.** The mix law (D-010, landed). Earlier if a host reports clipping where the passthrough did not
clip — expected on the figures above, which is why they are recorded.

## D-010 — The mix law: equal-power blend, endpoint snapping, unity, and the no-sidechain pin

**Status:** Accepted (M3). Amended by **D-038**: the dial addresses `p ∈ [-1, 0]`, so the dry weight always applies
to A and the `p > 0` rows below are the law's own arithmetic, not a state the product can reach. The equal-power
curve, the three snapped rows, unity and the pin are unchanged.

**Decision.** Equal power, snapped at three positions, at unity:

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

`g` the output-gain parameter (0 dB exactly `1.0f`, D-011), `β` the effect coefficient (1 in the effect, 0 bypassed
onto A).

- **The three exact rows are values, not a tolerance.** `cosf(pi/2f) == -4.371138829e-08`, so a literal law leaves
  `conv × −4.37e-08` in a sum that must be bit-exact at the endpoints; without snapping every endpoint leg goes red
  at both ±0.4 and full scale and in both the independent and the `A == B` pairing — what decides it is the size of
  the *sample* the residue lands on, not the level of the render. Snapping costs three comparisons once per hop.
- **There is no dry-bus hand-over** (D-038). The dry weight applies to A at every position the shell can ask for, so
  there is no crossing left to make silent. The sign-crossing snap this stage carried was not a neutral guard: it
  forced `p` to `0` for the sample after a crossing, which zeroes the dry contribution for that one sample — and it
  fired on the first hop after `prepare` too, because the ramp snaps while the sign tracker still holds the class's
  initial `0`. Re-running the pre-D-038 fill (M11 `02` arm 2) reddened the bit-exact dry legs at the first sample
  after `R`, so removing it changes bytes for the better rather than changing nothing.
- **Unity, no trim.** The dial's cost at the default configuration, reported rather than gated: peak **1.68**
  independent / **2.54** with `A == B` at ±0.4 input, **10.51** and **15.86** at full scale, worst at `p = 0` —
  pure convolution, which D-038 moved to the top of the dial — where the correlated case sits ~0.5 dB above the
  independent one across the middle of the sweep.
  So the soft clipper (D-012) is engaged at default settings and a ±2 dB output-flatness target is unreachable at
  unity, because A and conv are partially correlated.
- **The law adds no gain and no delay to the wet path.** The engine's 1/C remains the whole wet gain table; the
  `L − R` offset between dry and wet is D-004's, not this law's to close.
- **With the sidechain bus disabled the effective `p` is −1**, whatever the dial says — otherwise the plugin is silent
  at its own default on any host without a second bus. The dial keeps its value and a prepare that sees a sidechain
  restores it, so **a preset saved at full wet renders A in a host without one** — under D-038 that is the shipped
  default, which is why the editor states the pin in words rather than leaving a dial that means nothing. The editor
  reports the effective position, not the dial's.

**Revisit trigger.** Listening says unity is unusable — that reopens the trim, with the dial sweep as the input. The
normalizer's target may move these levels again. If the DC blocker breaks the endpoint legs, the exactness requirement
moves into that stage's contract.
## D-011 — Blend, output gain and bypass: the parameter layer and the sample-domain ramp

**Status:** Accepted (M3). Amended by **D-038**: the blend parameter is `[0, 1]` with default `1`. The ramp, its
times, the read discipline and the other two parameters are unchanged.

**Decision.** The ramp lives inside the stage, not the shell: `juce::SmoothedValue` would put a JUCE header into a
JUCE-light library (AGENTS §9), and a ramp is a function of output samples, which only the class holding the sample
counter knows. The stage takes three targets — `setBlendTarget` in dial units, `setOutputGainTarget` in dB,
`setBypassTarget` as a bool — and ramps each with a straight line in the stored domain per output sample:
**20 ms blend, 20 ms output gain, 10 ms bypass**. The final step *assigns* the target rather than evaluating the line
at progress 1.0, so every endpoint criterion holds once anything has settled. `prepare` and `reset` snap: both drop the
buffered stream and a ramp would otherwise spend its time constant replaying a gesture the transport discarded. The
shell hands targets down in `processBlock`, and again in `configureRings` and in `reset()` *before*
`scheduler.reset()` — that call is what snaps the ramps, so pushing afterwards crossfades through the first 20 ms of
the new transport.
- **Parameters.** Blend `[0, 1]`, default 1, automatable, quantum `1/128` — dyadic on this range as on the old one,
  so `q`, its normalised value and `p = q − 1` all denote exactly (D-038, F-010). Output gain `[-60, +12]` dB,
  default 0, automatable, quantum `0.125` dB. Bypass bool, default false, automatable. All three are appended after the
  two choice parameters, because the choices are positional in a saved preset while the float and bool ones are looked
  up by id.
- **Bypass is a crossfade onto dry A**, on the stream the scheduler already drained. It switches no routing and touches
  no buffer, so it cannot move R; the dial survives a bypass cycle bit for bit.
- **Reads are `apvts.getRawParameterValue(id)->load()`** on the audio thread, never UI state, and the no-sidechain pin
  lives in one function both the DSP path and the editor call, so readout and audio cannot disagree. None of the three
  triggers a re-prepare.

**Figures.** The per-callback one-pole this replaces had τ ≈ 49.5 callbacks, so one gesture took 66 ms at hostBlock 64
and 4.22 s at 4096 — inside this shell's own `maxHostBlockSamples` — and never landed exactly on target. The shipped
ramp is bit-identical through hostBlock 128, 256 and 512, and its click metric `d2(x) = max |x[n] − 2x[n−1] + x[n−2]|`
against the same content rendered static separates it from a stepped known-bad by 15×–39.6× against a limit of 8.0.
Gain matches `10^(dB/20)` bit-identically at −60, −12, 0, +6 and +12 dB.

**Revisit trigger.** The three ramp times are opening values to confirm by ear; a log-domain gain ramp is worth trying
if the bottom of the range feels abrupt.

## D-012 — Output safety stage: a memoryless tanh clip, knee at 0 dBFS, ceiling at +6 dB

**Status:** Accepted (M3).

**Decision.** `dsp::SoftClipper` (`src/Dsp`, JUCE-light) is the last thing the output sees. `knee = 1.0f`,
`ceiling = 2.0f`; above the knee
`y = sign(x) · (knee + (ceiling − knee) · tanh((|x| − knee) / (ceiling − knee)))`. Odd, monotone non-decreasing, C²
across the knee because `tanh''(0) = 0`. Identity-to-a-knee was taken over a whole-range `tanh` (never the identity, so
it rewrites signals a host already handles and kills the bit-exact `p = ±1` endpoints), a hard clamp (a slope step
exactly where an existing mix is busiest, i.e. a click) and lookahead or feedback limiting (the delay it needs is real,
and R is only honest for what the engine defers).
- **The ceiling is above unity, not at it:** identity up to `|x| = 1` plus a continuous first derivative at the knee
  leaves no monotone shape bounded by 1 — the bounded continuation approaches its bound asymptotically, so the bound
  must sit above the value the curve leaves from. +6 dB keeps the stage invisible to anything a host could already
  handle and still catches the sum-overrun the law produces.
- **Memoryless is the contract, not an optimisation:** one sample in, one out, no state — silence is `0.0f` on the
  sample it arrives, output *q* depends on input *q* alone, nothing to latch or drift subnormal, declared latency
  untouched.
- **Non-finite samples become `0.0f` and are counted** — a NaN reaching the sum would propagate through every later hop
  through the wet state; a relaxed atomic counter gives a cross-thread read-out without a lock and nothing reads it
  back into the signal.
- **The ceiling clamp is a class property, not a reachable branch:** `knee + (ceiling − knee) · tanh(u) ≤ ceiling` for
  every finite `u`, because IEEE float `tanh` saturates at exactly `1.0f`. It stays — a different shape function could
  approach from above — and the test asserts the bound rather than the branch.

**Operating points** (same input, same law, neither stage → shipped):

| Input | Peak unshaped → shipped | % above knee | d2 shipped / baseline |
|---|---|---|---|
| 0.4 independent | 1.68163 → 1.57266 | 1.57 % | 4.04159 / 4.06349 = 0.99461 |
| 0.4 A == B | 2.53687 → 1.90587 | 7.29 % | 5.4667 / 6.15304 = 0.888455 |
| 1.0 independent | 10.5102 → 2.0 | 45.4 % | 7.99857 / 25.3968 = 0.314944 |
| 1.0 A == B | 15.8554 → 2.0 | 51.4 % | 7.99998 / 38.4565 = 0.208027 |

Across the dial at amplitude 1.0 the post-clip peak is 2.0 from −0.5 to +0.5, 1.99883 at ±0.75, and 0.99997 at the
endpoints where the above-knee fraction is 0 %; no dial position leaves the chain above the ceiling. The `d2` column is
a ratio against the *unshaped* render, which at amplitude 1.0 is itself the loudest thing in the comparison, so a value
below 1 is not independent evidence of click-safety — that argument is structural (memoryless, C² at the knee) plus the
0 % engagement at the endpoints.

**Revisit trigger.** 45–51 % of samples shaped on full-scale correlated material is the risk realised, not a result to
defend: it is the level stage's decision input, and if the operating point moves the engagement figures must be re-run
before this table is quoted again.

## D-013 — DC blocker on the convolved path: 5 Hz, second-order Butterworth, wet path only

**Status:** Accepted (M3).

**Decision.** `dsp::DcBlocker`, transposed direct form II, `cornerHz = 5.0` as a constructor default so another anchor
or engine can be probed without touching the class. Second-order Butterworth (Q = 1/√2) is the highest Q that is
monotone, −0.0011 dB at eight times the corner; a first-order one-pole sits 0.067 dB down there, and elliptic/Chebyshev
buy transition width with passband ripple. The need is measured: with a 1 % offset on both buses the wet path carries DC
of **0.4096** with the stage out of the chain.
- **Coefficient form:** `b1 = −2·b0`, `b2 = b0`, written as relations, so `b0 + b1 + b2` is exactly zero in double — the
  DC null is a property of the coefficients and the recursion only has to hold what they promise. The residue scales
  linearly with the offset: 3.13e-8 of a full-scale offset, 3.13e-10 at 0.01, 3.13e-11 at 0.001.
- **State is double:** the pole radius is √a2 ≈ 0.99985, and rounding a float state that slow leaves a floor of order
  1e-7 *of the signal*, where the bound asks for 1e-6 *of the offset*.
- **Wet path only:** A and B reach the sum unfiltered, so D-010's bit-exact endpoints survive. Running the blocker over
  the dry buses reddens exactly the endpoint legs and nothing else; running it on nothing reddens the residue, corner,
  DC-on-buses and centre legs. That split is the proof of the placement.
- **Latency honesty:** output *n* depends on input samples ≤ *n* and its own past, so R is unchanged. What it costs is
  *settling*: **34 283 / 37 315 / 74 630 / 149 260 samples** at 44.1 / 48 / 96 / 192 kHz (0.777396 s at the reference
  rate), published through `settlingSamples()`. An in-flight DC figure must be read against that bound: where a
  33 024-sample render ends the DC is −58.06 dB; past the bound it is −232.43 dB.

**The corner, and what it costs** (H = 128, L = 8192, 48 kHz, over the raw anchor response):

| Corner | worst \|Hann − filtered\| | filtered peak | max \|y\| past support |
|---|---|---|---|
| 0.5 Hz | 0.321216 | 0.833155 | 0.308315 |
| 1 Hz | 0.552593 | 0.703238 | 0.485678 |
| 2 Hz | 0.842764 | 0.612171 | 0.551344 |
| **5 Hz (shipped)** | **1.11535** | **0.414009** | **0.0827042** |
| 10 Hz | 1.11024 | 0.160964 | 0.110448 |
| 20 Hz | 1.03926 | 0.0427595 | 0.0392494 |

The window deviation cannot decide it — a Hann of peak 1.0 carries mean 0.5, so removing DC moves the waveform at every
corner (F-011). Against the other two columns there is a knee: 2 Hz buys 3.40 dB of retained peak and costs 6.7× the
ring past the support; 10 Hz buys no shorter ring (0.110 against 0.083) and loses 8.21 dB of peak. What the corner costs
is set by the grain length, because the anchor response *is* a Hann of length `L` whose main lobe spans ±2·sr/L —
±93.8 Hz at L = 1024, ±11.7 Hz at L = 8192, ±1.46 Hz at L = 65536 — with the retained peak following it down (0.797657,
0.414009, 0.0107268). That is a property of the probe, not of program material: on program material the stage takes the
DC and almost nothing else (0.4096 → −232.43 dB settled, render peak 1.80055 → 1.70621 = −0.47 dB). A corner inaudible
at L = 1024 does shape the response at L = 65536, which argues for keeping the corner low, not for dropping the stage.

**Figures.** −3 dB corner realised to 5.00028 / 4.99974 / 5.00013 / 5.00006 Hz across the four rates (≤ 0.006 % off
design); gain at eight times the corner −0.00106 dB (the analytic 10·log10(4096/4097)); worst passband gain above unity
+3.2e-8 dB.

**Revisit trigger.** The level stage's threshold decision may change what the low end needs; the probe prints on every
run, so re-deciding is a read, not an experiment. If a future engine's response carries less DC the stage stays — the
program-material case is not the anchor.

## D-014 — Level detector: measurement only, a two-stage envelope, exact silence

**Status:** Accepted (M3). Its revisit trigger fired and D-015 took the target and the driving reading; the class and
its measured figures stand.

**Decision.** `dsp::LevelDetector`: three readings per channel, all live at once, no target, floor, gate or gain
anywhere in the class — the consumer's decisions stay out of it, and with no consumer nothing would break if a figure
were wrong, which is why each is a measured property of the reading rather than a comparison against a caller.
- **The readings:** `windowedRms` is the square root of the mean square over `windowSamples()`; `windowedPeak` the
  largest magnitude in the same window, kept in a monotone queue so it is O(1) per sample like the RMS; `envelope` the
  follower. Both windowed readings zero-pad before the start, so they settle over exactly one window and a window
  holding one blip reports the blip.
- **Time constants live on the published reading**, both poles `1 − exp(−1/(τ·sr))` on the amplitude, not the power.
  With τ_attack = τ_release the follower is a plain one-pole and unbiased by construction at any crest factor;
  asymmetry, and any bias, exists only when the caller asks for one.
- **The inner stage** runs at `min(τ_attack, τ_release)/100` — derived, not a fourth control, and published through
  `innerTimeConstantMs()` because it bounds how low a tone the envelope can resolve.
- **Why two stages.** A single-state follower of the square, placed where the crossing criterion demands (63.2 % of a
  step at τ_attack, 36.8 % at τ_release, i.e. exponents −0.51012 rising and −2 falling), reads a stationary sine
  **+23.24 % high** while passing the crossing criterion to +0.2 %: the two criteria point in opposite directions. A
  two-coefficient follower settles where α_a·E[(u−p)⁺] = α_r·E[(p−u)⁺], which is not the mean of u, and ripple in its
  *input* is the whole error term. Feeding one follower from the boxcar window was rejected too: a boxcar's area delay
  is W/3 samples, making the attack figure a function of a measurement-window control rather than a time constant. The
  two-stage structure costs step-crossing accuracy — +0.63 % of τ on the attack, +0.21 % on the release, against the
  single-stage variant's +0.2 %/+0.02 % — inside tolerance, and the figures print on every run.
- **Silence is exact, not asymptotic:** the RMS sum is forced to `0.0` the moment a count says the window holds nothing
  but zeros — exact by construction, not thresholded — so the windowed readings reach exact `0.0f` at exactly
  `windowSamples()`. Both envelope stages flush at `envelopeFloor` = 1e-12 (−240 dB) and `silenceSamples()` publishes
  the bound: τ_release·ln(from/floor) + 2·τ_inner = 129 313 samples = 2.694 s at release 100 ms/48 kHz. No state is
  ever subnormal.
- **hostBlock never enters:** every constant is milliseconds realised in `prepare` and the filtering is per sample —
  chunk 1 and chunk 4096 give a bit-identical reading series. The class reads samples and never delays one; it adds
  nothing to R or W.

**Figures.** Windowed RMS worst 0.076 % and envelope worst 0.043 % off A/√2 — against mean-rectifying's 9.97 %, which is
the distinction the tolerance exists to make. Peak deficit 5.96e-06 %. Attack 483 samples against 480, release 4 810
against 4 800. Attack 25.147/25.146/25.156/25.156 ms and release 75.510/75.5/75.5/75.5 ms at
44.1/48/96/192 kHz against 25 and 75 ms.

**Revisit trigger.** The target decision (taken by D-015). If a consumer needs `envelope()` to resolve content whose
period approaches `innerTimeConstantMs()`, the divisor 100 is the number to revisit; its cost is the attack lag above,
and 23.24 % is what happens when it is set to infinity.

## D-015 — Normalisation target: geometric-mean RMS of A and B, smoothed envelope, UI floor with unity below it

**Status:** Accepted (M3); implemented and measured by D-016, which carries the figures.

**Decision.**
- **Target** `target = √(envA · envB)` — the geometric mean of the two RMS readings.
- **Driving reading** — the smoothed envelope, `LevelDetector::envelope()`, on A, on B and on the convolved path. Not
  the windowed per-grain pair, which stays exposed.
- **Silence** — the floor is a **UI parameter**, not a constant, and below it the applied gain is **unity**: the stage
  stops correcting, rather than holding its last value, muting, or running to its range limit.

**Why the three hold each other up.**
1. Zero propagates through a geometric mean: if either bus reads zero the target is zero, so a silent bus would drive
   the correction to its range limit — worse than for an arithmetic mean, which would sit at half.
   Unity-below-the-floor closes it: the gated case is the identity, so a silent bus leaves the wet path at its own
   level and "no noise blow-up" holds by construction rather than by tuning.
2. In the log domain the target is the arithmetic mean of the two readings' dB values, so its sensitivity to either bus
   is exactly ½ at every level — a +6 dB change on A alone moves the target +3 dB. Seen linearly that is the unbounded
   derivative `∂target/∂envB = √envA / (2√envB)` as `envB → 0`. Consequence: floor **each reading** before the mean, in
   the domain where the mean is additive; clamping the resulting product leaves a discontinuity the per-reading floor
   does not.
3. The half-slope is a property of the target, not an error to correct: the wet path is *defined* to track
   `√(envA · envB)`, so a change hitting one bus alone must move the output by half that change. A ±2 dB output
   criterion therefore reads as **normalization error against the target**, not as "output level constant while one bus
   is swept" — only the first is satisfiable. At `p = ±1` the wet path carries zero weight, so the endpoints stay
   bit-exact provided the applied gain is finite at every sample, which unity-below-the-floor also delivers.

**Where conv is measured.** The gain needs conv's level to form `target / envConv`. Measured **upstream of the applied
gain**, on the engine's own output, that is feed-forward. Measured **downstream**, the identical formula is an AGC loop
wrapped around a follower: stable only inside some region of (τ_a, τ_r, slew), winding up on a burst, with a pump period
set by time constants nobody chose for that purpose. The difference is one line of wiring order, and the wrong one is
silent in every test that does not listen.

**Revisit trigger.** Listening. The geometric mean lets a quiet partner drag the result toward the floor, and the
half-slope means the wet level follows each bus at 6 dB per 12 dB of that bus; if that reads as the effect being shoved
around by the sidechain rather than sitting under it, the alternatives — one input, or a fixed reference — are a
two-line change in the target computation and nothing else here moves. Second trigger: pumping at the edges of silence
means the gate condition and its hysteresis are what to change, not the target.

## D-016 — Level normalizer: where the stage sits, what gates, the derived values, the non-finite rule

**Status:** Accepted (M3); implemented and measured. `dsp::LevelNormalizer` (`src/Dsp`), fed and applied by
`engine::MixEngine` between the DC blocker and the law, installed by `entangle::makeChain` in `ChainVariant::Shipped`,
driven by `normalisationAmount` and `silenceFloor`. D-015 owns the target; this owns the wiring, the gate, the values
and the non-finite rule.

**Decision.**
- **One primitive, three detectors, installed by a setter.** The stage owns three one-hop `LevelDetector`s — A, B, conv
  — and does the read/combine/gate/limit/ramp arithmetic; `MixEngine` still decides what runs and in what order
  (D-012's rule). Installation is `installLevelNormalizer` rather than a fourth constructor argument because the stage
  runs *before* the last of the two stages the constructor already orders, and a positional
  `(blocker, clipper, normalizer)` would say the opposite of what the chain does. It asserts a not-yet-prepared engine:
  the hop it ramps over is sized in `prepare`.
- **Insertion point: after the blocker, before the law** — grain engine → block conv → normalise conv → law → gain →
  clip. Reading DC-carrying conv inflates `envConv` and therefore the correction (D-013's reason for the blocker's
  place), and scaling before the law is what leaves the `p = ±1` endpoints bit-exact, because `coefWet` is exactly zero
  there and `0 × finite = 0`.
- **Feed-forward, measured on the sample the stage has not yet scaled:** `processChannel` reads the hop before scaling
  it in place, so `envConv` is the engine's own output level and the loop is open; the alternative is one line of
  ordering away (D-015). The leg asserts the stage's conv reading bit-equals a bare `LevelDetector` driven with the same
  stream while the correction runs, with a negative control on the scaled stream.
- **The gate runs before the arithmetic, on any of the three readings:** `gain = 1.0f` exactly when `envA`, `envB` or
  `envConv` is at or below the floor. Everything downstream then works on readings strictly above it, so the geometric
  mean needs no per-reading clamp and the ratio no divide guard. The gate is separate from the floor control: a loud A
  over a floored B yields unity, not the `√(envA · F)` boost a clamp-only design would apply.
- **`amount` is an exponent, and the applied gain is a line across the hop:** `gain = (target/envConv)^amount`, so
  amount 0 is bit-exact unity at every sample and the dial meets the gate at the same value. The hop is scaled along a
  straight line from the previous hop's gain with the **last sample assigned**, not evaluated at progress 1.0 (D-011's
  float fact).
- **A non-finite reading degrades to unity and is counted, not sanitised.** `LevelDetector` has no non-finite rule and
  none was added: a NaN poisons its state, which is what its contract implies. The stage checks the *computed* gain once
  per hop per channel; not finite ⇒ that hop's gain is `1.0f` and `nonFiniteHops()` increments, mirroring
  `SoftClipper::nonFiniteSamples()` as a diagnostic nothing synchronises on. The degradation is permanent for that
  stream and deliberate: unity and uncorrected beats silence, NaN, or a per-sample `isfinite` on three streams.

**The values, and what each was derived against.**

| Control | Value | Basis |
|---|---|---|
| Gain range | `[−36, +36]` dB | D-008's wet response peaks span 37.45 dB across the shipped L range, so ±36 dB covers it to within 1.45 dB and still leaves the ±2 dB gate reachable |
| Slew limit | The full 72 dB range in **200 ms** (360 dB/s), as `360·H/sampleRate` dB per hop | A per-hop cap is a unit error — 8× slower in time at H = 512 than at H = 64, and the shipped hop set spans exactly that |
| Detector τ | attack 20 ms, release 100 ms, all three buses | 20 ms matches D-011's ramps; 100 ms is the anti-pumping lean |
| Amount dial | `[0, 1]` at `1/128`, **default 1.0** | F-010's dyadic quantum; default fully normalising, because the correction is the feature |
| Floor dial | `[−90, −20]` dBFS at `0.125` dB, default `−60` | Dyadic step with both endpoints exactly expressible; 180 dB above `envelopeFloor` = 1e-12, so the two silence rules cannot interact |

**Figures.** Held level equals the analytic `√(envA · envB)` to −2.4e-07 dB from a wet level 14 dB below it; half-slope
0 / +6 / +6 / +12 dB for (baseline, A +12, B +12, both +12); amount 0.5 measured 6.9897 dB where the power form says
6.9897 and the linear form would say 9.54242. Cap 0.48 / 0.96 / 1.92 / 3.84 dB per hop at H = 64/128/256/512, 72 dB
crossed in 200 / 200 / 202.7 / 202.7 ms; unity again 434.7 ms after the input stops, against the detector's published
2766 ms bound. `latencySamples()` still one hop across the 28 (H, L) pairs; the dial travel's click metric never exceeds
the parked render's (0.728512 … 1.0); zero allocations across the hop path with both controls moving. Worst corrected
deviation **0.376 dB** against the ±2 dB gate, where the no-stage baseline spans **9.5997 dB**.

**Two facts that are documentation, not defects.**
1. *The follower's crest bias cancels in the applied ratio, and only when the buses share ripple content.* D-014's
   asymmetric follower reads a 1 kHz tone 7.8 % (+0.65 dB) high; all three buses are read by the same class, so the bias
   cancels between `target` and `envConv`. A rule column built from `envelope()` instead of the amplitudes puts that
   0.65 dB on every rung and reads as a gain error that is not there. It does **not** cancel when the buses have
   different ripple content — which is what happens when one is much quieter — and there it shows as a level offset.
2. *The L-spread on program material is not D-008's figure.* The 37.45 dB is a delta **response-peak** span; the settled
   RMS across the same L range at fixed H spans **1.39 dB** (bare −24.7915 to −26.1815 dBFS), and it is the hop half of
   `C = L/(2H)` that moves program level (9.17 dB at H = 64, L = 8192). The flatness criterion is a much smaller ask
   than the response-peak number suggests, while the peak figures D-009 and D-012 quote do move: the shipped peaks are
   with-the-stage numbers now (2.53687 → 1.90372 for a correlated pair at 0.4).

**Consequences.** Default amount 1 changes what a fresh instance does, so every peak figure in this file predating the
stage is a without-the-stage number unless the leg says otherwise. The floor gates on the *wet* reading too: at H = 64,
L = 8192 with −40 dBFS in, amount 0 and amount 1 render identically at the shipped −60 dBFS floor and the stage asks for
26 dB once the floor moves to −90 — the dial decides where "nothing is amplified" is.

**Three limits of the suite, worth more than the legs that pass.**
- **The gate carries D-010's endpoint criterion, not just the noise one.** Deleting it left the readings finite (so the
  finiteness rule passes), `log10(0)` reaching `−inf`, the difference of two infinities NaN, and `0 × NaN` NaN even
  where the law's wet coefficient is exactly zero — the clipper's non-finite rule then turned the `p = −1` and
  sidechain-disabled renders to **silence**. Hence the gate before the arithmetic, and the finiteness check on the three
  readings rather than on the computed gain: they are not the same test.
- **The slew cap and the ±36 dB clamp have no chain-level witness** — the harness never drives a gesture fast enough for
  the cap to bind, nor a wet level quiet enough to ask for the clamp. They are proved by the `DspTests` cases that name
  them and by nothing else.
- **The blocker-before-the-stage ordering has no chain-level witness.** Swapping the stages leaves the harness green: the
  blocker still removes DC before the output, and the only consequence — a reading inflated by DC, hence a slightly
  wrong gain — sits far inside the flatness gate. The invariant is real and held by the builder's call order, and is
  witnessed in `MixEngineTests` by a wet stub that puts a DC offset *under* a 1 kHz tone: with the blocker in front the
  applied gain is the tone ratio (**+7.95675 dB** against **7.9588 dB** analytic); with the order swapped the DC is
  inside the reading and the gain inverts to **−7.12531 dB**, a **15.08 dB** separation. Moving the stage past the law,
  by contrast, reddens seven harness legs including the bit-exact endpoints.

**Revisit trigger.** Listening (D-015). If pumping appears at the edges of silence, change the gate and add hysteresis,
not the target. If long-grain settings pump, scale `releaseMs` with L rather than touching the target — at the default
L = 8192 the wet response itself spans 171 ms against a 100 ms release.
## D-017 — The channel rule: broadcast from one channel only, asymmetric rejected, ceiling 8

**Status:** Accepted (M4); implemented and measured as `engine::ChannelMap` (header-only). Consumed by
`HopScheduler` (resolving it into pointers) and, through `engine::gateLayout`, by `isBusesLayoutSupported` (D-021).
**Its `chA == 1 && chB >= 2` row is superseded by D-018**; every other family resolves as measured here.

**Decision.**
- **`N = max(chA, chB)`, and a side broadcasts its channel 0 exactly when it has one channel.** A side with `ch > 1`
  maps output `c` to source `c`. No modulo, no wrap, no upmix.
- **`chA > 1`, `chB > 1` and `chA != chB` is rejected** — `ChannelMapStatus::asymmetric`. The reason is
  **injectivity, not commutativity**: under a modulo rule a 2-channel A against a 6-channel sidechain feeds A's channel
  1 into outputs 1, 3 and 5, so the independence criterion degenerates into "the same convolution, three times" and a
  source channel is silently reused rather than mapped. Commutativity does not discriminate between the two rules
  (F-012).
- **`channelsOut` may be at most `max(chA, chB)`** — every family except the consolidated mono main, where D-018 sets
  the ceiling to 1 and the one output reads the fold rather than a selected channel. Fewer outputs select the leading
  channels; more is `countMismatch`. Selection is not the downmix the design excludes: no output mixes two source
  channels, each is still exactly one convolution of one pair.
- **A missing sidechain is silence, and silence is not channel 0.** `chB == 0` puts `kSilentSource` (`0xFFFF`) in
  every B slot; the sentinel sits outside every real index range, so an unhandled one fails as an out-of-range index
  instead of aliasing the dry bus into the wet path. `chA == 0` is `empty`, because A is the mandatory side.
- **`kMaxChannels = 8`,** chosen over 16: per-channel state below the seam is linear in `N` (`MixEngine`'s scratch is
  `hop × N` floats; the level stage owns three window-length `LevelDetector`s per channel), so 16 doubles the
  footprint of *every* configuration including the ones no host asks for. 8 covers 7.1 first-order ambisonics and every
  layout the shell's own buses can express. The map is 36 bytes at this ceiling and asserted against the bound written
  beside the constant. `resolve`'s `capacity` argument lets a caller ask for less and a test drive the boundary; a
  `capacity` above `kMaxChannels` is clamped rather than trusted.
- **Rejection is a typed status, never an exception, in a fixed precedence:** empty → asymmetric → exceedsCapacity →
  countMismatch. A gate needs the reason, not a bare `false`, and the precedence means a triple that breaks several
  rules reports the most specific one and never fills what it is about to reject. On every rejection the escaping map is
  value-initialised.
- **One function owns all of it:** `isBusesLayoutSupported` and the pointer resolution both call
  `ChannelMap::resolve`, and neither may re-derive the rule. It lives in `Engine` because `Engine` already carries
  `EngineConfig::maxChannels` and the scheduler whose silence-filling this formalises, and because the exhaustive test
  has to run as a console program.

**Figures.** The exhaustive leg sweeps all 729 triples of `{0,1,2,3,4,6,8,16,17}³` against a table restated from the
header's own row list; the shipped mappings (1,0), (2,0), (1,1), (2,1), (2,2), (1,2), (8,0), (8,8) and the
past-the-ceiling (16,16) are pinned by `static_assert`, so several mutations are caught by the compiler rather than by
a test run.

**Revisit trigger.** A host that must be supported with an asymmetric layout buys a second status and a documented
rule in its own entry, never an `#ifdef`. A mono main over a stereo sidechain *folding* rather than dropping is the
downmix the design excludes and needs its own decision — that trigger fired and D-018 took the fold, confining it to
one family by a sentinel computed above the seam.

## D-018 — The mono-main case consolidates the sidechain to a mono mean; every other layout needs main ≥ sidechain

**Status:** Accepted (M4); implemented and measured. `ChannelMap::resolve` folds `chA == 1 && chB > 1` to one
processed channel with `kConsolidatedSource` (`0xFFFE`) in its B slot, and `HopScheduler::foldSidechain` computes the
mean above the `processHop` seam, so the fold is host-reachable through D-021's gate.

**Decision.**
- **`chA == 1 && chB > 1` → one processed channel, mono output, sidechain source the arithmetic mean
  `(1 / chB) · Σ_c B[c]`.** Not channel 0 — that throws away `chB − 1` channels of the sidechain's content — and not a
  broadcast to `chB` outputs, which asks the track for a wider output bus than it has. Cheaper than broadcasting: one
  convolution per hop instead of `chB`.
- **Every other accepted layout keeps `chB <= chA`, and the shell keeps `mainOut == mainIn`**, so the main-bus equality
  guard stays and `isBusesLayoutSupported` never has to admit unequal main buses.
- **Arithmetic mean, not a power mean and not a −3 dB-compensated sum.** Two equal-level uncorrelated channels fold to
  −3.01 dB, a coherent identical pair keeps its level exactly, and an antiphase pair folds to silence — which reaches
  `LevelNormalizer`'s floor gate and leaves the wet path at unity rather than boosting a folded-to-nothing sidechain.
- **The fold is the one documented exception to "channels never read each other's samples", and it lives above the
  seam:** the resolver computes the mean into a mono hop and hands the engine one B pointer, so nothing in `Engine`,
  `Dsp`, `Fft` or `Grain` gains a cross-channel read.
- **The contract rides on a sentinel, not a flag.** `resolve(1, 2, 1)` is byte-identical under the old and new rules —
  broadcast-channel-0 and consolidate-to-mean both leave `a[0] = 0`, `b[0] = 0` — so the map has to *state* the fold. A
  flag beside the arrays would give a caller two sources of truth to read in either order, and a `bool` member takes
  `ChannelMap` past the 36-byte bound its own `static_assert` pins. The leg that owns the marker compares it against
  `0` *and* against `kSilentSource`, so it cannot co-mutate with the constant.

**Level consequence, measured through the shipped builder** (mono main, decorrelated stereo sidechain folded against
channel 0 of the same pair, dial centred, `amount` = 1, RMS over the last second of 3 s):

| Reading | Folded vs a mono sidechain |
|---|---|
| wet path with the level stage parked at `amount` = 0 | **−2.985 dB** — the −3.01 dB arithmetic |
| the correction the stage applies | −0.117 dB on a mono side, **+1.441 dB** folded → **+1.558 dB** |
| corrected output | **−1.427 dB** |

The fold drops the side 3 dB and the *target* moves **+1.56 dB in the opposite direction**. Both reasons are
structural: `wantedDb` consumes `√(envA · envB)`, so halving `envB`'s power is −1.5 dB rather than −3; and `envConv`,
which the stage divides against, falls with the fold, so part of what the side loses the output has already given back.
Value legs are bit-exact at `chB` ∈ {2, 3, 4, 8} over four input patterns, an even-width antiphase pair folding to
exact zeros, and the hop path does not allocate.

**Revisit trigger.** A host that will not enable a wider-than-mono sidechain independently of the main bus; the
listening pass deciding the fold is audible (antiphase cancellation, the 3 dB drop, or a preference for a channel-0 or
power-mean fold — listening calls, not edits here); or a surround main bus, where per-channel broadcast and
consolidation would compete for the same family. Whether any of it is audible is not measurable in a console harness.

## D-019 — Shared-spectrum optimisation: deferred, because the seam already fits

**Status:** Accepted (M4). No code change; the work moves to wherever a cost figure for it will come from.

**Decision.** Nothing is added to `EngineConfig` or `processHop`. Both eventual routes are additive — pointer equality
needs no signature at all, and `EngineConfig` is a plain aggregate taken by const reference with every engine built from
source, so a new field is not a break at any call site. **A flag now was the option not taken**: it would put
layout-derived state below the seam, and every engine would carry a flag it does not use in exchange for an
optimisation nobody has measured a need for. **Pointer equality is the route to expect**, and its failure mode is why
it is safe to leave unimplemented: a shared spectrum must be bit-identical to computing the same hop twice, so a caller
whose buffers stop aliasing loses the optimisation and nothing else — wrong audio is not reachable through it. Its cost
is that performance becomes dependent on the caller's buffer policy, so whoever implements it must assert the aliasing
rather than assume it. The deferral pays: a broadcast configuration does `N` transforms per hop where a shared spectrum
would do one, and a consolidated mono main pays one cross-channel fold per hop above the seam.

**Revisit trigger.** A per-channel cost curve — the data on record is per-callback; or any implementation in which a
shared spectrum is not bit-identical to the recomputed one, which is a behaviour change needing its own entry.

**Seam facts:** `Engine::processHop` takes `const float* const*` for both sides (`src/Engine/Engine.hpp:49-50`) and
`EngineConfig` (`:6-13`) is an aggregate with no private state.

## D-020 — Independent per-channel normalisation stays; the linking question moves behind a measurement

**Status:** Accepted (M4). No code change. The row does **not** close: it is a listening call, and a host-only
criterion never counts as measured.

**Decision.** No linked mode, no new parameter, and no rule written from the mechanism alone.
`dsp::LevelNormalizer` decides per channel — `gain = (√(envA · envB) / envConv) ^ amount`, floor gate per channel —
so a hard-panned or level-asymmetric stereo pair receives different gain on each channel, and a channel below the
−60 dB floor sits at unity while its partner moves by up to ±36 dB. That is image steering, and it is why a linked
option is worth asking about. Before any linking decision there is to be a figure: the inter-channel gain delta the
level stage produces on decorrelated stereo, per shipped hop. A console harness can produce that delta; it cannot
decide whether it is audible, which stays with a named host.

**Revisit trigger.** That measurement, or the user's listen. Either way the entry that changes this one has to name the
delta it was decided against.

## D-021 — The layout gate: two rules of its own above `resolve`, main bus mono/stereo, sidechain to 8

**Status:** Accepted (M4); implemented and measured as `engine::gateLayout` (`src/Engine/LayoutGate.hpp`,
header-only, `constexpr`, JUCE-light) with `LayoutGateTests`. It closes the "a layout the rule rejects is met with
silence" gap and makes the D-018 fold host-reachable for the first time.

**Context worth keeping.** `isBusesLayoutSupported` used to be three hand-written guards that never cross-checked the
two buses and never called `ChannelMap::resolve`, so the rule lived twice. Against the 12 shapes the old gate could see
its answers were already rule-correct, so this is a derivation change plus a widening — and the widening is the
decision: the fold existed in the map and the scheduler while the plugin itself refused a wider-than-stereo sidechain,
so the design's one cross-channel read was unreachable not because a host declined to offer it but because our own gate
did.

**Decision.**
- **A — the widths.** The main bus stays **mono or stereo**; the sidechain may be **up to `kMaxChannels` (8)**. A wide
  main is refused with a verdict of its own (`mainWidth`) so it is never confused with a channel-rule rejection. The
  accepted set gains `(1, chB 2…8, 1)` — 8 newly reachable layouts among 192 probed — and refusals get real cases: a
  stereo main with a 5.1 send is refused `asymmetric`. Processed count stays ≤ 2, so nothing below the seam grows.
- **B — the wide main is deferred, not rejected on principle.** 8-wide mains (quad, 5.1, 7.1 — all of which `resolve`
  takes today) wait on prepare-time allocation figures, a per-channel cost curve, and the surround-bus naming answer.
  `kMaxMainBusChannels` is the single constant that changes when those exist, so a later reader does not take the 2 for
  a rule of the design.
- **C — a refusal names its reason.** `LayoutDecision` carries `verdict` (accepted / mainWidth / mainCountMismatch /
  channelRule), `rule` (the map's `ChannelMapStatus` **kept verbatim on every path**, including refusals the map did not
  cause) and `processed`. No fifth status was invented for the wide-main case: a status that fires in one family is
  where precedence bugs hide, and that refusal is not the map's to make.
- **`resolve` is consulted on every path**, so `(9, 9, 9)` is `mainWidth` whose `rule` still reads `exceedsCapacity`, and
  a leg asserts exactly that. The gate is compared against `resolve` and never against a sentence about it, because the
  sentence — `chB <= chA`, except the mono-main family — is a *necessary condition* that over-admits by **22
  gate-visible triples**: 21 `asymmetric` pairs (`(3, 2, 3)` through `(8, 7, 8)`) plus `(1, 9, 1)` past the ceiling.
- **Two callers of `resolve`, one decider.** `gateLayout` and `entangle::makeSchedulerConfig`
  (`src/Entangle/EngineChain.h`) both call the pure function, as does `tools/CpuBaseline.cpp`; what is forbidden is a
  second place that *decides*. Collapsing them would thread the host-facing check into `prepareToPlay`, where the live
  buses are authoritative and can differ from the layout the host last asked about.
- **`configureRings` takes `activeChannels` from the gate's `processed`**, not from a bare `resolve`. Its early return on
  `activeChannels <= 0` stays — `processBlock` must be total — but is now unreachable from a layout a host can apply,
  measured by driving a callback whose buffers start as NaN: 0 unwritten output samples on every accepted layout.

**Figures.** 192 layouts probed through `checkBusesLayoutSupported` with 0 host/gate disagreements and 10 refusals
applying nothing; the per-bus substitution table (F-015); advertisement main 2 unchanged, sidechain 8 beside a mono main
and 2 beside a stereo one; 94 `AudioChannelSet`s, 0 split by naming. The new shape's footprint — B-side rings plus hop
scratch: 65 KiB at `chB` = 2 → **260 KiB** at `chB` = 8, H = 128, `maxHostBlockSamples` 4096 — recorded, not bounded.
The newly reachable family renders as the fold: shell bit-identical to the shared builder, differing from both
single-channel references (peak Δ 0.183 and 0.177), and −2.985 dB / −3.020 dB against them with the stage parked.

**Disclosed, not fixed.** D-018's antiphase cancellation is now field-reachable: a wide sidechain fed an antiphase pair
folds to exact zeros, falls through `LevelNormalizer`'s floor gate and leaves the wet path at unity — quiet,
uncorrected, no error anywhere. Before this item a host could not select it; after, it is a layout choice with no
control and no console answer.

**Revisit trigger.** Wide mains need allocation figures (on record in D-022 C: a stereo layout and the 8-wide fold each
≈1.02 MiB at the default grain length, and the axis that costs is a wide *main*, where `processed == chA` multiplies
every engine buffer — exactly the family this entry refuses) **and** a per-channel cost curve **and** the surround-bus
naming answer; two of three remain. Admitting the `asymmetric` family is a `resolve` change with an entry of its own,
never a gate edit. If no target host exposes a wider-than-mono sidechain, A buys a reachable refusal and no reachable
fold — which changes nothing about the rule, since it should still be able to say *why*.
## D-022 — Prepare-time sizing: a floor with a name, a re-prepare keyed on seven fields, a footprint measured not bounded

**Status:** Accepted (M4); implemented and measured as `engine::ReconfigureKey` (header-only, `constexpr`
comparable, JUCE-light) with `ReconfigureTests`, plus `EntangleAudioProcessor::maxHostBlockFor`,
`OfflineRender` §5.19 and `tools/PrepareFootprint`. Discharges the first of D-021 B's three dependencies.

**Context.** `configureRings` sized every ring against `std::max(samplesPerBlock, maxHostBlockSamples)` — a floor
in fact, unnamed at the site, with two more copies of the same `std::max` loose in the harness — and it
re-prepared on **every** arrival. A host that re-applies an unchanged layout therefore cleared the rings, rebuilt
the transform and re-applied the pre-fill, dropping R samples of a running transport; some hosts re-query and
re-prepare without meaning anything by it (F-001).

**Decision.**
- **A — `maxHostBlockSamples = 4096` stays a floor and becomes a named function.** `maxHostBlockFor(hostBlock)` is
  the one place the policy lives; the shell and both harness `ChainConfig`s call it instead of recomputing it.
  Consequence: every ring is `nextPow2(4096 + H)` = **8192 floats = 32 KiB at every hop the UI can select**, for
  every host block up to 4096 — a 64-sample host pays the same 32 KiB as a 4096 one, and the ceiling triple pays
  ~320 KiB of rings where the delay lines it serves need ~13 KiB. Above the floor the host's number reaches the
  sizing: 4097 → 8192 f, 8192 → 16 384 f, 65 536 → 131 072 f. What the floor buys is that sizing does not depend
  on host behaviour, so an oversized callback trips `assert(numSamples <= config.maxHostBlock)` in an assert build
  instead of degrading — as it does in Release — to a short `CircularBuffer::write` and a hole in the output.
  Since `nextPow2(hostBlock + hop) > hostBlock` for every hop ≥ 1, **that assert cannot fire on host behaviour as
  the sizing stands** — it is a regression tripwire on the sizing expression.
- **B — the re-prepare is conditional on a seven-field key:** `sampleRate`, `hostBlock` (as prepared, before A),
  `chA`, `chB` (`0` = sidechain absent, so it carries D-002's enabled bit), `chOut`, `hop`, and `grainLength`
  **after the clamp**. All seven are swept one at a time and each must still produce a re-prepare — a missed
  dimension is a real change silently ignored. Second prepare with an unchanged key allocates **0 bytes**; a
  mid-render `setBusesLayout` + `processorLayoutsChanged` + `prepareToPlay` carrying the layout already in force
  leaves a running transport bit-identical. Post-clamp `L` is what makes 32768 → 65536 at H = 256 a no-op
  (ceiling 32768, render bit-identical) and the same move at H = 512 a real re-prepare (peak Δ 0.212).
- **C — the footprint is measured, documented, never asserted.** No test holds a byte limit.
  `tools/PrepareFootprint` counts live bytes through a replaced global `operator new`, prints host, compiler,
  build type and assert state first, and is deliberately outside CTest. Accepted consequence: a later sizing
  change drifts the documented figure instead of reddening a test, and running that tool re-establishes it. What
  *is* asserted is behaviour: a narrowing gives back exactly the ring storage it no longer needs, and the first
  callback after a re-prepare allocates nothing.

**Figures** (Linux aarch64, gcc 13.3, Release with `-flto`, `ENTANGLE_ASSERTS=ON`, `fft::JuceFft`):

| triple | scheduler alone, H = 128 | whole chain, default L = 8192 | whole chain, L = 65536 (H = 512 only) |
|---|---|---|---|
| `(1, 0, 1)` mono | 81.6 KiB | 760.7 KiB | 5 488.7 KiB |
| `(2, 2, 2)` stereo | 211.9 KiB | 1 024.4 KiB | 6 669.4 KiB |
| `(1, 8, 1)` the gate's ceiling | 342.1 KiB | 1 021.2 KiB | 5 761.2 KiB |

- **The wet path dwarfs the scheduler:** 10.5F + 6 floats at one processed channel and 12.5F + 6 at two —
  7.1 KiB mono / 12.5 KiB stereo above those figures at the default grain length, 26.6 / 47.0 KiB at the cap —
  `juce::dsp::FFT`'s own tables named as excluded rather than folded in, because the backend is chosen at runtime.
- **An 8-wide sidechain is not the expensive shape:** consolidation processes one channel, so the ceiling triple
  pays +130 KiB of scheduler and saves 133 KiB of engine beside stereo — 1021.2 against 1024.4 KiB, a net wash.
  The axis that costs is the one the gate still refuses, where `processed == chA` multiplies every engine buffer.
- The shell is 265 KiB before any prepare, so what the guard skips per redundant arrival is 761–1024 KiB of churn
  plus a transform rebuild, against a real hop change's 8–15 KiB net.

**Revisit trigger.** Wide mains, per D-021 B — this table is the input that was missing; a per-channel cost curve
and the surround-bus naming answer remain. Separately: **memory follows the widest configuration a session has
seen**, because `assign` and `clear` keep capacity — at the ceiling and H = 256 a narrowing gives back exactly
224 KiB of ring storage and keeps 7.0 KiB of hop scratch plus 0.4 KiB of ring-object array. That is the price of a
callback that never grows anything; it bites only against a host that flaps between wide and narrow layouts, and
the fix then is to release capacity at prepare, never to add a second decider.

## D-023 — The layout report: one line naming what the plugin was *told*, assembled at the tail of a real prepare

**Status:** Accepted (M4); implemented as `EntangleAudioProcessor::layoutReport()` over a body written at the tail
of `configureRings`, the editor's `layoutReadout` label, and `OfflineRender` §5.21 (28 lines decided, 0 FAIL, 26
report lines reaching an installed `juce::Logger`). **What it exists to answer is still open:** every row of
`docs/host-layout-matrix.md` is unanswered, because no host has loaded this plugin and a container answer does not
count. The instrument is measured; the answers are not.

**Context.** Nothing in this repo's evidence has ever loaded the plugin in a host, so the open questions — which
hosts expose a sidechain send wider than mono, whether it can be enabled independently, what a refused layout does
in the wild, how JUCE reports surround and ambisonic buses — have no answer path that does not go through a DAW.
Reading the host's labels is not a path: the advertisement (8 sidechain channels beside a mono main, 2 beside a
stereo one) is computed from the plugin's own gate (F-015). So the deliverable is an instrument that records what
the plugin was *told*, plus a document whose rows count only when they carry that line verbatim.

**Decision.**
- **One function, at the tail of `configureRings`** — after the `gateLayout` call, after the
  `activeChannels`/`sidechainActive` members, and **after** D-022 B's guard returns early, so the line describes a
  configuration that was actually built. The ordering is the decision: above the guard, a report rebuilt by an
  arrival that changed nothing could no longer distinguish "we rebuilt" from "the host re-applied and we left the
  transport alone".
- **The fields.** Live count and `AudioChannelSet::getDescription()` verbatim for each of the three buses plus each
  bus's `isEnabled()` flag, the gate's `verdict/rule` and `processed`, `getTotalNumOutputChannels()`, the rate, the
  hop, the requested and running grain length, the declared latency, `hostBlock/maxHostBlockFor(hostBlock)`,
  `sidechainActive`, `effectiveBlendPosition()`, and the two counters. The method is `getDescription()` — **there is
  no `getName()`** (F-018). Counters are formatted *at read time*: a declined arrival never reaches the report body,
  so counters frozen into it could never show a skip, which is the one thing `skipped=` exists to say. Configuration
  fields still describe the last real prepare; only the prefix moves.
- **Two deliveries, both message-thread, neither blocking the audio path:** the editor label (whole line in its
  tooltip), and one `juce::Logger::writeToLog` per real prepare behind
  `MessageManager::existsAndIsCurrentThread()`. The guard is not decoration — `writeToLog` reaches stdout or `NSLog`
  and can block, and hosts call `prepareToPlay` from threads of their own choosing. How sharp it is:
  `MessageManager`'s constructor records *its own* thread, so any harness that initialises JUCE's GUI passes the
  guard on `main()` and logs every prepare; only a genuinely other thread is silenced.
- **The counters and the lock.** `prepares=` counts arrivals that rebuilt, `skipped=` those the key declined. Both
  are prepare-time state reachable from nothing in `processBlock`, and both live under a `mutable
  juce::CriticalSection` with the getter returning by value — `prepareToPlay` has no fixed thread and a
  `juce::String` assigned under a reader is a crash, not a cosmetic race.

**Rejected:** writing a file from the plugin (a host-specific user-data path, a privacy question nobody asked, and
a console harness needing an env var — which splits the tested path from the shipped one); an env-var-gated
diagnostic (same split, plus a mechanism with one user); the host's own meter (answers one question, qualitatively).

**Figures.** Counter walk `1/0 → 1/1 → 1/2 → 2/2 → 3/2`; three refusals moving no configuration field with
`skipped` reaching 2 and the render unchanged; a hop-only reconfigure whose diff set is exactly `hop,R`; one line
per real prepare and none per declined arrival; an off-thread prepare logging nothing and the next message-thread
prepare logging. Memory, printed not asserted: **+64 B at construction, +287…+294 B at the first prepare** (the
report's own `juce::String`) and **0 KiB** for a second prepare with an unchanged key. Under a `Logger`, note that
`Logger::~Logger` **asserts the object being destroyed is not the installed one** (`juce_Logger.cpp:44`), so a
harness that swaps in a filter must `Logger::setCurrentLogger(nullptr)` before `main()` returns.

**Revisit trigger.** If a named host never shows the log line, the delivery is the readout alone. The instrument
relocates with the editor rewrite; it follows an existing readout pattern precisely so that it moves *with* the
pattern rather than being designed twice.

## D-024 — Interface colour is five tokens in one file, type is seven roles, and a state is ink or geometry

**Status:** Accepted (M10); implemented and measured as `src/Ui` (`Palette.hpp`, `TypeRamp.hpp`, `Grid.hpp`) and
`src/UiJuce` (`Theme`, `FontFactory`).

**Decision — five tokens, as data.** `ground 0xFFFCD5F7` (the sheet), `fill 0xFFF9E9D5` (value: bar fill, dial
wedge, the selection behind ink), `track 0xFFD3E6F8` (surface: bar bodies, dial faces, any popup's ground),
`trace 0xFFD3F9DA` (spent **exactly once**, on the third waveform trace), `ink 0xFF000000` (every glyph, both
display panels, the only line the product draws). `std::uint32_t` rather than `juce::Colour`, because the light
half has no JUCE and the light half is where the scheme's arithmetic is testable; `ui::Theme::toColour()` is the
only conversion, and a source-policy case fails the build on a `Colours::` name, a hex literal, a numeric
`setFont`, or an em figure written outside the ramp table anywhere in `src/UiJuce` or `src/Entangle`.

**The tetrad's hues, lifted — the lift is the decision.** The four tints keep the design's hues and move from HSL
*L* 80 % to **90.0–91.2 %**. There no two tints contrast by more than **1.1458** (all six pairs: ground↔fill
1.10278, ground↔track 1.02776, ground↔trace **1.14579**, fill↔track 1.07300, fill↔trace 1.03900, track↔trace
1.11484) while ink against them is **15.99987 / 17.64433 / 16.44397 / 18.33244**. Consequence: **a tint can never
be the difference between two states.** Rest / hover / selected / disabled are expressed by ink, by the
fill-over-track pair, or by geometry (the 0.8 mm inset, the ring, the outline) — never by a fifth colour and never
by a hue shift. `contrastRatio()` and `hslLightness()` live in the light half so that editing a token reddens a
case instead of arriving as a screenshot.

**Rejected tones stay rejected.** `#ADC9EB` measures **1.3335** against `track` and **1.2975** against `ground` —
outside the band by more than 15 % — and is the tone the design refused. It survives only as a discriminator arm
and a standing leg of `PaletteTests`, which is where a refused colour keeps doing work.

**Seven roles, two weights, one em each.** wordmark 64 bold · caption 56 thin · barLabel 80 thin at horizontal
scale **0.65** · barValue 63 thin at **1.10** · rotatedCaption 52 thin · figure 35 bold · micro 22 thin.
`resolve(role, k) = em × k`, `k = windowWidthPx / 315.0`, and no widget carries a pixel size of its own. Two
weights, not three, and the squeeze belongs to exactly two roles.

**What the palette costs at the JUCE seam.** `ColourScheme` has **nine** slots and `static_assert`s that arity, so
five tokens must collide: ground once, track twice (`widgetBackground`, `menuBackground`), fill twice
(`defaultFill`, `highlightedFill`), ink four times, and `trace` not at all — the token the scheme cannot express is
the one the design spends once, on a panel the product draws itself. F-022 carries what `LookAndFeel_V4` does to
the derived table after the scheme lands.

**Figures.** The grid at 1190 × 794 (`k` = 3.7778): origins x {38, 321, 604, 888}, y {38, 283, 529}, cell
264 × 227, gutters 19/19/20 across and 18/19 down, sheet closing on 1189 of 1190, a two-cell span
547 = 2·264 + 19. Cap-top placement lands 1 px off the requested row at both `k` = 1 and `k` = 3.778, where
ascent-placement would be 10 px and 41 px off. The 0.65 squeeze draws 89 px against 137 px (ratio 0.6496) on
"VOL". Seven `Font` builds for `prepareFontsFor(k)` and still seven after a paint pass at that `k`.

**Revisit trigger.** (a) A state needing a *third* distinguishable tint — the answer is geometry or ink; a
saturated tone needs a measured contrast figure and the user's explicit OK, because the quietness is the decision,
not the hues. (b) `trace` spent on anything but the third waveform trace — it is spent once by rule, so a second
spending re-opens this entry rather than changing the token. (c) A host that cannot render Thin — answered by
D-025's embedded face. (d) A resize answer needing anything other than `k` — the geometry rests on millimetres so
that stays cheap; a second size path re-opens the grid, not this entry.

## D-025 — The design's own face ships inside the binary; the family name is gone from the product

**Status:** Accepted (M10) as an instruction from the user — the design's own face, non-negotiable — implemented as
`juce_add_binary_data(EntangleFonts)` under `src/UiJuce`, linked `PUBLIC` into `UiJuce`, read only by `FontFactory`.
Amended M11: this row owns the *mechanism* — vendored bytes, the ramp as the only route to a size, no family name in
the shell — and the shell rules that come with it. Identity, licence, weight map, byte figures and the ramp table are
**D-037**: the M10 face (Helvetica Neue) was replaced by Clash Display. Every shell rule here survived that swap
untouched, which is the evidence the seam sits in the right place — 48 call sites had named the old family and none
of them moved.

**Decision — the bytes, not the name.** `typefaceFor(weight)` returns a `Typeface::Ptr` built once from
`createSystemTypefaceFor(ClashDisplay_Light_otf, ClashDisplay_Light_otfSize)` or the Bold pair, and `makeFont` hands
that pointer to `FontOptions{}.withTypeface(face)`. No `Font` in the product carries a family string, so there is
nothing left for a host to substitute — which is the point: JUCE 9 resolves **no** typeface for a family fontconfig
cannot match and hands back null (F-022), so a family-name request is a question the *host* answers, and off macOS
this container's answer was DejaVu Sans, leaving 18 of the mockup's 22 Thin text elements not Thin. A null face is
now a broken build rather than a design state: `jassert` fires and JUCE's default face keeps the process alive,
while the skin's face leg reddens on the name.

**Two faces, not sixteen.** The ramp declares two weights, so exactly the two it binds to are embedded — **Light
27 000 B** and **Bold 25 276 B** (D-037 maps that weight pair). A third face in every bundle is bytes nobody renders.

**Cost.** `libEntangleFonts.a` is **57 170 B** against a standalone of 11 235 400 B — **0.51 %** of the shipped
binary — where the M10 face cost 915 914 B (8.8 %). `-flto` cannot recover it: both symbols are referenced from
`FontFactory`, which every paint reaches. Payload is therefore not an argument against a third weight (D-037).

**Licence.** The vendored faces carry the **ITF Free Font License** (`public/ClashDisplay_Complete/License/FFL.txt`,
12 734 B), read in full on 2026-11-21. Clause 01 grants use "for personal or commercial purposes, free of charge and
for an unlimited period of time" and, in its own words, "**You may embed the Font Software in mobile or desktop
applications and digital documents for the uses permitted under this License**" — the grant this product stands on,
not an inference from the carve-in at the end of clause 02, which says the same thing for the Licensee's own
applications. Clause 02 refuses modification (it names subsetting, format conversion and metadata edits) and refuses
making the Font Software available to anyone else *as a font*. D-037 holds the clause table and the one point the text
leaves open.

**Packaging.** The build reads the foundry's own tracked drop: `src/UiJuce/CMakeLists.txt` names
`public/ClashDisplay_Complete/Fonts/OTF/ClashDisplay-Light.otf` and `-Bold.otf` and nothing else from the drop, so
the italics, the other four weights, the variable file and the web formats stay out of the binary while the two it
ships sit where they arrived, licence text beside them. Tracked since `d84784e` and `7128e9d`, so a clone already
has both files and a second copy of the same bytes could only drift from them (ruled 2026-11-24). An M10 build
failed at configure time on a `public/` source path and the same failure recurs the moment a listed path drifts, so
the leg is on the list rather than the directory: every path it names is tracked (`git ls-files --error-unmatch` on
both) and the generated `build.ninja` names the two Clash OTFs and no Helvetica one, `EntangleFonts` linking
`libEntangleFonts.a` at 57 170 B (c18).

**Revisit trigger.** (a) A third weight — the ramp's weight column is the one place that changes, plus one more
embedded face; the six-weight option and its cost are D-037. (b) The first macOS host run, where the installed family
and the embedded face can be compared; divergence there hints the embedded face is the wrong cut. (c) Any
bundle-size budget that makes 57 KB material — the fallback is not the family name (that is F-022) but one weight.

## D-026 — The editor window is resizable, and the sheet scales as one sheet

**Status:** Answered by the user: "make it resizable with all components/ui elements fixed in relative size to one
another." Implemented.

**Decision.**
- **The range.** Default **1190 × 794** (the reference sheet); floor **595 × 397**; ceiling **2380 × 1588** — exact
  halves and doubles, so one aspect holds at every size and there is one ratio to lock rather than three. The
  floor's basis is readability: at half scale the `micro` em is **11 px**, where the readouts stop being readable.
  The ceiling expresses no design limit — nothing breaks above it — and exists to bound the drag.
- **The aspect.** `1190 / 794 = 1.49874`, set on the constrainer the editor already owns
  (`ComponentBoundsConstrainer::setFixedAspectRatio`) — what the standalone wrapper reads and what a host calling
  `IPlugView::checkSizeConstraint` is told. A host that ignores it still gets a proportional sheet: the layout
  reads the width alone, so an off-aspect window puts the surplus in the ground below the last row rather than in a
  stretched box — measured at **1000 × 794**, worst rounding 1 px.
- **No corner grip.** `setResizable(true, false)`: the second argument would add JUCE's
  `ResizableCornerComponent`, which paints through `LookAndFeel::drawCornerResizer` in colours the five tokens
  never named — D-024's rule applied at the window's corner. The host's own chrome is therefore the resize
  affordance, so a host that gives the editor none gives the user no visible way to resize. That is a row of the
  host layout matrix, not a code defect.

**Cost:** five constants in `src/Ui/Grid.hpp`, three calls in the editor's constructor before `setSize`, five
report lines in `OfflineRender` §5.22. The one layout change it forced was the truth stack's rounding chain (F-024
item 2) — a reserve that steps each line off the previous line's rounded bottom carries the em's half-pixel six
times, and the resize legs are the instrument that sees it.

**Figures.** §5.22: 79 lines decided, 0 FAIL. Range reported as `595x397 to 2380x1588`, aspect `1.49874, want
1.49874`; worst rounding by scaled width — 595 × 397 **1 px**, 893 × 596 **1 px**, 2380 × 1588 **2 px**,
1000 × 794 (off-aspect) **1 px**, worst offender `statusLine` at 2380.

**Revisit trigger.** (a) A host that refuses a constrained window or ignores the aspect and hands over a tall one —
the sheet then leaves ground below row 3, a preference queue, not a layout bug. (b) Any second size path: the
moment something on the sheet is not a function of `k`, this decision's cheapness dies with it and the grid has to
change. (c) A need for a visible grip, which re-opens the palette rule rather than the resize rule — the grip would
need a token of its own, and D-024 has none to spare.
## D-027 — The display publication: an SPSC ring injected at the engine seam, never an accessor into the level stage

**Status:** Answered by the user on 2026-11-18 — of `04` §0's three arms, *arm 2, an SPSC `TraceRing`*. Implemented
in `04` tier 1.

**Why a decision was needed at all.** `LevelNormalizer` already computes every figure the two panels draw
(`gainDb(c)`, `envelopeA/B/Conv(c)`, all `const noexcept`), which is what led the spec to claim tier 1 needed no new
seam. It needed one. The stage is a private member of the chain the shell builds, and its getters are plain doubles
written by the hop thread: reading them from the message thread is a torn read of a four-word set *and* a cross-thread
reach into hop state, which `../M10-impl-plan/INDEX.md` §3 rule 4 forbids without a decision entry — this one.

**Decision.**
- **The route.** `container::TraceRing<engine::TraceSnapshot>`, owned by the processor, written by `MixEngine` once
  per hop, read by the editor on its 100 ms tick through four processor getters — `traceWindow`, `traceLatest`,
  `traceDepth`, `traceSecondsPerHop` — and nothing else. The last exists so neither panel converts a hop into
  seconds on its own: the time axis is the shell's, from the configuration the rings were built for.
  The editor holds no reference into DSP state and nothing in DSP references the editor.
- **Injected, not exposed.** `MixEngine::installTraceSink(TraceSink)` sits beside the existing
  `installLevelNormalizer`, and a sink is a non-owning borrow of the ring. An accessor handing back the stage would
  make every consumer below the shell responsible for knowing whether a display exists, and would put a live pointer
  into hop state in the message thread's hands. Injection makes the engine's only act `trace.publish(snapshot)`, which
  on an empty sink is one null test and no call. Nothing below the shell names the UI: `Engine` knows a 144-byte
  record and a ring, never a panel.
- **Drop the oldest.** A full ring laps its oldest unread snapshot and charges itself the drop (`dropped()`) rather
  than refusing the new one: the newest record is the signal's current state, and a publication that loses it has
  stopped meaning anything, while an unread old one is history the reader simply missed. Per-slot sequence tags
  (odd = being written, even = written) keep a reader from ever delivering a record the writer was mid-overwrite on.
- **Peek, not drain.** Both panels and the box-6 meter read the same window through `peekNewestInto`/`newest`, so
  neither panel can starve the other and one read serves the whole tick. The display is a rolling window, not a
  queue — a hop nobody looked at is worth nothing — which is why `Container` gained a second ring type rather than a
  mode on `CircularBuffer`, whose contract is that a full buffer refuses the new item.
- **Capacity.** `ui::kTraceWindowSeconds` (2.0 s) of hops at the *prepared* rate and hop, `+1` so a full window fits
  instead of lapping itself at the exact boundary, rounded up to a power of two by the ring; sized in `prepareToPlay`
  and `reconfigureSafely`, never grown on the step path. The shell sizes its scratch from the ring's rounded
  `capacity()`, so a panel can never ask for history the publication cannot hold. The rounding costs up to 2× the
  needed bytes — accepted for the mask instead of a divide.
- **Publication also needs a stage.** `processHop` publishes only when `levelNormalizer != nullptr` *and* the sink is
  non-empty. A chain with no level stage has no correction and no envelopes, so it publishes nothing and the panels
  say `idle` rather than drawing zeros that would read as a held signal.
- **Swap and teardown.** The sink is a raw borrow and is not atomic, so every rebuild is staged park → rebuild →
  re-borrow inside the processor's `traceLock`, and the reconfigure path runs inside `suspendProcessing(true)`.
  `releaseResources` parks the sink before it frees the ring. `traceLock` is taken by the message thread only — the
  hop path sees either the old sink or the new one, never a half-built ring.
- **The axis is the bound, the rule is the value.** The envelope panel's y axis spans the `silenceFloor` parameter's
  own lower bound to 0 dBFS; its one rule sits at that parameter's *live* value. Two separate inputs on purpose: an
  axis tied to the gate pins the rule to the field's bottom edge, and a rule that cannot move when the dial does is a
  photograph — `04` §5 arm 5 is that arm.

**Cost:** one header in `Container` (a template, no `.cpp`), one in `Engine` (`TraceSink`, `TraceSnapshot`), two
members and two methods in `MixEngine`, one ring plus one lock plus four getters in the processor, one timer leg in
the editor. With no editor attached the hop path pays one null test per hop.

**Figures.** `§5.23`: 37 lines, 0 FAIL. **256 slots × 144 B ≈ 36 KB** at 48 kHz and H = 512 (the window is 187 hops
+ 1, rounded up); a 281-hop render hands back a full 256-slot window, hops 25…280 in order with the newest last; on a
quiesced transport a published snapshot equals the stage's own getters exactly, both channels; widest inter-channel
correction spread **10.32 dB** (`04` §2.1's figure, the input M9 row 1 is still asking about); the new `Container`
allocation binary reads **0 allocations** across write/peek/drain/newest/clear including a deliberate overflow. Full
suite **310/310, exit 0**, 39.23 s; `ctest -R "ui:"` 84/84; `vstplug_VST3` and `vstplug_Standalone` link at `-j4`.

**Revisit trigger.** (a) A consumer that must not miss a hop — a logger, an offline renderer — needs its own ring or
a gap-aware reader, not a shared peek. (b) Any change to `kTraceWindowSeconds`, which moves the capacity and
`kMaxTraceHistory`'s bound in the same change. (c) A configured hop × rate pair faster than 6000 readings per window,
past which the panel's history bound truncates the trace short of its left edge. (d) A second writer behind the same
seam: two engines publishing into one ring voids the single-producer rule the tags rest on.

## F-001 — JUCE 9 latency plumbing

**Status:** Finding.

`AudioProcessor` lives in `juce_audio_processors_headless/processors/` in JUCE 9. `setLatencySamples` is
byte-identical to 8.0.7: it calls `updateHostDisplay(ChangeDetails().withLatencyChanged(true))` itself, so calling
it is enough to notify the host. **`updateReportedLatency()` does not exist in either version** — searching for it
wastes time. Whether a host re-queries mid-session is host-specific and belongs to the host matrix, not here.

## F-002 — `/tmp` was noexec and broke juceaide

**Status:** Finding, fixed.

A configure into `/tmp` died at "Testing juceaide" because `compose.yml` mounted `/tmp` with the `type: tmpfs`
short form, which always mounts `noexec`; CMake compiles a probe into `TMPDIR` and then *runs* it. Fixed by the
named tmpfs volume `vst3-tmp` with `o: "mode=1777,size=512m,exec"`.

**Caveat that survives the fix:** an explicit `o:` string **replaces** Docker's tmpfs defaults instead of merging,
so `nosuid` and `nodev` were dropped along with `noexec`. Restate every flag you want
(`o: "mode=1777,size=512m,exec,nosuid,nodev"`) the next time the container is recreated.

## F-003 — JUCE real-only FFT: per-engine scaling, locking fallback

**Status:** Finding. Line numbers are `juce/modules/juce_dsp/frequency/juce_FFT.cpp` at the pinned submodule commit.

- **Real-only scaling is per-engine internally, so reading one constant gives the wrong answer.** FFTW's
  `performRealOnlyForwardTransform` (`:696-710`) is a raw `fftwf_execute_dft_r2c` with **no scaling**, and its
  inverse (`:712-718`) multiplies by `1/n`. Apple's (`:483-494`) runs `vDSP_fft_zrip` then `vsmul`s by
  `forwardNormalisation`, which its ctor sets to `0.5f` — a **correction, not a net scale**, because
  `vDSP_fft_zrip`'s own forward gain is 2.0 (measured against Accelerate on a unit impulse: DC returns
  `(2.000, 2.000)`). The two cancel, so JUCE's real-only forward gain is **1.0 on both engines**; inferring the net
  from `forwardNormalisation` alone predicts `0.5` and is 2× wrong. `JuceFft::forwardGain()`/`inverseGain()` exist
  so scaling is a measured number rather than a constant read off one engine.
- **The engines pack the half-spectrum differently and JUCE hides it:** vDSP's `zrip` stores Nyquist's real part in
  the DC bin's imaginary slot, undone by `AppleFFT::mirrorResult` (`:515-529`) on the forward and restored at `:506`
  on the inverse. Both present the layout `Fft.hpp` specifies, which is why the adapter's only correction is a
  scalar and why a `Dft`-vs-`JuceFft` bin-for-bin test catches layout as well as scale.
- **The fallback engine is numerically fine and structurally unsafe:** it takes a `SpinLock` on **every** `perform()`
  (`:115`) and `alloca`s its scratch (`:146`), and the real-only entries route through `perform()`
  (`:136 → :176-181`), so the lock sits on the hop path — an AGENTS §7 violation. It is also
  **indistinguishable by result or gain**: forward unnormalised, inverse `1/size`, so FFTW, vDSP and the fallback
  all report 1.0. **A passing numeric suite is never evidence the fallback is not running.** Proving identity is
  platform-specific (D-006): on Linux FFTW is `dlopen`ed, so `RTLD_NOLOAD` residency of `libfftw3f.so` is decisive;
  on macOS Accelerate is mapped by `juce_dsp` regardless of the winner, so residency is necessary but not
  sufficient and the closed engine list does the excluding.
- **FFTW is safe on the hop path:** `FFTWImpl` locks only in ctor (`:660`) and dtor (`:674`) to guard the
  non-thread-safe `plan_*`/`destroy_*`; the `execute_*` path takes no lock. `prepare()` therefore serialises against
  other FFTW users — `prepareToPlay`, never the callback.
- **Engine priorities, so a silent downgrade is predictable:** static FFTW 10 (`:570`), IPP 9 (`:831`), MKL 8
  (`:745`), **vDSP 5** (`:446`, compiled for `(JUCE_MAC||JUCE_IOS) && JUCE_USE_VDSP_FRAMEWORK`, default on),
  **shared FFTW 3** (`:572`), **fallback −1** (`:92`); `createBestEngineForPlatform` (`:60-66`) takes the highest
  that `create()`s. All three `JUCE_DSP_USE_*` flags default to 0, so without D-006's define even a host with FFTW
  gets the locking fallback.
- **Missing FFTW is silent:** the soname is per-OS — `libfftw3f.dylib` / `.dll` / `.so` (`:619-623`) — and a failed
  `lib.open` returns null from `create()`, falling through **with no diagnostic**. On macOS that name is not on
  dyld's default path, so a Mac with FFTW installed may still not use it.
- **The FFTW c2r ignores exactly the two slots `JuceFft::inverse` clears, so that clearing is untestable here.**
  Deleting `scratch[1] = 0.0f` and `scratch[F + 1] = 0.0f` and feeding the inverse a product spectrum with `NaN` in
  `bins[1]` and `Inf` in `bins[F + 1]` leaves every output sample **bit-identical** at `F ∈ {64, 256, 1024, 4096}` —
  even-`n` c2r discards the imaginary parts of bin 0 and bin `F/2`, the same two slots `Fft.hpp` declares never-read.
  The clearing is engine-portable hygiene for vDSP's packing, not a live guard on FFTW; **no test here can
  distinguish its presence from its absence**, so a future reader must not "fix" the redundancy on the strength of a
  green suite.
- **On Apple the installed engine is vDSP**, proven the platform-specific way: the closed-engine assertion passes
  and `otool -L` shows `Accelerate.framework` with `nm -u` reporting `_vDSP_create_fftsetup`, `_vDSP_fft_zrip`,
  `_vDSP_fft_zop` and no FFTW symbol. The gain contributes nothing to that conclusion.
- **A caller-authored spectrum survives vDSP's packing:** `forward · multiplyHalfSpectrum · inverse` on the `Dft`
  oracle and the vDSP-backed adapter matches in the time domain at peak-relative `1e-4`, green at
  `F ∈ {64, 256, 1024, 4096}` — under the 1.0/1.0 calibration a spurious half-scale or stray `1/F` is a 6 dB /
  `F`-fold error that bound cannot hide.
- **A briefed "known vDSP split-radix defect at `F = 4096`" has no referent** — nothing in the tree records one and
  `JuceFft` matches `Dft` bin for bin at 4096. A future 4096-only red on Apple is an unexplained new finding, not a
  known issue to wave through.

## F-004 — A spectral product needs no extra factor under D-006's convention

**Status:** Finding. No scale change anywhere: `Dft` and `JuceFft` were correct as written.

`fft::multiplyHalfSpectrum` (`src/Fft/Spectral.hpp`) applies no scale, and nothing downstream should add one on the
grounds that FFT normalisation is owed. With forward unnormalised and inverse carrying `1/F` — the convention
`Fft.hpp` pins and D-006 calibrates to on both shipped engines — `IDFT(X·Y)[n] = Σ_m x[m]·y[(n−m) mod F]`:
circular convolution, nothing left to apply. `Dft::inverse` (`Dft.hpp:65`) applies `scale = 1.0 / F` (`:68`) and
nothing on the forward path does.

**Why a finding rather than a footnote:** a contract sentence saying the `1/F` "is the caller's to fold into its own
gain table" makes an implementer divide by `F` a *second* time — output `20·log10(F)` dB quiet, **−72 dB at
`F = 4096`**, effectively silence — and having "already handled the scaling", they hunt the bug in the window or the
OLA. **Consequence:** the engine's gain table carries the window/OLA correction only; there is no FFT normalisation
term waiting for it. Pinned by `SpectralTests`.

## F-005 — The OLA order *is* the latency model: emit, zero, advance, then add

**Status:** Finding, pinned by tests.

`grain::OlaAccumulator::emitAndAdd` (`src/Grain/OlaAccumulator.hpp`) emits the H samples at the head and zeroes
them, advances the head by H, then adds the arriving grain at the new head. The reverse order — add, then emit — is
the natural reading of "overlap-add" and it is wrong three ways at once, which is why the order is pinned in the
header rather than left to the implementation:
- the delta anchor moves from `out[n0+L+m] = w(m)` to `out[n0+L−H+m]`, so the physical delay reads `L−H`, not `L`;
- output sample q then draws on input at and after q, breaking the strict causality that lets a reported R of 0 be
  honest;
- the live span at emit time becomes `2L−1+H`, which no power-of-two `F = nextPow2(2L−1)` satisfies — the ring bound
  holds only because the emitted H are removed before the new grain lands.

**What pins it:** `expectDeltaIsWindow` asserts `out[n0+L+m] == w(m)` to `1e-5` with silence either side;
`expectCausalAtZeroLatency` asserts output q is unchanged when every input at and after `q` is perturbed;
`OlaAccumulatorTests` compares against a naive overlap-add bit-exactly. Green on the `Dft` oracle and on
`fft::JuceFft`.

**Why a finding:** planning pseudo-code and its anchors disagreed on this order, so the ambiguity is in the spec.
Resolve it against the delta anchor — the only statement of the three that fixes delay, causality and ring size
simultaneously.

## F-006 — `HopScheduler` cannot drive a reported latency of 0 at `hostBlock < H`

**Status:** Finding, open (the latency representation is parked).

`HopScheduler::prepare` pre-fills the output ring with exactly `latencySamples` and then requires every callback to
drain what it asked for. With `R = 0` and a host block smaller than the hop, the ring is empty on the first
callback, so the drain assert fires before any audio is produced. Reproduced at `R = 0` and again at `R = 1` on
`H = 128, hostBlock = 32`: what fires is `assert(delivered == arrived)` on the *first* callback. The failure is not
"`R` is too small" in the abstract but "the ring held less than the pre-hop callbacks drain" — at
`H = 128, hostBlock = 32` that is the 3 × 32 = 96 samples consumed before the input first reaches a full hop.

**Consequence for the tests:** block-size invariance runs at `R = H`, and `R = 0` is proven at the engine seam only
— where it belongs, since `processHop` never sees the reported latency. The two are consistent: `R` shifts where the
host cuts the stream, not its shape.

**Revisit trigger.** The latency wiring: it settles between pre-filling `max(R, H−1)` and changing the drain assert
to tolerate a short first callback — a latency-policy question (D-004) that should land there, not in the plugin.

## F-007 — Grain-engine per-callback cost: hop-boundary callbacks dwarf idle callbacks

**Status:** Finding. The absolute µs are host-specific and carry their build type; the ratio and the shape are the
finding.

`tools/CpuBaseline.cpp` (target `CpuBaseline`, not in CTest — wall-clock on a shared machine is a measurement, not a
pass/fail signal) drives the real `GrainConvolutionEngine` through `HopScheduler` at 48 kHz, 3000 hops, `R = H`,
bucketing callbacks by whether `hopCount()` advanced. `processHop` means, `processHop` alone (`µs`):

| L | H | Apple Silicon `-O3` | Ubuntu x86-64, Ryzen 5950X `-O3` |
|---|---|---|---|
| 512 | 128 | 8.2 | 5.1 |
| 1024 | 256 | 16.6 | 10.8 |
| 2048 | 512 | 37.9 | 22.0 |

- Total cost is ~0.7 % of one core at every size — hop work scales with `F log F` while hops/s scales with `1/H`,
  and `H ∝ L`, so the two cancel. The problem is not load, it is distribution.
- **With `R = 0` the whole FFT must finish inside the callback that crosses the boundary**, so this is a spike
  repeating every `H` samples, not a smooth percentage. At `-O3` the hop-boundary/idle ratio is **202× on Apple
  Silicon and 104× on the 5950X**.
- **hostBlock does not help.** At `H = 512`, hostBlock 32 gives one spike per 16 callbacks and 64 one per 8; the hop
  mean is flat (77.11 vs 77.37 µs) because the same work moves into a different number of buckets. A larger host
  block lowers spike *frequency*, never spike *cost* — the lever is partitioning the FFT across hops, not tuning the
  host's buffer size.
- **Stereo is 2× mono, and the spike is what bounds the preset set** — ratio **1.94–2.06× above `L` = 2048** (1.69×
  at 1024, where two channels still fit in cache), so the per-channel loop is linear and every 2×-mono
  extrapolation was arithmetically sound. A host experiences an absolute deadline, not a ratio: at `L` = 65536
  stereo the hop callback spikes to 3.2–3.4 ms, busting a 128-sample callback at 48 kHz (2.67 ms) while the *mean*
  load sits at 20 % of a core; `L` = 16384 spikes to 578 µs and still clears a 32-sample callback. Hence D-008's
  bound on the (`L`, `H`) pair with the spike recorded beside it.
- Hop work scales with `F log2 F`: measured step-ups 2.02× and 2.28× against the 2.2× and 2.18× predicted.

**Re-running it:** quote the scheduler-driven column. `processHop alone` shows occasional outliers (156 µs at
L = 512, 224 µs at L = 2048) far above its own p99 in only some rows — a scheduling hiccup, cause not measured. Any
CPU figure quoted without its build type is meaningless (F-008).
## F-008 — The default configure emitted no `-O` flag: everything including the shipped plugin built `-O0`

**Status:** Finding, fixed — `CMAKE_BUILD_TYPE` now defaults to `Release` for a single-config configure, and
`option(ENTANGLE_ASSERTS … ON)` keeps the tripwires alive at that default. Every CPU figure recorded before the fix
is an `-O0` number and needs re-measuring before it is quoted (F-007).

`CMAKE_BUILD_TYPE` was empty in the cache and `CMAKE_CXX_FLAGS` empty, so **no translation unit received an
optimisation level** — the default when no `-O` is given. Read off ninja's own command graph:

```
/usr/bin/c++ -I…/src/Grain -I…/src/Engine -I…/src/Container -I…/src/Fft -std=gnu++2b -arch arm64 -c GrainEngine.cpp
```

`FftJuce` and the plugin picked up `-DNDEBUG=1` from JUCE's interface definitions and still got no `-O`, so the FFT
wrapper and the engine were unoptimised too. `CMAKE_CXX_FLAGS_RELEASE` (`-O3 -DNDEBUG`) sat unused, and this was the
configuration a plugin binary was actually shipped in, not merely a test-harness artefact.

```cmake
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()
```

The test is on the *value* and the set is `FORCE`d, so a tree cached with an empty `CMAKE_BUILD_TYPE` self-heals on
its next reconfigure; an explicit `Debug` or `RelWithDebInfo` is respected and a multi-config generator is left alone.

**The consequence that had to be decided:** `Release` adds `-DNDEBUG`, which compiles out all 26 `assert()`s in
`src/`, and the test programs inherit that — including the scheduler's short-delivery tripwire
(`assert(delivered == arrived)`, `HopScheduler.cpp:121`, D-004's forbidden-pad guard) and the `L % H` / `F >= 2L−1` /
power-of-two prepare invariants in `GrainHistory`, `OlaAccumulator` and `RingSpan`. They are kept: `ENTANGLE_ASSERTS`
carries `-UNDEBUG` on an `EntangleAsserts` interface target, linked `PRIVATE` into `Engine`, `Grain` and `Fft` — the
JUCE-light libraries that own the tripwires — and into `GrainTests` and `FftTests`, whose own translation units
instantiate the header-inline checks in `Window.hpp`, `RingSpan.hpp`, `Spectral.hpp` and `Dft.hpp`.

**Why per-target and why not everywhere** (measured in isolated projects at `-DCMAKE_BUILD_TYPE=Release`):

| Wiring | Assert inside a static library's `.cpp` |
|---|---|
| no flag | `exit=0` — compiled out |
| `-UNDEBUG` as `INTERFACE` on the library, consumed by the **test executable** | `exit=0` — **still compiled out** |
| `-UNDEBUG` reaching the **library's own** compile lines | `exit=134` — live, at `-O3` |

A consumer's flags do not rebuild a static library's objects, so "add it to the test targets" does nothing for the
asserts in `HopScheduler.cpp` and `Grain/*.cpp`. Equally it must **not** be global: `juce_TargetPlatform.h:145-146`
turns `JUCE_DEBUG` on when neither `NDEBUG` nor `DEBUG` is defined, so a project-wide `-UNDEBUG` would put JUCE
itself into debug mode; and applied to a TU that includes JUCE headers (`FftJuce`, the plugin, `OfflineRender`,
`GrainEngineJuceTests`, `FftJuceTests`) it changes JUCE's class layout in that TU while the modules were built with
`NDEBUG` — an ODR break. Those targets keep `NDEBUG`.

**Stated plainly:** `vstplug` links `Engine` and `Grain`, so the objects it links carry live tripwires — an invariant
violation in a host is `abort()`, not a silent wrong number. No configuration keeps the tripwires in `ctest` and not
in the plugin, because both link the same objects. `-DENTANGLE_ASSERTS=OFF` is the knob for a build that must not
abort, and it turns them off for the tests too. Measured both ways: `ON` → `exit=134`, `OFF` → clean exit, `-O3`
present in both.

**Still open:** `JuceFft.cpp:31`'s power-of-two guard stays compiled out at `Release` because it sits in a
JUCE-including TU; converting it to `jassert` would be the fix.

## F-009 — `libFftJuce.a` holds the whole JUCE module closure, and non-PIC objects only fail when a shared object links them

**Status:** Finding, fixed by `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` in the root `CMakeLists.txt`.

The first `vstplug_VST3` link — the first time anything here linked the static libraries into a shared object —
failed on a symbol nobody had edited:

```
ld: relocation R_AARCH64_ADR_PREL_PG_HI21 against symbol
    `_ZN4juce10FlacReader14errorCallback_EPvPKci' which may bind externally
    can not be used when making a shared object; recompile with -fPIC
    src/FftJuce/libFftJuce.a(juce_audio_formats.cpp.o)
```

Adding `-fPIC` to `FftJuce` alone moved the failure twice rather than fixing it: next
`libGrain.a(GrainEngine.cpp.o)` against a vtable, then `libEngine.a(DryWetEngine.cpp.o)` against
`_ZN6engine12DryWetEngine5resetEv`.

- **The archives are bigger than their sources:** `ar t build/release/src/FftJuce/libFftJuce.a` returns 12 objects
  for one `JuceFft.cpp` — nine of them `juce_core`, `juce_audio_formats` and FLAC. `juce_add_module` attaches a
  module's `.cpp` as `INTERFACE` sources, so the library that first pulls in `juce_dsp` absorbs the closure — which
  is why a FLAC error handler is the first thing the linker complains about when the change under test was a plugin
  engine.
- **Non-PIC static objects fail only at the shared link, and only for addresses.** CMake defaults
  `POSITION_INDEPENDENT_CODE` off for a static library, and everything linked so far was reachable through calls,
  which a non-PIC `BL` resolves fine. What breaks it is a TU taking *the address* of something with external linkage
  — a vtable reference or virtual call — because that needs an `ADRP`+`ADD` of the symbol's page, and an `ADRP` that
  could bind externally cannot be relaxed. Installing a polymorphic `Engine` under a `std::unique_ptr` is exactly that.

**Why the fix is project-wide:** per-target `-fPIC` turned a single error into whack-a-mole across three archives in
a row, each fix hiding the next. Every static library here is reachable from `Entangle.so`, and the plugin is the
only shared object, so no target pays for PIC and gets nothing.

**Revisit trigger:** a second shared object with different flags, or an embedded/older toolchain where PIC costs
measurable registers on a hot loop — neither exists today. The cost of PIC on this workload was never measured.

## F-010 — A JUCE float parameter with a non-dyadic interval cannot express its own default or endpoints

**Status:** Finding, worked around in D-011.

A `juce::AudioParameterFloat` does not hold the value it was given. It holds a normalised float, and a
`NormalisableRange` with an interval snaps on the way through, in two places that both lose the last bits:

- `convertTo0to1(v)` then `convertFrom0to1(n)` — a value whose proportion of the range is not dyadic does not come
  back as itself. For a `[-60, +12]` dB range with no interval, the default 0 is `60/72` normalised and returns as
  **−1.96e-06 dB** — a stage running at **0.99999977** instead of unity.
- `snapToLegalValue(v)` = `start + interval * floor((v − start) / interval + 0.5)`. For `[-1, 1]` with the `0.01`
  interval the blend shipped with, that is `-1 + 0.01f * 100` = **−2.23517418e-08** — the parameter's *own default*
  is not zero. The topmost position fares worse: `-1 + 0.01f * 200` returns **0.99999994**, one step of the float
  grid inside 1.0.

The evidence routes differ and should be recorded as such: `−2.23517418e-08` was read straight off
`apvts.getRawParameterValue(id)->load()` at the default, while `0.99999994` is inferred from the same
`snapToLegalValue` expression — its direct evidence is the four `p = +1` legs, red by a few ULP and green once the
step became dyadic. Together they explain two legs that looked like DSP bugs: a `3.5e-08 × A` leak of dry signal in
a render that had to be pure convolution — `9.26354e-09` at the first sample, which is
`sin(2.2e-08 · π/2) × a[0]` — and a `p = +1` render that kept `7.05e-08` of the wet stream because a p that stops
short of 1.0 never reaches a law's endpoint snap.

**Why it bites here:** a UI reading the parameter formats to two decimals and sees `0.00` either way, and ordinary
plugin work never compares two floats for equality. Where the criteria are bit-exactness at `p = ±1` and at unity
gain, a residue in the *parameter* is indistinguishable from a residue in the law — and invisible in the UI.

**The rule.** Any parameter whose value a test compares bit-exactly needs a dyadic quantum, and the test belongs on
the round trip rather than the arithmetic: `-1 + 256 × (1/128)` is exactly `1.0f` and `-60 + 480 × 0.125` is exactly
`0`, in every evaluation order. So the blend runs on a `1/128` step (257 positions, finer than the 0.01 it replaced)
and the output gain on `0.125` dB. A decimal step can be repaired on the read side — `roundf(v * 100) / 100` — but
that leaves the UI, the parameter and the DSP holding three slightly different numbers, which is the situation this
finding exists to avoid. A leg that compares a saved value compares the writer's held value, not the literal.

## F-011 — The delta anchor is DC-heavy, so a filter in front of the physics legs un-pins the engine's gain

**Status:** Finding.

The three physics legs — nothing before `n0 + R + L`, a Hann of peak 1.0 inside the window, nothing after
`n0 + R + 2L − 1` — are the harness's only assertions that pin the engine's *absolute* gain; everything else compares
one render against another. They ran against whatever the shell emits, which was fine while the shell added nothing
but delay and sum, and stops meaning anything the moment the shell contains a filter: the legs then measure the
filter, and an engine change that alters gain can hide behind it.

**Why this anchor in particular:** the response is a Hann of length L, mean 0.5 against peak 1.0, main lobe ±2 bins
(±2·sr/L — ±11.7 Hz at L = 8192, 48 kHz). It is dominated by exactly the band a DC blocker exists to remove: at the
shipped corner the shipped chain's response peak is 0.414009 where the leg demands 1.0, and `|y|` past the analytic
support is 0.0827042 where the leg demands ≤ 1e-5.

**Measured** (legs pointed at the shipped chain): all three shapes go red, unevenly. The post-support residue fails
by four orders of magnitude — 0.0827042 at L = 8192, 0.364114 at L = 1024, 0.0104987 at L = 65536, against 1e-5 —
while the window deviation fails by 2 % (1.0176e-5 at m = 61, 1.193e-5 at m = 16, 1.0007e-5 at m = 244). So the
support leg is what catches the filter; the window leg catches it only by luck of tolerance, and widening a
tolerance to accommodate a filter would delete the gain pin as a side effect.

**The rule.** Engine physics runs on a chain variant with no safety stage (`entangle::ChainVariant::RawWet`); the
shipped chain is asserted against the same chain built without the shell, which is where a stage that moves the
arrival index or reported latency is caught; the arrival-index leg runs on `ClipperOnly`, because across `Shipped`
and `RawWet` it would measure the filter's phase rather than a delay. A stage added to the output path gets a
variant in that enumeration, not a wider tolerance on an existing leg.

**The other half of the trap:** the centre leg and gain legs compare the shipped chain against a reference staged
from the very classes the chain uses, so they pin wiring and order and cannot see a defect *inside* a stage — both
sides make the same mistake together. Unit tests own stage behaviour; the same-stage reference owns composition. Any
"the harness is green" claim about a stage's numbers has to name which of the two it rests on.

## F-012 — Four channel-map mutation predictions that did not survive the run

**Status:** Finding — recorded because each would otherwise be re-derived, and because two were load-bearing in
D-017's rationale.

- **`a[c] = c % chA` is not a mutation.** With asymmetric pairs rejected and `channelsOut ≤ max(chA, chB)`, every
  filled slot satisfies `c < ch` for a multi-channel side, so `c % ch == c`; for a mono side `c % 1 == 0`, which *is*
  the broadcast rule. The modulo alternative is observationally identical on the whole accepted set — the mutation
  left all cases green where the plan predicted four red. The leg that discriminates the index rule is the one that
  *accepts* the asymmetric case and resolves it with modulo.
- **Commutativity does not argue for rejecting asymmetric.** Under modulo, `resolve(2, 6, 6)` gives
  `a = [0,1,0,1,0,1]`, `b = [0,1,2,3,4,5]` and the swap exchanges exactly those arrays — the commutativity leg was
  green where the plan predicted red. What a modulo rule really costs is injectivity: two outputs reading one source,
  so an impulse lands in three outputs and independence stops meaning "mapped to it".
- **"Nothing half-written" cannot be mutated into view through a value-returning `resolve`.** Moving the capacity
  check after the fill loop writes indices into a local that is then discarded and a fresh value-initialised map
  returned, so the pristine check on the rejected result stays green — only the table leg went red, and on *status
  precedence*, not the fill. With this API shape the property is structural; testing it behaviourally needs an
  out-parameter, a worse API for a map copied by value onto the audio path.
- **`ctest` runs whatever binary is on disk.** Mutations caught by the `static_assert`s fail the compile and `ctest`
  then reports the *previous* mutant's failures, which read as a genuine result until the build status was checked.
  A mutation run must gate the test run on the build's exit status.

**The rule.** Write mutation predictions as predictions, run them, and let the measured column overrule the plan —
including when the overruled claim justified a restriction. Where a criterion's discrimination is structural rather
than behavioural, say so in the criterion rather than let a later reader conclude the test does work it cannot do.

## F-013 — A rule keyed to the main bus leaves commutativity with no domain to test over

**Status:** Finding.

The accepted map was symmetric in (`chA`, `chB`) before D-018, which is what let a commutativity leg swap every `ok`
triple with a sidechain and check the two index arrays exchange bit-exactly. D-018 keys the rule to the **main**
bus: a mono main with a wide sidechain consolidates to one channel, a wide main with a mono sidechain broadcasts to
`chA`. Different channel counts, so the swap partner for the whole (`1`, `> 1`) family is a layout of a different
shape.

| Family | Swap partner | Non-trivial? |
|---|---|---|
| `chB == 0` | none — the swap moves to `empty` (A is mandatory) | n/a |
| `chA == chB` (incl. `1, 1`) | itself; both sides map by index | no — identity both ways |
| `chA > 1`, `chB == 1` | (`1`, `chA`) → consolidated, one channel | no — different channel count |
| `chA == 1`, `chB > 1` | (`chB`, `1`) → broadcast, `chB` channels | no — the same asymmetry from the other side |

The only pairs still swappable are diagonal, where both sides map `c → c`. **A leg written as "swap and compare"
becomes a tautology.** When consolidation landed, the `(1, 2) → 2 outputs` pin and the leg's wide-domain guard went
red while its companion stayed green — exactly how a half-dead leg looks on the console.

**What follows:** "A-mono with B-stereo equals B-stereo with A-mono" is superseded because A and B are not
interchangeable roles — A is the track's own signal, B is the sidechain, and a rule may key on that. Do not re-add
commutativity as a discriminator for anything in this file's history: F-012 measured that a modulo map commutes
cleanly, so it discriminated nothing even with a wide domain.

**Landed as a restatement, not a deletion:** the leg is named
`"commutativity survives only on the equal-count diagonal, where it is trivial (F-013)"`, checks the diagonal
triples under the ceiling, and asserts per slot that both sides read index `c` — the proof that the swap it then
performs is a tautology. It asserts the two families that lost their partner (`(4, 1)` → 4 channels, `(1, 4)` → 1)
side by side, and inverts the old wide-domain guard (`resolve(1, 4, 4)` was `ok`) into the positive claim that such a
request is now `countMismatch`.

**The general rule:** a property that holds only while the rule is symmetric in its two arguments cannot survive as
an acceptance criterion once the rule is keyed to which argument plays which role. When a decision breaks that
symmetry, check which criteria still have a *domain* before carrying any forward — a criterion whose test set has
quietly become the diagonal still reports green, and green is what makes it dangerous.
## F-014 — A buffer sized by the processed count and indexed by the source count: a live overflow 158 green legs could not see

**Status:** Finding, fixed. Found by sanitizer, not by reading.

`HopScheduler::prepare` allocated `hopBufferB` with `hop * config.channels` — the A-side width — while `process`
wrote it with `hop * config.sidechainChannels`, one slice per B ring. Any layout where the sidechain is wider than
the main input wrote past the end of that allocation:

```text
WRITE of size 4 at 0x511000000280 thread T0
    #1 container::CircularBuffer<float>::read(float*, unsigned long)
    #2 engine::HopScheduler::process(...) src/Engine/HopScheduler.cpp:149
```

The layout is not exotic: the gate checked `mainIn == mainOut` and capacity, never the relation between the two
input buses, so a mono main with a stereo sidechain — a normal thing to route — was accepted, configured and run.
Nothing in 158 test cases touched it, because every leg that drives a sidechain used matching widths: the value
tests asserted arithmetic on a buffer whose *extent* nothing asserted.

- **A value-level suite gives no evidence about memory safety, and no number of green cases buys any.** A wrong
  extent is not a wrong number — samples are read and written correctly inside the slice, and the overrun lands in
  slack that allocator rounding may have supplied anyway, at a size where the run stays silent and the next `free`
  decides. The check is a sanitizer run over the shapes the rule permits, not another assertion on the samples.
- **Derive the extent and the index from the same source value.** The fix is not a bound — bounding the write would
  hide a wrong answer — it is that sizing is per *source* on the way in (`chA` A rings, `chB` B rings) and per
  *processed* channel on the way out, with `prepare` throwing on a map whose index names a ring that does not exist.
  The consolidation family is exactly where the two counts differ, which is why a rule about *levels* (D-018) became
  a memory-safety finding.

**Closed by** source-sized rings with processed-count pointers, plus an ASan/UBSan repro over `(1,2,1)`, `(1,8,1)`,
`(2,1,2)`, `(2,2,2)`, `(2,0,2)` exiting 0 with nothing on stderr; re-running the old sizing reproduces the fault. A
prediction that "no value leg can see it" measured as glibc aborting four legs on this host — a crash falling out of
allocator state, not a test that would fail. The memory-safety leg is now part of every such item's bar.

## F-015 — JUCE asks the gate two different questions, and only one is the layout the host asked for

**Status:** Finding, measured; no code change.

- **The clean path.** `AudioProcessor::setBusesLayout` → `canApplyBusesLayout`, which defaults to
  `isBusesLayoutSupported(layouts, /*allowReplacing=*/false)` (`juce_AudioProcessor.cpp:118-131`). Counts reach the
  gate exactly as the host wrote them and a `false` applies nothing: over 10 refusal shapes — wide main, unequal
  mains, `chB` 3…9 on a stereo main, `chB` 9 on a mono main, a 0-channel main — 0 layouts applied, 0 buses moved. Bus
  *count* changes are checked before the virtual (`checkBusesLayoutSupported`, `:222-227`) and
  `canAddBus`/`canRemoveBus` default to false, so the inventory is fixed at 2 in / 1 out.
- **The substituting path.** `AudioProcessor::Bus::setLayout` (`:196-206`) never asks the whole question: it calls
  `getBusesLayoutForLayoutChangeOfBus` → `getNextBestLayout` (`:231-330`), whose ladder is the request as-is; the
  *opposite* bus at the requested width; the opposite bus at its default; all buses at the request; then whichever of
  default and current is nearer. Whatever that returns is applied **without `canApplyBusesLayout` consulted again**,
  because the rung that produced it is JUCE's own — so a per-bus probe can move a bus the host never mentioned.
  Measured from a mono origin: setting the main input to stereo is "honoured" and lands on `(2, 0, 2)`; setting the
  main output to stereo does the same; setting the sidechain to stereo lands on `(1, 2, 1)`, which the gate genuinely
  accepts. 3 of 12 probes were honoured and 0 landed on a refused layout — the rungs stay inside the accepted family
  because every rung keeps `mainIn == mainOut` — but two of the three honoured probes moved a bus nobody asked about.
  From a stereo origin, all 9 wide requests are refused and nothing moves.
- **The advertisement depends where you start.** `Bus::getMaxSupportedChannels` (`:1082-1090`) and
  `supportedLayoutWithChannels` (`:1140-1159`) walk widths asking the gate *with the current layout in place*, so the
  same plugin advertises a sidechain maximum of **8 beside a mono main** and **2 beside a stereo main**. That is a
  correct consequence of a rule that keys on the pair (D-018), not a defect; what is wrong is any statement of the
  form "the sidechain takes up to 8 channels" with no main-bus qualifier.

**The rule.** A gate can only be asked about whole layouts, so it cannot prevent substitution — only guarantee that
whatever JUCE lands on is a layout it serves. For everything downstream (this repo's harness, any preset loader, any
host-facing doc): **drive layouts with `setBusesLayout`**; use the per-bus probe only to learn what JUCE would
substitute, expecting that answer to depend on the layout you are standing in.

## F-016 — A footprint figure that re-evaluates the sizing expression cannot notice the expression changing

**Status:** Finding, fixed: the floor has one home (`maxHostBlockFor`, called by the harness), a restated figure is
labelled a restatement, and the measured tables come from `tools/PrepareFootprint`.

- **The documented bound understated the engine by 1.5F.** `EntangleProcessor.h` stated the cap's footprint as
  "stereo, 9F + 6 floats of engine buffers plus the transform's 2F scratch, so ~704 KB at the default 8192 and
  ~5.5 MB at this cap". The allocation sites say otherwise: two history rings and one accumulator ring per processed
  channel totalling 2F (`GrainHistory.cpp:19-21`, `OlaAccumulator.cpp:18-20`), six shared scratch vectors at 6F + 6
  (`GrainEngine.cpp:60-65`), the Hann table at L = F/2 (`Window.cpp:13`) and `JuceFft`'s 2F in-place scratch
  (`JuceFft.cpp:29`) — **10.5F + 6 floats at one processed channel, 12.5F + 6 at two**. At L = 8192 (F = 16 384)
  that is 800 KiB against the comment's 704; at L = 65 536 (F = 131 072) it is 6.25 MiB against 5.5 MB. Measured:
  812.5 KiB and 6 447 KiB for the stereo engine. The residue over the corrected bound — 7.1 KiB mono / 12.5 KiB
  stereo at the default, 26.6 / 47.0 KiB at the cap — is `juce::dsp::FFT`'s own tables plus the three safety stages,
  and it grows with F, the half the comment left out: the transform's tables are not in the bound and cannot be,
  because the backend is chosen at runtime.
- **A harness figure that copies production arithmetic is a figure about the expression.** One section built
  `CircularBuffer<float>(maxHostBlockSamples + hop)` and read `capacity()` — a second copy of the scheduler's sizing
  — so changing that expression leaves the leg printing the old number with the suite green. It was not isolated:
  two harness `ChainConfig`s recomputed `std::max(kMaxBlock, maxHostBlockSamples)` and a third set `maxHostBlock` to
  the bare constant where the shell applies the floor. Four copies of a policy expression, none of them the policy.
  The proving mutation: reverting the hop-scratch sizing to F-014's shape makes `OfflineRender` die of heap
  corruption before the footprint block runs, without a single leg failing.

**The rule.** A printed figure about a production expression either **calls the production function** or says out
loud that it is a restatement and points at the measurement. Measuring costs one binary with a replaced global
`operator new` — and that is also why it cannot be folded into anything that exists: one `operator new` per binary, so
`PrepareFootprint` is its own target and `OfflineRender` stays free of the hook.

## F-017 — A leg whose expected output is produced by the component under test cannot witness a mutation inside it

**Status:** Finding, fixed by requiring an absolute half on every such leg.

Two discriminator arms that should have been caught at render level were not. Replacing the scheduler's
`hopA[slot] = hopBufferA + map.a[slot] * hopSpan` with `slot` printed **175 `ok` and 0 FAIL** in the channel matrix,
the only red in the whole suite one `EngineTests` case. Resolving a broadcast B slot to silence instead of its source
left both broadcast cells green while the consolidated cells reddened. The reason is structural: every model the
matrix compares against reaches the chain through the same reference renderer, which builds **its own
`engine::HopScheduler`**. A mutation inside the scheduler moves the render and its expectation together, so
`max |got − want|` stays `0.000e+00` and a bit-exact verdict prints next to wrong audio. The leg is not weak because
its tolerance is loose; it is weak because what it measures and what it predicts share the code under test.

- **The fix is a second, model-free half on the same leg:** a mapped slot must carry a sample in absolute terms
  (`deadMapped`, counted as `!(slotPeak > 0.0)` and printed as `%d dead`) *before* the comparison decides anything.
  With it the broadcast-silence arm reddens **37 lines** including both broadcast cells. Silence was already asserted
  absolutely (an unmapped output must be exactly `0.0f`) and that half caught the fold-dropping-its-last-source arm
  at 10 lines on its own — the sign the two halves do different jobs.
- **A harness crash that was the same finding.** The arm where `applyLayout` ignores `bus.sideSet`, and its
  matrix-local twin that applies one layout while keeping each cell's literal expectations, both died with SIGSEGV —
  the second inside the matrix checker, the log ending mid-section only because stdout is block-buffered. The mirror
  of F-014: the preflight filled its callback from `getChannelCountOfBus`, the *applied* counts, while its streams are
  sized by the cell's, so `(1,0,1)` read `b[0]` on an empty `Stream`. A leg whose job is to *report* a layout that is
  not the one it declares must not be the first thing to dereference it. With the fills bounded by the streams and the
  sample legs gated on the preflight, the same mutant prints **9 FAIL** lines, the cell whose declared layout is the
  injected one runs all six legs green, and nothing crashes.

**The rule.** Any leg whose expectation is computed rather than written down has to say which of its assertions
survive the expectation being wrong, and at least one must be absolute: a non-zero sample, an exact `0.0f`, a byte
count, a name. A comparison against a model states a difference between two paths, and a mutation the paths share is
invisible at every tolerance — the more machinery the harness reuses, the more of the product it has quietly placed
above test. Reusing the chain is still right (the alternative is a second implementation to keep in sync); what was
wrong was treating the comparison as the whole leg.

## F-018 — JUCE claims a message thread for whoever constructs one, calls a disabled bus `Discrete #0`, and a prepare-snapshot report can never name a refusal

**Status:** Finding, measured.

- **`MessageManager`'s constructor records the thread that built it** (`messageThreadId
  (Thread::getCurrentThreadId())`, `juce_MessageManager.cpp:38`), so `initialiseJuce_GUI()` — and therefore
  `ScopedJuceInitialiser_GUI` — makes that thread the message thread without any
  `setCurrentThreadAsMessageThread()`, which exists in exactly one place in JUCE: the plugin client
  (`juce_audio_plugin_client_VST3.cpp:280`). So (i) a console harness that initialises JUCE's GUI satisfies the
  report's thread guard on `main()`, sending the report line out on **every** prepare and interleaving it into every
  earlier section — a byte-comparison leg therefore needs a logger in `main()` that drops report lines, replaced
  inside the report section by one that counts them; (ii) the negative branch cannot be rehearsed by "doing nothing" —
  it takes a real second thread. For a host this cuts the other way: the wrapper designates its own thread, so a host
  that prepares on the audio thread shows no log line anywhere, which is why the on-screen readout is a delivery
  rather than a convenience.
- **`AudioChannelSet` has no `getName()`** and `disabled().getDescription()` returns **`Discrete #0`**, because
  `getDescription()` tests `isDiscreteLayout()` before comparing against `disabled()`
  (`juce_AudioChannelSet.cpp:424`). A line saying `side=0 sideName=[Discrete #0] sideEn=0` is JUCE saying *there is no
  send*; the count with the enable flag is what a reader must trust, not the words. Pinned as a leg, because the
  tempting "fix" — print `Disabled` — loses the verbatim property the report depends on.
- **The naming table for the shapes the report can carry**, verbatim: `0:Discrete #0 1:Mono 2:Stereo 4:Quadraphonic
  4:1st Order Ambisonics 6:5.1 Surround 8:7.1 Surround 8:Octagonal 8:Discrete #8 9:2nd Order Ambisonics
  9:Discrete #9`. Width 4 is shared by quadraphonic and first-order ambisonic and width 8 by three shapes, so the
  name distinguishes them where `processed` does not — the treatment question the surround deferral leaves open, now
  with the words attached.
- **A report assembled at prepare can never carry a refusal word.** `configureRings` runs only for a layout that was
  applied, and 0 of 192 probes landed on a refused one, because `isBusesLayoutSupported` refuses before JUCE builds
  anything. So `gate=mainWidth` and `gate=channelRule` are unreachable in a pasted line, a "refusal sample" clause in
  a report is unachievable as written, and the vocabulary is pinned where it *is* observable: the probe table and the
  `gateLayout` comparisons, with the counters as evidence the attempt happened. Same reason one mutation arm is half
  an equivalent mutant: "publish a processed count for a refused triple" cannot be reached from a console; "rebuild
  on an arrival that changed nothing" can, and that is what reddens it.
- **`Logger::~Logger` asserts the dying object is not the installed one** (`juce_Logger.cpp:44`: *"You're deleting
  this logger while it's still being used!"*), and a stack-allocated filter in `main()` is exactly that case.
  Invisible in `Release`, where JUCE's assertions are compiled out; a Debug run reddens on the way out even though
  every leg printed `ok`. `OfflineRender` calls `Logger::setCurrentLogger(nullptr)` before returning, and a Debug tree
  configured from scratch prints 0 assertions with the same `ok` tally.

**One number to stop re-investigating:** `OfflineRender` under LeakSanitizer exits 1 reporting **14.48 MB across
161 540 allocations in 297 records, every one** rooted in `juce::dsp::FFT`'s FFTW backend — `FFTWImpl::create`
(`juce_FFT.cpp:84`), under `fft::JuceFft::Impl`, reached from `configureRings`. 274 records (12.0 MB) come from legs
that predate the report and 23 (1.68 MB) from its own fresh-instance prepares: one cached plan set per transform,
never returned to the OS. No invalid access, no UB, and the report's own allocation appears nowhere. Read it as
JUCE-plus-FFTW global planning state on Linux, not product leakage; vDSP on macOS is unmeasured here.

## F-019 — `-O3` deletes an untouched `new`/`delete` pair, so an allocation counter can read zero through a working replacement

**Status:** Finding, ruled on. The repo's other allocation shims are **not audited** against it.

The count-one control of a no-allocation tripwire printed 0 on a `-O3`/`-flto` build. The shim was not broken: the
binary defined the replaced `operator new`, `operator new[]` and the aligned forms, and `nm` showed exactly one
`allocwatch::allocations`, so the header's inline state was shared as intended. The allocation had simply never
happened — `new float[64]` followed by `delete[]` with nothing written in between is removed by the optimiser before
the call is ever made. Writing one element and reading it back turned the count into 1. The trap runs the other way
for the count-none control too: a loop whose result nothing checks is also deleted, so its zero proves only that
nothing was left to run — which is why that leg now asserts `H(4 096) = 8.895 103 896 966` outside the armed window.

**The rule.** A no-allocation leg must keep the allocation it counts *observable*: write to the storage, read it
back, consume whatever the surrounding arithmetic produces. A counter that reads zero is not evidence until something
proves it could have seen an allocation, and the cheapest such proof is a count-one control in the same binary, in the
same armed-window idiom, that could only print 1 if the path really allocated.

**Not audited:** the six existing shims (`src/Dsp/tests/AllocationWatch.hpp`,
`src/Grain/tests/AllocationWatch.{hpp,cpp}`, the inline versions across `src/Engine/tests`, and
`tools/PrepareFootprint`'s byte counter) were not re-checked against this trap. Whichever item first states a verdict
about the shell's hop path from one of them owns that audit, in the same build configuration as the run that quoted it.

## F-020 — A static archive lets a recompiled copy of a shell TU replace the plugin's own, and every leg stays green

**Status:** Finding. "Link the plugin's shared code, never recompile the shell" is a build-file rule, not a
link-time guarantee.

Adding `src/Entangle/EntangleProcessor.cpp` to the harness shell library's sources **links, and both consumers stay
green** — the monolith at 458 lines / 204 `ok` / `PASS` and exit 0, the shell test binary exit 0 with all cases
passing. The link map says why there is no diagnostic: only one member supplies `createPluginFilter()`,
`libEntangleHarnessShell.a(EntangleProcessor.cpp.o)`, and `libEntangle_SharedCode.a(EntangleProcessor.cpp.o)` is
**absent from the map entirely**. The harness library precedes the plugin archive on the link line, the symbol
resolves while the harness archive is scanned, and the plugin's copy is never pulled — the two copies never meet, so
the linker has nothing to complain about. The duplicate even compiled without the plugin target's `PRIVATE`
definitions (`JUCE_APPLICATION_NAME_STRING`, `JUCE_APPLICATION_VERSION_STRING`), which is how a shell built with a
different macro set ends up serving a test binary silently.

The contrast case is the same rule applied to a TU linked unconditionally rather than pulled on demand: re-adding a
helper to `OfflineRender.cpp` fails with `multiple definition` and `collect2: error: ld returned 1 exit status`.
Archive-member laziness is the whole difference.

**The rule.** The actual protection is that `tools/harness/CMakeLists.txt` names no source under `src/Entangle`, and
the identity legs cannot strengthen it: they run two *routes through whichever single class is served*, so a second
copy compiled from the same source passes by construction. A future harness target that wants shell code — a stub
engine, a test-only variant — must carry its own source, not the real file. A target that must guarantee presence uses
an `OBJECT` library or `--whole-archive`, where both copies link and the duplicate check is live again.

**Not measured:** whether a copy that *differs* from the plugin's would redden anything; the expectation is it would
not, for the same reason the identical one did not — which is the gap this entry records.

**Revisit trigger.** Any harness target adding a source under `src/Entangle`; any change to the order of
`EntangleHarnessShell` and `libEntangle_SharedCode.a` on a link line; any conversion of either to an `OBJECT` library,
a unity build, or `--whole-archive`. Then "which copy serves?" must be answered with a link map before any identity
claim is trusted.

## F-021 — A plan-invariance suite cannot be the length guard: a long-only DSP defect leaves both halves green

**Status:** Finding, binding on the run sets.

A mutation that makes the accumulator land the grain `H` late whenever `ring.size() > 2048` is real and long-only:
inert at `F` = 2048 — the short-`L` run's capacity — and live at the shipped `F` = 16 384 and 65 536. The monolith
exits **1** with **5** `FAIL` lines, every one a physics leg at a shipped shape (`ch0 wet m=9 = 3.5677e-08 want
1.19125e-05`, `ch0 n32896 = 0.00240759 after the wet window`, `the audible response lies outside its analytic
support`, and the same pair again at `H` = 512 `L` = 65536). The cross-product binary exits **0** with **16/16** cases
passing in either half — a structural reason, not a weak mutant.

**Mechanism.** Every expectation in that binary is one of three things. A **plan delta** compares two renders of the
same engine fed the same total sample count; the hop grid is a function of the arrival total, not of where callback
boundaries fell, so a defect keyed to absolute hop position moves both sides equally and the delta is `0.0` — F-017
with the arrow turned the other way: not "the model is built by the component under test" but "the *baseline* is". An
**absolute claim about the dry path** (`out == A` delayed by `R`, an unmapped slot exactly `0.0f`) is untouchable by
the accumulator: with the opposite side silent the wet is exactly zero whatever the ring does. The remaining absolute
claims — a mapped slot's peak `> 0.0`, a folded response `1/chB` of a single-channel render — are presence and ratio,
both of which survive misalignment because the ratio is shared: the model render is misaligned the same way.
Lengthening the run cannot help, because the blindness is a property of the *comparison method*, not the window.

**The rule.** A long arm earns its cost only by carrying an expectation that does not share the code under it — an
independent oracle (the analytic-support and raw-wet physics legs, where this mutant actually died) or an absolute
claim about the wet's *value* rather than its presence. Cross-product-style length coverage holds something real and
smaller: geometry, gate verdict, mapping, declared `R`, the fold reciprocal and the whole callback set at the shape
the plugin ships on. Do not justify a long half with "length-dependent bugs live there" — name the leg that would
catch one. For a fuzzed callback-plan corpus over this baseline the same constraint is live: its value is the
*coverage of shapes*, and any DSP claim it makes needs the oracle beside it.

**Not measured:** whether a defect changing the wet's *magnitude* rather than its alignment reddens anything in the
cross product — the `> 0.0` peak check only catches a dead response and the ratio check shares the error.

**Revisit trigger.** Any long arm proposed on the strength of catching a length-dependent DSP defect; a fuzz corpus
pointed at this baseline; any leg that wants the fold or the wet asserted against an absolute figure at the shipped
`L`. The oracle has to be named before the run set is.
## F-022 — `LookAndFeel_V4` takes nine slots for five colours and re-derives the widget table behind your back; JUCE 9 resolves no typeface for a family it cannot match

**Status:** Finding. Superseded on the font half by D-025, which embeds the faces so nothing resolves by name; the
colour-side facts stand.

Both halves — a `LookAndFeel_V4` subclass between a five-token palette and JUCE's widget colour table, a font
factory between a seven-role ramp and the platform's faces — looked like mapping exercises. Neither is, and one of
them segfaults.

**Colour side.**
1. **Nine slots, asserted:** `ColourScheme`'s constructor is variadic with `static_assert(numValues == 9)`, so five
   tokens cannot be five colours. Shipped mapping: ground ×1, track ×2 (`widgetBackground`, `menuBackground`), fill
   ×2 (`defaultFill`, `highlightedFill`), ink ×4, trace ×0 — collisions load-bearing, because what a slot cannot say
   has to be said by a widget.
2. **`setColourScheme()` erases work done before it:** it runs `initialiseColours()`
   (`juce_LookAndFeel_V4.cpp:83-87`), reassigning the entire derived widget-id table. Six of the nine ids the skin
   overrides live in that table, so constructor order is the difference between a themed bar and a JUCE-coloured one,
   with no diagnostic — a misplaced scheme call leaves the build green and reddens `Slider::trackColourId`,
   `Slider::thumbColourId` and the rest.
3. **The base class manufactures off-palette tones:** three ids are built as `.withAlpha (0.5f)` or `.withAlpha (0.4f)`
   of a scheme colour, and `TooltipWindow::backgroundColourId` comes from `highlightedFill` with
   `TooltipWindow::outlineColourId` set **transparent** — on the one floating surface this plugin shows a host by
   default, since the layout report travels in a tooltip. Palette purity therefore holds for the ids the skin
   overrides plus those the scheme derives, **not** every id in JUCE; hence the wording "every colour the product
   *names*".
4. **The default scheme is dark**, so a `Theme` that forgot `setColourScheme` would still render — in the wrong
   palette. There is a leg for exactly that.
5. **There is no `getColourScheme()`, and the read path is a write path:** `currentColourScheme` is private and
   `getCurrentColourScheme()` hands out `ColourScheme&`, so reading the palette also sets it. Both are shadowed in
   `Theme` — and **shadowing does not produce `false` in a `requires` expression**: naming an inaccessible member is a
   hard error outside the immediate context, and GCC stops the build with
   `"'void juce::LookAndFeel_V4::setColourScheme(ColourScheme)' is inaccessible within this context"`. So "no public
   setter" cannot be a compile-time negative in the test binary; it is decided by a leg over `Theme.hpp`'s text
   asserting both `using` declarations sit in the private section with nothing re-opening the class after them, plus a
   scratch TU that makes the two calls and is refused. The residual is named: through a `LookAndFeel_V4&` both setters
   remain live, which is what the source-policy case is for.
6. **Tooltip colours are `TooltipWindow::{backgroundColourId,textColourId,outlineColourId}`** in JUCE 9 — there are no
   `Desktop::tooltip*` ids, and searching for them wastes a build cycle.
7. **Read the scheme back through derived ids:** the palette legs assert through ids `initialiseColours()` derives
   from each slot (`TextButton::buttonColourId`, `TextEditor::outlineColourId`,
   `PopupMenu::highlightedBackgroundColourId`, `ScrollBar::thumbColourId`, the text ids), which is what gives a slot
   mutation consequences inside the item that owns the slot.

**Font side.**
8. **JUCE 9 does not substitute for a *named* family:** `Typeface::createSystemTypefaceFor(Font)` returned **null** for
   `Helvetica Neue`/`Thin`, for `NoSuchFamily`/`Thin` and for an empty family on a host with eight DejaVu files;
   `Font::getTypefacePtr()` on a `Font` carrying that name returns null too, and resolves only the generic sentinel —
   `Font{FontOptions{height}}`'s `<Sans-Serif>` came back as DejaVu Sans / Book. "Ask for Neue and let the platform
   substitute" is not what happens: a theme that asks by name draws **nothing** off macOS. D-025 ends the question by
   embedding the faces.
9. **`FontOptions(Typeface::Ptr)` does not check for null** — it copies the face's name, so a `makeFont` built from a
   `Typeface::Ptr` the resolver returned null for took SIGSEGV in `juce::String::String` under
   `FontOptions::FontOptions`. A legitimate-looking argument that turns a missing font into a crash instead of an empty
   drawing.
10. **The squeeze floor is 0.7 and it is relative:** `Font::getDefaultMinimumHorizontalScaleFactor()` is `0.7f`
    (`juce_FontOptions.cpp:51`), applied inside `createFittedText` as `minimumRelativeHorizontalScale ×
    f.getHorizontalScale()`, so it bounds *further* squeezing and cannot lift a 0.65 font to 0.7 — measured, the 0.65
    face draws "VOL" at 89 px against 137 px at 1.0, ratio 0.6496. What it does mean: a 0.65 face drawn with
    `drawFittedText` can be squeezed to **0.455**. `drawCapAlignedText` uses `drawMultiLineText`, which never enters
    that path.
11. **There is no cap-height API:** `Typeface::getMetrics()` returns ascent and descent summing to 1 and says nothing
    about caps; `GlyphArrangement::getBoundingBox()` is ascent/descent-based, not ink. Cap height must be measured from
    the outline — `addLineOfText(font, "H")` → `PositionedGlyph::createPath` → `Path::getBounds()`. Because JUCE's own
    text placement lays the baseline out from the ascent, faithful-looking em-box placement sits **10 px** low at
    `k` = 1 and **41 px** low at `k` = 3.778 on the wordmark — which is why the mockup's alignment is not reproducible
    without this.
12. **`Font()` is deprecated** in favour of the `FontOptions` constructor, so a fixed-size font cache holds
    `std::optional<Entry>`, not default-constructed `Font`s.

**The rule.** Ask for a family only in a form that resolves, and print what resolved on every host run. Treat the
scheme as write-once — order is the API's problem, so remove the choice (D-024) rather than document it. Verify a theme
through ids the scheme *derives*, never through a copy of the scheme alone. Place text by a measured cap height, and
never hand a `Typeface::Ptr` to `FontOptions`.

**Not measured:** whether any `LookAndFeel_V4` id outside the fourteen the skin reads ends up off-palette on a widget
not yet replaced; whether the 0.455 `drawFittedText` squeeze is reachable in the shipped kit — no component uses fitted
text.

**Revisit trigger.** Replacing the drawing of any widget whose id is alpha-derived or untouched by the overrides; any
use of `drawFittedText` in the kit, which has to carry the 0.455 note with it; any caller that wants to repaint the
palette at run time — the residual shadowing cannot reach, needing a design answer rather than a cast.

## F-023 — JUCE 9's paint hooks are on the widget, its leaf slider hooks are unreachable once the top-level hook is overridden, and its rasteriser allocates in every paint

**Status:** Finding, ruled into the kit's rules and its two instruments (a pixel probe and a 200-paint allocation
counter).

1. **`Component::hitTest` returns `bool`** (`juce_Component.h:915`); overriding it as `Component*` — the signature some
   examples still suggest — is a `conflicting return types` error on the first try.
2. **A hit-test leg must pass local coordinates; passing the widget's screen rect makes it pass for the wrong
   reason.** A probe leg tested `hitTest` with the *translated* body rect, which is outside the component in its own
   space, so it returned `false` whatever the widget did, and the mutation that drops the meter's refusal stayed green.
   Rewritten against `body()` untranslated with a positive control (the same point hits the same widget the moment it
   stops being a meter), the mutation reddens on the first assertion. **A refusal leg without a positive control is not
   a leg.**
3. **Once the top-level `drawLinearSlider` is overridden, `drawLinearSliderOutline` and `drawLinearSliderThumb` are
   never called.** Handing both back to `LookAndFeel_V4` — which does draw them — left all four kit binaries green,
   including the leg that counts thumb pixels inside the body. The suppression list that carries the flatness is the
   top-level hook plus `getSliderThumbRadius` (0) and `getSliderLayout`; the two leaf overrides are
   belt-and-braces against a future JUCE and documented as such, not as necessity.
4. **`paintEntireComponent (g, true)` is the headless paint entry point and does not offset by the component's own
   position** — the parent's paint pass normally applies that translation, so a probe rendering one component into a
   shared canvas must `g.setOrigin (component.getPosition ())` first or every pixel lands at the sheet's origin.
5. **`paintComponentAndChildren` skips any child failing `isVisibleWithNonZeroArea`, and `addChildComponent` does not
   set the visibility flag** — a component added that way to a panel being probed is simply not painted, with no
   diagnostic. `addAndMakeVisible` is what production and the probe do.
6. **The widget constructors are narrower than they look:** `Slider` has `explicit Slider (const String&)` and
   `Slider (SliderStyle, TextEntryBoxPosition)` — no three-argument form, so the name goes through `setName`.
   `setPopupDisplayEnabled` is `(bool, bool, Component*, int = 2000)`. `ComboBox` has no `setArrowSize`, no
   `setScrollOnPopupEnabled` and no `getLabel()` in this version: the kit's arrowless choice bar is the absence of a
   drawing in `drawComboBox`, not a flag, and its value words are the `ComboBox`'s own child `Label`, reached through
   `positionComboBoxText` and `ComboBox::textColourId`.
7. **`Slider::valueToProportionOfLength (double)` is non-const**, so the paint path that resolves the fill from the
   parameter rather than the hook's `sliderPos` float cannot take a `const Slider&`. Taking the value from the
   parameter is the choice that makes the painted fill and the drag position the same number, and it costs that const.
8. **JUCE 9 gives a paint no way to measure a string:** `Font` has no `getStringWidthFloat`, and neither
   `GlyphLayout` nor `TextLayout` is in `juce_graphics`. Right- and centre-aligned words therefore go to
   `drawMultiLineText` through a `juce::Justification`, which lays the line out against the *clip*, not a private
   measurement — adding the measurement would put an allocation inside `paint()`.
9. **`Path::clear()` — not `clearPath` — and it is `data.clearQuick()`, so capacity is kept.** Two scratch paths held
   by the `Theme` mean the dial's wedge and datum radial are rebuilt every frame allocating nothing; an enabled dial
   costs the same per paint as one face fill plus one reused-path wedge.
10. **The rasteriser's own per-paint cost is the floor, so a paint's real-time claim is a comparison.** Counting only
    inside `paintEntireComponent`, over 200 warm paints with a fresh 320×240 canvas each: a single `fillAll` component
    costs **5 allocations per paint**, a two-fill bar with two cap-aligned runs **40**, a wedge dial **6**, one text run
    **57**. An absolute zero is not a claim a JUCE-linked binary can make. What is decidable, and what the suite
    decides: the light-half calls a paint makes allocate **0**; each kit component costs exactly what the same drawing
    with no kit in it costs in the same state; and each kit widget costs exactly what the stock widget under the same
    skin costs — with the hook's count-one and count-none controls either side of those equalities.
11. **A window scale passed where the ramp's `k` belongs is silent and 3.778× too big.** Handing `kitScaleFor` (px per
    mm) to `drawCapAlignedText`, which wants px per mm divided by `kReferenceScale`, failed nothing: the widget drew,
    enormously, clipped to its box. The allocation instrument caught it because the hand-drawn reference inked
    **3 586** px while the widget inked **5 953** — more ink *and* fewer allocations, which is what a clipped giant
    face looks like. `Theme::fontScaleFor` is the only place the kit makes that division, and the inked-pixel equality
    is part of the figures leg.
12. **`ComboBox` treats item id 0 as "nothing selected"** — subtracting one from every choice id shows an empty box.
    It reddens the probe's choice-bar legs and, less obviously, the kit-vs-stock allocation leg, because a box with no
    selection draws no text child and so costs one allocation less per paint than the reference box with one. The
    attachment leg is where the id contract belongs; this is the earlier warning that 1-based is not optional.

**The rule.** Suppress at the top-level paint hook, not at the leaves, and keep leaf overrides only with a note saying
they are belt-and-braces. Every refusal leg gets a positive control in the same case. A real-time claim about a paint
is a *comparison* — against the same drawing without the kit and against the stock widget, plus the hook's own
controls — never an absolute count. Convert the window scale to the ramp scale at one place, and prove a text
component draws the same run as the reference by its inked pixels, not its allocation count alone.

**Not measured:** whether the `PopupMenu` a `ChoiceBar` opens honours the palette (a headless popup needs a peer);
whether a real host's text shaping lands on the same inked pixel count as this container's (3 586 is a like-for-like
control inside one process, not a port target); what the 5-per-paint floor becomes on a 10-bit or ARGB-premultiplied
surface.

**Revisit trigger.** Any JUCE upgrade, for the leaf-hook finding and the rasteriser floor; the first host run, which
decides the host halves of items 4, 5 and 8; any kit component wanting right-aligned words in a box narrower than its
text, where item 8's clip-relative justification becomes visible rather than theoretical.

## F-024 — Installing the design's own face moved four metrics the layout had quietly assumed

**Status:** Finding, from D-025's face swap, measured on both sides of it in the same container and build
configuration. Each turned up as a red leg, not as a look at a screenshot.

1. **A drawn line is one em tall, and the cap was being counted on top of it.** The truth column reserved
   `cap + n × em` per readout, which at the reference sheet is **200 px of reserve in the 183 px** the cell leaves
   above its heading: `statusLine` reached **321,21** against a cell top of **38** — ink in the sheet's margin — and
   because both figures scale with `k`, the shortfall worsened at every width above half-scale. The read in JUCE 9:
   `juce_JustifiedText.cpp` advances `nextLineTop = baseline + leading × descent` then
   `baseline = nextLineTop + leading × nextLineAscent`, with `lineHeight = Font::getHeight()` and `leading == 1.0f`
   (`ShapedTextOptions::leading`'s default; `drawMultiLineText`'s additive `leading` argument defaults to 0). The pitch
   is therefore **exactly one em**, so n lines end exactly on the bottom edge of an n-em box and the reserve is
   `n × em` with no cap term. After the fix: `statusLine` at **321,89 264x22**, section figure
   `em 22.000 px, reserve 132 of 183, ground 51 above the top line`.
2. **A reserve built by stepping off rounded figures is as many roundings deep as it has lines.** With the em correct,
   the same stack still left **4 px** of slack at 893 px wide: `round(22 × 0.750423) = 17` against a true 16.509,
   carried through six lines. Taking every edge as `round(n × em)` from the stack's own top rather than
   `previous + unit` brought the resize sweep to **1 px at 595, 1 px at 893, 1 px at 1000 (off-aspect), 2 px at
   2380**. The rule generalises: `round(n · x) ≠ n · round(x)`, and only the first is proportional.
3. **`getAscent()` was the cap height on the face of the day, so a control that relied on their differing went blind.**
   The cap-placement leg laid its control out from `getAscent()` and required it to miss the requested row; on the M10
   face the two were **the same figure at all seven roles at both `k` = 1 and `k` = 3.778** — a 0 px gap — and the leg
   was green while seeing nothing. The control now uses `getHeight()`, the em box, which cannot equal the cap for
   any face with lowercase. This is F-017's trap one level down: a control whose power comes from two metrics
   *disagreeing* has to be checked for the disagreement, not just for the miss. (Clash Display diverges — hhea ascent
   0.8900 em against a 0.6700 em cap, F-030 — so the leg's old power would have returned with the new face, which is
   the reason the fix moved to a metric rather than to a tolerance.)
4. **Words overrun their boxes: 7 of 16 with the M10 face, 5 of 16 with the host's substitute — and the ramp cannot
   fix it.** The ramp is a height ramp (`resolve(role, k) = em × k`) and the design's figures come from the mockup's
   measured **cap heights**; the widths came from whatever face the mockup was drawn in. At the reference sheet, with
   the face embedded at the time:

   | word | drawn | box | verdict |
   |---|---|---|---|
   | `wordmark` "Entangle" | 294.9 px | 264 | 11 % over |
   | `grainReadout` "8192 samples = 170.7 ms" | 274.8 | 264 | 4 % over |
   | `volumeCaption` "Volume Ctrl" | 295.9 | 264 | 12 % over |
   | `windowCaption` "Window Size" | 337.9 | 264 | **28 % over** |
   | `waveformCaption` "Waveform" | 242.4 | 227 (cell less its inset) | 7 % over |
   | `outputGain` label "Out" | 88.0 | 75 | 17 % over |
   | `normalisationAmount` label "Amt" | 95.4 | 75 | **28 % over** |

   The same leg on the substitute face read **5 of 16** over with the worst at **22 %** (`normalisationAmount`), so the
   swap added two words and moved the worst case 6 points. Not a criterion failure — the loss is bounded at half a
   string and the kit clips at the box's own edge — but it is why a caption can break inside its paint, and the table
   any width relief (a shorter string, a wider column, a squeeze on one role) has to be written against. **D-031 wrote
   that relief**: the same instrument now measures every word against the room it actually draws in and the gate refuses
   any deficit at all, so the sheet reads **0 rows over at 595, 1190 and 2380 px** with the M11 face. The table above
   is the M10 face's capture, kept because it is what the relief was priced against.
5. **`ComboBox` centres its child label's em box, so the kit's cap arithmetic had to anchor it top-left.** A choice
   bar's value words are the `ComboBox`'s own `Label`, placed by `positionComboBoxText`, and `Justification::left`
   centres that box vertically: the inked cap top landed **3 px under the box top (93 against 90**, cap height
   49.951). `positionComboBoxText` now sets `topLeft`, the anchor `drawCapAlignedText` uses, so a bar's label and a
   choice bar's value share one caption band instead of two.
6. **The dial's disc costs exactly one allocation, and a paint-cost equality moves with the kit's shape.** When the
   dial stopped filling its widget rect and started filling the inscribed disc (`PieGeometry::discRectOf`), three
   allocation legs read **1200 == 1000** and **1000 == 800**: an ellipse costs the rasteriser one more allocation per
   paint than a `fillRect`. The *reference drawing inside the instrument* gained the same ellipse; the kit was not
   tuned down. That is what "the same drawing with no kit in it" means — the reference defines the drawing, so when the
   drawing changes, the reference changes with it and the equality keeps its meaning.

**The rule.** Font metrics are load-bearing in the layout, not just the paint: a reserve built from them must be
rounded once as a multiple, must not double-count the cap band, and must not rely on two of them differing. When a
metric changes, the legs that consume it are re-measured rather than re-tuned — here three layout fixes, one instrument
reference, and one control moved to a metric that cannot coincide.

**Figures after the fix.** §5.22: 79 lines decided, 0 FAIL, exit 0. `UiTests` 38 cases / **16 369** assertions;
`UiJuceTests` 9 / **272**; `UiProbe` 11 / **134**, the dial's pixels reading `inside 33 632 against a disc area of
34 307, tinted outside 0`; `UiAllocTests` 8 / 25, printing 5 per paint for a bare `fillAll`, 8 000 for a bar's 200
warm paints, 1 400 for a dial's. Full `ctest` **278 / 278**, exit 0, 1 `Skipped` (`ShellTests`' dump leg, which needs
`--dump-wavs`); `ctest -R "ui:"` **66 / 66**, exit 0.

**Not measured:** whether the 3 px `ComboBox` offset is 3 px on another face; where an overrunning caption breaks when
a host's shaping differs from this container's (in our paint it is clipped at the box edge, not broken); the wordmark's
drawn *weight* against the mockup's — the 11 % is a width figure and argues nothing about Black versus Bold.

**Revisit trigger.** A change to the ramp's weight column (D-025's third-weight trigger); any decision to widen a
column, shorten a caption string or squeeze a role, which has to be written against item 4's table; a JUCE upgrade,
because item 1's line-advance reading and item 5's `positionComboBoxText` behaviour are both 9.0.4-specific.

## F-025 — A kit widget reads its own default until its attachment exists, and a peerless `repaint()` never reaches `paint()`

**Status:** Finding. Both halves cost `04` a red leg each; both are ordering or observability facts about JUCE, not
defects in this code.

- **Read the parameter, not the widget, and attach before you read either.** Every kit control is a `juce::Slider`
  subclass, and until `SliderAttachment` is constructed for it the slider holds its *own* default — `0.0` — not the
  parameter's value. `EntangleAudioProcessorEditor`'s constructor set the envelope panel's gate from
  `silenceFloor.getValue()` while `silenceFloor` was still unattached, so the panel held 0.0 dB and drew its rule on
  the axis's top edge (y 19 where the parameter's −60 dB is y 82). §5.23's C5 leg reddened on it. The constructor
  order is therefore: build the widgets → create the attachments → only then read a widget's value into anything.
  This is the widget side of the construction-order rule the `juce-apvts-parameters` reference carries for the value
  tree, and a headless leg catches it while a look at the running plugin does not — a freshly-attached slider is
  corrected by the attachment's initial push before the window ever shows.
- **A `repaint()` with no peer is dropped at the root, so a zero paint count proves nothing.**
  `Component::internalRepaintUnchecked` bubbles the dirty rect to the parent and the peerless root discards it, so in
  `UiProbe`, `UiAllocTests` and every `OfflineRender` rig `paint()` is never reached — those programs drive it
  explicitly through `paintEntireComponent`. A leg must therefore never claim "no repaint" from a paint counter in a
  console run: `paintCount() == 0` is a property of the rig. Count what the call path actually reaches — drains and
  path rebuilds, which `§5.23` does — and leave the *extent* of a repaint (criterion 9's "confined to the two panel
  rects") to A8, where a peer exists and a dirty region is visible.

**Do not** re-arm either as a counter: the first is fixed by the constructor's order and its leg is the parameter move
(arm 5), the second is not observable headless at all.

## F-026 — Two display legs pass without their guard: the ring's mid-copy tag, and the shell's frozen window

**Status:** Finding, from `04` §5's arms on 2026-11-18. Neither is a defect in what shipped. Both are places where a
regression would print green, which is the only reason they are on this log rather than in a risk table.

- **The tearing leg does not exercise the tag protocol.** `trace ring: a racing writer cannot tear a record` — 20 000
  writes against a capacity-4 ring, writer thread against reader — stays green when `TraceRing::write` stores the
  *written* tag **before** it copies the payload. The tag's **value** still moves with every lap, and refusing a
  lapped slot is what that loop actually measures; the odd/even in-progress marker matters only for a read landing
  inside the single assignment, and 20 000 iterations never hit it here — all 30 `trace` cases, all 84 `ui:` cases
  and `§5.23` read 0 FAIL on that mutant. So the leg decides order and lap-refusal. Arming the mid-copy property
  needs a mutation that holds the slot in the writing state across something observable (a payload wide enough to
  copy slowly, or a writer that pauses inside `write`); re-arming it the obvious way buys a green and no information.
- **`§5.23`'s shell legs never assert that the newest published hop is the last hop driven.** A ring that refuses the
  new record when full instead of lapping it freezes the window at capacity, and the section does not notice: C1's
  shell premise prints **hop 255 for a 281-hop render** and the section still reads **37 lines, 0 FAIL**, because
  every shell leg compares the window against itself and against `newest` — order, depth, consumption. The drop
  policy itself is decided where it lives (the ring: 5 of 30 cases red on the same mutation), so shipped behaviour is
  covered; what is uncovered is the shell publishing into a ring that stopped moving. One line in
  `theShellPublishesTheHopSequence` — the newest hop index against the hops the render drove — closes it.
- **`UiAllocTests` weighs the kit's five controls plus `Panel` and `Text`, not the two trace panels**, so a
  `std::vector` built inside `ResponsePanel::paint()` has no allocation witness. Arm 7 was caught by the palette legs
  — that mutant also lays ink down — and not by an allocation count, which is the rule it was armed for.

**Do not** read any of those greens as a pass for the property its leg's name claims. **Do** add the shell's hop line
and a trace-panel allocation case the next time either file is open, rather than as a change of their own.

## F-027 — A header-writing `operator new` shim aborts under Mach-O's two-level namespace

**Status:** Fixed 2026-11-18, the day it appeared. Found building the tree on macOS arm64 (AppleClang 16), where
`ReconfigureTests` died during Catch2's post-build test discovery. The ledger is now `#ifdef __linux__` and the three
byte legs SKIP elsewhere; the counted legs run on both platforms.

- ELF resolves the executable's strong `operator new`/`operator delete` for **every** caller in the process,
  libstdc++'s own internal calls included. Mach-O's two-level namespace binds `libc++.1.dylib`'s internal references
  to libc++'s exported pair, so the app's shim never sees what the dylib allocates.
- `ReconfigureKeyTests.cpp` held the only shim in the tree that moves the payload up by `span` and keeps size and raw
  base in the 16 bytes below it. A libc++-owned block reaching `releaseCounted` reads a header that was never written
  and calls `std::free` on the value found there → SIGABRT inside malloc, before any test body runs.
- The signature to recognise: it fails on the **link** edge (`FAILED: src/Engine/tests/ReconfigureTests`) with
  `Error running test executable … Result: Subprocess aborted` and `Output:` empty. That is `CatchAddTests.cmake`
  running the freshly linked binary with `--list-tests`; the script captures stdout only, and malloc's diagnostic goes
  to stderr. Not a link error. Run the binary directly for the reason.
- The sibling shims in `HopSchedulerTests`, `MixEngineTests`, `ChannelMapTests`, `LayoutGateTests` and
  `TraceRingAllocTests` pass `size` straight to `std::malloc` and free the pointer unchanged, so a cross-boundary free
  is still `free()` of a live block — they are immune, which is why only one target died. Take that shape for a new
  counter unless the leg genuinely needs the size at unsized-delete time. Second way these shims can lie, after F-019.

**Figures.** Linux Release: 14/14 `reconfigure` cases. Mutation `liveBytes += size + 1` inside the ledger reddened
exactly the three byte legs — `prepare sizes by source in and processed out`, `a narrower re-prepare releases the
sidechain rings`, `hop scratch keeps its capacity across a narrowing` — and left the counted legs green, so the gate
did not blind them. With `kLiveByteLedger` forced false to stand in for the Apple branch: the same three Skipped, 11
passed, exit 0, no abort. Ten JUCE-light binaries after the change: 203 cases, all passed.

**Do** treat `Subprocess aborted` on a link edge as test-discovery death. **Revisit** when a byte leg must run on a
Mac: `DYLD_INTERPOSE` behind a launcher is the route, and it buys a binary that needs an environment to run.

## D-028 — The container's build volume mounts outside the `/workspace` export

**Status:** Accepted (M10), to close F-028.

**Decision.** The build volume mounts at `/ct/build` and compose sets `BUILD_ROOT=/ct/build`, so a container tree is
`/ct/build/<config>` while a host tree stays `build/<config>`. `make install`, `make build`, `cmd/install` and
`cmd/build` resolve `BUILD_ROOT` themselves, so no command in the repo carries a container-specific build path. The
image creates and owns `/ct/build`; the entrypoint probes it for writability and names `docker volume rm vst3-build`,
because a named volume takes its ownership only when first created and a wrong seed survives every rebuild.

**Consequence.** Nothing the container mounts creates a directory in the host tree any more, so the host owns
`build/` outright and `rm -rf build` works with the stack up. The trade: a container tree that reaches `/workspace`
anyway — an explicit `cmake -B build/…`, or `BUILD_ROOT` unset — is no longer shielded, and appears on the host at the
container's numeric uid. The `USER_UID`/`USER_GID` build args match this host, so those files stay deletable; the
escape is visible rather than fatal. `chown -R "${USERNAME}:"` (colon, not bare user) keeps the volume root's group at
the login group — `chown -R user path` leaves it `root`.

**Revisit trigger:** a container that cannot run the host's uid, or a second container that must share these trees.

## F-028 — A mountpoint inside the shared tree carries `deny delete`, so the host cannot remove `build/`

**Status:** Fixed 2026-11-21 by D-028. Found on macOS with Docker Desktop's VirtioFS sharing, clearing a host build
tree while the stack ran.

- The symptom is a single line — `rm: build: Permission denied` — printed *after* everything inside `build/` is
  already deleted. Owner is the invoking user, mode 0755, no flags, no foreign uid anywhere in the tree: every
  ownership hypothesis tests green and is wrong. `ls -lde build` shows the cause, `0: user:mike deny delete`. A deny
  ACE is evaluated before any allow, so `unlink`, `rmdir` and `rename` all return EACCES on that one directory while
  its children delete normally.
- Stripping the ACE (`chmod -a "user:mike deny delete" build`) let `rmdir` succeed immediately **with the stack still
  up**, which is what separates an ACE from a vnode hold. The earlier directory had no ACE at all and still refused:
  there `lsof` showed `com.apple.Virtualization.Virtua` holding `30r` on its inode, and `docker compose down`
  released it. Two Docker mechanisms, one symptom.
- `docker compose up` recreates the directory. Its birth timestamp landed on the container-start second and it arrived
  with the ACE already applied, so while the volume mounts inside the export the host cannot have both a running
  stack and no `build/`.
- `deny delete` misleads triage: nothing in EACCES-on-`rmdir` points at a sharing layer, so the search goes to uid
  mapping and seed ownership first. Read the ACL before the ownership story — `ls -lde <dir>`, or `stat -f '%Sp'` and
  look for the `+`.

**Do** suspect a deny ACE whenever the *owner* cannot remove a directory on macOS. **Revisit** if a Docker Desktop
upgrade changes how it guards mountpoints inside a shared tree.

## D-037 — Clash Display is vendored under the ITF Free Font License and re-fit to the cap the sheet was laid out for

**Status:** Accepted (M11) on the reading recorded below. The licence was read from the text that ships with the
family rather than answered by the foundry — the user's ruling of 2026-11-21 was to ship without the enquiry — and the
two points that reading cannot settle are named where a reply lands.

**What changed.** The M10 face (D-025) left two statements open with nothing to close them: a licence nobody held and
a drop that had to stay untracked. The replacement closes both and is the better fit for the sheet: a geometric
grotesque with open apertures, which is what survives a 17 px label on a dark ground. The
mechanism did not move — `EntangleFonts` still holds the bytes, `typefaceFor(weight)` still resolves through the ramp,
and no call site names a family (D-025).

**The bytes.** Vendored and tracked in the foundry's own drop, `public/ClashDisplay_Complete/`:

| File | Bytes | Role in the ramp |
|---|---|---|
| `Fonts/OTF/ClashDisplay-Light.otf` | 27 000 | `TypeWeight::light` |
| `Fonts/OTF/ClashDisplay-Bold.otf` | 25 276 | `TypeWeight::bold` |
| `License/FFL.txt` | 12 734 | the grant the product relies on |

`src/UiJuce/CMakeLists.txt` names those two face paths and nothing else from the drop, so the other four weights,
the italics, `ClashDisplay-Variable.ttf` and the web formats sit in the tree and stay out of the binary. They link
`libEntangleFonts.a` at **57 170 B** against the M10 face's 915 914 B (`EmbeddedResource` sizes
`ClashDisplayLight_otfSize = 27000`, `ClashDisplayBold_otfSize = 25276`). Both paths are tracked — the drop since
`7128e9d`, the Helvetica set since `d84784e` — which is what lets a clone configure, and why no second copy of the
bytes is kept: it could only drift. The packaging rule and its leg are D-025's. That the whole drop is in a repository
pushed to `origin` is a distribution question of its own — clause 02 names a "repository" among the channels it
restricts — so it is recorded here beside the two open points below rather than argued; removing it is a history
rewrite, which is the user's call, not this item's.

**The licence, and the clause relied on.** The **ITF Free Font License** (Indian Type Foundry's Fontshare EULA), read
in full on 2026-11-21 — the clauses the decision turns on, in their own words:

| Clause | What it says | What it decides here |
|---|---|---|
| 01 Grant | "a non-exclusive, non-assignable, non-transferable and terminable license to access, download, install, store and use the Font Software **for personal or commercial purposes**, free of charge and for an unlimited period of time"; "in any media, including Print, Websites, Mobile or Desktop Applications … at any scale and in any location worldwide"; the device count is unrestricted; internal sharing inside an organisation "does not constitute redistribution" | A commercial plugin is a permitted *use*, and there is no seat or device ceiling to design around |
| 01, fourth paragraph | "**You may embed the Font Software in mobile or desktop applications and digital documents for the uses permitted under this License.**" | The express grant, not an inference: embedding the face in a desktop application is one of the permitted uses named in the grant itself |
| 02 Limitations | No modification without written consent — naming "modifying or replacing glyphs, **subsetting, format conversion**, or altering font names, copyright information, ownership information or other metadata". No distributing, duplicating, loaning, reselling, sublicensing, transferring, donating, giving away or otherwise making available to any other person or entity, "whether for free or for a fee", including through "another font website, font library, marketplace, repository, download service, application or platform". No hosting, serving or embedding "for use by third parties" so that they can "create, edit, customize or generate their own content" | The shipped bytes are the foundry's own, unmodified (arm 16 is a licence violation, not an optimisation), and nothing offers the face to anyone as a font |
| 02, final paragraph | "For the avoidance of doubt, nothing in this Section 02 restricts the self-hosting, **embedding** or other use of the Font Software by the Licensee **for the Licensee's own** websites, **applications** or other permitted uses under Section 01" | The limitation-relief that keeps clause 01's embed grant usable once the application leaves the Licensee's own machine: Entangle is the Licensee's own application, drawing its own words |
| 03 Embedding | Embedding in "PDFs and other digital documents for your permitted uses", "provided that the embedded Font Software cannot be extracted or used independently from the document"; extraction for independent use is prohibited | Its condition is written for documents, so it does not attach to the clause 01 grant an application embeds under. The practical exposure is the same one — a plugin's resources can be lifted, and clause 02 prohibits extraction for independent use — so it is recorded, not argued away |

The reading, stated plainly: clause 01 grants embedding in a desktop application in as many words, so what ships is
an *application the Licensee is entitled to build*, not the Font Software offered to anyone as a font. The
prohibitions this reading must keep clean are modification (so the `.otf`s are byte-for-byte the foundry's), offering
the face to third parties as a font (never), and hosting it so others may select it (never). Two things remain open
and are the reason this row is not closed: clause 02 forbids distributing the Font Software "through another …
application or platform", and it does not say whether a plugin bundle whose resources contain the face is that case or
the embedded-application case clause 01 grants — the carve-in at the end of clause 02 points the permissive way, and
clause 03's no-extraction condition is written for documents, not applications, so on the text there is no
un-extractability requirement on an application; and nothing in the licence addresses a VST3, a format whose bundle is
a directory a user can open. The user's ruling of 2026-11-21 is to ship on the reading above without waiting for ITF's
reply; a reply that contradicts it is a distribution question, not a code question, and lands here.

**The weight map: the second arm.** `TypeWeight` is now `{light, bold}`:

| Arm | What it ships | Why not |
|---|---|---|
| (a) caption stays a medium weight | Restores the mockup's heavier captions; +1 weight ≈ 26 kB | Breaks the cap-hold rule for three roles at once; it is a look, not a contract, and D-024's row already says the caption's weight is D-025's to fix |
| **(b) two faces, `{light, bold}`** | `caption` renders Light, one step lighter than the mockup | **Taken.** The cap-hold — the sheet must look the same size after the swap — outranks a weight restored from a mockup. Flipping one ramp row to Medium costs a re-fit of that role's squeeze and ~26 kB, not a code change |
| (c) `ClashDisplay-Variable.ttf`, one instance per role | One file, every weight on the `wght` axis | Rejected **on measurement** — §0 H's probe is below. It works on this backend, and costs more than the two statics while removing the identity every face leg reads |

The seven roles keep their weight column: `wordmark` and `figure` Bold, the other five Light. `caption` moving from
Medium to Light is the one visible consequence of arm (b).

**Arm (c), probed 2026-11-21.** `public/ClashDisplay_Complete/Fonts/TTF/ClashDisplay-Variable.ttf`, 88 064 B, one
axis: `wght` 200 … 700, default **700**, six named instances, nameID 1 "Clash Display Variable". Loaded through
`Typeface::createSystemTypefaceFor` and cloned per weight, the advance of `Window Size 128` at a 100 px height:

| Instance | 200 | 300 | 400 | 500 | 600 | 700 | static Light | static Bold |
|---|---|---|---|---|---|---|---|---|
| advance px | 660.702 | 667.632 | 680.439 | 698.158 | 718.684 | 737.105 | **667.632** | **737.105** |

Both of §0 H's proofs hold on this backend (FreeType, Linux): the clones differ monotonically, and the shipped static
Light is the `wght = 300` instance to the last printed digit, the static Bold the `wght = 700` one. The route also
keeps D-025's rule — `FontOptions{}.withTypeface(clone)` yields a `Font` whose typeface *is* the clone (pointer
equality), so no family-name lookup is needed; `withTypeface` does clear the options' variable list, but the instance
is already baked into the clone by `cloneWithVariableSettings`.

Arm (c) is refused anyway, on two figures:

* **It costs more, not less.** 88 064 B against the 52 276 B the two statics occupy — **+35 788 B for the two weights
  the ramp binds**. The variable file only wins past three weights, which is a D-032 question, not this one.
* **The instance has no name.** Every clone reports the same nameID 1 and style — "Clash Display Variable" / "Bold",
  including the one drawing at 200 — so the identity assertion D-025 exists for, and which c13 runs, cannot tell the
  weights apart. `getConfiguredVariables()` reports the value that was *asked for*, never what the rasteriser used, so
  a variable face would leave the sheet with no checkable claim about what drew. The static pair each carry their own
  PostScript name and are the version that can be audited.

The macOS and Windows backends were not probed and cannot be here (INDEX §4); the refusal does not depend on them.

**The ramp, re-fit.** The rule was *hold the cap height the M10 sheet was laid out for* and take up the width with a
squeeze. Two measurements drive it: the drawn cap of the old face at each role, and this face's cap as it actually
draws. `FontOptions::withHeight` sizes the **line box**, not the em, and with the pinned pair the line box is 0.898 em,
so a target cap of *C* needs em = *C* / 0.7461 — the factor between the drawn caps is **×1.0627**, not the ×1.0448 the
declared `sCapHeight` figures suggest (F-030).

| Role | Weight | em | Squeeze | Drawn cap at the reference sheet |
|---|---|---|---|---|
| `wordmark` | bold | 68.0 | 0.77 | 50.73 px |
| `caption` | light | 59.5 | 0.67 | 44.39 px |
| `barLabel` | light | 67.5 | 0.65 | 50.36 px — the one role that does **not** hold (D-031) |
| `barValue` | light | 67.0 | 0.70 | 49.99 px |
| `rotatedCaption` | light | 55.3 | 0.66 | 41.26 px |
| `figure` | bold | 37.2 | — | 27.76 px |
| `micro` | light | 23.4 | 1.00 | 17.46 px |

The squeeze floor is `juce::Font::getDefaultMinimumHorizontalScaleFactor()` = 0.7 **for a font that requests a squeezed
layout**; below it a *fitted* layout stops squeezing, so a role that needs less than that paints a deficit rather than
clipped words — which is why the squeeze lives in the ramp and the fit rule (D-031) refuses a role that needs more than
it gives.

**Digits carry the rows, so the digits are pitched.** The instrument measures `0`–`9` only: 0.681 em each, fixed, with
`1` at 0.273 em unpitched. The separators and signs are deliberately left out of the pitch set (a digit-width `.`
would be a visible artefact in prose readouts; `+`/`−` are not digits, and nothing sits beside the meter's number for
a shift to move against — C16 prints `+12` against `-12` as an observation, not a gate).

**Pinned face metrics.** `sLineGap = 0`, `sCapHeight = 0.6700`, `sXHeight` 0.4910 Light / 0.5040 Bold — the factory
writes them on both faces and the instrument reads them back through `Font::getCapHeight()`/`getXHeight()`, which pass
through to `Typeface::getMetrics` where the substitution happens (measured == declared to 0.000 em). The *layout* box
stays the face's own — 0.7951 above / 0.2049 below against the face's 0.7807 / 0.2193 — because `getAscent()`/
`getDescent()` have no substitution hook. Pinning is what stops the seven-role fit drifting with a different rasteriser.
It is a runtime hint written into the typeface this process loaded: the vendored file's own tables are untouched, which
is what clause 02's bar on edited metadata requires and what the byte check below proves.

**Figures.** Face leg: `ClashDisplay-Light` 27 000 B and `ClashDisplay-Bold` 25 276 B from the embedded bytes, units per
em 1000, declared cap 0.6700 em, drawn gap to declared 0.000 em. The files the build reads are the foundry's own
drop — `public/ClashDisplay_Complete/Fonts/OTF/`, sha256 `3059ccd0afe04a35…` (Light) and `cbf5670a6d502f69…` (Bold) —
so nothing subset, converted or re-titled is in the tree (c12, c19). C15 squeeze: **0 of 7 roles** below the floor.
C17 coverage: **58 codepoints, 0 missing**; the control character U+2192 reads absent as designed. C14: six of seven
roles hold within 1 px, `barLabel` 50.36 against 63.43 — the authorized deviation (D-031). A fresh
`cmake -S . -B <empty>` of the tracked tree configures and builds `EntangleFonts`.

**Not measured:** hinting and stem darkening on another rasteriser (FreeType here); the wordmark's drawn *weight*
against the mockup — it is Bold, and `03`/D-032 owns whether a heavier cut is the sheet's; the pinned metrics on macOS,
where CoreText reads the same tables through a different path.

**Revisit trigger.** (a) Any reply from the foundry, contradicting or confirming; (b) a third weight, which is one ramp
row and ~26 kB (D-032 asks it for `figure`); (c) a word no squeeze clears, which is D-031's menu rather than a new
face; (d) a JUCE upgrade, because the line-box reading behind the ×1.0627 factor and the 0.7 squeeze floor are 9.0.4
behaviour.

## D-031 — Text fits by measured deficit: widen the column, squeeze the role, or reword the string, decided per word

**Status:** Accepted (M11). The per-string rulings are the user's, 2026-11-21. Ships with F-031, which is how the class
escaped the suite.

**Why a decision was needed.** QA's A-1 found clipped words on the sheet. Measuring properly — the advance against the
room the `paint()` actually gives, both axes — found **46 of 63** word rows over at the reference sheet, so this was
the sheet's normal state, not an edge case. The relief is not one change: a column can widen, a role can squeeze, a
string can change, and which of the three applies is a per-word judgement about what the word is *for*.

**The rule.** The three moves, in the order that costs the design least:
1. **Squeeze the role** when the word is the only tenant of its room and the deficit is a few percent — one table row,
   no geometry moves. The floor is the squeeze the toolkit will still honour (D-037).
2. **Widen the column** when the words are the column's purpose and something else can pay — a column is a *derived*
   figure; its millimetre value is the floor, never the ceiling.
3. **Reword** when the words are longer than the sheet's grammar needs. Abbreviate to the same register as their
   neighbours; a word may lose a unit or a decimal only if nothing else on the sheet still carries it.
Auto-shrink inside `paint()` is refused: it is a fit rule that runs at the wrong end, silently, per frame, and it would
have to re-derive the geometry it is squeezing into.

**Per string.**

| Word (role) | Short by, at the reference sheet | Move | After |
|---|---|---|---|
| `wordmark` "Entangle" | 30.9 px of 264 | squeeze 0.82 → **0.77** | 260.6 in 264 |
| `levelCaption` "Volume Ctrl" | 53.6 | reword → **"Level Ctrl"**, squeeze 0.75 → 0.67 | 193.0 in 262.4 |
| `windowCaption` "Window Size" | 161.4 | same squeeze (no grid change) | 262.4 in 264, 1.6 px spare |
| `outputGain` label "Out" | 8.4 of 75 | **widen the label column to the words** | 83.4 in 100 |
| `normalisationAmount` "Amt" | 23.6 | same | 98.6 in 100, 1.4 px spare |
| `silenceFloor` "Flr" | — | same | 60.4 in 100 |
| `appliedCorrection` "Adj" | 25.3 (the `j` descender) | keep the word, **shrink the role** 85 → 67.5 em | 71.9 in 100; its cap is 50.36 px, **13.07 px under** the old face — the one role exempt from c14's hold |
| a bar's value "0.0 dB" | 3.5 of body − inset | squeeze 0.75 → **0.70** | 4.4 px spare |
| the meter's value "−36.0 dB" | 104.8 | reword to sign + whole dB → **`-36`** | 86.7 in 160 |
| `grainReadout` "8192 samples = 170.7 ms" | 59.1 | squeeze → 0.70, and the stack reserve counts the **lines the words make** | fits; 7 lines at the reference sheet |
| `rotatedCaption` ×3 | 6.6 (Worst: Waveform, rotated) | em 52 → 55.3, squeeze 0.66 | Waveform 206.4 into an extent of 211 |
| cell 9's new labels | n/a | **"Grain" / "Hop"** at `micro`, inset by their own measured column | 65.1 / 49.1 in 75; items 142.3 / 106.7 in 181 |

**Rejected.** A tolerance on the deficit (the deficit is what the eye reads); scaling `k` down (shrinks the whole sheet
to rescue four words); shrinking every role (loses legibility at half scale, where the `micro` em is already 11 px);
the declared-metrics cap factor (D-037); and a grid change — widening the label column is a derived figure, the 19.8 mm
millimetre value stays as the floor, and the cell pitch never moved.

**Figures.** Red capture (the shipped sheet measured the new way): 46 of 63 rows over at 1190 px, worst `windowCaption`
+161.4 px. After: **0 deficits at 595, 1190 and 2380 px**; tightest word at the floor is `windowCaption` 131.2 of
132 px; the layout sweep's worst rounding stays **2 px** (bound 3) including the off-aspect 1000 × 794; the sheet still
places 22 components with none inside a reserved cell.

**Not measured:** what a host's shaping does to a word 1.6 px from its box — the margin is a pixel, and this container's
FreeType is the only rasteriser measured; whether "Level Ctrl" is the word a user expects (c11, human-judged, host
named).

**Revisit trigger.** Any new word on the sheet — it runs the same gate before it lands; a column whose widest tenant
changes, since the column is derived per resize; the first host screenshot, which is where a 1.6 px margin either holds
or does not.

## F-030 — A contemporary variable family under JUCE: the pair you get is not the name you asked for, and the line box is not the em

**Status:** Finding, from the swap that D-037 records. Each item cost a leg or a figure, and the first four are silent
on any face whose metrics are legacy ones.

1. **JUCE matches the *typographic* pair, not the product name.** `Typeface::createSystemTypefaceFor` resolves a family
   through the name table, and for a weight-specific legacy family the pair it reports is nameID 16/17 ("Clash
   Display" / "Light") while nameID 1/2 carry "Clash Display Light" / "Regular". A leg asserting the family name is
   therefore asserting whichever pair JUCE chose, and a face whose subfamily naming differs inverts the expectation
   without any code changing. **Identity is nameID 6 (`postScriptName`)** — `ClashDisplay-Light` — so the skin's face
   leg reads the name table out of the embedded bytes and asserts that. The harness header that does it is
   `tools/harness/FontNameTable.hpp`; the same file also yields the units-per-em and the hhea range, which is how the
   instrument gets a second opinion on the metrics JUCE pins.
2. **No `tnum`.** One figure set, with `1` at 0.273 em and `0` at 0.681 em. There is no tabular axis to request, so
   figure stability is a property this project builds (pitching digits in the factory) rather than one the face
   provides. A future `Ligature`/`cv` feature request has no hook either: the static OTFs expose `calt` and `liga` and
   nothing else worth disabling.
3. **`getAscent()` is not the cap, and the *layout* box is neither.** hhea ascent 0.8900 em against a 0.6700 em cap;
   the layout box JUCE computes is 0.7951 above / 0.2049 below. The M10 face's `ascent == cap` equality (F-024 item 3)
   was a legacy-family coincidence, and a leg whose power came from the two figures *disagreeing* would have quietly
   regained it here. Nothing in the layout uses either figure for cap alignment.
4. **The pinned line box is smaller than a cap plus a descender's ink.** With `sLineGap = 0` and a declared ascent +
   descent of 0.898 em, a cap-aligned baseline leaves 0.152 em below it, where a descender needs ≈0.225 em. So
   `getAscent() + getDescent() < capHeight + descentInk` and **no** em makes both a cap hold and a descender fit a
   cap-aligned row. That is the geometry behind D-031's `Adj` ruling: the row's height was never the binding term —
   the box the toolkit computes is.
5. **`FontOptions::withHeight` sizes the line box, not the em.** With the declared pair pinned, em = height / 0.898, so
   a cap measured at height *E* is 0.7461 × *E*, not the declared 0.6700 × *E*. Every re-fit figure in D-037 divides by
   the line-box term; the instrument sidesteps it by pinning 1/0 so em = height exactly.
6. **`juce::String(juce_wchar)` formats the codepoint instead of building the one-character string.** `char32_t` binds
   `String(int)`, so the pitch of `0` measured this way is the width of `"48"` — twice too wide, and every digit-bearing
   row in the first capture was inflated (`128` read 297.5 px where it draws 152.4). `String::charToString` is the
   constructor that means one character.
7. **A variable instance has no name.** The same drop's `ClashDisplay-Variable.ttf` (88 064 B, `wght` 200–700, default
   700, six named instances) clones correctly under FreeType and each clone advances differently — 660.702 px at 200
   through 737.105 px at 700 for `Window Size 128` at 100 px — and the static Light is the 300 instance and the static
   Bold the 700 one to the last digit. But **every clone reports the same nameID 1 and style: "Clash Display Variable"
   / "Bold", at 200 as at 700**, and `getConfiguredVariables()` echoes what was asked for rather than what the
   rasteriser used. The identity check D-025 exists for, and which c13 runs, would have nothing to read.
   `FontOptions::withTypeface` also clears the options' variable list, so the instance has to be baked into a clone the
   factory keeps alive. Refused on both counts — see D-037 arm (c), where the payload figure (+35 788 B over the two
   statics) is the smaller objection.

## F-031 — A tolerance on a proxy axis: four UI legs gated box positions and the instrument claimed the words were in bounds

**Status:** Finding, found by measuring the thing the legs did not; closed by D-031. Numbered F-031 because F-028 was
spent on the mount finding (D-028) before this row was written.

- **The four legs never measured glyphs.** C1 checked each component's box against its cell, C9 checked a bar's label
  *box* against its value *box*, C6 checked the ink-field rectangles, C7 checked child placement. All passed. The
  instrument printed a line that read as a verdict on the words — "all words in bounds" — while gating `x` and `width`
  of *containers*. The paint clips to the box, so the words were being cut on the sheet the whole time.
- **The tolerance was inherited from the wrong subject.** ±3 px is the right bound for a box placed from millimetres:
  it is the rounding floor. Words are roughly five times more sensitive in width than in height, so the same figure is
  ~15 px of latitude on the axis where the sheet has none. A tolerance copied from a geometry leg into a text leg buys
  a green suite and a clipped UI.
- **The fix is a different gate, not a tighter one.** Every word on the sheet — 6 `ui::Text`, four bar labels and their
  values, three rotated captions, the choice-bar items, the meter's number — is measured through the same route the
  `paint()` takes and gated at **zero** deficit against the room that paint gives, on both axes, with the rotated
  caption's room the rotated extent rather than the box's width. Box placement keeps its ±3 px. Two gates, two
  subjects.
- **The instrument's own arithmetic caught the same class twice.** Charging a stack as `ceil(ink) + (lines − 1) ×
  round(em)` manufactured a 1 px deficit in a word that fitted, because the placer rounds its edges once
  (`round(n × em)`) — the verdict now speaks the unrounded em (F-024 item 2, one level further down). And measuring a
  digit's pitch with `String(char32_t)` (F-030 item 6) inflated every row that carries a number, which is most of them.
- **The rule for the next leg.** A text leg gates the measured extent, not the box that holds it, and its threshold has
  an arm that breaks it. A leg whose pass condition cannot fail is a printout, not a check.
- **What the class cost:** 46 of 63 word rows over at the reference sheet, the worst 161 px in a 264 px box — clipped
  on every shipped build of M10 while four legs and a full-suite green printed above it.

## D-038 — Blend runs `[0, 1]` from dry A to the convolution; dry B leaves the sum; the datum leaves the sheet

**Status:** Accepted (M11); measured, arms run. Amends **D-010** (the dry bus, the reachable `p`, the pin's dial
position) and **D-011** (the blend parameter's range, its default and its words). `mixLawWeights` is untouched and
still spans `[-1, 1]`; what changed is which part of it the product can ask for.

**Why.** The dial's upper half faded the convolution into dry B — a signal the sheet never claimed, on a product
whose sidechain is a level source and not a second voice. The mark that explained that half (a datum radial on the
boundary) was the only thing on the sheet a user could neither act on nor read. Both go, and the parameter's bipolar
domain goes with them.

**Decision.**

| Surface | Before | Now |
|---|---|---|
| Dial | `p ∈ [-1, 1]`, default `0` | `q ∈ [0, 1]`, default `1` — same id, same `1/128` quantum, automatable |
| Meaning | −1 dry A · 0 the convolution · +1 dry B | `q = 0` dry A · `q = 1` the convolution |
| Output sum | `g·[β·(w_dry·dry + w_wet·conv) + (1−β)·A]`, `dry` chosen by the sign of `p` | `g·[β·(w_dry·A + w_wet·conv) + (1−β)·A]` |
| Dial face | `PieMode::datumAtZero` | `PieMode::proportional` |
| Readout | a number beside a boundary word | `fully dry A` · `dry A + wet` · `fully wet`, plus the pin clause in words |

- **One shift, in one place.** `entangle::effectiveBlend(q, sidechainActive)` (`src/Entangle/EngineChain.h`) is the
  only expression that turns `q` into the law's `p` and the only home of the no-sidechain pin:
  `sidechainActive ? q - 1.0f : -1.0f`. The editor, the parameter, the layout report's `blendEff` and the readout
  speak `q`; `MixEngine` speaks `p`; nothing in between converts, so the shift cannot be applied twice. The shift is
  a subtraction, so the reachable law domain is `p ∈ [-1, 0]` and the endpoint rows are the `t = |p|` law's own —
  the law did not need re-writing and was not re-written.
- **`coefB` is removed, not pinned at zero.** The member, its per-hop fill and its term in the sum are gone, so the
  sidechain pointers no longer reach the output at all. `q = 1` is bit-exactly the old `p = 0`, which is why the
  shipped default needed no re-based leg; any restored third term reddens the wet-window legs at full wet (arm 9:
  32 769 failed assertions in the engine's reachable-domain case).
- **The sign-crossing snap went with it and was not neutral.** It forced `p` to `0` for the sample after a crossing,
  which zeroes the dry contribution for that one sample, and it fired on the first hop after every `prepare` because
  `prepare` snaps the ramp while the tracker still holds its initial `0`. Rebuilt as it stood (arm 2) it reddens the
  bit-exact dry legs at the **first sample after `R`** in every block plan — 3 709 failed assertions and 15 render
  legs. The item predicted an equivalent mutant; the measurement is what D-010's amended bullet now says.
- **The stored value is unchanged and its meaning moved.** `getStateInformation` is the APVTS XML and carries no
  version, so a recalled `n` is simply re-read: `p = n − 1` where it was `p = 2n − 1`. A lane written before this
  entry plays elsewhere — the old `n = 0.25` sat halfway from dry A to the convolution and now sits a quarter of the
  way, nearer dry A. There is no migration to write and no version to bump, because no host has ever loaded this
  plugin: `docs/host-layout-matrix.md` §5 has no row and the first save is M9 row 7. If a lane written under the old
  law ever surfaces, its successor is `q = 2n` for `n ≤ 0.5` and has none above it — dry B has no destination.
- **The datum is a kit capability, not a sheet feature.** `PieMode::datumAtZero` stays in `src/Ui` and in the kit's
  paint path; the blend dial does not ask for it. M10 `02`'s C5 clause that leaned on the mark — the wedge mirroring
  across the datum — is re-ruled as monotonicity in `q` with zero `ink` inside the disc, amended in that item's §2.4.

**Figures.** From `OfflineRender` §5.20/§5.22 and `MixEngineTests` on the shipped tree, 2026-11-23:

| Measured | Figure |
|---|---|
| Endpoint table, live A and silent B | `q = 0` peak 0.999987 · `q = 0.5` 0.707098 · `q = 1` **0** (`coefA` exactly `0.0f`) |
| No sidechain | 129 dial positions report pure A; every 20th renders A delayed by `R`, bit-exact |
| Dial grid | 129 positions tile `[0, 1]`, each round-tripping the parameter's snap and denoting `p = q − 1` exactly |
| Stored position | `n = 0 → q = 0 → p = −1`, `n = 0.5 → 0.5 → −0.5`, `n = 1 → 1 → 0` |
| The dial's face | `proportional`; `ink` inside the disc **0** at `q ∈ {0, 0.25, 0.5, 0.75, 1}`; sweep within 0.001 turn of `2πq`; `q = 0` plain `track` |
| Off-palette scan | dark shell 0 off-line of 0; live 138 off-line, of which **138** inside the dial's rect |
| Words | 75 rows × 3 widths, 0 deficits; the readout strings verbatim, pin clause included |
| Suite | quick set 317 passed / 0 failed; `harness:` 7 passed, 1 skipped; `OfflineRender` passed; `vstplug_VST3` links at `-j4` |

**Arms.** Sixteen valid runs of eighteen invocations; the two discarded attempts never reached a verdict (one
mutant never arrived, one campaign stopped at its own pristine gate). Red as predicted: the double shift at the
shell read; the deleted endpoint snap; the pin dropped and the pin moved to `+1`; the datum put back on the sheet
(three dial legs red, every law leg green — the datum and the law are separately watched); the matrix default left
in `p`; `coefB` carrying a weight at full wet; the C3 scan rect moved 200 px off the ink; the fold's `1/chB`
replaced by `1`. Survived as predicted: aliasing the
§5.1 identity leg's reference route against itself, and reverting C3's *sentence* — the leg's teeth are its rect,
which the other arm moved and reddened. Wrong, and recorded: arm 2 (above) and arm 4, whose `0.01` turns out to tile
`[0, 1]` exactly because F-010's residue needed the bipolar `−1 + interval × k` cancellation. The shape that does
bite is a quantum that fails to tile — `0.03`, whose top position is `0.98999995`, so full wet stops being a dial
position and 86 render legs go red. **F-032** is the leg-design rule that came out of it.

**Refused.** Re-parameterising `mixLawWeights` to `[0, 1]` — the curve is dimensionless and its table is a tested
contract, and a second copy of it is D-009's mistake. Keeping `coefB` as a zero term, or hiding the offset in the
editor. Painting the datum at the top of the dial to mark full wet: a mark at the end of the travel is not a datum.
A preset-rescale facility (index §9 — nobody has asked) and a state-version bump on the strength of a state that has
never existed. Retuning any M3 level figure so that an endpoint passes.

**Revisit trigger.** A mode that wants a datum re-admits the radial through `PieMode`, which still has it. The first
host save (M9 row 7) makes the stored-`n` paragraph above a live compatibility question rather than a hypothetical.
M9 row 9's listen owns whether full wet is the right default on material. A host that cannot automate a
zero-based float would reopen the range, and no such host is known.

## F-032 — A quantum guard built from the parameter's own constant cannot see a quantum that fails to tile its range

**Status:** Finding, from the arm that D-038 records. Three of JUCE's behaviours stack so that a decimal interval on
a zero-based range is indistinguishable from a dyadic one to any leg that asks for grid multiples.

1. **`NormalisableRange::convertTo0to1` does not snap** (juce_NormalisableRange.h:136). The interval lives only in
   `snapToLegalValue` (:188), reached through `RangedAudioParameter::convertTo0to1` (juce_RangedAudioParameter.cpp:51).
   A parameter's construction-time default therefore never sees the quantum, whatever it is.
2. **`AudioParameterFloat::operator=` short-circuits** a write of the value already held
   (juce_AudioParameterFloat.cpp:107), so a leg that writes the shipped default writes nothing and observes nothing.
3. **`snapToLegalValue` clamps to `end` after the interval arithmetic** (:196), and with `start = 0` what is left,
   `interval × floor(v/interval + 0.5)`, has no cancellation: on `[0, 1]` with `0.01f`, `1.0f / 0.01f` rounds to
   `100.0f` and `0.01f × 100` is `1.0f`. F-010's residue was the bipolar `−1 + interval × k` and it does not
   transfer, so the interval that broke the old blend range is inert on the new one.
4. **What does bite is a quantum that fails to tile the range.** With `0.03f` the top position is
   `0.03f × 33 = 0.98999995`, so full wet is not a dial position, the law's endpoint snap sits a hundredth of a turn
   away, and 86 render legs go red off that one constant.

**The rule for the next leg.** A quantum guard asserts three things, and none of them is "the grid multiples
round-trip": the grid tiles the range, so its last position *is* the endpoint; the endpoint is reachable by literal
from the opposite end; and the arm meant to break the guard breaks the tiling, not the arithmetic. Reading a
parameter back through the constant that built it is a tautology with a float in it.

## F-033 — A Bundle-only generator expression in a POST_BUILD step stops the whole project configuring on Linux

**Status:** Finding, 2026-11-24, surfaced by a change to a different library's CMake list. The re-sign
`add_custom_command` in `src/Entangle/CMakeLists.txt` evaluates `$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_VST3>`. On Apple
a JUCE VST3 is a bundle; on Linux it is a shared library, and CMake refuses the expression for a non-Bundle target
(`TARGET_BUNDLE_DIR is allowed only for Bundle targets`), so the generate step fails and no target in the project
builds — tools and tests included, none of which touches signing.

**Why it hid.** Ninja regenerates only when a `CMakeLists.txt` is newer than `build.ninja`, so a tree configured
before the block landed kept building through every source edit. The refusal appears at the first CMake-list edit
after it, and its text names a generator expression rather than a platform, so triage goes to the edit at hand.

**The rule.** An expression or command that means something on one platform goes inside `if(<platform>)` in the same
change that adds it. The re-sign is guarded now; on macOS the expression still evaluates and the step is unchanged.
