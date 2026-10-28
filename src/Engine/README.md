# Engine

The processing seam the plugin shell drives. JUCE-light by rule (§9): no JUCE header in
`Engine.hpp`, `PassthroughEngine.hpp` or `HopScheduler.hpp`, and the library links only
`Container`, so it compiles and tests as a console program with no `AudioProcessor`.

## The seam

`Engine` (M1 §5) is the only thing the shell knows about what the plugin does:

```cpp
virtual void prepare(const EngineConfig&) = 0;
virtual void reset() = 0;
virtual int  latencySamples() const = 0;
virtual void processHop(const float* const* a, const float* const* b,
                        float* const* out, int numChannels) = 0;
```

`EngineConfig{sampleRate, hop, maxChannels, latencySamples}` is the whole world view an
Engine gets. **Contract:** `processHop` is allocation-free, deterministic, unaware of
hostBlock, and must not add delay beyond what `latencySamples()` declares. M2 replaces
`PassthroughEngine` without touching the scheduler (T8).

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
The scheduler borrows the `Engine&`; the shell owns its lifetime.

## Build and test

```bash
cmake --build build/release --target Engine -j4
ctest --test-dir build/release -R HopScheduler --output-on-failure
```

Tests live in `tests/` and link no JUCE.
