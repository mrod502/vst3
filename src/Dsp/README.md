# Dsp

Signal primitives, JUCE-light by rule (§9): nothing in this directory includes a JUCE header, so the
library and its tests build and run as plain console programs. It carries the two output-path safety
stages M3 item 02 landed, the level detector item 03 landed, and the level normalizer item 04 landed.
The first three are per-sample; the normalizer works a hop at a time because that is the unit its
readings and its ramps are stated in. All four are allocation-free on the audio path and know nothing
about grain lengths or the plugin.

## Contracts

| Class | What it promises | Figures |
|---|---|---|
| `dsp::DcBlocker` | Total DC removal on a stream that must keep its band. Second-order Butterworth high-pass, transposed direct form II, double state. | corner `cornerHz` = 5.0 Hz (constructor argument); realised to ≤ 0.006 % of design at 44.1/48/96/192 kHz; −0.00106 dB at 8× the corner and ≤ +3.2e-8 dB anywhere above it; DC residue 3.13e-8 of the offset it holds back; settling bound 37 315 samples = 0.777 s at 48 kHz, published by `settlingSamples()` |
| `dsp::SoftClipper` | Output safety: the identity up to 0 dBFS, then a monotone `tanh` ramp to a hard ceiling. Memoryless. | `knee` = 1.0f (0 dBFS), `ceiling` = 2.0f (+6 dB); C² across the knee; non-finite in → `0.0f` out, counted by `nonFiniteSamples()` |
| `dsp::LevelDetector` | Reads level and decides nothing: windowed RMS and windowed peak over `windowSamples()`, and an attack/release envelope of a smoothed RMS, per channel. Measurement only — no target, floor, gate or gain. | Windowed RMS reads A/√2 of a stationary sine to 0.076 % worst (997 Hz, 100 ms window) and the envelope to 0.043 %; mean-rectifying would read 9.97 % low, which is what the tolerance is set against. Envelope crosses 63.2 % of a step at `attackMs()` (+0.63 % measured) and 36.8 % at `releaseMs()` (+0.21 %); the same ms land within 0.68 % at all four rates. `envelopeFloor` = 1e-12; silence is exact at `windowSamples()` on the windowed pair and at `silenceSamples()` on the envelope — 129 313 samples = 2.694 s at release 100 ms/48 kHz |
| `dsp::LevelNormalizer` | Decides and applies the correction: three detectors (A, B, wet) → target `√(envA·envB)` → one gain on the wet hop. Hop-granular, silent below the floor, bounded in gain and in slew. | `gain = (√(envA·envB) / envConv)^amount`, with conv read *upstream* of the gain it sets, clamped to `gainRangeDb` = ±36 dB; slew capped at `slewDbPerHop()` = `2·gainRangeDb` crossed in `fullTravelMs` = 200 ms, i.e. 360 dB/s — 0.96 dB at H = 128 / 48 kHz; `attackMs` = 20 / `releaseMs` = 100 ms; `defaultAmount` = 1.0 over `[0, 1]` in 1/128 steps, `defaultFloorDb` = −60 over `[−90, −20]` in 0.125 dB steps; unity whenever any of the three readings is at or below `floorDb()`; non-finite reading → unity for that hop and `nonFiniteHops()` counted. On the anchor it lands the correction to 2.4e-07 dB and holds the worst hop-level error to 0.38 dB where the bare path spans 9.6 dB |

`DcBlocker` writes `b1 = -2*b0` and `b2 = b0` so `b0 + b1 + b2` is exactly zero in double — the DC null
is a property of the coefficients rather than something the recursion converges towards. Its state is
double because the pole radius is ≈ 0.99985 at the shipped corner and a float state that slow leaves a
floor of order 1e-7 *of the signal*, where the criterion is stated against the *offset*.

`SoftClipper` holds no signal state at all, so output sample *q* depends on input sample *q* alone.
That is what keeps both stages out of the latency model: neither defers a sample, and the ceiling sits
above unity because identity up to `|x| = 1` plus a continuous first derivative at the knee leaves no
monotone shape bounded by 1. See D-012 and D-013 in `docs/decisions.md` for what the figures were
picked against and what they cost on the anchor.

`LevelDetector`'s envelope is two stages — a linear mean-square smoother at
`innerTimeConstantMs()` = min(attack, release)/100, then an amplitude-domain follower — because a
single state cannot do both jobs. A follower of the raw square is biased by the ripple in its own
input, so it reads a stationary sine high; a follower slow enough to average that ripple away cannot
follow a step at the rate its attack constant names. D-014 records the arithmetic. With attack and
release set equal the second stage is a plain one-pole and has no crest bias at all; unequal constants
leave a bias of the order of the ripple the inner stage leaves through, which is why the inner
constant is reported rather than hidden. Silence is exact on all three readings: the RMS sum is forced
to 0.0 the moment its window holds only zeros (tracked by count, not threshold) and both envelope
stages flush, so no state ever sits at a subnormal value.

`LevelNormalizer` is the policy `LevelDetector` refuses to hold, and three things about it are easy to
get wrong from the outside. The conv detector is fed *before* the wet hop is scaled, so the gain is set
from the level the engine actually produced; feed it after and the stage becomes an AGC loop with a pump
period nobody chose. Half-slope is the behaviour rather than an error to correct: in the dB domain the
target is the arithmetic mean of the two bus readings, so a change that hits one bus alone moves the held
level by half of it. D-014's follower also reads a stationary sine about 7.8 % high when attack and
release differ, and that bias cancels in the applied ratio only while the buses and the wet stream carry
the same ripple content — D-016 records where it stops cancelling. And the slew cap is stated as a speed
(360 dB/s), not a per-hop increment, so the same dial travel takes the same time at H = 64 as at
H = 4096; the applied gain walks a straight line to the hop's decided value and the *last* sample is
assigned rather than evaluated, because `from + (to - from) * 1.0f` does not land on `to` in float.

## Threading

`prepare` allocates and belongs off the audio thread — it derives the coefficients and sizes one
`State` per channel for the largest configuration the caller will drive, and for the detector one
window of samples plus a monotone queue of at most one entry per slot, 12 bytes × `windowSamples()` ×
channels. The normalizer's `prepare` takes the hop as well as the rate, because its slew cap is per hop,
and sizes three of those detectors plus one gain state per channel. `processChannel` touches only the
state of the channel it names: no allocation, no lock, no
I/O, one sample at a time, so the same stream renders bit-identically whatever chunk it arrives in and
one channel's content cannot reach another's. The clipper's and the normalizer's non-finite counters are
`std::atomic` with relaxed ordering — a diagnostic read-out that nothing synchronises on.

## Dependencies

None. `Dsp` links no library but `EntangleAsserts` (private), and consumers `target_link_libraries`
against it: `Engine` uses it PUBLIC for its stages, holding one `DcBlocker` and one `SoftClipper` and —
when the caller installs one — one `LevelNormalizer`. `LevelNormalizer` is the only consumer here that
includes another class of this library rather than standing alone: it holds three `LevelDetector`s, one
per bus, all reading `envelope()`. That is **D-015** (2026-10-30): target `√(envA · envB)`, the envelope as
the driving reading, a UI-set silence floor with unity gain below it. `LevelDetector` is unchanged by it
and stays policy-free; the point of exposing the windowed pair alongside the envelope was exactly this,
that the rejected per-grain scheme stays live on the same object so the target can be revisited without
touching this library. **D-016** (2026-10-31) is the normalizer's own record — insertion point, gate,
derived values, the non-finite rule, and what was measured.

## Build and test

```bash
cmake --build build/release --target Dsp -j4
ctest --test-dir build/release --output-on-failure
```

`tests/` holds `DcBlockerTests.cpp`, `LevelDetectorTests.cpp`, `LevelNormalizerTests.cpp` and
`SoftClipperTests.cpp` in one `DspTests` binary. They drive the classes directly with synthetic streams —
a constant input, an offset on a carrier, a step, a known sine measured through a Goertzel bin — and print
the figures the decision entries quote. Allocation and chunk-invariance legs are in the same binary; the
tests that count allocations replace global `operator new`, so this binary must not gain a second one.

`catch_discover_tests` registers each case by its case name, so `ctest -R DspTests` matches nothing.
Filter on the names (`ctest -R "level normalizer"`) or run the binary at
`build/release/src/Dsp/tests/DspTests`.

Shell-level legs (A9, A10, B6, B7, the level stage's §5.12–§5.16 and the chain's §C criteria) live in
`tools/OfflineRender.cpp`, not here: they need the shell to say what the chain is, and this library must
stay testable without one.
