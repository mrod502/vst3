# Grain

Mode 1's wet path: windowed grains of A and B, linearly cross-convolved by FFT, overlap-added.
JUCE-light by rule (§9) — nothing here includes a JUCE header, so the engine compiles and tests as
a console program and the transform arrives through `grain::TransformFactory`.

## The seam

`GrainConvolutionEngine final : engine::Engine` — the shell drives it through the existing seam and
never learns which transform is installed. Per hop, per channel:

```
push both → extract both windowed → forward both → multiplyHalfSpectrum → inverse → emitAndAdd
```

`grain::TransformFactory::create(int sizeF)` is the only injection point. The engine cannot build its
own backend (the JUCE implementation is a separate library), and owning the choice is what lets the
identical engine run against `fft::Dft` and `fft::JuceFft` — which is how §6.1 is satisfied twice.

| Piece | Contract |
|---|---|
| `GrainHistory` | last L samples of one signal on one channel; `extractWindowed` writes `dst[0,L) = w[i]·x[t−L+i]` and zeroes `dst[L,F)`. Absolute indices below zero read as silence, so the first grain needs no special case. |
| `OlaAccumulator` | emit H at head → zero them → advance head by H → add `scale·y[0,2L−1)`. **That order is the latency model** — see F-005. |
| `GrainWindow` | periodic Hann `0.5·(1 − cos(2πi/L))`, built in double, cast once. `colaSumAtPhase`/`idealColaSum` = `L/(2H)`. |
| `RingSpan` | shared power-of-two wrap arithmetic, so no caller open-codes a mask. |

## Time and gain

A grain starting at `s` is complete at `s+L` and its result sample `j` is emitted at `s+L+j`, so
**physical wet delay is exactly L** and the engine adds nothing beyond it. `latencySamples()` returns
the declared `R ∈ [0, L]`, an independent choice that moves what the host compensates for and M3's
dry-path delay — never this arithmetic. `wetDelaySamples()` reports L for diagnostics only.

`F = nextPow2(2L−1)`, the linear-convolution length; the accumulator's live span is `2L−1`, which is
what makes F a sufficient ring.

Wet gain is `1/C` with `C = L/(2H)`, computed once in `prepare`. That is the whole gain table: the
transform owns `1/F` and every backend factor, so a second factor here would cost `20·log10(F)` dB
(F-004). Because `multiplyHalfSpectrum` takes **no conjugate**, the inverse is a convolution of span
`2L−1` — which is what makes the delta anchor exact. Add a conjugate and both the span and the anchor
change; five tests go red if you try it.

## Threading

`prepare` allocates, builds the transform and throws `std::invalid_argument` on an unusable config
(L not a positive multiple of H; `R` outside `[0, L]`; non-positive channel capacity). `processHop`
allocates nothing, takes no lock, throws nothing — the callback is total (§7). One instance is driven
by one thread. Scratch is one set reused across channels, sized in `prepare` to `F` and `F+2`.

## Build and test

```bash
cmake --build build --target Grain -j4
ctest --test-dir build -R grain --output-on-failure
```

Two test programs run one shared set of assertions (`tests/GrainEngineChecks.hpp`) — `GrainTests` on
the `Dft` oracle, `GrainEngineJuceTests` on the platform transform. `GrainEngineTests.cpp` also
carries the hand-built `ManualPipeline`, proven bit-identical to the engine, which is what lets a
test run the same arithmetic behind a window the engine will never install (the hop-rate ripple
test's symmetric-Hann known-bad).

Not driven here: `R = 0` through `HopScheduler` at `hostBlock < H` aborts on an empty output ring
(F-006), so block-size invariance runs at `R = H` and `R = 0` is proven at the engine seam.
