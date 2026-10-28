#pragma once

#include <cassert>

namespace fft
{
/** Pointwise product of two half-spectra in the Fft.hpp layout: out[k] = a[k]·b[k] for
    k in [0, F/2], written as F + 2 floats.

    Convolution, not correlation: no conjugate anywhere. With forward unnormalised and inverse
    carrying 1/F, inverse(out) is the circular convolution of the two time signals with nothing
    left to apply, so this writes no scale of its own.

    The imaginary slots of DC and Nyquist are never read — bins[1] and bins[F + 1] of the inputs
    may hold anything at all, including a NaN, without changing one output sample. They are
    written 0.0f so the product satisfies the same contract its inputs do. Every slot of out is
    written, so a caller may reuse a buffer without seeing a stale bin.

    out may alias a or b. Buffers are caller-owned and nothing here allocates, takes a lock or
    calls virtually, so it is audio-thread safe.

    Two unnormalised spectra multiply, so the DC bin is (Σa)·(Σb) and a loud pair can yield time
    values up to F times the input peak. Level belongs to the caller's gain table, not here:
    taming it would break the delta identity that pins the no-factor result.

    @param sizeF F, a power of two >= 2; all three buffers hold F + 2 floats. */
inline void multiplyHalfSpectrum(const float* a, const float* b, float* out,
                                 int sizeF) noexcept
{
    assert(sizeF >= 2 && (sizeF & (sizeF - 1)) == 0);

    const int half = sizeF / 2;

    out[0] = a[0] * b[0];
    out[1] = 0.0f;
    for (int k = 1; k < half; ++k)
    {
        // Both parts are evaluated from the inputs before either is stored: writing the real
        // slot first would hand this expression an already-overwritten a[2k] when out aliases
        // a, which is the case the F + 2 scratch buffer saved by D-g depends on.
        const float real = a[2 * k] * b[2 * k] - a[2 * k + 1] * b[2 * k + 1];
        const float imag = a[2 * k] * b[2 * k + 1] + a[2 * k + 1] * b[2 * k];
        out[2 * k] = real;
        out[2 * k + 1] = imag;
    }
    out[2 * half] = a[2 * half] * b[2 * half];
    out[2 * half + 1] = 0.0f;
}
}
