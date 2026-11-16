# Engine

The processing seam the plugin shell drives, plus the scheduler that feeds it. JUCE-light: no JUCE
header here, and the library links only `Container` and `Dsp`, so it compiles and tests as a console
program with no `AudioProcessor`.

```
Engine.hpp / PassthroughEngine.hpp   the seam, and a bit-exact copy implementation
MixEngine.hpp                        the host controls: blend, gain, bypass, the three stages
ChannelMap.hpp                       which source each output reads, per side
LayoutGate.hpp                       whether a layout may exist at all
ReconfigureKey.hpp                   what a prepared transport was built for
HopScheduler.hpp/.cpp                rings, hop clock, latency pre-fill, channel pointers
```

## The seam

```cpp
virtual void prepare(const EngineConfig&) = 0;
virtual void reset() = 0;
virtual int  latencySamples() const = 0;
virtual int  wetDelaySamples() const { return latencySamples(); }
virtual void processHop(const float* const* a, const float* const* b,
                        float* const* out, int numChannels) = 0;
```

`EngineConfig{sampleRate, hop, maxChannels, latencySamples, grainLength}` is the whole world view an
Engine gets. `processHop` is allocation-free, deterministic, unaware of `hostBlock`, and adds no
delay beyond what `latencySamples()` declares. Either input side may be narrower than
`numChannels` and one pointer may appear in several slots — `numChannels` counts *processed*
channels, never a bus width — so per-channel state is keyed by index, not by address.

## MixEngine

`out = clip( g · [ β · ( w_dry · dry + w_wet · block(conv) ) + (1 − β) · A ] )`, with `t = |p|` on the
blend dial `p ∈ [-1, 1]`, `w_dry = sinf(t·π/2)`, `w_wet = cosf(t·π/2)`, `β` the bypass crossfade and
`g` the gain dial converted from dB. The dry bus is A for `p < 0` and B otherwise; the handover at
`p == 0` is silent because `w_dry == 0`. `mixLawWeights` snaps `t == 0` to (0, 1) and `t >= 1` to
(1, 0), because `cosf(pi/2f)` is `-4.37e-08` and a wet term surviving at a hard-dry dial makes
"±1 is the pure dry render" unmeasurable. The no-sidechain pin that forces `p = -1` is the shell's
(`EngineChain.h`), not this stage's.

- **Chain order is fixed:** `conv → DcBlocker → LevelNormalizer → sum → g → SoftClipper`. Only
  `conv` passes through the stages — A and B reach the sum unfiltered, which keeps the ±1 endpoints
  bit-exact. Blocker-before-normalizer is load-bearing: swapped, the normalizer reads the DC inside
  its envelope and applies the reciprocal gain instead of the tone ratio.
- **A stage that is not installed is not applied at all.** The blocker and clipper come from the
  three-argument constructor (the two-argument one installs neither); the normalizer arrives through
  `installLevelNormalizer` and is asserted present at install time, because it is sized for the hop
  in `prepare`.
- **No gain and no delay of its own.** `latencySamples()` and `wetDelaySamples()` forward untouched;
  the dry path needs no buffer because the scheduler's pre-fill delays everything it drains, dry
  included. Bypass crossfades rather than reroutes, so it never moves the declared latency.
- **Ramps are in the sample domain:** `setBlendTarget` / `setOutputGainTarget` / `setBypassTarget`
  step once per output sample — 20 ms for blend and gain, 10 ms for bypass, the last step assigning
  the target exactly. A ramp length is milliseconds, never host callbacks. `prepare` and `reset` snap.
  The level targets (`setNormalisationAmountTarget`, `setSilenceFloorTarget`) are deliberately not
  ramped here — the applied gain is already continuous along the hop — and both no-op with no stage
  installed.

## ChannelMap

`ChannelMap::resolve(chA, chB, channelsOut, capacity = kMaxChannels)` answers one question — how
many channels are processed, and which source does each read on each side — once, off the audio
thread. It is the only place the channel rule lives; the gate and the scheduler both consume it and
neither may re-derive it.

Rules in check order: a side broadcasts channel 0 **exactly when it has one channel**; the sides must
agree when both are multi-channel; **a mono main with a wider sidechain consolidates** (one processed
channel, the sidechain as the arithmetic mean of its channels); `channelsOut ≤ max(chA, chB)`, or
exactly 1 once consolidation applies. Fewer outputs select the leading channels.

- **The rule keys on the main bus and is not symmetric** in (`chA`, `chB`): `(1, 4)` is one
  consolidated channel, `(4, 1)` is four broadcast channels.
- **`kConsolidatedSource` (`0xFFFE`)** marks a consolidated B slot. The mean itself is computed by
  `HopScheduler`, which owns the one mono hop it lands in, so nothing in `Dsp`, `Fft` or `Grain`
  reads across channels. It is a source selector, not a flag, and it sits outside every real index
  range — a consumer that treats it as an index fails out of range rather than quietly reading
  channel 0.
- **`kSilentSource` (`0xFFFF`)** marks every B slot when `chB == 0`. A missing sidechain and a mono
  sidechain are different states; collapsing them would turn "no sidechain" into "convolve against
  the dry bus". `chA == 0` is `empty` — A is mandatory.
- **Statuses, not exceptions:** `ok`, `empty`, `asymmetric`, `countMismatch`, `exceedsCapacity`, in
  that precedence, so a gate can report *which* rule rejected a layout. A rejected map is
  value-initialised. `kMaxChannels` is 8, `capacity` above it is clamped, and the ceiling binds the
  **bus** widths — a 9-wide sidechain on a mono main is `exceedsCapacity` even though the fold
  processes one channel.
- **Resolved at prepare time, copied by value onto the audio path:** trivially copyable, 36 bytes at
  the ceiling, both `static_assert`ed, with the shipped mappings pinned in `ChannelMapTests`.

## LayoutGate

`gateLayout(chA, chB, chOut)` answers *may this layout exist* for `isBusesLayoutSupported` and for
the shell's prepare. It calls `resolve` on every path, keeps `resolve`'s status in `rule`, and adds
the two rules the map cannot see: `chA ≤ kMaxMainBusChannels` (2) and `chOut == chA`.

| chA | chB | chOut | verdict | `rule` | processed |
|---|---|---|---|---|---|
| 1 | 0 / 1 | 1 | accepted | ok | 1 |
| 1 | 2…8 | 1 | accepted | ok | 1 (consolidated) |
| 1 | 9 | 1 | refused | exceedsCapacity | 0 |
| 2 | 0 / 1 / 2 | 2 | accepted | ok | 2 |
| 2 | 3…9 | 2 | refused | asymmetric | 0 |
| 3…8 | any | = chA | refused | `resolve`'s answer | 0 |
| any | any | ≠ chA | refused | `resolve`'s answer | 0 |
| 0 | any | any | refused | empty | 0 |
| any | any | 0 | refused | empty | 0 |

- `chB <= chA` is a necessary condition, **not** the decision procedure — read that way it admits 22
  triples this gate refuses. The gate is compared against `resolve`, never against prose.
- The width line is a deferral, not a law: `resolve` takes an 8-wide main today. Opening it is a
  one-constant edit, gated on the revisit triggers in D-021.
- **Count-blind by construction**: every `AudioChannelSet` with the same channel count gets the same
  verdict, named surround and ambisonic sets included. That is not a claim of surround support.
- The answer is 12 bytes, trivially copyable, `static_assert`ed. `processed` is what `configureRings`
  uses as `activeChannels`, so gate and scheduler config cannot disagree.

## ReconfigureKey

Names the configuration a prepared transport was built for, so a re-prepare is earned rather than
assumed: `sampleRate`, `hostBlock`, `channels`, `sidechainChannels`, `outputChannels`, `hop`,
`grainLength`, compared by defaulted `operator==`.

- The fields are the ones the rings, the engine and the pre-fill were built from. Blend, gain,
  bypass and the level dials are absent: those move through targets, not geometry.
- **`grainLength` is the post-clamp value** — the shell's `clampGrainLength` maps several dial
  positions onto one running length, and only the running length sizes buffers.
- **`maxHostBlock` is not a field** — it is `maxHostBlockFor(hostBlock)`; storing both gives two
  places to disagree.
- Equality only, no policy: allowance is `LayoutGate`'s, consequences are the shell's
  `configureRings`. 32 bytes, `static_assert`ed; the shell holds it in a `std::optional`, and the
  refusal path **clears** it, so a configuration that was never built can never be "already prepared".

## HopScheduler

Owns the per-channel A and B input rings and the output ring. Per callback: append what arrived,
fire `processHop` only while every input holds a full `H`, then drain exactly the sample count that
arrived. A zero-length callback returns without touching state.

- **Samples out == samples in, always** — including `numSamples == 0`, `< H`, and `> H` (several hops
  in one call). Short delivery asserts rather than padding with zeros.
- **No partial hops**, so behaviour cannot depend on `hostBlock`.
- **A missing sidechain cannot stall the hop clock** — pass `bIn == nullptr`; B is silence at ingest
  and at hop read, and an absent bus is never read as channel 0.
- **`SchedulerConfig` counts sources; `map.channels` counts what is processed.** Rings and hop scratch
  are allocated per *source* on the way in and per *processed* channel on the way out. The two differ
  exactly in the consolidation family, and sizing B scratch by the processed count overflows.
- **Broadcast is a repeated pointer, not a copy** — `prepare` resolves the map into `hopA`/`hopB`
  once and `process` never consults it again.
- **The fold is the only cross-channel read** — a `kConsolidatedSource` B slot sums the `chB` sources
  into one mono hop, scaled once by `1/chB` after the sum, between the B hop-reads and `processHop`.
  It never writes into a source slice and adds no allocation to the callback.
- **`prepare` refuses a map it cannot honour** — a processed count outside `[1, kMaxChannels]` or an
  index naming a ring that does not exist throws `std::invalid_argument` *before* any member moves,
  leaving the previous configuration standing. This is the seam's one place a caller error can be
  reported.
- **Latency lives in the pre-fill** — `prepare` writes `latencySamples` zeros into the output ring,
  which is what makes exact-count delivery possible.

## Capacity bounds

Derived in `prepare`, never guessed at a call site.

| Ring | Bound |
|---|---|
| input (A, B) | `maxHostBlock + hop` — at most `hop - 1` leftover plus one callback |
| output | `latencySamples + maxHostBlock` — pre-fill plus one callback's production |
| hop scratch | `hop * channels` per buffer, contiguous because the Engine contract is |

There are `chA` A rings, `chB` B rings and `map.channels` output rings, with hop scratch split the
same way plus one `hop`-sized fold buffer when the map consolidates. A side narrower than the
processed count therefore costs *less* memory, never a copy. `CircularBuffer` rounds each capacity up
to a power of two — see `src/Container`. A re-prepare at a narrower triple releases the rings it no
longer needs and keeps the hop scratch's high-water capacity.

## Threading

`prepare` allocates and is message-thread/host-thread only. `process` and `reset` allocate nothing,
lock nothing and touch only pre-allocated state. The scheduler borrows the `Engine&`; the shell owns
its lifetime. The mix and level targets are plain members: `processBlock` writes them on the audio
thread, and message-thread writes happen with `suspendProcessing(true)` held, so no lock and no
atomic is needed.

## Build and test

```bash
cmake --build build/release --target Engine -j4
./build/release/src/Engine/tests/EngineTests        # scheduler
./build/release/src/Engine/tests/MixEngineTests     # law, stages, ramps, chain order
./build/release/src/Engine/tests/ChannelMapTests    # channel map
./build/release/src/Engine/tests/LayoutGateTests    # layout gate
./build/release/src/Engine/tests/ReconfigureTests   # key + ring accounting
ctest --test-dir build/release -R "channel map" --output-on-failure
ctest --test-dir build/release -R "layout gate" --output-on-failure
ctest --test-dir build/release -R "reconfigure" --output-on-failure
```

Five binaries because a test that counts allocations replaces global `operator new`, and a program may
define that once. **Never filter by target name** — `ctest -R EngineTests` selects nothing and exits 0.
Case names carry the prefix: `"channel map"`, `"layout gate"`, `"reconfigure"`, `"level stage runs after"`.
The byte-level claims live in `ReconfigureTests`; what the guard does inside a real
`setBusesLayout` is `OfflineRender` §5.19, which needs an `AudioProcessor` and may not define the hook.

## Decisions

D-004, D-010, D-011, D-017, D-018, D-021, D-022 — `docs/decisions.md`.
