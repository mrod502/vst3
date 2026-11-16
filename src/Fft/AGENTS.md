# Fft

The transform seam: forward/inverse behind a small interface, so the installed implementation can be
replaced without touching convolution or grain code. JUCE-light. The JUCE-backed adapter is the
separate library `src/FftJuce`.

| Header | Holds |
|---|---|
| `Fft.hpp` | the contract, `fft::IFft` |
| `Dft.hpp` | the naive `O(F²)` oracle that defines what correct means |
| `Spectral.hpp` | `fft::multiplyHalfSpectrum`, the half-spectrum product |

## The contract

```cpp
virtual int  size() const noexcept = 0;                            // F
virtual void forward(const float* in, float* bins) const noexcept = 0;   // F    -> F + 2
virtual void inverse(const float* bins, float* out) const noexcept = 0;  // F + 2 -> F
```

Buffers are caller-owned and never aliased to each other.

**Layout** — half-spectrum, real and imaginary interleaved, bins `0 … F/2` inclusive, so `F + 2`
floats: `bins[2k]` is Re of bin k, `bins[2k+1]` is Im. `bins[1]` (DC imaginary) and `bins[F+1]`
(Nyquist imaginary) are written zero and **never read**: a caller may leave them unwritten and no
result may change. `Dft` reads only the real slots of those two bins; `JuceFft` zeroes both slots on
the way into the engine, which makes the promise hold for any engine.

**Normalisation** — forward is the plain unnormalised DFT, `X[k] = Σ x[n]·e^(-i2πkn/F)`; the inverse
carries `1/F`. So `inverse(forward(x)) == x` with no factor left over, and that `1/F` is *also* the
whole normalisation a spectral product needs: with both spectra unnormalised, `inverse(a·b)` is the
circular convolution of the two time signals and **no further factor is owed**. Applying one divides
by `F` twice and leaves the output `20·log10(F)` dB quiet.

## `Dft`

Sums accumulate in `double` and only the stored result is `float`, so it is more accurate than any
engine compared against it and a mismatch is always the engine's error. The cardinal twiddles at
`m = 0` and `m = F/2` are forced to exact values, because `std::sin(π)` returns ~1.2e-16 and that
residue would land in the Nyquist imaginary slot the contract promises is zero.

## `multiplyHalfSpectrum(a, b, out, sizeF)`

`out[k] = a[k]·b[k]` for `k` in `[0, F/2]`, all three buffers `F + 2` floats. It lives next to the
layout because the never-read rule was written into `Fft.hpp` to make its DC/Nyquist special-casing
legal.

- **Convolution, not correlation** — no conjugate anywhere. A conjugate reverses time, keeps every
  level test green and sounds plausible; the delayed-delta case is what turns it red.
- **No scale of its own**, per Normalisation.
- **DC and Nyquist** are real × real and both output imaginary slots are written `0.0f`, so the
  product satisfies the contract its inputs do. Every slot of `out` is written, so a caller may reuse
  a buffer. `sizeF == 2` degenerates correctly.
- **`out` may alias `a` or `b`** — each output is computed from both inputs *before* either is stored.
- **Magnitude:** two unnormalised spectra multiply, so the DC bin is `(Σa)·(Σb)` and a loud pair can
  yield time values up to `F` times the input peak. Taming level is the engine's job, not this
  primitive's — a scalar here would break the delta identity that pins the no-factor result.

## Threading

Constructing an implementation may allocate (`Dft` builds its twiddle table). `forward`, `inverse` and
`multiplyHalfSpectrum` allocate nothing, take no lock and touch nothing but their arguments and
pre-built state, so they are audio-thread safe. One instance is driven by one thread.

## Build and test

```bash
cmake --build build/release --target Fft -j4
ctest --test-dir build/release -R Dft --output-on-failure
ctest --test-dir build/release -R Spectral --output-on-failure
```

Tests live in `tests/` and link no JUCE. Case names carry the `Dft` or `Spectral` prefix, because
`-R` matches `TEST_CASE` strings rather than target names.

## Decisions

D-006, F-003, F-004 — `docs/decisions.md`.
