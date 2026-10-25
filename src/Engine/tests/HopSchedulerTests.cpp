#include <Engine.hpp>
#include <HopScheduler.hpp>
#include <PassthroughEngine.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <new>
#include <numeric>
#include <random>
#include <vector>

namespace
{
constexpr float kUnwritten = std::numeric_limits<float>::quiet_NaN();

/** Integer-valued and distinct, so bit-exactness is the only passing comparison. */
float pattern(int channel, int index)
{
    return static_cast<float>(index * 4 + channel) - 1000.0f;
}

std::vector<std::vector<float>> makeInput(int channels, int samples, int firstIndex, int offset)
{
    std::vector<std::vector<float>> data(static_cast<std::size_t>(channels),
                                         std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] =
                pattern(channel, firstIndex + i) + static_cast<float>(offset);
    return data;
}

struct Run
{
    std::vector<std::vector<float>> channel; ///< delivered samples, per channel
    std::vector<int> delivered;              ///< samples handed back per callback
};

Run runSequence(engine::HopScheduler& scheduler, int channels,
                const std::vector<int>& blocks, bool feedSidechain, int bOffset = 7)
{
    Run run;
    int nextIndex = 0;

    for (int samples : blocks)
    {
        const auto a = makeInput(channels, samples, nextIndex, 0);
        const auto b = feedSidechain ? makeInput(channels, samples, nextIndex, bOffset)
                                     : std::vector<std::vector<float>>{};

        std::vector<const float*> aPtrs(static_cast<std::size_t>(channels));
        std::vector<const float*> bPtrs(static_cast<std::size_t>(channels));
        for (int channel = 0; channel < channels; ++channel)
        {
            aPtrs[static_cast<std::size_t>(channel)] = a[static_cast<std::size_t>(channel)].data();
            if (feedSidechain)
                bPtrs[static_cast<std::size_t>(channel)] = b[static_cast<std::size_t>(channel)].data();
        }

        std::vector<std::vector<float>> out(
            static_cast<std::size_t>(channels),
            std::vector<float>(static_cast<std::size_t>(samples), kUnwritten));
        std::vector<float*> outPtrs(static_cast<std::size_t>(channels));
        for (int channel = 0; channel < channels; ++channel)
            outPtrs[static_cast<std::size_t>(channel)] = out[static_cast<std::size_t>(channel)].data();

        scheduler.process(aPtrs.data(), feedSidechain ? bPtrs.data() : nullptr, outPtrs.data(),
                          channels, samples);

        for (int channel = 0; channel < channels; ++channel)
            run.channel.push_back(std::move(out[static_cast<std::size_t>(channel)]));
        run.delivered.push_back(samples);
        nextIndex += samples;
    }

    return run;
}

const std::vector<float>& channelSamples(const Run& run, int channels, int channel, int callback)
{
    return run.channel.at(static_cast<std::size_t>(callback) * static_cast<std::size_t>(channels)
                          + static_cast<std::size_t>(channel));
}

void expectEverySampleWritten(const Run& run, int channels)
{
    for (std::size_t callback = 0; callback < run.delivered.size(); ++callback)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto& samples =
                channelSamples(run, channels, channel, static_cast<int>(callback));
            REQUIRE(static_cast<int>(samples.size()) == run.delivered[callback]);
            for (const float sample : samples)
                CHECK(sample == sample); ///< a NaN here means a sample was never delivered
        }
    }
}

/** Output stream must equal the input stream delayed by exactly the declared latency. */
void expectDelayedBitExact(const Run& run, int channels, int latency, int bOffset = 7,
                           bool fromSidechain = false)
{
    int streamStart = 0;
    for (std::size_t callback = 0; callback < run.delivered.size(); ++callback)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto& samples =
                channelSamples(run, channels, channel, static_cast<int>(callback));
            for (int i = 0; i < static_cast<int>(samples.size()); ++i)
            {
                const int streamIndex = streamStart + i;
                const float expected =
                    streamIndex < latency
                        ? 0.0f
                        : pattern(channel, streamIndex - latency) +
                              (fromSidechain ? static_cast<float>(bOffset) : 0.0f);
                INFO("callback " << callback << " channel " << channel << " sample " << i
                                 << " of stream position " << streamIndex);
                REQUIRE(samples[static_cast<std::size_t>(i)] == expected);
            }
        }
        streamStart += run.delivered[callback];
    }
}

engine::SchedulerConfig makeConfig(int hop, int latency, int channels, int sidechain,
                                   int maxHostBlock)
{
    engine::SchedulerConfig config;
    config.engine.sampleRate = 48000.0;
    config.engine.hop = hop;
    config.engine.maxChannels = channels;
    config.engine.latencySamples = latency;
    config.channels = channels;
    config.sidechainChannels = sidechain;
    config.maxHostBlock = maxHostBlock;
    return config;
}

std::vector<int> interleaveZeros(const std::vector<int>& blocks)
{
    std::vector<int> out;
    for (int block : blocks)
    {
        out.push_back(0);
        out.push_back(block);
    }
    out.push_back(0);
    return out;
}

/** Records what the Engine saw, so hostBlock-independence is observable. */
class RecordingEngine final : public engine::Engine
{
public:
    void prepare(const engine::EngineConfig& config) override
    {
        hop = config.hop;
        latency = config.latencySamples;
    }

    void reset() override { recorded.clear(); }

    int latencySamples() const override { return latency; }

    void processHop(const float* const* a, const float* const*, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < hop; ++i)
            {
                recorded.push_back(a[channel][i]);
                out[channel][i] = a[channel][i];
            }
    }

    std::vector<float> recorded;
    int hop = 0;
    int latency = 0;
};

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
}

TEST_CASE("passthrough is bit-exact for every host block size in the M1 list", "[scheduler]")
{
    for (int hostBlock : {1, 7, 32, 64, 128, 512, 1000, 2048})
    {
        const int hop = 64;
        const int latency = hop;
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, latency, 2, 2, std::max(hostBlock, 2048)));

        const auto run = runSequence(scheduler, 2, std::vector<int>(16, hostBlock), true);

        INFO("hostBlock " << hostBlock);
        expectEverySampleWritten(run, 2);
        expectDelayedBitExact(run, 2, latency);
    }
}

TEST_CASE("latency that is not a multiple of the hop is still exact", "[scheduler]")
{
    const int hop = 64;
    const int latency = 100;
    engine::PassthroughEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(makeConfig(hop, latency, 1, 1, 2048));

    const auto run = runSequence(scheduler, 1, {1, 3, 200, 64, 1024, 7, 512}, true);

    expectEverySampleWritten(run, 1);
    expectDelayedBitExact(run, 1, latency);
}

TEST_CASE("samples out equals samples in for every callback, including zero-length",
          "[scheduler]")
{
    const std::vector<int> blocks =
        interleaveZeros({1, 7, 32, 63, 64, 65, 128, 512, 1000, 2048});

    engine::PassthroughEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(makeConfig(64, 64, 2, 2, 2048));

    const auto run = runSequence(scheduler, 2, blocks, true);

    REQUIRE(run.delivered == blocks);
    expectEverySampleWritten(run, 2);
    expectDelayedBitExact(run, 2, 64);
}

TEST_CASE("hop count equals floor(accumulated samples / H)", "[scheduler]")
{
    const int hop = 128;
    const std::vector<int> blocks{1, 7, 32, 64, 128, 512, 1000, 2048, 0, 3, 127, 129};

    engine::PassthroughEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(makeConfig(hop, hop, 2, 2, 2048));

    int accumulated = 0;
    for (int samples : blocks)
    {
        const auto run = runSequence(scheduler, 2, {samples}, true);
        accumulated += samples;
        INFO("after " << samples << " samples, accumulated " << accumulated);
        CHECK(scheduler.hopCount() == static_cast<std::uint64_t>(accumulated / hop));
    }
}

TEST_CASE("randomised block sizes from a fixed seed stay bit-exact", "[scheduler]")
{
    std::mt19937 rng(20261022U);
    std::uniform_int_distribution<int> size(0, 2048);

    std::vector<int> blocks;
    for (int i = 0; i < 500; ++i)
        blocks.push_back(size(rng));

    const int hop = 64;
    engine::PassthroughEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(makeConfig(hop, hop, 2, 2, 2048));

    const auto run = runSequence(scheduler, 2, blocks, true);

    REQUIRE(run.delivered == blocks);
    expectEverySampleWritten(run, 2);
    expectDelayedBitExact(run, 2, hop);

    int accumulated = 0;
    for (int samples : blocks)
        accumulated += samples;
    CHECK(scheduler.hopCount() == static_cast<std::uint64_t>(accumulated / hop));
}

TEST_CASE("a missing sidechain never stalls the hop clock", "[scheduler]")
{
    const int hop = 64;
    const std::vector<int> blocks{0, 63, 1, 64, 2048, 7, 0, 129};
    int accumulated = 0;
    for (int samples : blocks)
        accumulated += samples;

    SECTION("bus never present: no B rings at all")
    {
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, 2, 0, 2048));

        const auto run = runSequence(scheduler, 2, blocks, false);

        expectEverySampleWritten(run, 2);
        expectDelayedBitExact(run, 2, hop);
        CHECK(scheduler.hopCount() == static_cast<std::uint64_t>(accumulated / hop));
    }

    SECTION("bus disabled after prepare: B is silence-filled at ingest")
    {
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, 2, 2, 2048));

        const auto run = runSequence(scheduler, 2, blocks, false);

        expectEverySampleWritten(run, 2);
        expectDelayedBitExact(run, 2, hop);
        CHECK(scheduler.hopCount() == static_cast<std::uint64_t>(accumulated / hop));
    }
}

TEST_CASE("an engine fed by B sees the sidechain, and silence when it is absent",
          "[scheduler]")
{
    class CopyBEngine final : public engine::Engine
    {
    public:
        void prepare(const engine::EngineConfig& config) override
        {
            hop = config.hop;
            latency = config.latencySamples;
        }
        void reset() override {}
        int latencySamples() const override { return latency; }
        void processHop(const float* const*, const float* const* b, float* const* out,
                        int numChannels) override
        {
            for (int channel = 0; channel < numChannels; ++channel)
                for (int i = 0; i < hop; ++i)
                    out[channel][i] = b[channel][i];
        }
        int hop = 0;
        int latency = 0;
    };

    const int hop = 64;
    const std::vector<int> blocks{200, 100, 0, 256};

    SECTION("connected sidechain drives the output")
    {
        CopyBEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, 2, 2, 2048));

        const auto run = runSequence(scheduler, 2, blocks, true);
        expectDelayedBitExact(run, 2, hop, 7, true);
    }

    SECTION("absent sidechain reads silence, not garbage")
    {
        CopyBEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, 2, 0, 2048));

        const auto run = runSequence(scheduler, 2, blocks, false);
        for (const auto& samples : run.channel)
            for (float sample : samples)
                CHECK(sample == 0.0f);
    }
}

TEST_CASE("the engine cannot observe hostBlock", "[scheduler]")
{
    const int hop = 64;
    const int total = 4096;

    auto recordFor = [&](const std::vector<int>& blocks) {
        RecordingEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, 1, 1, 2048));
        runSequence(scheduler, 1, blocks, true);
        return engine.recorded;
    };

    std::vector<int> ones(static_cast<std::size_t>(total), 1);
    std::vector<int> big(static_cast<std::size_t>(total / 2048), 2048);
    std::vector<int> mixed{7, 33, 2048, 1, 63, 512, 1000, 64, 128, 0, 0};
    mixed.push_back(total - std::accumulate(mixed.begin(), mixed.end(), 0));

    const auto fromOnes = recordFor(ones);
    const auto fromBig = recordFor(big);
    const auto fromMixed = recordFor(mixed);

    CHECK(fromOnes.size() == static_cast<std::size_t>(total));
    REQUIRE(fromBig.size() == fromOnes.size());
    REQUIRE(fromMixed.size() == fromOnes.size());
    CHECK(fromBig == fromOnes);
    CHECK(fromMixed == fromOnes);
}

TEST_CASE("nothing is allocated on the hop path", "[scheduler]")
{
    engine::PassthroughEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(makeConfig(64, 64, 2, 2, 2048));

    constexpr int channels = 2;
    auto buffers = [](float value) {
        return std::vector<std::vector<float>>(
            static_cast<std::size_t>(channels), std::vector<float>(2048, value));
    };
    auto in = buffers(1.0f);
    auto side = buffers(2.0f);
    auto out = buffers(0.0f);

    std::vector<const float*> inPtrs(static_cast<std::size_t>(channels));
    std::vector<const float*> sidePtrs(static_cast<std::size_t>(channels));
    std::vector<float*> outPtrs(static_cast<std::size_t>(channels));
    for (int channel = 0; channel < channels; ++channel)
    {
        inPtrs[static_cast<std::size_t>(channel)] = in[static_cast<std::size_t>(channel)].data();
        sidePtrs[static_cast<std::size_t>(channel)] = side[static_cast<std::size_t>(channel)].data();
        outPtrs[static_cast<std::size_t>(channel)] = out[static_cast<std::size_t>(channel)].data();
    }

    const std::vector<int> blocks{2048, 0, 1, 63, 64, 129, 512, 2048, 1000};

    // The harness buffers are already at their final size, so anything allocated inside the
    // watched loop belongs to the scheduler or the engine.
    for (int samples : blocks)
        scheduler.process(inPtrs.data(), sidePtrs.data(), outPtrs.data(), channels, samples);

    long observed = 0;
    {
        AllocationWatch watch;
        for (int samples : blocks)
            scheduler.process(inPtrs.data(), sidePtrs.data(), outPtrs.data(), channels, samples);
        observed = allocationCount;
    }

    CHECK(observed == 0);
}

TEST_CASE("reset restores the latency pre-fill", "[scheduler]")
{
    const int hop = 64;
    const int latency = 64;
    engine::PassthroughEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(makeConfig(hop, latency, 1, 1, 2048));

    runSequence(scheduler, 1, {500}, true);
    scheduler.reset();
    CHECK(scheduler.hopCount() == 0);

    const auto run = runSequence(scheduler, 1, {100, 300}, true);
    expectEverySampleWritten(run, 1);
    expectDelayedBitExact(run, 1, latency);
}
