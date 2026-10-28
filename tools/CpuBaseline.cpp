/** Per-callback cost of the grain engine, for M7's flattening work (M2 plan §6.7).

    Reports what one host callback costs when it fires a hop and when it does not, because with
    R = 0 the whole FFT for a hop has to finish inside the callback that crosses the boundary
    (plan §0) — an average alone hides exactly the spike M7 has to remove.

    Not registered with CTest: wall-clock timings on a shared machine are not a pass/fail signal.
    Run it by hand and record the numbers; the shape (hop-boundary cost vs idle cost) is the
    finding, not the last significant digit. */

#include "GrainEngine.hpp"

#include <HopScheduler.hpp>
#include <JuceFft.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <vector>

namespace
{
class JuceFactory final : public grain::TransformFactory
{
public:
    [[nodiscard]] std::unique_ptr<fft::IFft> create(int sizeF) const override
    {
        return std::make_unique<fft::JuceFft>(sizeF);
    }
};

using Clock = std::chrono::steady_clock;

/** Nearest-rank quantile of a duration vector; sorts in place. */
double quantileUs(std::vector<double>& samples, double rank)
{
    if (samples.empty())
        return 0.0;
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<std::size_t>(rank * static_cast<double>(samples.size() - 1));
    return samples[index];
}

double meanUs(const std::vector<double>& samples)
{
    if (samples.empty())
        return 0.0;
    double sum = 0.0;
    for (double s : samples)
        sum += s;
    return sum / static_cast<double>(samples.size());
}

struct Result
{
    std::vector<double> hopCallbacks;
    std::vector<double> idleCallbacks;
    std::vector<double> hopWork;
};

Result measure(int length, int hop, int hostBlock, int hops, const grain::TransformFactory& factory,
               double sampleRate)
{
    std::mt19937 rng(20261027U);
    std::uniform_real_distribution<float> spread(-0.25f, 0.25f);
    const std::size_t total = static_cast<std::size_t>(hops) * static_cast<std::size_t>(hop);
    std::vector<float> a(total);
    std::vector<float> b(total);
    for (std::size_t i = 0; i < total; ++i)
    {
        a[i] = spread(rng);
        b[i] = spread(rng);
    }
    std::vector<float> out(total, 0.0f);

    grain::GrainConvolutionEngine engine(factory);
    engine::EngineConfig engineConfig;
    engineConfig.sampleRate = sampleRate;
    engineConfig.hop = hop;
    engineConfig.maxChannels = 1;
    /** R = H rather than 0: HopScheduler pre-fills exactly latencySamples and then requires every
        callback to drain what it asked for, so R = 0 with hostBlock < H aborts on an empty ring
        (F-006). Cost is unaffected — the DSP work is hop-driven and R only moves where the host
        cuts the stream. */
    engineConfig.latencySamples = hop;
    engineConfig.grainLength = length;

    engine::SchedulerConfig config;
    config.engine = engineConfig;
    config.channels = 1;
    config.sidechainChannels = 1;
    config.maxHostBlock = hostBlock;

    engine::HopScheduler scheduler(engine);
    scheduler.prepare(config);

    auto runCallback = [&](int position) {
        const float* aPtr = a.data() + position;
        const float* bPtr = b.data() + position;
        float* outPtr = out.data() + position;
        scheduler.process(&aPtr, &bPtr, &outPtr, 1, hostBlock);
    };

    /** Warm the caches and let the transform's first-call setup happen outside every timed region. */
    const int warmup = std::min(hops, 5 * (hop / hostBlock));
    for (int k = 0; k < warmup; ++k)
        runCallback(k * hostBlock);

    Result result;
    const int iterations = hops - warmup;
    result.hopCallbacks.reserve(static_cast<std::size_t>(iterations));
    result.idleCallbacks.reserve(static_cast<std::size_t>(iterations));

    for (int k = 0; k < iterations; ++k)
    {
        const int position = (warmup + k) * hostBlock;
        const std::uint64_t hopsBefore = scheduler.hopCount();
        const auto start = Clock::now();
        runCallback(position);
        const auto end = Clock::now();
        const double us = std::chrono::duration<double, std::micro>(end - start).count();
        if (scheduler.hopCount() != hopsBefore)
            result.hopCallbacks.push_back(us);
        else
            result.idleCallbacks.push_back(us);
    }

    /** The engine seam on its own, to separate "the hop is expensive" from "the callback around it
        is expensive". */
    std::vector<float> hopInA(static_cast<std::size_t>(hop));
    std::vector<float> hopInB(static_cast<std::size_t>(hop));
    std::vector<float> hopOut(static_cast<std::size_t>(hop), 0.0f);
    for (int i = 0; i < hop; ++i)
    {
        hopInA[static_cast<std::size_t>(i)] = a[static_cast<std::size_t>(i)];
        hopInB[static_cast<std::size_t>(i)] = b[static_cast<std::size_t>(i)];
    }
    const float* aPtr = hopInA.data();
    const float* bPtr = hopInB.data();
    float* outPtr = hopOut.data();
    for (int k = 0; k < iterations; ++k)
    {
        const auto start = Clock::now();
        engine.processHop(&aPtr, &bPtr, &outPtr, 1);
        const auto end = Clock::now();
        result.hopWork.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }
    return result;
}

void report(int length, int hop, int hostBlock, double sampleRate, const Result& r)
{
    const double hopsPerSecond = sampleRate / static_cast<double>(hop);
    const double meanHopWork = meanUs(r.hopWork);
    const double cpuPercent = meanHopWork * 1e-6 * hopsPerSecond * 100.0;

    std::printf("L=%5d H=%4d hostBlock=%3d  callbacks: hop n=%-5zu mean=%8.2f p99=%8.2f max=%8.2f us"
                " | idle n=%-5zu mean=%6.2f max=%6.2f us\n",
                length, hop, hostBlock, r.hopCallbacks.size(), meanUs(r.hopCallbacks),
                quantileUs(const_cast<std::vector<double>&>(r.hopCallbacks), 0.99),
                quantileUs(const_cast<std::vector<double>&>(r.hopCallbacks), 1.0),
                r.idleCallbacks.size(), meanUs(r.idleCallbacks),
                quantileUs(const_cast<std::vector<double>&>(r.idleCallbacks), 1.0));
    std::printf("                 processHop alone: mean=%8.2f p99=%8.2f max=%8.2f us"
                "  -> %5.2f%% of one core at %.0f Hz\n",
                meanHopWork, quantileUs(const_cast<std::vector<double>&>(r.hopWork), 0.99),
                quantileUs(const_cast<std::vector<double>&>(r.hopWork), 1.0), cpuPercent,
                sampleRate);
}
}

int main()
{
    const double sampleRate = 48000.0;
    const int hops = 3000;
    const JuceFactory factory;

    fft::JuceFft probe(4096);
    std::printf("transform: juce::dsp::FFT, measured forward gain=%.3f inverse gain=%.3f\n",
                probe.forwardGain(), probe.inverseGain());
    std::printf("engine: grain::GrainConvolutionEngine, R = H, mono, %d hops per row\n\n", hops);

    for (const auto& shape : std::vector<std::pair<int, int>>{{512, 128}, {1024, 256},
                                                              {2048, 512}})
    {
        for (const int hostBlock : {32, 64})
            report(shape.first, shape.second, hostBlock, sampleRate,
                   measure(shape.first, shape.second, hostBlock, hops, factory, sampleRate));
        std::printf("\n");
    }
    return 0;
}
