# Dsp

Small, independent signal primitives — JUCE-light, allocation-free on the audio path, unaware the
plugin exists. `engine::MixEngine` (`src/Engine`) is the only caller that decides which of them run
and in what order.

## Contracts

| Class | Promise | Constants |
|---|---|---|
| `dsp::DcBlocker` | Total DC removal on a stream that must keep its band. Second-order Butterworth high-pass, transposed direct form II, `double` state. | `cornerHz` = 5.0 (constructor argument); `settlingSamples()` publishes the settling bound |
| `dsp::SoftClipper` | Output safety: identity to 0 dBFS, then a monotone `tanh` ramp to a hard ceiling. Memoryless. | `knee` = 1.0 (0 dBFS), `ceiling` = 2.0 (+6 dB); non-finite in → `0.0f` out, counted by `nonFiniteSamples()` |
| `dsp::LevelDetector` | Reads level, decides nothing: windowed RMS and windowed peak over `windowSamples()`, plus an attack/release envelope of a smoothed RMS, per channel. | `envelopeFloor` = 1e-12; `attackMs()` / `releaseMs()` are the caller's; `innerTimeConstantMs()` = min(attack, release)/100 |
| `dsp::LevelNormalizer` | Decides and applies the correction: three detectors (A, B, wet) → target → one gain on the wet hop. Hop-granular. | `gain = (√(envA·envB) / envConv)^amount`, conv read **upstream** of the gain it sets; `gainRangeDb` = ±36; slew cap `2·gainRangeDb` crossed in `fullTravelMs` = 200 ms (360 dB/s); `attackMs` = 20 / `releaseMs` = 100; `defaultAmount` = 1.0 over `[0, 1]` in 1/128 steps; `defaultFloorDb` = −60 over `[−90, −20]` in 0.125 dB steps |

## Invariants

- **The blocker's DC null is a property of its coefficients** — `b1 = -2·b0`, `b2 = b0`, so
  `b0 + b1 + b2` is exactly zero in `double`. Its state stays `double`: at the shipped corner the
  pole radius is ≈ 0.99985 and a `float` state floors the residue at ~1e-7 *of the signal*, where
  the requirement is stated against the *offset*.
- **The clipper holds no signal state** — output sample *q* depends on input sample *q* alone.
  Neither it nor the blocker defers a sample, so neither enters the latency model.
- **The detector's envelope is two stages** — a linear mean-square smoother at
  `innerTimeConstantMs()`, then an amplitude-domain follower. A follower of the raw square is biased
  by the ripple in its own input; a follower slow enough to average that ripple cannot follow a step
  at its own attack rate. Equal attack and release leave a plain one-pole with no crest bias.
- **Silence is exact** on all three readings — the RMS sum is forced to 0.0 the moment its window
  holds only zeros (tracked by count, not threshold) and both envelope stages flush.
- **The normalizer's conv detector is fed *before* the wet hop is scaled**, so the gain is set from
  the level the engine actually produced. Feeding it after makes the stage an AGC loop with a pump
  period nobody chose.
- **Half-slope is the behaviour**: in the dB domain the target is the arithmetic mean of the two bus
  readings, so a change that hits one bus alone moves the held level by half of it.
- **Unity whenever any of the three readings is at or below `floorDb()`**; a non-finite reading gives
  unity for that hop and increments `nonFiniteHops()`.
- **The slew cap is a speed (360 dB/s), not a per-hop increment** — the same dial travel takes the
  same time at H = 64 as at H = 4096. The applied gain walks a straight line and the *last* sample is
  assigned, because `from + (to - from) * 1.0f` does not land on `to` in float.

## Threading

`prepare` allocates and belongs off the audio thread: it derives coefficients and sizes one `State`
per channel (detector: one window of samples plus a monotone queue of at most one entry per slot,
12 bytes × `windowSamples()` × channels). The normalizer's `prepare` takes the hop as well as the
rate, because its slew cap is per hop, and sizes three detectors plus one gain state per channel.
`processChannel` touches only the channel it names — no allocation, no lock, no I/O — so a stream
renders bit-identically whatever chunk it arrives in and one channel cannot reach another's. The
non-finite counters are `std::atomic` with relaxed ordering: a diagnostic read-out nothing
synchronises on.

## Build and test

```bash
cmake --build build/release --target Dsp -j4
build/release/src/Dsp/tests/DspTests
ctest --test-dir build/release -R "level normalizer" --output-on-failure
```

One `DspTests` binary over `tests/`, driving the classes directly with synthetic streams. The legs
that count allocations replace global `operator new`, so this directory must not gain a second
binary with that hook. `ctest -R DspTests` matches nothing — filter on case-name text.

Shell-level legs (what the whole shipped chain does with these stages) live in `tools/OfflineRender`,
not here: this library stays testable without an `AudioProcessor`.

## Decisions

D-012, D-013, D-014, D-015, D-016 — `docs/decisions.md` carries the measured figures each was picked
against.
