#include "GrainEngineChecks.hpp"

#include <Dft.hpp>
#include <JuceFft.hpp>

#include <catch_amalgamated.hpp>

#include <memory>
#include <vector>

namespace
{
using graincheck::Case;
using graincheck::configFor;
using graincheck::hopsFor;
using graincheck::maxAbsDiff;
using graincheck::nextPow2;
using graincheck::noise;
using graincheck::peakAbs;
using graincheck::runEngine;

/** The platform transform: whatever juce::dsp::FFT installs at runtime — vDSP on Apple, FFTW here. */
class JuceFactory final : public grain::TransformFactory
{
public:
    [[nodiscard]] std::unique_ptr<fft::IFft> create(int sizeF) const override
    {
        return std::make_unique<fft::JuceFft>(sizeF);
    }
} factory;

/** The oracle, so this binary can compare the platform backend against a known-correct one
    without a second process. */
class DftFactory final : public grain::TransformFactory
{
public:
    [[nodiscard]] std::unique_ptr<fft::IFft> create(int sizeF) const override
    {
        return std::make_unique<fft::Dft>(sizeF);
    }
} oracleFactory;
}

TEST_CASE("JuceFft-backed engine equals a windowed-convolve reference", "[grain-engine-juce]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}, Case{1024, 256}, Case{2048, 512}})
        graincheck::expectMatchesReference(factory, c, 20261027U);
}

TEST_CASE("JuceFft: the delta response is the window at n0 + L, zero on both sides",
          "[grain-engine-juce]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}, Case{1024, 256}})
        graincheck::expectDeltaIsWindow(factory, c);
}

TEST_CASE("JuceFft: the delta's energy centroid is n0 + L + L/2", "[grain-engine-juce]")
{
    for (const Case c : {Case{512, 128}, Case{1024, 256}})
        graincheck::expectDeltaCentroid(factory, c);
}

TEST_CASE("JuceFft: latencySamples is the configured R, wetDelaySamples is L",
          "[grain-engine-juce]")
{
    graincheck::expectLatencyContract(factory);
}

TEST_CASE("JuceFft: prepare rejects an R outside [0, L] and an L that is not a multiple of H",
          "[grain-engine-juce]")
{
    graincheck::expectPrepareRejectsUnusableConfig(factory);
}

TEST_CASE("JuceFft: with R = 0 the wet stream is causal", "[grain-engine-juce]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}})
        graincheck::expectCausalAtZeroLatency(factory, c);
}

TEST_CASE("JuceFft: swapping A and B leaves the wet stream unchanged", "[grain-engine-juce]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}})
        graincheck::expectCommutative(factory, c);
}

TEST_CASE("JuceFft: reset and a re-prepare both return a bit-identical stream",
          "[grain-engine-juce]")
{
    graincheck::expectResetIsFresh(factory, Case{256, 64});
}

TEST_CASE("JuceFft: channels are independent", "[grain-engine-juce]")
{
    graincheck::expectChannelsIndependent(factory, Case{256, 64});
}

TEST_CASE("JuceFft: processHop allocates nothing", "[grain-engine-juce]")
{
    graincheck::expectNoAllocationInProcessHop(factory, Case{2048, 512});
}

TEST_CASE("JuceFft: host callback size never reaches the DSP", "[grain-engine-juce]")
{
    graincheck::expectBlockSizeInvariance(factory, Case{512, 128});
}

TEST_CASE("The platform transform and the oracle produce the same stream", "[grain-engine-juce]")
{
    // The check that pins the gain calibration specifically: both engines run the same arithmetic
    // on the same samples, so any per-backend factor JuceFft failed to divide out shows up here as
    // a level offset rather than as a 20·log10(F) dB surprise in the mix.
    for (const Case c : {Case{256, 64}, Case{512, 128}, Case{1024, 256}})
    {
        const int total = hopsFor(c) * c.hop;
        const auto a = noise(static_cast<std::size_t>(total), 4242U);
        const auto b = noise(static_cast<std::size_t>(total), 4243U);

        grain::GrainConvolutionEngine platform(factory);
        platform.prepare(configFor(c.length, c.hop, 0));
        const auto got = runEngine(platform, c.hop, a, b);

        grain::GrainConvolutionEngine oracle(oracleFactory);
        oracle.prepare(configFor(c.length, c.hop, 0));
        const auto want = runEngine(oracle, c.hop, a, b);

        REQUIRE(got.size() == want.size());
        const double peak = peakAbs(std::vector<double>(want.begin(), want.end()));
        REQUIRE(peak > 0.0);
        const fft::JuceFft backend(nextPow2(2 * c.length - 1));
        INFO("L=" << c.length << " H=" << c.hop
             << " installed forward gain=" << backend.forwardGain()
             << " inverse gain=" << backend.inverseGain());
        CHECK(maxAbsDiff(got, std::vector<double>(want.begin(), want.end())) <= 1e-4 * peak);
    }
}
