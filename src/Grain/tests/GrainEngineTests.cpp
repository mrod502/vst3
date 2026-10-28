#include "GrainEngineChecks.hpp"

#include <Dft.hpp>

#include <catch_amalgamated.hpp>

#include <cmath>
#include <memory>
#include <vector>

namespace
{
using graincheck::Case;
using graincheck::configFor;
using graincheck::hopsFor;
using graincheck::nextPow2;
using graincheck::noise;
using graincheck::probeStart;
using graincheck::probeTotal;
using graincheck::runDeltaProbe;
using graincheck::runEngine;
using graincheck::wetScale;

/** The engine driven by the Dft oracle: no JUCE, no platform transform, no FFTW. */
class DftFactory final : public grain::TransformFactory
{
public:
    [[nodiscard]] std::unique_ptr<fft::IFft> create(int sizeF) const override
    {
        return std::make_unique<fft::Dft>(sizeF);
    }
} factory;

/** The engine's chain rewired by hand with a caller-chosen window table, so the identical
    arithmetic can be run behind a window the engine will never install.

    The table for the good case is taken from GrainWindow itself, which makes the bit-identity
    check below about the wiring rather than about two ways of evaluating a cosine. */
class ManualPipeline
{
public:
    ManualPipeline(int length, int hop, std::vector<float> windowTable, int sizeF)
        : sizeFValue(sizeF),
          window(std::move(windowTable)),
          grainA(static_cast<std::size_t>(sizeF), 0.0f),
          grainB(static_cast<std::size_t>(sizeF), 0.0f),
          binsA(static_cast<std::size_t>(sizeF) + 2, 0.0f),
          binsB(static_cast<std::size_t>(sizeF) + 2, 0.0f),
          product(static_cast<std::size_t>(sizeF) + 2, 0.0f),
          impulse(static_cast<std::size_t>(sizeF), 0.0f)
    {
        historyA.prepare(length, hop, sizeF);
        historyB.prepare(length, hop, sizeF);
        ola.prepare(length, hop, sizeF);
    }

    std::vector<float> run(const std::vector<float>& a, const std::vector<float>& b, int hop,
                           float scale, const fft::IFft& fft)
    {
        const int hops = static_cast<int>(a.size()) / hop;
        std::vector<float> out(static_cast<std::size_t>(hops) * static_cast<std::size_t>(hop),
                               0.0f);
        const float* aPtr = a.data();
        const float* bPtr = b.data();
        float* outPtr = out.data();
        for (int k = 0; k < hops; ++k)
        {
            historyA.push(aPtr, hop);
            historyB.push(bPtr, hop);
            historyA.extractWindowed(window.data(), grainA.data());
            historyB.extractWindowed(window.data(), grainB.data());
            fft.forward(grainA.data(), binsA.data());
            fft.forward(grainB.data(), binsB.data());
            fft::multiplyHalfSpectrum(binsA.data(), binsB.data(), product.data(), sizeFValue);
            fft.inverse(product.data(), impulse.data());
            ola.emitAndAdd(impulse.data(), scale, outPtr);
            aPtr += hop;
            bPtr += hop;
            outPtr += hop;
        }
        return out;
    }

private:
    int sizeFValue;
    std::vector<float> window;
    grain::GrainHistory historyA;
    grain::GrainHistory historyB;
    grain::OlaAccumulator ola;
    std::vector<float> grainA;
    std::vector<float> grainB;
    std::vector<float> binsA;
    std::vector<float> binsB;
    std::vector<float> product;
    std::vector<float> impulse;
};

std::vector<float> windowTable(int length, grain::GrainWindow::Kind kind)
{
    const grain::GrainWindow window(length, kind);
    return std::vector<float>(window.data(), window.data() + length);
}

/** The window the engine must never install: denominator L-1, so the hop-phase class sums are not
    constant and an event's gain depends on where it falls relative to the hop grid.
    M2's window correction is the change from this to the periodic form. */
std::vector<float> symmetricHannTable(int length)
{
    std::vector<float> table(static_cast<std::size_t>(length));
    for (int i = 0; i < length; ++i)
    {
        const double value = 0.5 * (1.0 - std::cos(2.0 * graincheck::kPi
                                                   * static_cast<double>(i)
                                                   / static_cast<double>(length - 1)));
        table[static_cast<std::size_t>(i)] = static_cast<float>(value);
    }
    return table;
}

/** Total wet output an event contributes: the sum of its response, which the derivation makes
    proportional to the window's hop-phase class sum. */
double responseGain(const std::vector<float>& out, int n0, int length)
{
    double total = 0.0;
    for (int m = 0; m < length; ++m)
        total += static_cast<double>(out[static_cast<std::size_t>(n0 + length + m)]);
    return total;
}

/** Peak-to-peak spread of the per-phase gains, relative to their mean. */
double gainSpreadAcrossPhases(const std::vector<double>& gains)
{
    double lowest = gains.front();
    double highest = gains.front();
    double sum = 0.0;
    for (const double gain : gains)
    {
        lowest = std::min(lowest, gain);
        highest = std::max(highest, gain);
        sum += gain;
    }
    const double mean = sum / static_cast<double>(gains.size());
    REQUIRE(mean > 0.0);
    return (highest - lowest) / mean;
}
}

TEST_CASE("GrainConvolutionEngine equals a windowed-convolve reference", "[grain-engine]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}, Case{1024, 256}, Case{2048, 512}})
        graincheck::expectMatchesReference(factory, c, 20261027U);
}

TEST_CASE("The delta response is the window at n0 + L, zero on both sides", "[grain-engine]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}, Case{1024, 256}})
        graincheck::expectDeltaIsWindow(factory, c);
}

TEST_CASE("The delta response's energy centroid is n0 + L + L/2: delay L, no hidden hop",
          "[grain-engine]")
{
    for (const Case c : {Case{512, 128}, Case{1024, 256}})
        graincheck::expectDeltaCentroid(factory, c);
}

TEST_CASE("latencySamples is the configured R, wetDelaySamples is L", "[grain-engine]")
{
    graincheck::expectLatencyContract(factory);
}

TEST_CASE("prepare rejects an R outside [0, L] and an L that is not a multiple of H",
          "[grain-engine]")
{
    graincheck::expectPrepareRejectsUnusableConfig(factory);
}

TEST_CASE("With R = 0 the wet stream is causal: output q ignores input at and after q",
          "[grain-engine]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}})
        graincheck::expectCausalAtZeroLatency(factory, c);
}

TEST_CASE("Swapping A and B leaves the wet stream unchanged", "[grain-engine]")
{
    for (const Case c : {Case{256, 64}, Case{512, 128}})
        graincheck::expectCommutative(factory, c);
}

TEST_CASE("reset and a re-prepare both return a bit-identical stream", "[grain-engine]")
{
    graincheck::expectResetIsFresh(factory, Case{256, 64});
}

TEST_CASE("Channels are independent, and share nothing through the scratch", "[grain-engine]")
{
    graincheck::expectChannelsIndependent(factory, Case{256, 64});
}

TEST_CASE("processHop allocates nothing", "[grain-engine]")
{
    graincheck::expectNoAllocationInProcessHop(factory, Case{2048, 512});
}

TEST_CASE("Host callback size never reaches the DSP, through the scheduler", "[grain-engine]")
{
    graincheck::expectBlockSizeInvariance(factory, Case{512, 128});
}

TEST_CASE("The hand-built pipeline is bit-identical to the engine on the shared window",
          "[grain-engine]")
{
    const Case c{256, 64};
    const int total = hopsFor(c) * c.hop;
    const auto a = noise(static_cast<std::size_t>(total), 8080U);
    const auto b = noise(static_cast<std::size_t>(total), 8081U);

    grain::GrainConvolutionEngine engine(factory);
    engine.prepare(configFor(c.length, c.hop, 0));
    const auto fromEngine = runEngine(engine, c.hop, a, b);

    const int sizeF = nextPow2(2 * c.length - 1);
    auto oracle = factory.create(sizeF);
    ManualPipeline manual(c.length, c.hop, windowTable(c.length,
                                                       grain::GrainWindow::Kind::PeriodicHann),
                          sizeF);
    const auto fromManual = manual.run(a, b, c.hop,
                                       static_cast<float>(wetScale(c.length, c.hop)), *oracle);

    REQUIRE(fromManual.size() == fromEngine.size());
    for (std::size_t q = 0; q < fromEngine.size(); ++q)
    {
        INFO("q=" << q);
        CHECK(fromManual[q] == fromEngine[q]);
    }
}

TEST_CASE("An event's wet gain does not depend on where it falls relative to the hop grid",
          "[grain-engine]")
{
    // The hop-rate ripple test. A delta at n0 answers as out[n0 + L + m] = scale·w(m)·G(phase),
    // where G(phase) is the window's hop-phase class sum and phase = n0 mod H. A COLA window holds
    // G constant at C, so all H responses are identical and the 1/C gain leaves exactly w(m); a
    // non-COLA window makes event gain a function of grid phase, which is hop-rate gain modulation.
    //
    // Deterministic, which is the point: a per-phase energy average of a noise run carries roughly
    // +/-16% estimator noise at any length a test can afford -- the same size as the ripple it is
    // meant to detect -- and the plan's suggested calibration against a rectangular window is
    // unusable because a rectangular window is exactly COLA whenever H divides L. The known-bad
    // reference is therefore the symmetric Hann (denominator L-1), the very window M2 corrected.
    const Case c{128, 32};
    const int sizeF = nextPow2(2 * c.length - 1);
    const int total = probeTotal(c) + c.hop;
    const float scale = static_cast<float>(wetScale(c.length, c.hop));

    auto oracle = factory.create(sizeF);
    ManualPipeline symmetric(c.length, c.hop, symmetricHannTable(c.length), sizeF);

    std::vector<double> engineGains;
    std::vector<double> symmetricGains;
    for (int phase = 0; phase < c.hop; ++phase)
    {
        const int n0 = probeStart(c) + phase;
        std::vector<float> a(static_cast<std::size_t>(total), 0.0f);
        std::vector<float> b(static_cast<std::size_t>(total), 1.0f);
        a[static_cast<std::size_t>(n0)] = 1.0f;

        engineGains.push_back(responseGain(runDeltaProbe(factory, c, n0, total), n0, c.length));
        symmetricGains.push_back(
            responseGain(symmetric.run(a, b, c.hop, scale, *oracle), n0, c.length));
    }

    const double engineSpread = gainSpreadAcrossPhases(engineGains);
    const double symmetricSpread = gainSpreadAcrossPhases(symmetricGains);

    INFO("event-gain spread across hop phases: engine=" << engineSpread
         << " symmetric-Hann=" << symmetricSpread);
    REQUIRE(symmetricSpread > 1e-4);
    CHECK(engineSpread < 0.01 * symmetricSpread);
}
