# Engine

The processing seam the plugin shell drives. JUCE-light by rule (§9): no JUCE header in
`Engine.hpp`, `PassthroughEngine.hpp`, `MixEngine.hpp` or `HopScheduler.hpp`, and the library links
only `Container` and `Dsp`, so it compiles and tests as a console program with no `AudioProcessor`.

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
hostBlock, and must not add delay beyond what `latencySamples()` declares. The shell installs
`MixEngine(GrainConvolutionEngine)` through this seam without any change to the scheduler (T8);
`PassthroughEngine` stays for a harness that wants a pure copy.

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

* **Two safety stages, in decision C's order (D-012, D-013).** The convolved stream passes through
  `dsp::DcBlocker` *before* the sum — A and B reach the sum unfiltered, which is what keeps the ±1
  endpoints bit-exact — and the summed, gain-scaled output passes through `dsp::SoftClipper` last, so
  nothing downstream of this stage can exceed the ceiling. Neither has a say in the latency: the
  blocker defers nothing and the clipper is memoryless. Both are installed by the three-argument
  constructor; the two-argument one installs neither, which is what a harness that wants the chain
  without the safety stages uses.

* **No gain, no delay of its own.** `latencySamples()` and `wetDelaySamples()` forward untouched, so
  the wet path is exactly what the inner engine declared; the dry path needs no buffer of its own
  because the scheduler's pre-fill delays everything it drains by `R`, dry included. Bypass
  crossfades rather than reroutes, so it must never move the declared latency.
* **The ramps live here, in the sample domain.** `setBlendTarget`/`setOutputGainTarget`/
  `setBypassTarget` store a target that is then stepped once per output sample — straight line, 20 ms
  for blend and gain, 10 ms for bypass, the last step assigning the target exactly. A ramp length is
  milliseconds, never host callbacks. `prepare` and `reset` snap rather than ramp, so a new transport
  opens on the current setting.

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
  hop read (T3, D-002). Pass `bIn == nullptr` when the bus is inactive; channels beyond
  `sidechainChannels` read silence, and upmix policy is M4's, not this file's.
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

`CircularBuffer` rounds each capacity up to a power of two.

## Threading

`prepare` allocates and is message-thread/host-thread only. `process` and `reset` allocate
nothing, lock nothing, and touch only pre-allocated state, so they are audio-thread safe.
The scheduler borrows the `Engine&`; the shell owns its lifetime. The mix targets are plain members:
`processBlock` writes them on the audio thread, and the shell's message-thread writes happen with
`suspendProcessing(true)` held, so nothing races the callback and the stage needs no lock and no
atomic.

## Build and test

```bash
cmake --build build/release --target Engine -j4
ctest --test-dir build/release -R "HopScheduler|MixEngine" --output-on-failure
```

Tests live in `tests/` and link no JUCE. `EngineTests` and `MixEngineTests` are two binaries because
a test that counts allocations replaces global `operator new`, and a program may only define that
once.
