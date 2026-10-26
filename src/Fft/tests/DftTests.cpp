#include <Dft.hpp>
#include <Fft.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace
{
constexpr float kUnwritten = std::numeric_limits<float>::quiet_NaN();

std::vector<float> makeInput(int size, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
    std::vector<float> in(static_cast<std::size_t>(size));
    for (float& sample : in)
        sample = spread(rng);
    return in;
}

/** X[k] as a literal sum over every sample, computed independently of Dft's twiddle table. */
void referenceBin(const float* in, int size, int k, double& re, double& im)
{
    re = 0.0;
    im = 0.0;
    for (int n = 0; n < size; ++n)
    {
        const double angle = -2.0 * 3.14159265358979323846 * k * n / size;
        re += static_cast<double>(in[n]) * std::cos(angle);
        im += static_cast<double>(in[n]) * std::sin(angle);
    }
}
}

TEST_CASE("Dft impulse puts exactly 1 + 0i in every bin", "[dft]")
{
    for (int size : {8, 64, 256, 4096})
    {
        const fft::Dft dft(size);
        std::vector<float> in(static_cast<std::size_t>(size), 0.0f);
        in[0] = 1.0f;
        std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);

        dft.forward(in.data(), bins.data());

        for (int k = 0; k <= size / 2; ++k)
        {
            CHECK(bins[static_cast<std::size_t>(2 * k)] == 1.0f);
            CHECK(bins[static_cast<std::size_t>(2 * k + 1)] == 0.0f);
        }
    }
}

TEST_CASE("Dft matches a term-by-term DFT sum on random input", "[dft]")
{
    constexpr double kRelativeTolerance = 1e-6;

    for (int size : {16, 64, 256})
    {
        const fft::Dft dft(size);
        const auto in = makeInput(size, 0xBEEF);
        std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);

        dft.forward(in.data(), bins.data());

        for (int k = 0; k <= size / 2; ++k)
        {
            double re = 0.0;
            double im = 0.0;
            referenceBin(in.data(), size, k, re, im);
            // The bins are float, so one ulp of the bin's own magnitude is the floor of any
            // achievable agreement; the sum itself is accumulated in double.
            const double bound =
                kRelativeTolerance * std::max(1.0, std::hypot(re, im));
            INFO("F=" << size << " k=" << k << " |X|=" << std::hypot(re, im));
            CHECK(std::abs(bins[static_cast<std::size_t>(2 * k)] - re) < bound);
            CHECK(std::abs(bins[static_cast<std::size_t>(2 * k + 1)] - im) < bound);
        }
    }
}

TEST_CASE("Dft DC and Nyquist imaginary slots are exactly zero", "[dft]")
{
    for (int size : {8, 64, 1024, 4096})
    {
        const fft::Dft dft(size);
        const auto in = makeInput(size, 7);
        std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);

        dft.forward(in.data(), bins.data());

        CHECK(bins[1] == 0.0f);
        CHECK(bins[static_cast<std::size_t>(size + 1)] == 0.0f);
    }
}

TEST_CASE("Dft concentration of a real cosine leaves one non-zero half-spectrum bin", "[dft]")
{
    constexpr double kTolerance = 1e-3;

    for (const int size : {64, 256, 1024})
    {
        for (const int k : {1, 3, size / 2 - 1})
        {
            const fft::Dft dft(size);
            std::vector<float> in(static_cast<std::size_t>(size));
            for (int n = 0; n < size; ++n)
                in[static_cast<std::size_t>(n)] =
                    static_cast<float>(std::cos(2.0 * 3.14159265358979323846 * k * n / size));

            std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);
            dft.forward(in.data(), bins.data());

            for (int b = 0; b <= size / 2; ++b)
            {
                const double expectRe = (b == k) ? size / 2.0 : 0.0;
                INFO("F=" << size << " k=" << k << " bin=" << b);
                CHECK(std::abs(bins[static_cast<std::size_t>(2 * b)] - expectRe) < kTolerance);
                CHECK(std::abs(bins[static_cast<std::size_t>(2 * b + 1)]) < kTolerance);
            }
        }
    }
}

TEST_CASE("Dft inverse of its own forward returns the input", "[dft]")
{
    constexpr double kTolerance = 1e-6;

    for (const int size : {8, 64, 256, 4096})
    {
        const fft::Dft dft(size);
        const auto in = makeInput(size, 99);
        std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);
        std::vector<float> out(static_cast<std::size_t>(size), kUnwritten);

        dft.forward(in.data(), bins.data());
        dft.inverse(bins.data(), out.data());

        double worst = 0.0;
        for (int n = 0; n < size; ++n)
            worst = std::max(worst, std::abs(static_cast<double>(out[n]) - static_cast<double>(in[n])));
        INFO("F=" << size << " worst=" << worst);
        CHECK(worst < kTolerance);
    }
}

TEST_CASE("Dft inverse ignores the DC and Nyquist imaginary slots", "[dft]")
{
    constexpr double kTolerance = 1e-6;
    const int size = 128;
    const fft::Dft dft(size);
    const auto in = makeInput(size, 5);

    std::vector<float> binsA(static_cast<std::size_t>(size + 2), kUnwritten);
    dft.forward(in.data(), binsA.data());
    std::vector<float> binsB = binsA;
    binsB[1] = 4.25e7f;
    binsB[static_cast<std::size_t>(size + 1)] = -8.5f;

    std::vector<float> outA(static_cast<std::size_t>(size), kUnwritten);
    std::vector<float> outB(static_cast<std::size_t>(size), kUnwritten);
    dft.inverse(binsA.data(), outA.data());
    dft.inverse(binsB.data(), outB.data());

    for (int n = 0; n < size; ++n)
        CHECK(outA[n] == outB[n]);
}
