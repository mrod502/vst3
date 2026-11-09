# Engine

The processing seam the plugin shell drives. JUCE-light by rule (§9): no JUCE header in
`Engine.hpp`, `PassthroughEngine.hpp`, `MixEngine.hpp`, `ChannelMap.hpp`, `LayoutGate.hpp`,
`ReconfigureKey.hpp` or `HopScheduler.hpp`, and the library links only `Container` and `Dsp`, so it compiles
and tests as a console program with no `AudioProcessor`.

## The seam

`Engine` (M1 §5) is the only thing the shell knows about what the plugin does:

```cpp
virtual void prepare(const EngineConfig&) = 0;
virtual void reset() = 0;
virtual int  latencySamples() const = 0;
virtual int  wetDelaySamples() const { return latencySamples(); }
virtual void processHop(const float* const* a, const float* const* b,
                        float* const* out, int numChannels) = 0;
```

`EngineConfig{sampleRate, hop, maxChannels, latencySamples, grainLength}` is the whole world
view an Engine gets. **Contract:** `processHop` is allocation-free, deterministic, unaware of
hostBlock, and must not add delay beyond what `latencySamples()` declares. Either input side may be
narrower than `numChannels` and one pointer may appear in several slots — `numChannels` counts
*processed* channels, never a bus width — so per-channel state is keyed by index, not by address.
The shell installs `MixEngine(GrainConvolutionEngine)` through this seam without any change to the
scheduler (T8); `PassthroughEngine` stays for a harness that wants a pure copy.

## MixEngine (D-010, D-011)

The stage the host's controls talk to. It wraps the wet Engine and applies
`out = clip( g · [ β · ( w_dry · dry + w_wet · block(conv) ) + (1 − β) · A ] )`, with `t = |p|` on the
blend dial `p ∈ [-1, 1]`, `w_dry = sinf(t·π/2)`, `w_wet = cosf(t·π/2)`, `β` the bypass crossfade (1
effect, 0 dry onto A) and `g` the gain dial converted from dB. The dry bus is A for `p < 0` and B
otherwise, handing over at `p == 0`, where `w_dry == 0` makes it silent. `mixLawWeights` snaps `t == 0`
to (0, 1) and `t >= 1` to (1, 0): `cosf(pi/2f)` is `-4.37e-08`, not zero, and a wet term surviving at a
hard-dry dial makes "±1 is the pure dry render" unmeasurable. The no-sidechain pin that forces
`p = -1` does **not** live here — the shell's `EngineChain.h` decides it, and this stage only knows
the position it is handed.

* **Three optional stages around the law, in chain order (D-012, D-013, D-016).** The convolved stream
  passes through `dsp::DcBlocker` *before* anything else looks at it, then through `dsp::LevelNormalizer`
  if one is installed, then into the sum; the summed, gain-scaled output passes through
  `dsp::SoftClipper` last, so nothing downstream of this stage can exceed the ceiling. Only `conv` passes
  through them — A and B reach the sum unfiltered, which is what keeps the ±1 endpoints bit-exact — and
  the law therefore mixes and the host's gain acts on a stream already held at `√(envA · envB)`, which is
  decision B's slot and the only one where the reading is the engine's own output level while the dry
  buses stay untouched.
  Blocker-before-stage is the one ordering in that chain that M3 could not test: swapping the two left every
  harness leg and every other test green, because the blocker still removes DC before the host sees it (D-016).
  It is tested now. `MixEngineTests`' "level stage runs after the DC blocker" feeds a wet stream that hides a
  DC offset under a 1 kHz tone, so the correct order applies the tone ratio (**+7.96 dB**) and the swapped one
  reads the DC inside the envelope and applies the reciprocal (**−7.13 dB**).
  A stage that is not installed is not applied at all — that is how the harness renders the same chain as
  engine physics, as what ships, and as what ships minus one stage. The blocker and clipper come from the
  three-argument constructor (the two-argument one installs neither); the normalizer is the one stage
  installed by `installLevelNormalizer`, because it runs *before* the last of the two the constructor
  already orders, and a fourth positional argument reading `(blocker, clipper, normalizer)` would say the
  opposite of what the chain does. It is asserted at install time, because the stage is sized for the hop
  in `prepare` and must be present before it. None of the three has a say in the latency: the blocker
  defers nothing, the normalizer decides from the hop it is holding, and the clipper is memoryless.

* **No gain, no delay of its own.** `latencySamples()` and `wetDelaySamples()` forward untouched, so
  the wet path is exactly what the inner engine declared; the dry path needs no buffer of its own
  because the scheduler's pre-fill delays everything it drains by `R`, dry included. Bypass
  crossfades rather than reroutes, so it must never move the declared latency.
* **The ramps live here, in the sample domain.** `setBlendTarget`/`setOutputGainTarget`/
  `setBypassTarget` store a target that is then stepped once per output sample — straight line, 20 ms
  for blend and gain, 10 ms for bypass, the last step assigning the target exactly. A ramp length is
  milliseconds, never host callbacks. `prepare` and `reset` snap rather than ramp, so a new transport
  opens on the current setting. The two level targets (`setNormalisationAmountTarget`,
  `setSilenceFloorTarget`) are deliberately *not* ramped here: the applied gain is already continuous
  along the hop, and they are forwarded to the stage rather than smoothed twice. Both no-op with no
  stage installed, so the shell and the harness push the same targets at every variant.

## ChannelMap (D-017, amended by D-018)

`ChannelMap.hpp` answers one question — *given `chA` main-input channels, `chB` sidechain channels
(`0` = the bus is absent) and `channelsOut` main outputs, how many channels are there and which source
does each read on each side?* — and answers it once, off the audio thread. It is the only place the M4
channel rule lives: `isBusesLayoutSupported` gates on it and the seam turns its indices into pointers, and
neither may re-derive it (M4 `INDEX.md` §3 rule 2).

```cpp
static constexpr ResolvedChannelMap ChannelMap::resolve(int chA, int chB, int channelsOut,
                                                        int capacity = kMaxChannels) noexcept;
```

Four rules, in the order they are checked: a side broadcasts its channel 0 **exactly when it has one
channel** (otherwise output `c` reads source `c`); the two sides must agree whenever both are
multi-channel; **a mono main with a wider sidechain consolidates** — one processed channel, and that
sidechain read as the arithmetic mean of its channels (D-018); and `channelsOut` is at most `max(chA, chB)`,
or exactly 1 once consolidation applies. Elsewhere fewer outputs select the leading channels rather than
mixing anything.

The rule keys on the **main** bus, so it is *not* symmetric in (`chA`, `chB`): `(1, 4)` is one consolidated
channel and `(4, 1)` is four broadcast channels. Nothing here commutes — F-013 records why the property
M4 §1 originally asked for has no domain left, and `ChannelMapTests`' diagonal leg carries that in its name
so a future reader cannot mistake it for a symmetry check.

* **The fold is the design's only cross-channel read, and this library only names it.** `chA == 1 && chB > 1`
  puts `kConsolidatedSource` (`0xFFFE`) in the B slot; the mean itself is computed by `HopScheduler`, which
  owns the one mono hop it lands in (D-018), so nothing in `Dsp`, `Fft` or `Grain` ever reads across
  channels and no stage above the scheduler has to know the fold happened (M4 `INDEX.md` §3 rule 3). It lives
  in the index field rather than in a flag beside it because that field is a *source selector* and "the mean
  of this side" is one of its values, on the same footing as "no channel here"; and it is outside every real
  index range, so a consumer that treats it as an index fails out of range rather than quietly reading
  channel 0. `resolve(1, 2, 1)` is byte-identical under the fold and under the retired broadcast rule, so the
  marker is the contract and the index value is not.
* **Silence is a sentinel, not channel 0.** `chB == 0` puts `kSilentSource` (`0xFFFF`) in every B slot. A
  missing sidechain and a mono sidechain are different states (D-002) and the map carries both; collapsing
  them would turn "no sidechain" into "convolve against the dry bus", so the sentinel sits outside every
  real index range and an unhandled one fails as out-of-range. `chA == 0` is `empty` — A is mandatory.
* **Statuses, not exceptions:** `ok`, `empty`, `asymmetric`, `countMismatch`, `exceedsCapacity`, checked in
  that precedence so a layout gate can report *which* rule rejected a layout. A rejected result's map is
  value-initialised. `kMaxChannels` is 8 and `capacity` above it is clamped, never trusted. The ceiling binds
  the **bus** widths, not the processed count: a 9-wide sidechain on a mono main is `exceedsCapacity` even
  though the fold processes one channel.
* **Resolved at prepare time, copied by value onto the audio path.** No pointer, no allocation, no sample
  read; trivially copyable and 36 bytes at the ceiling, both `static_assert`ed. The shipped mappings
  (1,0), (2,0), (1,1), (2,1), (2,2), (1,2), (1,8), (8,0), (8,8) and past-ceiling (16,16) are pinned by
  `static_assert` in `ChannelMapTests.cpp`, so a rule edit that changes one of them fails the build. The
  consolidated family's *rejection* at `channelsOut > 1` is deliberately left to the runtime legs — a pin
  would hide that rule behind a compile error, which proves less about the code a caller runs (F-012).

Nothing in this library decides whether a layout may *exist* — that is `LayoutGate` below, which calls
`resolve` and adds the two rules the map cannot see. Its consumers are `HopScheduler::prepare` (indices →
pointers), `entangle::makeSchedulerConfig` in the shell (indices → ring sizes) and, through the gate,
`isBusesLayoutSupported`.

## LayoutGate (D-021)

`LayoutGate.hpp` answers the *other* question — **may this layout exist at all** — for
`EntangleAudioProcessor::isBusesLayoutSupported` and for the shell's prepare, from the same three counts:

```cpp
static constexpr LayoutDecision gateLayout(int chA, int chB, int chOut) noexcept;
```

| chA | chB | chOut | verdict | `rule` | processed |
|---|---|---|---|---|---|
| 1 | 0 / 1 | 1 | accepted | ok | 1 |
| 1 | 2…8 | 1 | accepted | ok | 1 | consolidated (D-018) |
| 1 | 9 | 1 | refused | exceedsCapacity | 0 |
| 2 | 0 / 1 / 2 | 2 | accepted | ok | 2 |
| 2 | 3…9 | 2 | refused | asymmetric | 0 |
| 3…8 | any | = chA | refused | `resolve`'s answer, kept | 0 | `mainWidth` |
| any | any | ≠ chA | refused | `resolve`'s answer, kept | 0 | `mainCountMismatch` |
| 0 | any | any | refused | empty | 0 | A is mandatory (D-017) |
| any | any | 0 | refused | empty | 0 | a disabled main output |

* **It owns exactly two rules, and `resolve` owns the rest.** `resolve` is called on every path and its
  status is kept in `rule` whatever the verdict, because a refusal that cannot say which rule it tripped on
  is the bare `false` D-017 ruled out; the two rules above it — the main-bus width (`kMaxMainBusChannels`,
  2) and `chOut == chA` (D-018) — are ones the map cannot express, since it answers "which source does
  output `c` read" and has no notion of a bus the plugin is willing to host. The ladder order is why a
  4-wide main with an unequal output answers `mainWidth` and a 0-channel main answers `channelRule` + `empty`
  rather than a re-labelled count complaint.
* **`chB <= chA` is a necessary condition, not this function's rule.** Read as a decision procedure that
  sentence admits 22 triples a host can present — 21 `asymmetric` ones (`(3, 2, 3)` … `(8, 7, 8)`) plus
  `(1, 9, 1)` past the ceiling. Measured 2026-11-04, and the reason the gate inherits the map's answer;
  `LayoutGateTests` asserts the paraphrase's over-admission as a failure and compares the gate against
  `resolve`, never against prose.
* **The width line is a deferral, not a law.** `resolve` takes an 8-wide main today; D-021 B keeps quad, 5.1
  and 7.1 out until item 04's allocation figures, M7's per-channel cost curve and M4 §6's surround-reporting
  question exist. Opening it is a one-constant edit *and* the revisit trigger says so.
* **The answer is 12 bytes and the accepted family is host-reachable now.** `verdict` + `rule` + `processed`,
  trivially copyable, `static_assert`ed. The `(1, chB 2…8, 1)` row is the reason this file exists: the fold
  has resolved since D-018 and computed since item 02, but the shell refused a wider-than-stereo sidechain
  itself, so nothing a host routed could reach it. `processed` is what `configureRings` uses as
  `activeChannels`, which is why the gate and the scheduler config cannot disagree about a triple (both read
  the same three counts, and `OfflineRender` §5.18 pins the equality on a live layout).
* **Count-blind by construction**, and measured that way: every `AudioChannelSet` with the same channel count
  gets the same verdict, named surround, quadraphonic and ambisonic included (§5.18, 94 named sets). That is
  the gate-level answer to M4 §6's ambisonic question and it is not a claim of surround support — an
  ambisonic first-order sidechain on a mono main folds to mono, like any other 4-channel set.

## ReconfigureKey (D-022)

`ReconfigureKey.hpp` names the configuration a prepared transport was built for, so a re-prepare can be
*earned* rather than assumed. It is the seventh field-list value type in this library and the only one that
exists to be compared:

```cpp
struct ReconfigureKey
{
    double sampleRate = 0.0;
    int hostBlock = 0, channels = 0, sidechainChannels = 0, outputChannels = 0;
    int hop = 0, grainLength = 0;
    friend constexpr bool operator==(const ReconfigureKey&, const ReconfigureKey&) noexcept = default;
};
```

* **Seven fields, and they are the ones the rings, the engine and the pre-fill were built from.** Sample rate
  (the DC blocker's coefficients, the follower's time constants), the host's callback size (every ring bound),
  `chA`/`chB`/`chOut` (ring counts, the map, the fold), the hop (scratch geometry, hop count, `R`) and the grain
  length (`F`, the history depth, the window table). Anything a prepare reads is in the key; anything it does
  not read — blend, gain, bypass, the level dials — is not, because those move through targets, not geometry.
* **`grainLength` is the post-clamp value.** The shell's CPU rule (`clampGrainLength`, `EntangleProcessor.h`)
  maps several dial positions onto one running length, and only the running length is what the buffers are sized
  for. Storing the dial position would re-prepare on a move the clamp swallows — exactly the restart the guard
  exists to prevent, and C6's H = 256 half. It also means the key describes the state the host gets rather than
  a number the host asked for that the plugin declined.
* **Equality only, no policy.** The type has no idea whether a layout is allowed (`LayoutGate`) or what a change
  implies (`configureRings`). The guard lives in the shell because the shell owns `activeChannels`,
  `sidechainActive` and the latency call, and INDEX §3 rule 2 wants one decider per rule, not a rule smuggled
  into a struct.
* **32 bytes, pinned** the way `ChannelMap` pins 36 and `LayoutDecision` pins 12: `static_assert`ed
  `sizeof ≤ sizeof(double) + 6 * sizeof(int)`, trivially copyable, and
  `ReconfigureKey{} != ReconfigureKey{48000.0, 4096, 2, 2, 2, 128, 8192}` so a value-initialised key cannot be
  the configuration someone is about to prepare. The shell holds it as `std::optional`, so "nothing prepared
  yet" is a state, not a magic triple; the refusal path **clears** it, because remembering the key of a
  configuration that was never built would make the next prepare skippable, and a skipped prepare on something
  that was never built is silence with no reason attached (C13). `maxHostBlock` is deliberately *not* a field —
  it is `maxHostBlockFor(hostBlock)`, and storing both would be a second place for the two to disagree.

## HopScheduler

Owns the per-channel A and B input rings and the output ring. Per callback: append what
arrived, fire `processHop` only while every input holds a full `H`, then drain exactly the
sample count that arrived. A zero-length callback returns without touching state.

Consequences the shell relies on:

- **samples out == samples in, always** — including `numSamples == 0`, `numSamples < H`, and
  `numSamples > H` (several hops in one call). Short delivery asserts rather than padding
  with zeros (T2).
- **No partial hops** — a hop never fires with fewer than `H` per channel, so behaviour
  cannot depend on hostBlock (T1).
- **A missing sidechain cannot stall the hop clock** — B is silence, filled at ingest and at
  hop read (T3, D-002). Pass `bIn == nullptr` when the bus is inactive; a `kSilentSource` slot points
  at the same zeros, and an absent bus is never read as channel 0 (D-017).
- **`SchedulerConfig` counts sources, `map.channels` counts what gets processed** — `channels` is the
  A-side bus width (how many A pointers `process` holds) and `sidechainChannels` the B-side one, so a
  mono main with a stereo sidechain is `channels = 1, sidechainChannels = 2`. Rings and hop scratch
  are allocated per *source* on the way in and per *processed* channel on the way out; the two differ
  exactly in the consolidation family, and sizing B scratch by the processed count is the overflow
  F-014 records.
- **Broadcast is a repeated pointer, not a copy** — `prepare` resolves the map into `hopA`/`hopB` once
  and `process` never consults it again, so a side narrower than the processed count reaches the Engine
  as one address in several slots. Nothing is duplicated and no per-slot scratch exists (D-017).
- **The fold is the only cross-channel read, and it is one mono hop** — when a B slot says
  `kConsolidatedSource` (D-018), every hop sums the `chB` sidechain sources into `foldBuffer`, scaled
  once by `1/chB` after the sum, and every consolidated slot points at that one buffer. It runs between
  the B hop-reads and `processHop`, never writes into a source slice, and adds no allocation to the
  callback. Two decorrelated sources averaged are 3.01 dB quieter than one of them alone (measured
  −2.985 dB through the shipped builder, `OfflineRender` §5.17); what that does to the *correction* is
  not the same −3 dB, because the level stage reads the side through `√(envA · envB)` — half-slope —
  and its own convolved level moves with it. Measured figure and the arithmetic it corrects: D-018.
- **`prepare` refuses a map it cannot honour** — a processed count outside `[1, kMaxChannels]` or an
  index naming a ring that does not exist throws `std::invalid_argument` *before* any member moves, so
  the callback never runs on half-written geometry and a rejected call leaves the previous
  configuration standing. The map is a value the shell builds off the audio thread; this is the seam's
  one place that a caller error can be reported (F-014).
- **Latency lives in the pre-fill** — `prepare` writes `latencySamples` zeros into the output
  ring, which is what makes exact-count delivery possible at all. `PassthroughEngine` is a
  bit-exact copy, so the end-to-end stream is exactly A delayed by `latencySamples` for any
  value, including one that is not a multiple of `H`. See D-004 in `docs/decisions.md`.

## Capacity bounds

Derived in `prepare`, never guessed at a call site:

| Ring | Bound |
|---|---|
| input (A, B) | `maxHostBlock + hop` — at most `hop - 1` leftover plus one callback |
| output | `latencySamples + maxHostBlock` — pre-fill plus one callback's production |
| hop scratch | `hop * channels` per buffer, contiguous because the Engine contract is |

`CircularBuffer` rounds each capacity up to a power of two, so the input ring is 8192 floats (32 KiB) at every
hop the shell can dial while the host block sits at or below the 4096 floor — the rounding, not the hop, is what
that byte costs (measured per configuration in `tools/PrepareFootprint`, and the reason `maxHostBlockFor` is a
floor with a documented consequence rather than a bare constant). A re-prepare at a narrower triple releases the
*rings* it no longer needs and keeps the hop scratch's high-water capacity; both figures, and which is which, are
asserted in `reconfigure: a narrower re-prepare releases the sidechain rings` and
`reconfigure: hop scratch keeps its capacity across a narrowing` (D-022 C, C10).

There are `chA` A rings, `chB` B rings and `map.channels` output rings; the hop scratch follows the
same split (`hop × chA`, `hop × chB`, `hop × map.channels`) plus one `hop`-sized fold buffer when the
map consolidates. A side narrower than the processed count therefore costs *less* memory, never a
copy — which is the invariant F-014 exists to keep.

## Threading

`prepare` allocates and is message-thread/host-thread only. `process` and `reset` allocate
nothing, lock nothing, and touch only pre-allocated state, so they are audio-thread safe.
The scheduler borrows the `Engine&`; the shell owns its lifetime. The mix and level targets are plain
members: `processBlock` writes them on the audio thread, and the shell's message-thread writes happen
with `suspendProcessing(true)` held, so nothing races the callback and the stage needs no lock and no
atomic.

## Build and test

```bash
cmake --build build/release --target Engine -j4
./build/release/src/Engine/tests/EngineTests        # the scheduler's cases
./build/release/src/Engine/tests/MixEngineTests     # the law, the stages, the ramps, the chain order
./build/release/src/Engine/tests/ChannelMapTests    # the channel map
./build/release/src/Engine/tests/LayoutGateTests    # the layout gate
./build/release/src/Engine/tests/ReconfigureTests   # the reconfigure key + the scheduler's ring accounting
ctest --test-dir build/release -R "channel map" --output-on-failure
ctest --test-dir build/release -R "layout gate" --output-on-failure
ctest --test-dir build/release -R "reconfigure" --output-on-failure
ctest --test-dir build/release -R "level stage runs after" --output-on-failure
ctest --test-dir build/release -R "broadcast|fold|consolidat" --output-on-failure
```

Tests live in `tests/` and link no JUCE. `EngineTests`, `MixEngineTests`, `ChannelMapTests`, `LayoutGateTests`
and `ReconfigureTests` are five binaries because a test that counts allocations replaces global `operator new`,
and a program may only define that once. `ReconfigureTests` carries that hook for the same reason and is where
the *byte* claims live — live bytes per triple, what a narrowing gives back, what hop scratch keeps — while the
shell-level consequences of the guard (bit-exactness across a real `setBusesLayout`, the clamp-swallowed dial
move, the latency figure, the sizing floor) live in `OfflineRender` §5.19, because they need a real
`AudioProcessor` and that program may not define the hook at all (§0 C of `plan/M4-impl-plan/04`).

**Do not filter these by target name.** `catch_discover_tests` registers by Catch2 case name and the cases
in this directory carry no class prefix, so the `-R "HopScheduler|MixEngine"` this file used to document
selects **1** case where the 35 scheduler and mix cases are what it was meant to run — and exits 0, a false
green (measured 2026-11-01, against 26 then). Either run the binaries above, or filter on case text:
`"channel map"` matches all 17 map cases because their names start with those words, `"layout gate"` all 9
gate cases for the same reason, `"level stage runs after"` matches exactly the one case that guards the
blocker-before-stage ordering, and `"broadcast|fold|consolidat"` matches 13 — the 6 scheduler legs item 02
put around the pointer resolution and the mono-main fold, 6 map cases whose names happen to carry the
word *broadcast* or *consolidated* (#80–#85), and the gate case item 03 added for the newly reachable family
(#99, "…accepted as one consolidated channel"), so read a red map case as a rule change, not a scheduler
regression. It is the quickest check that `prepare` or `foldSidechain` did not move a pointer or rescale a
mean.
