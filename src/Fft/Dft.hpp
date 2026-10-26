#pragma once

#include "Fft.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fft
{
/** Naive O(F^2) real DFT behind IFft — the oracle every engine is compared against, and a
    reference engine in its own right.

    Sums accumulate in double, so this is more accurate than any float engine checked against
    it: a mismatch is the engine's error, never the oracle's. Only bins [0, F/2] are produced;
    the negative half is reconstructed from Hermitian symmetry, which is what makes the inverse
    read the real slots of DC and Nyquist and nothing else. */
class Dft final : public IFft
{
public:
    /** @param sizeF transform size F; a power of two >= 2. */
    explicit Dft(int sizeF)
        : sizeValue(sizeF),
          cosine(static_cast<std::size_t>(sizeF)),
          sine(static_cast<std::size_t>(sizeF))
    {
        assert(sizeF >= 2 && (sizeF & (sizeF - 1)) == 0);

        constexpr double kTwoPi = 6.28318530717958647692528676655900577;
        const int half = sizeF / 2;
        for (int m = 0; m < sizeF; ++m)
        {
            const double angle = kTwoPi * static_cast<double>(m) / static_cast<double>(sizeF);
            cosine[static_cast<std::size_t>(m)] = std::cos(angle);
            // m == F/2 evaluates sin(pi) as ~1.2e-16, and that residue is what a DC/Nyquist
            // imaginary slot would then carry, so the cardinal angles are forced exact.
            sine[static_cast<std::size_t>(m)] =
                (m == 0 || m == half) ? 0.0 : -std::sin(angle);
        }
    }

    int size() const noexcept override { return sizeValue; }

    void forward(const float* in, float* bins) const noexcept override
    {
        const int half = sizeValue / 2;
        for (int k = 0; k <= half; ++k)
        {
            double re = 0.0;
            double im = 0.0;
            for (int n = 0; n < sizeValue; ++n)
            {
                const std::size_t m = static_cast<std::size_t>(indexFor(k, n));
                const double sample = static_cast<double>(in[n]);
                re += sample * cosine[m];
                im += sample * sine[m];
            }
            bins[static_cast<std::size_t>(2 * k)] = static_cast<float>(re);
            bins[static_cast<std::size_t>(2 * k + 1)] = static_cast<float>(im);
        }
    }

    void inverse(const float* bins, float* out) const noexcept override
    {
        const int half = sizeValue / 2;
        const double scale = 1.0 / static_cast<double>(sizeValue);
        const double dc = static_cast<double>(bins[0]);
        const double nyquist = static_cast<double>(bins[2 * half]);

        for (int n = 0; n < sizeValue; ++n)
        {
            double acc = (n % 2 == 0) ? dc + nyquist : dc - nyquist;
            for (int k = 1; k < half; ++k)
            {
                const std::size_t m = static_cast<std::size_t>(indexFor(k, n));
                const double re = static_cast<double>(bins[2 * k]);
                const double im = static_cast<double>(bins[2 * k + 1]);
                acc += 2.0 * (re * cosine[m] + im * sine[m]);
            }
            out[n] = static_cast<float>(acc * scale);
        }
    }

private:
    /** Twiddle index for bin k at sample n; the table holds e^(-i 2 pi m / F) for m in [0, F). */
    int indexFor(int k, int n) const noexcept
    {
        return static_cast<int>((static_cast<std::int64_t>(k) * n) % sizeValue);
    }

    int sizeValue;
    std::vector<double> cosine;
    std::vector<double> sine;
};
}
