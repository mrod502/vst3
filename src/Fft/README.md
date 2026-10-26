# Fft

The transform seam from AGENTS §9: forward/inverse behind a small interface, so the installed
implementation can be replaced without touching convolution or grain code. JUCE-light by rule —
nothing here includes a JUCE header, so it compiles and tests as a plain console program.

Two headers, one library target. `Fft.hpp` is the contract (`fft::IFft`); `Dft.hpp` is the naive
`O(F^2)` oracle that defines what correct means. The JUCE-backed adapter is a separate library,
`src/FftJuce`, because it is the only piece allowed to know JUCE exists.

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
a bare `inverse(forward(x)) == x` with no factor left over, and the `1/F` belonging to a spectral
*product* is the caller's to fold into its own gain table. Stating this is the reason the header
exists: per-engine scaling is otherwise a 6 dB difference between builds, with no compile error and
no failing test to announce it (D-006).

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

## Dependencies and build

Depends on nothing but `<cmath>`/`<vector>`; no JUCE.

```bash
cmake --build build --target Fft -j4
ctest --test-dir build -R Dft --output-on-failure
```

Tests live in `tests/` and link no JUCE. Case names carry the `Dft` prefix because CTest's `-R`
matches `TEST_CASE` strings, not target names.
