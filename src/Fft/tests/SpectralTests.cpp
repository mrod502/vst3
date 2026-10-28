#include <Dft.hpp>
#include <Fft.hpp>
#include <Spectral.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace
{
constexpr float kUnwritten = std::numeric_limits<float>::quiet_NaN();
constexpr unsigned kSeed = 20261026u;

std::vector<float> makeInput(int size, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
    std::vector<float> in(static_cast<std::size_t>(size));
    for (float& sample : in)
        sample = spread(rng);
    return in;
}

std::vector<float> forwardBoth(const fft::Dft& dft, const std::vector<float>& in)
{
    std::vector<float> bins(static_cast<std::size_t>(in.size() + 2), kUnwritten);
    dft.forward(in.data(), bins.data());
    return bins;
}

/** ref[n] = Σ_m a[m]·b[(n − m + F) % F], accumulated in double, computed without a transform. */
std::vector<double> circularConvolution(const std::vector<float>& a, const std::vector<float>& b)
{
    const int size = static_cast<int>(a.size());
    std::vector<double> ref(static_cast<std::size_t>(size), 0.0);
    for (int n = 0; n < size; ++n)
    {
        double acc = 0.0;
        for (int m = 0; m < size; ++m)
            acc += static_cast<double>(a[static_cast<std::size_t>(m)])
                   * static_cast<double>(b[static_cast<std::size_t>((n - m + size) % size)]);
        ref[static_cast<std::size_t>(n)] = acc;
    }
    return ref;
}

double peakOf(const std::vector<double>& values)
{
    double peak = 0.0;
    for (const double value : values)
        peak = std::max(peak, std::abs(value));
    return peak;
}

bool sameBits(float x, float y) noexcept
{
    return std::bit_cast<std::uint32_t>(x) == std::bit_cast<std::uint32_t>(y);
}

/** Bound for an expectation that has been through forward · multiply · inverse in float. The
    bins are float, so the round trip cannot return the input bit-for-bit; the peak sets the
    scale because the transform is linear. Still tight enough to name a conjugate or a dropped
    Nyquist bin, both of which are O(peak) errors. */
double roundTripBound(double peak)
{
    return 1e-5 * std::max(peak, 1.0);
}

std::vector<float> inverseOf(const fft::Dft& dft, const std::vector<float>& bins)
{
    std::vector<float> out(static_cast<std::size_t>(bins.size() - 2), kUnwritten);
    dft.inverse(bins.data(), out.data());
    return out;
}
}

TEST_CASE("Spectral multiply inverse of the product equals a direct circular convolution",
          "[spectral]")
{
    // The product's own float rounding is one ulp per bin and the Dft accumulates in double, so
    // agreement is bounded by the bins, not by the reference. Tightened until exactly one size
    // went red, then backed off one step.
    constexpr double kRelativeTolerance = 1e-5;
    constexpr double kAbsoluteFloor = 1e-6;

    for (const int size : {64, 256, 1024, 4096})
    {
        const fft::Dft dft(size);
        const auto a = makeInput(size, kSeed);
        const auto b = makeInput(size, kSeed + 1u);
        const auto ref = circularConvolution(a, b);
        const double bound =
            std::max(kAbsoluteFloor, kRelativeTolerance * peakOf(ref));

        std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
        fft::multiplyHalfSpectrum(forwardBoth(dft, a).data(), forwardBoth(dft, b).data(),
                                  product.data(), size);
        const auto out = inverseOf(dft, product);

        double worst = 0.0;
        for (int n = 0; n < size; ++n)
        {
            const double error =
                std::abs(static_cast<double>(out[static_cast<std::size_t>(n)])
                         - ref[static_cast<std::size_t>(n)]);
            worst = std::max(worst, error);
        }
        INFO("F=" << size << " peak=" << peakOf(ref) << " worst=" << worst
                  << " bound=" << bound);
        CHECK(worst < bound);
    }
}

TEST_CASE("Spectral multiply adds no scale factor of its own", "[spectral]")
{
    // A factor of 1/F anywhere in the product leaves this ratio at 1/F instead of 1.
    const int size = 256;
    const fft::Dft dft(size);
    const auto a = makeInput(size, kSeed + 2u);
    const auto b = makeInput(size, kSeed + 3u);

    std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(forwardBoth(dft, a).data(), forwardBoth(dft, b).data(),
                              product.data(), size);
    const auto convolved = inverseOf(dft, product);
    const auto ref = circularConvolution(a, b);

    double refPeak = 0.0;
    double convPeak = 0.0;
    for (int n = 0; n < size; ++n)
    {
        refPeak = std::max(refPeak, std::abs(ref[static_cast<std::size_t>(n)]));
        convPeak = std::max(convPeak, std::abs(static_cast<double>(
                                          convolved[static_cast<std::size_t>(n)])));
    }
    INFO("refPeak=" << refPeak << " convPeak=" << convPeak);
    CHECK(std::abs(convPeak / refPeak - 1.0) < 1e-4);
}

TEST_CASE("Spectral multiply never reads the DC or Nyquist imaginary slots", "[spectral]")
{
    // The load-bearing case: a blanket loop over k = 0 … F/2 passes every numeric test while
    // reading them, because both producers write 0 there today. Only a NaN distinguishes the two
    // implementations, and the arithmetic otherwise identical, so the demand is bit equality.
    for (const int size : {64, 256, 1024})
    {
        const fft::Dft dft(size);
        const auto a = forwardBoth(dft, makeInput(size, kSeed + 4u));
        const auto b = forwardBoth(dft, makeInput(size, kSeed + 5u));

        auto poisonedA = a;
        auto poisonedB = b;
        poisonedA[1] = kUnwritten;
        poisonedA[static_cast<std::size_t>(size + 1)] = kUnwritten;
        poisonedB[1] = -kUnwritten;
        poisonedB[static_cast<std::size_t>(size + 1)] = std::numeric_limits<float>::infinity();

        std::vector<float> clean(static_cast<std::size_t>(size + 2), kUnwritten);
        std::vector<float> poisoned(static_cast<std::size_t>(size + 2), kUnwritten);
        fft::multiplyHalfSpectrum(a.data(), b.data(), clean.data(), size);
        fft::multiplyHalfSpectrum(poisonedA.data(), poisonedB.data(), poisoned.data(), size);

        const auto outClean = inverseOf(dft, clean);
        const auto outPoisoned = inverseOf(dft, poisoned);

        INFO("F=" << size);
        for (int slot = 0; slot < size + 2; ++slot)
        {
            INFO("bin slot=" << slot);
            CHECK(sameBits(clean[static_cast<std::size_t>(slot)],
                           poisoned[static_cast<std::size_t>(slot)]));
        }
        for (int n = 0; n < size; ++n)
        {
            INFO("sample n=" << n);
            CHECK(sameBits(outClean[static_cast<std::size_t>(n)],
                           outPoisoned[static_cast<std::size_t>(n)]));
        }
    }
}

TEST_CASE("Spectral multiply writes exactly zero into both imaginary cardinal slots",
          "[spectral]")
{
    for (const int size : {8, 64, 1024, 4096})
    {
        const fft::Dft dft(size);
        const auto a = forwardBoth(dft, makeInput(size, kSeed + 6u));
        const auto b = forwardBoth(dft, makeInput(size, kSeed + 7u));
        std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);

        fft::multiplyHalfSpectrum(a.data(), b.data(), product.data(), size);

        INFO("F=" << size);
        CHECK(product[1] == 0.0f);
        CHECK(product[static_cast<std::size_t>(size + 1)] == 0.0f);
    }
}

TEST_CASE("Spectral multiply writes every slot of out so a reused buffer cannot leak",
          "[spectral]")
{
    const int size = 128;
    const fft::Dft dft(size);
    const auto a = forwardBoth(dft, makeInput(size, kSeed + 8u));
    const auto b = forwardBoth(dft, makeInput(size, kSeed + 9u));

    for (const float stale : {kUnwritten, 1.25e30f})
    {
        std::vector<float> product(static_cast<std::size_t>(size + 2), stale);
        fft::multiplyHalfSpectrum(a.data(), b.data(), product.data(), size);

        INFO("stale=" << stale);
        for (int slot = 0; slot < size + 2; ++slot)
        {
            INFO("slot=" << slot);
            CHECK(std::isfinite(product[static_cast<std::size_t>(slot)]));
        }
    }
}

TEST_CASE("Spectral multiply keeps Nyquist: (-1)^n against a delta returns (-1)^n", "[spectral]")
{
    // A loop stopping at k < half drops bin F/2; a caller sizing out as F floats rather than
    // F + 2 loses it the same way. Both leave this short by the Nyquist alternating term.
    const int size = 256;
    const fft::Dft dft(size);
    std::vector<float> a(static_cast<std::size_t>(size));
    for (int n = 0; n < size; ++n)
        a[static_cast<std::size_t>(n)] = (n % 2 == 0) ? 1.0f : -1.0f;
    std::vector<float> delta(static_cast<std::size_t>(size), 0.0f);
    delta[0] = 1.0f;

    std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(forwardBoth(dft, a).data(), forwardBoth(dft, delta).data(),
                              product.data(), size);
    const auto out = inverseOf(dft, product);

    const std::size_t nyquist = static_cast<std::size_t>(2 * (size / 2));
    INFO("product Nyquist re=" << product[nyquist] << " im=" << product[nyquist + 1]);
    CHECK(product[nyquist] == static_cast<float>(size));
    for (int n = 0; n < size; ++n)
    {
        INFO("n=" << n << " out=" << out[static_cast<std::size_t>(n)]);
        CHECK(out[static_cast<std::size_t>(n)] == a[static_cast<std::size_t>(n)]);
    }
}

TEST_CASE("Spectral multiply DC bin is the product of the two sums", "[spectral]")
{
    constexpr double kTolerance = 1e-5;
    const int size = 256;
    const fft::Dft dft(size);
    const auto a = makeInput(size, kSeed + 10u);
    const auto b = makeInput(size, kSeed + 11u);

    double sumA = 0.0;
    double sumB = 0.0;
    for (const float sample : a)
        sumA += static_cast<double>(sample);
    for (const float sample : b)
        sumB += static_cast<double>(sample);

    std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(forwardBoth(dft, a).data(), forwardBoth(dft, b).data(),
                              product.data(), size);

    const double expected = sumA * sumB;
    INFO("out[0]=" << product[0] << " expected=" << expected);
    CHECK(std::abs(static_cast<double>(product[0]) - expected)
          < kTolerance * std::max(1.0, std::abs(expected)));
}

TEST_CASE("Spectral multiply of a constant against a zero-mean signal is near silence",
          "[spectral]")
{
    constexpr double kTolerance = 1e-5;
    const int size = 256;
    const fft::Dft dft(size);
    std::vector<float> constant(static_cast<std::size_t>(size), 0.5f);
    auto b = makeInput(size, kSeed + 12u);
    double mean = 0.0;
    for (const float sample : b)
        mean += static_cast<double>(sample);
    mean /= static_cast<double>(size);
    for (float& sample : b)
        sample -= static_cast<float>(mean);

    std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(forwardBoth(dft, constant).data(), forwardBoth(dft, b).data(),
                              product.data(), size);
    const auto out = inverseOf(dft, product);

    double worst = 0.0;
    for (const float sample : out)
        worst = std::max(worst, std::abs(static_cast<double>(sample)));
    INFO("worst=" << worst);
    CHECK(worst < kTolerance);
}

TEST_CASE("Spectral multiply delta identity holds in both directions", "[spectral]")
{
    // M2 §7's delta test with the engine removed: a delta convolved with B returns B.
    const int size = 256;
    const fft::Dft dft(size);
    const auto a = makeInput(size, kSeed + 13u);
    const auto b = makeInput(size, kSeed + 14u);

    std::vector<float> delta(static_cast<std::size_t>(size), 0.0f);
    delta[0] = 1.0f;
    std::vector<float> shifted(static_cast<std::size_t>(size), 0.0f);
    shifted[7] = 1.0f;

    std::vector<float> deltaTimesB(static_cast<std::size_t>(size + 2), kUnwritten);
    std::vector<float> aTimesDelta7(static_cast<std::size_t>(size + 2), kUnwritten);

    const auto binsDelta = forwardBoth(dft, delta);
    const auto binsB = forwardBoth(dft, b);
    const auto binsA = forwardBoth(dft, a);
    const auto binsShifted = forwardBoth(dft, shifted);

    fft::multiplyHalfSpectrum(binsDelta.data(), binsB.data(), deltaTimesB.data(), size);
    fft::multiplyHalfSpectrum(binsA.data(), binsShifted.data(), aTimesDelta7.data(), size);

    const auto deltaLeft = inverseOf(dft, deltaTimesB);
    const auto delayed = inverseOf(dft, aTimesDelta7);

    // δ's spectrum is exactly (1,0) in every bin, so δ·B must reproduce B's bins with no scale at
    // all. Exact rather than a tolerance is what pins the no-factor result: a 1/F folded into the
    // multiply mismatches every slot. Value equality, not bit equality, so a sign of zero cannot
    // decide the outcome.
    for (int slot = 0; slot < size + 2; ++slot)
    {
        const std::size_t index = static_cast<std::size_t>(slot);
        INFO("delta·B bin slot=" << slot << " product=" << deltaTimesB[index]
                                 << " binsB=" << binsB[index]);
        CHECK(deltaTimesB[index] == binsB[index]);
    }

    for (int n = 0; n < size; ++n)
    {
        INFO("delta·B n=" << n << " out=" << deltaLeft[static_cast<std::size_t>(n)]
                          << " b=" << b[static_cast<std::size_t>(n)]);
        CHECK(std::abs(static_cast<double>(deltaLeft[static_cast<std::size_t>(n)])
                       - static_cast<double>(b[static_cast<std::size_t>(n)]))
              < roundTripBound(1.0));

        const std::size_t source = static_cast<std::size_t>((n - 7 + size) % size);
        INFO("A·delta7 n=" << n << " out=" << delayed[static_cast<std::size_t>(n)]
                           << " a=" << a[source]);
        CHECK(std::abs(static_cast<double>(delayed[static_cast<std::size_t>(n)])
                       - static_cast<double>(a[source]))
              < roundTripBound(1.0));
    }
}

TEST_CASE("Spectral multiply is convolution not correlation: a delayed delta shifts later",
          "[spectral]")
{
    // X·conj(Y) reverses time. This case is the guard (T1): conjugating b's bins must turn it red.
    const int size = 128;
    const int delay = 3;
    const fft::Dft dft(size);

    std::vector<float> a(static_cast<std::size_t>(size), 0.0f);
    a[static_cast<std::size_t>(delay)] = 1.0f;
    std::vector<float> b(static_cast<std::size_t>(size));
    for (int n = 0; n < size; ++n)
        b[static_cast<std::size_t>(n)] = static_cast<float>(n) / static_cast<float>(size);

    const auto binsB = forwardBoth(dft, b);
    std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(forwardBoth(dft, a).data(), binsB.data(), product.data(), size);
    const auto out = inverseOf(dft, product);

    for (int n = 0; n < size; ++n)
    {
        const std::size_t source = static_cast<std::size_t>((n - delay + size) % size);
        INFO("n=" << n << " out=" << out[static_cast<std::size_t>(n)]
                  << " expected b[" << source << "]=" << b[source]);
        CHECK(std::abs(static_cast<double>(out[static_cast<std::size_t>(n)])
                       - static_cast<double>(b[source]))
              < roundTripBound(1.0));
    }
}

TEST_CASE("Spectral multiply accepts out aliasing a and out aliasing b", "[spectral]")
{
    // D-g is what saves the engine one F + 2 scratch buffer, so in-place must reproduce the
    // out-of-place product exactly: the expressions are identical, only the store order differs.
    // Value equality rather than bits, so a differing sign of zero cannot decide the outcome.
    const int size = 256;
    const fft::Dft dft(size);
    const auto binsA = forwardBoth(dft, makeInput(size, kSeed + 15u));
    const auto binsB = forwardBoth(dft, makeInput(size, kSeed + 16u));

    std::vector<float> separate(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(binsA.data(), binsB.data(), separate.data(), size);

    auto aliasA = binsA;
    fft::multiplyHalfSpectrum(aliasA.data(), binsB.data(), aliasA.data(), size);
    auto aliasB = binsB;
    fft::multiplyHalfSpectrum(binsA.data(), aliasB.data(), aliasB.data(), size);

    for (int slot = 0; slot < size + 2; ++slot)
    {
        const std::size_t index = static_cast<std::size_t>(slot);
        INFO("out aliased a, slot=" << slot << " separate=" << separate[index]
                                    << " alias=" << aliasA[index]);
        CHECK(separate[index] == aliasA[index]);
        INFO("out aliased b, slot=" << slot << " separate=" << separate[index]
                                    << " alias=" << aliasB[index]);
        CHECK(separate[index] == aliasB[index]);
    }
}
