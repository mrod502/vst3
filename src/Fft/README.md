# Fft

The transform seam from AGENTS §9: forward/inverse behind a small interface, so the installed
implementation can be replaced without touching convolution or grain code. JUCE-light by rule —
nothing here includes a JUCE header, so it compiles and tests as a plain console program.

Two headers, one library target. `Fft.hpp` is the contract (`fft::IFft`); `Dft.hpp` is the naive
`O(F^2)` oracle that defines what correct means; `Spectral.hpp` is the half-spectrum product that
the contract's never-read rule exists to make possible. The JUCE-backed adapter is a separate
library, `src/FftJuce`, because it is the only piece allowed to know JUCE exists.

## The contract

```cpp
virtual int  size() const noexcept = 0;                            // F
virtual void forward(const float* in, float* bins) const noexcept = 0;   // F    -> F + 2
virtual void inverse(const float* bins, float* out) const noexcept = 0;  // F + 2 -> F
```

Buffers are caller-owned and never aliased to each other, because the engine's grain and OLA
accumulator live in the engine's arena and AGENTS §8 forbids the transform choosing the allocator.

### Layout

Half-spectrum, real and imaginary interleaved, bins `0 … F/2` inclusive — **`F + 2` floats**:

| Slot | Holds |
|---|---|
| `bins[2k]` | Re of bin k |
| `bins[2k + 1]` | Im of bin k |
| `bins[0]`, `bins[1]` | DC; `bins[1]` is written zero and never read |
| `bins[F]`, `bins[F + 1]` | Nyquist; `bins[F + 1]` is written zero and never read |

For real input DC and Nyquist are real, so those two imaginary slots carry nothing. They are
documented as never-read rather than merely usually-zero: a caller may leave them unwritten and no
result may change. `Dft` reads only the real slots of those two bins, and `JuceFft` zeroes both
slots on the way into the engine, which is what makes the promise hold for any engine.

### Normalisation

Forward is the plain unnormalised DFT, `X[k] = Σ x[n]·e^(-i2πkn/F)`; the inverse carries `1/F`. So
a bare `inverse(forward(x)) == x` with no factor left over — and that `1/F` is *also* the whole
normalisation a spectral product needs: with both spectra unnormalised, `inverse(a·b)` is the
circular convolution of the two time signals, so **no further factor is owed**. This sentence used
to say the product's `1/F` was the caller's to fold into its gain table, which is wrong in the
direction that costs a day: obeying it divides by `F` twice, leaving the output `20·log10(F)` dB
quiet — `−72 dB` at `F = 4096`, effectively silence — while pointing the hunt at the window and the
OLA. Pinned by `SpectralTests` ("…equals a direct circular convolution" and "…adds no scale factor
of its own"); rationale in F-004. Stating the convention at all is the reason the header exists:
per-engine scaling is otherwise a 6 dB difference between builds, with no compile error and no
failing test to announce it (D-006).

## Dft

The oracle. Sums accumulate in `double` and only the stored result is `float`, so it is more
accurate than any engine compared against it and a mismatch is always the engine's error. The
cardinal twiddles at `m = 0` and `m = F/2` are forced to exact values, because `std::sin(π)` returns
~1.2e-16 and that residue would otherwise land in the Nyquist imaginary slot the contract promises
is zero.

## Threading

Constructing an implementation may allocate (`Dft` builds its twiddle table). `forward` and
`inverse` allocate nothing, take no lock, and touch nothing but their arguments and pre-built
state, so they are audio-thread safe. One instance is driven by one thread; the state is
per-instance and unsynchronised.

## multiplyHalfSpectrum

`Spectral.hpp`, `fft::multiplyHalfSpectrum(a, b, out, sizeF)` — the pointwise product of two
half-spectra, `out[k] = a[k]·b[k]` for `k` in `[0, F/2]`, all three buffers `F + 2` floats. It lives
here rather than in a library of its own because the layout it depends on is owned by `Fft.hpp`:
the never-read rule was written into that contract precisely to make this function's DC/Nyquist
special-casing legal, and keeping the only consumer next to the rule means they cannot drift.

* **Convolution, not correlation.** No conjugate anywhere. A conjugate reverses time, keeps every
  level test green, and sounds plausible — guarded by the delayed-delta case, which a conjugate
  turns red.
* **No scale of its own**, per Normalisation above.
* **DC and Nyquist** are real × real; both output imaginary slots are written `0.0f`, so the product
  satisfies the same contract its inputs do. Every slot of `out` is written, so a caller may reuse a
  buffer. `sizeF == 2` degenerates correctly: the interior loop never runs and both cardinal bins
  are still written.
* **`out` may alias `a` or `b`**, which is what saves the engine one `F + 2` scratch buffer. Each
  output bin depends only on the same-index input bins, but both parts must be evaluated from the
  inputs *before* either is stored — storing the real slot first hands the imaginary expression an
  already-overwritten `a[2k]`.
* **Magnitude.** Two unnormalised spectra multiply, so the DC bin is `(Σa)·(Σb)` and a loud pair can
  yield time values up to `F` times the input peak. Taming level here would be the gain table's job
  wearing a primitive's clothes, and it breaks the delta identity that pins the no-factor result.
  The provisional scalar and the safety clamp are the engine's (M2 items 5 and 6).
* **Threading.** A plain loop over caller-owned buffers: no allocation, no lock, no virtual call,
  `noexcept`. Its signature is the real-time guarantee, so there is nothing for an
  allocation-watch test to catch — the reason AGENTS §10's "tested or a written reason" bar is met
  by review here.

## Dependencies and build

Depends on nothing but `<cmath>`/`<vector>`; no JUCE.

```bash
cmake --build build --target Fft -j4
ctest --test-dir build -R Dft --output-on-failure
ctest --test-dir build -R Spectral --output-on-failure
```

Tests live in `tests/` and link no JUCE. Case names carry the `Dft` or `Spectral` prefix because
CTest's `-R` matches `TEST_CASE` strings, not target names.
