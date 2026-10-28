#include <Dft.hpp>
#include <Fft.hpp>
#include <JuceFft.hpp>
#include <Spectral.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#include <juce_dsp/juce_dsp.h>
#endif

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

bool armAllocations = false;
long allocationCount = 0;
}

void* operator new(std::size_t size)
{
    if (armAllocations)
        ++allocationCount;
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

void* operator new[](std::size_t size)
{
    if (armAllocations)
        ++allocationCount;
    void* memory = std::malloc(size != 0 ? size : 1);
    if (memory == nullptr)
        throw std::bad_alloc();
    return memory;
}

void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
struct AllocationWatch
{
    AllocationWatch()
    {
        allocationCount = 0;
        armAllocations = true;
    }
    ~AllocationWatch() { armAllocations = false; }
};

/** Every size the grain engine will ask for: small hops through a full FFT. */
constexpr int kSizes[] = {64, 256, 1024, 4096};
}

TEST_CASE("JuceFft forward matches the naive Dft bin for bin", "[jucefft]")
{
    constexpr float kAbsoluteFloor = 1e-4f;
    constexpr float kRelativeTolerance = 1e-5f;

    for (const int size : kSizes)
    {
        const fft::Dft oracle(size);
        const fft::JuceFft juce(size);
        const auto in = makeInput(size, 0xC0FFEE);

        std::vector<float> expected(static_cast<std::size_t>(size + 2), kUnwritten);
        std::vector<float> actual(static_cast<std::size_t>(size + 2), kUnwritten);
        oracle.forward(in.data(), expected.data());
        juce.forward(in.data(), actual.data());

        double worst = 0.0;
        for (int k = 0; k <= size / 2; ++k)
        {
            for (const int part : {0, 1})
            {
                const std::size_t index = static_cast<std::size_t>(2 * k + part);
                const double reference = std::abs(expected[index]);
                const double bound =
                    kAbsoluteFloor + kRelativeTolerance * std::max(reference, 1.0);
                const double error = std::abs(static_cast<double>(actual[index])
                                              - static_cast<double>(expected[index]));
                if (error > worst)
                    worst = error;
                INFO("F=" << size << " bin=" << k << (part ? ".im" : ".re")
                          << " expected=" << expected[index] << " actual=" << actual[index]);
                REQUIRE(error < bound);
            }
        }
        CAPTURE(size, worst);
    }
}

TEST_CASE("JuceFft inverse of its own forward returns the input", "[jucefft]")
{
    constexpr float kRelativeTolerance = 1e-5f;

    for (const int size : kSizes)
    {
        const fft::JuceFft juce(size);
        const auto in = makeInput(size, 31337);

        std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);
        std::vector<float> out(static_cast<std::size_t>(size), kUnwritten);
        juce.forward(in.data(), bins.data());
        juce.inverse(bins.data(), out.data());

        const float peak =
            *std::max_element(in.begin(), in.end(),
                              [](float a, float b) { return std::abs(a) < std::abs(b); });
        const double bound = kRelativeTolerance * std::max(std::abs(peak), 1.0f);

        double worst = 0.0;
        for (int n = 0; n < size; ++n)
            worst = std::max(worst, std::abs(static_cast<double>(out[n])
                                             - static_cast<double>(in[n])));
        INFO("F=" << size << " worst=" << worst << " bound=" << bound);
        REQUIRE(worst < bound);
    }
}

TEST_CASE("JuceFft DC and Nyquist imaginary slots are exactly zero", "[jucefft]")
{
    for (const int size : kSizes)
    {
        const fft::JuceFft juce(size);
        const auto in = makeInput(size, 1234);
        std::vector<float> bins(static_cast<std::size_t>(size + 2), kUnwritten);

        juce.forward(in.data(), bins.data());

        INFO("F=" << size);
        REQUIRE(bins[1] == 0.0f);
        REQUIRE(bins[static_cast<std::size_t>(size + 1)] == 0.0f);
    }
}

TEST_CASE("JuceFft calibrated gains are the engine's own and non-degenerate", "[jucefft]")
{
    for (const int size : kSizes)
    {
        const fft::JuceFft juce(size);
        const float forward = juce.forwardGain();
        const float inverse = juce.inverseGain();

        // Printed unconditionally: INFO only reaches the log on a failure, and the macOS
        // hand-off needs the engine's raw gain from a *passing* run to record in D-006.
        std::cout << "JuceFft F=" << size << " forwardGain=" << forward
                  << " inverseGain=" << inverse << '\n';

        INFO("F=" << size << " forwardGain=" << forward << " inverseGain=" << inverse);
        REQUIRE(std::isfinite(forward));
        REQUIRE(std::isfinite(inverse));
        REQUIRE(forward != 0.0f);
        REQUIRE(inverse != 0.0f);
    }
}

TEST_CASE("JuceFft allocates nothing inside forward or inverse", "[jucefft]")
{
    const int size = 1024;
    const fft::JuceFft juce(size);
    const auto in = makeInput(size, 77);
    std::vector<float> bins(static_cast<std::size_t>(size + 2), 0.0f);
    std::vector<float> out(static_cast<std::size_t>(size), 0.0f);

    juce.forward(in.data(), bins.data());
    juce.inverse(bins.data(), out.data());

    AllocationWatch watch;
    for (int pass = 0; pass < 8; ++pass)
    {
        juce.forward(in.data(), bins.data());
        juce.inverse(bins.data(), out.data());
    }

    REQUIRE(allocationCount == 0);
}

namespace
{
std::vector<float> forwardInto(const fft::IFft& engine, const std::vector<float>& in)
{
    std::vector<float> bins(static_cast<std::size_t>(in.size() + 2), kUnwritten);
    engine.forward(in.data(), bins.data());
    return bins;
}

std::vector<float> productOf(const fft::IFft& engine, const std::vector<float>& a,
                             const std::vector<float>& b)
{
    const int size = static_cast<int>(a.size());
    const auto binsA = forwardInto(engine, a);
    const auto binsB = forwardInto(engine, b);
    std::vector<float> product(static_cast<std::size_t>(size + 2), kUnwritten);
    fft::multiplyHalfSpectrum(binsA.data(), binsB.data(), product.data(), size);
    return product;
}

bool sameBits(float x, float y) noexcept
{
    return std::bit_cast<std::uint32_t>(x) == std::bit_cast<std::uint32_t>(y);
}
}

TEST_CASE("Spectral multiply through JuceFft matches the Dft circular convolution", "[jucefft]")
{
    // The first test that feeds a caller-authored spectrum — a product, rather than a transform's
    // own output — into JuceFft::inverse, which is the path the grain engine walks in M2 item 6.
    // Compared in the time domain, where both paths must land on the same circular convolution.
    // The oracle accumulates in double and this engine does not, so the bound is peak-relative.
    constexpr double kRelativeTolerance = 1e-4;

    for (const int size : kSizes)
    {
        const fft::Dft oracle(size);
        const fft::JuceFft juce(size);
        const auto a = makeInput(size, 0x5EED);
        const auto b = makeInput(size, 0xABCD);

        std::vector<float> expected(static_cast<std::size_t>(size), kUnwritten);
        std::vector<float> actual(static_cast<std::size_t>(size), kUnwritten);

        const auto oracleProduct = productOf(oracle, a, b);
        oracle.inverse(oracleProduct.data(), expected.data());
        const auto juceProduct = productOf(juce, a, b);
        juce.inverse(juceProduct.data(), actual.data());

        double peak = 0.0;
        for (const float value : expected)
            peak = std::max(peak, std::abs(static_cast<double>(value)));
        const double bound = kRelativeTolerance * std::max(peak, 1.0);

        double worst = 0.0;
        for (int n = 0; n < size; ++n)
            worst = std::max(worst, std::abs(static_cast<double>(actual[static_cast<std::size_t>(n)])
                                             - static_cast<double>(expected[static_cast<std::size_t>(n)])));

        INFO("F=" << size << " peak=" << peak << " worst=" << worst << " bound=" << bound);
        REQUIRE(worst < bound);
    }
}

TEST_CASE("Spectral multiply leaves JuceFft::inverse blind to the cardinal imaginary slots",
          "[jucefft]")
{
    // A caller-authored product spectrum, cardinal imaginary slots poisoned, must still round-trip
    // to the same samples — which is what item 6 relies on when it hands JuceFft::inverse a
    // spectrum the engine never produced.
    //
    // This is NOT a guard on the zeroing in JuceFft::inverse: deleting those two statements was
    // measured to leave this test green, because the even-n c2r the FFTW engine runs ignores
    // exactly these slots. The zeroing stays as engine-portable hygiene (vDSP packs Nyquist into
    // DC's imaginary slot), but no test in this container can distinguish it.
    for (const int size : kSizes)
    {
        const fft::JuceFft juce(size);
        auto product = productOf(juce, makeInput(size, 0x1234), makeInput(size, 0x5678));
        auto poisoned = product;
        poisoned[1] = kUnwritten;
        poisoned[static_cast<std::size_t>(size + 1)] = std::numeric_limits<float>::infinity();

        std::vector<float> clean(static_cast<std::size_t>(size), kUnwritten);
        std::vector<float> result(static_cast<std::size_t>(size), kUnwritten);
        juce.inverse(product.data(), clean.data());
        juce.inverse(poisoned.data(), result.data());

        INFO("F=" << size);
        for (int n = 0; n < size; ++n)
        {
            INFO("n=" << n << " clean=" << clean[static_cast<std::size_t>(n)]
                      << " poisoned=" << result[static_cast<std::size_t>(n)]);
            CHECK(sameBits(clean[static_cast<std::size_t>(n)], result[static_cast<std::size_t>(n)]));
        }
    }
}

#if defined(__linux__)
/** The fallback engine takes a SpinLock on every perform() and alloca()s scratch, which is an
    AGENTS §7 violation on the hop path. It is numerically correct and its gains are the same 1.0
    as FFTW's, so no result or gain comparison distinguishes the two — only which library the
    process actually mapped does. JUCE's FFTWImpl holds its DynamicLibrary open for the lifetime of
    the transform, so building one and then asking whether the soname is already resident proves the
    FFTW engine won; RTLD_NOLOAD makes that a check rather than a load of our own. */
TEST_CASE("JuceFft does not run on the locking fallback engine", "[jucefft]")
{
    constexpr const char* kSoname = "libfftw3f.so";
    const fft::JuceFft juce(256);
    void* resident = dlopen(kSoname, RTLD_LAZY | RTLD_NOLOAD);

    INFO(kSoname << " was not mapped by the transform, so JUCE fell through to the fallback engine");
    REQUIRE(resident != nullptr);

    static const char* symbols[] = {"fftwf_plan_dft_1d",
                                    "fftwf_plan_dft_r2c_1d",
                                    "fftwf_plan_dft_c2r_1d",
                                    "fftwf_destroy_plan",
                                    "fftwf_execute_dft",
                                    "fftwf_execute_dft_r2c",
                                    "fftwf_execute_dft_c2r"};
    for (const char* name : symbols)
    {
        INFO("missing " << name);
        CHECK(dlsym(resident, name) != nullptr);
    }
}
#endif

#if defined(__APPLE__)
/** On macOS the gain cannot name the engine: vDSP, FFTW and the fallback all report 1.0, because
    vDSP's raw zrip forward gain is 2.0 and forwardNormalisation = 0.5f (juce_FFT.cpp:456, :491)
    divides exactly that out. The fallback is excluded here by the engine list being closed instead.
    Its positive half — AppleFFT linked, so priority 5 beats the fallback's -1 — is a build-artifact
    fact (nm: EngineImpl<AppleFFT>), and a mapped Accelerate is necessary but not sufficient, since
    juce_dsp calls vDSP outside the FFT engines too. See D-006. */
TEST_CASE("JuceFft on macOS has no FFTW engine to fall through to", "[jucefft]")
{
    static_assert(JUCE_DSP_USE_SHARED_FFTW == 0 && JUCE_DSP_USE_STATIC_FFTW == 0,
                  "FFTW compiled in on Apple would put a dlopen-dependent engine ahead of vDSP");
    static_assert(JUCE_DSP_USE_INTEL_MKL == 0, "MKL has priority 8, ahead of vDSP's 5");

    // Built for its side effect: this is the call that resolves an engine, so the residency
    // questions below are asked of the process that actually picked one.
    const fft::JuceFft engine(256);
    (void) engine;

    void* fftw = dlopen("libfftw3f.dylib", RTLD_LAZY | RTLD_NOLOAD);
    INFO("libfftw3f.dylib is mapped, so an FFTW the build never asked for was loaded");
    REQUIRE(fftw == nullptr);

    static const char* kAccelerate = "/System/Library/Frameworks/Accelerate.framework/Accelerate";
    void* accelerate = dlopen(kAccelerate, RTLD_LAZY | RTLD_NOLOAD);
    INFO(kAccelerate << " is not mapped, so vDSP cannot be the installed engine");
    REQUIRE(accelerate != nullptr);

    static const char* kSymbols[] = {"vDSP_create_fftsetup", "vDSP_fft_zrip", "vDSP_vsmul"};
    for (const char* name : kSymbols)
    {
        INFO("missing " << name);
        CHECK(dlsym(accelerate, name) != nullptr);
    }
}
#endif
