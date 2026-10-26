#include "JuceFft.hpp"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

namespace fft
{
namespace
{
int orderFor(int sizeF) noexcept
{
    int order = 0;
    while ((1 << order) < sizeF)
        ++order;
    return order;
}
}

struct JuceFft::Impl
{
    explicit Impl(int sizeF)
        : size(sizeF),
          transform(orderFor(sizeF)),
          scratch(static_cast<std::size_t>(sizeF) * 2, 0.0f)
    {
        assert(sizeF >= 2 && (sizeF & (sizeF - 1)) == 0);

        std::fill(scratch.begin(), scratch.end(), 0.0f);
        scratch[0] = 1.0f;
        transform.performRealOnlyForwardTransform(scratch.data(), true);

        double forwardSum = 0.0;
        for (int k = 0; k <= size / 2; ++k)
            forwardSum += static_cast<double>(scratch[static_cast<std::size_t>(2 * k)]);
        measuredForward = static_cast<float>(forwardSum / static_cast<double>(size / 2 + 1));

        for (int k = 0; k <= size / 2; ++k)
        {
            scratch[static_cast<std::size_t>(2 * k)] = 1.0f;
            scratch[static_cast<std::size_t>(2 * k + 1)] = 0.0f;
        }
        transform.performRealOnlyInverseTransform(scratch.data());
        measuredInverse = scratch[0];

        jassert(std::isfinite(measuredForward) && measuredForward != 0.0f);
        jassert(std::isfinite(measuredInverse) && measuredInverse != 0.0f);

        forwardScale = 1.0f / measuredForward;
        inverseScale = 1.0f / measuredInverse;
    }

    void forward(const float* in, float* bins) const noexcept
    {
        std::copy_n(in, size, scratch.begin());

        // The half-spectrum flag is what keeps the engine's output in the F + 2 float layout:
        // without it JUCE mirrors the conjugates out to the full 2F buffer we are not asked for.
        transform.performRealOnlyForwardTransform(scratch.data(), true);

        for (int k = 0; k <= size / 2; ++k)
        {
            bins[static_cast<std::size_t>(2 * k)] =
                scratch[static_cast<std::size_t>(2 * k)] * forwardScale;
            bins[static_cast<std::size_t>(2 * k + 1)] =
                scratch[static_cast<std::size_t>(2 * k + 1)] * forwardScale;
        }
    }

    void inverse(const float* bins, float* out) const noexcept
    {
        const std::size_t binCount = static_cast<std::size_t>(size) + 2;
        std::copy_n(bins, static_cast<int>(binCount), scratch.begin());

        // Zeroed rather than trusted: the contract says these two slots are never read, and the
        // copy above would otherwise hand the engine whatever a caller left in them. FFTW's c2r
        // also overwrites the buffer it is given, which is why the copy is not optional.
        scratch[1] = 0.0f;
        scratch[static_cast<std::size_t>(size) + 1] = 0.0f;

        transform.performRealOnlyInverseTransform(scratch.data());

        for (int n = 0; n < size; ++n)
            out[n] = scratch[static_cast<std::size_t>(n)] * inverseScale;
    }

    int size;
    juce::dsp::FFT transform;
    mutable std::vector<float> scratch;
    float measuredForward = 1.0f;
    float measuredInverse = 1.0f;
    float forwardScale = 1.0f;
    float inverseScale = 1.0f;
};

JuceFft::JuceFft(int sizeF)
    : sizeValue(sizeF), impl(std::make_unique<Impl>(sizeF))
{
}

JuceFft::~JuceFft() = default;

void JuceFft::forward(const float* in, float* bins) const noexcept
{
    impl->forward(in, bins);
}

void JuceFft::inverse(const float* bins, float* out) const noexcept
{
    impl->inverse(bins, out);
}

float JuceFft::forwardGain() const noexcept { return impl->measuredForward; }

float JuceFft::inverseGain() const noexcept { return impl->measuredInverse; }
}
