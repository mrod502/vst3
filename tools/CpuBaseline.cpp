/** Per-callback cost of the grain engine, for M7's flattening work (M2 plan §6.7) and for the
    item-6 §3 feasibility gate over the chosen grain-length set.

    Reports what one host callback costs when it fires a hop and when it does not, because with
    R a declaration rather than a deferral the whole FFT for a hop has to finish inside the
    callback that crosses the boundary — an average alone hides exactly the spike M7 has to remove.

    Measured in both channel counts because the feasibility table in the item-6 plan §3 extrapolated
    stereo as 2x mono and nothing had ever measured it: the engine loops channels sequentially, so
    the ratio is a claim to test, not a fact to assume. Rows are mono unless the line says ch=2.

    Not registered with CTest: wall-clock timings on a shared machine are not a pass/fail signal.
    Run it by hand and record the numbers; the shape (hop-boundary cost vs idle cost) is the
    finding, not the last significant digit. Always quote a number with its build type — at -O0
    these are 2.6-3.1x worse (F-008). */

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
               double sampleRate, int channels)
{
    std::mt19937 rng(20261027U);
    std::uniform_real_distribution<float> spread(-0.25f, 0.25f);
    const std::size_t total = static_cast<std::size_t>(hops) * static_cast<std::size_t>(hop);

    std::vector<std::vector<float>> a(static_cast<std::size_t>(channels),
                                      std::vector<float>(total));
    std::vector<std::vector<float>> b(static_cast<std::size_t>(channels),
                                      std::vector<float>(total));
    std::vector<std::vector<float>> out(static_cast<std::size_t>(channels),
                                        std::vector<float>(total, 0.0f));
    for (auto& channel : a)
        for (float& sample : channel)
            sample = spread(rng);
    for (auto& channel : b)
        for (float& sample : channel)
            sample = spread(rng);

    std::vector<const float*> aPtrs(static_cast<std::size_t>(channels));
    std::vector<const float*> bPtrs(static_cast<std::size_t>(channels));
    std::vector<float*> outPtrs(static_cast<std::size_t>(channels));

    grain::GrainConvolutionEngine engine(factory);
    engine::EngineConfig engineConfig;
    engineConfig.sampleRate = sampleRate;
    engineConfig.hop = hop;
    engineConfig.maxChannels = channels;
    /** R = H rather than 0: HopScheduler pre-fills exactly latencySamples and then requires every
        callback to drain what it asked for, so R = 0 with hostBlock < H aborts on an empty ring
        (F-006). Cost is unaffected — the DSP work is hop-driven and R only moves where the host
        cuts the stream. */
    engineConfig.latencySamples = hop;
    engineConfig.grainLength = length;

    engine::SchedulerConfig config;
    config.engine = engineConfig;
    config.channels = channels;
    config.sidechainChannels = channels;
    config.maxHostBlock = hostBlock;

    engine::HopScheduler scheduler(engine);
    scheduler.prepare(config);

    auto runCallback = [&](int position) {
        for (int channel = 0; channel < channels; ++channel)
        {
            aPtrs[static_cast<std::size_t>(channel)] =
                a[static_cast<std::size_t>(channel)].data() + position;
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data() + position;
            outPtrs[static_cast<std::size_t>(channel)] =
                out[static_cast<std::size_t>(channel)].data() + position;
        }
        scheduler.process(aPtrs.data(), bPtrs.data(), outPtrs.data(), channels, hostBlock);
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
        is expensive". All channels in one call, so the figure is what one hop of the installed
        channel count actually costs. */
    for (int channel = 0; channel < channels; ++channel)
    {
        aPtrs[static_cast<std::size_t>(channel)] = a[static_cast<std::size_t>(channel)].data();
        bPtrs[static_cast<std::size_t>(channel)] = b[static_cast<std::size_t>(channel)].data();
        outPtrs[static_cast<std::size_t>(channel)] = out[static_cast<std::size_t>(channel)].data();
    }
    for (int k = 0; k < iterations; ++k)
    {
        const auto start = Clock::now();
        engine.processHop(aPtrs.data(), bPtrs.data(), outPtrs.data(), channels);
        const auto end = Clock::now();
        result.hopWork.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    }
    return result;
}

void report(int length, int hop, int hostBlock, int channels, double sampleRate, const Result& r)
{
    const double hopsPerSecond = sampleRate / static_cast<double>(hop);
    const double meanHopWork = meanUs(r.hopWork);
    const double cpuPercent = meanHopWork * 1e-6 * hopsPerSecond * 100.0;

    std::printf("L=%5d H=%4d ch=%d hostBlock=%3d  callbacks: hop n=%-5zu mean=%8.2f p99=%8.2f max=%8.2f us"
                " | idle n=%-5zu mean=%6.2f max=%6.2f us\n",
                length, hop, channels, hostBlock, r.hopCallbacks.size(), meanUs(r.hopCallbacks),
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
    std::printf("engine: grain::GrainConvolutionEngine, R = H, %d hops per row\n\n", hops);

    /* The whole grain-length set D-008 puts in the UI, at the shortest and longest hop it can be
       paired with. The cost per second is F log F / H, so it is the (L, H) pair and not L alone
       that decides feasibility, and the corner the arithmetic condemns is long L with short H. */
    for (const auto& shape : std::vector<std::pair<int, int>>{{1024, 128}, {2048, 128},
                                                              {4096, 128}, {8192, 128},
                                                              {16384, 128}, {32768, 128},
                                                              {65536, 128}, {1024, 512},
                                                              {2048, 512}, {4096, 512},
                                                              {8192, 512}, {16384, 512},
                                                              {32768, 512}, {65536, 512}})
    {
        const int length = shape.first;
        const int hop = shape.second;

        double monoUs = 0.0;
        double stereoUs = 0.0;
        for (const int channels : {1, 2})
        {
            for (const int hostBlock : {32, 64})
            {
                const Result result =
                    measure(length, hop, hostBlock, hops, factory, sampleRate, channels);
                report(length, hop, hostBlock, channels, sampleRate, result);
                if (hostBlock == 32)
                {
                    if (channels == 1)
                        monoUs = meanUs(result.hopWork);
                    else
                        stereoUs = meanUs(result.hopWork);
                }
            }
        }
        std::printf("                 stereo/mono for one hop: %.2fx (2.00x would justify the"
                    " 2x-mono extrapolation)\n\n",
                    stereoUs > 0.0 ? stereoUs / monoUs : 0.0);
    }
    return 0;
}
