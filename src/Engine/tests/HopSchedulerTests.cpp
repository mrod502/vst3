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
#include <stdexcept>
#include <tuple>
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

/** The map is what a caller resolves, so the harness resolves it the same way the shipped builder
    does rather than hand-writing indices — the rule stays in one function (INDEX §3 rule 2). */
engine::SchedulerConfig makeConfigForOutputs(int hop, int latency, int channels, int sidechain,
                                             int outputs, int maxHostBlock)
{
    const auto resolved = engine::ChannelMap::resolve(channels, sidechain, outputs);
    REQUIRE(resolved.status == engine::ChannelMapStatus::ok);

    engine::SchedulerConfig config;
    config.engine.sampleRate = 48000.0;
    config.engine.hop = hop;
    config.engine.maxChannels = resolved.map.channels;
    config.engine.latencySamples = latency;
    config.channels = channels;
    config.sidechainChannels = sidechain;
    config.maxHostBlock = maxHostBlock;
    config.map = resolved.map;
    return config;
}

/** The output count equal to the A width, which is every configuration the shipped gate can reach. */
engine::SchedulerConfig makeConfig(int hop, int latency, int channels, int sidechain,
                                   int maxHostBlock)
{
    return makeConfigForOutputs(hop, latency, channels, sidechain, channels, maxHostBlock);
}

std::vector<std::vector<float>> makeStream(int rows, int samples, float offset)
{
    std::vector<std::vector<float>> data(
        static_cast<std::size_t>(rows), std::vector<float>(static_cast<std::size_t>(samples)));
    for (int row = 0; row < rows; ++row)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(row)][static_cast<std::size_t>(i)] =
                pattern(row, i) + offset;
    return data;
}

/** Whole source streams sliced per callback, the way a host's bus arrays slice a running stream, so a
    leg can state the A width, the B width and the processed count independently. Pass an empty @p b to
    hand over a null B array. */
Run runWithWidths(engine::HopScheduler& scheduler, int sources, int sideSources, int processed,
                  const std::vector<std::vector<float>>& a,
                  const std::vector<std::vector<float>>& b, const std::vector<int>& blocks)
{
    Run run;
    int position = 0;
    for (const int samples : blocks)
    {
        std::vector<const float*> aPtrs(static_cast<std::size_t>(sources));
        std::vector<const float*> bPtrs(static_cast<std::size_t>(sideSources));
        std::vector<std::vector<float>> out(
            static_cast<std::size_t>(processed),
            std::vector<float>(static_cast<std::size_t>(samples), kUnwritten));
        std::vector<float*> outPtrs(static_cast<std::size_t>(processed));

        for (int channel = 0; channel < sources; ++channel)
            aPtrs[static_cast<std::size_t>(channel)] =
                a[static_cast<std::size_t>(channel)].data() + position;
        for (int channel = 0; channel < sideSources; ++channel)
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data() + position;
        for (int channel = 0; channel < processed; ++channel)
            outPtrs[static_cast<std::size_t>(channel)] = out[static_cast<std::size_t>(channel)].data();

        scheduler.process(aPtrs.data(), sideSources > 0 ? bPtrs.data() : nullptr, outPtrs.data(),
                          processed, samples);

        for (auto& row : out)
            run.channel.push_back(std::move(row));
        run.delivered.push_back(samples);
        position += samples;
    }
    return run;
}

/** Records each hop the scheduler hands over, on both sides, and the pointer identity of every slot —
    pointer equality is the only way to see a broadcast rather than two streams that happen to carry the
    same numbers. Passes A through so the delivery accounting still has something to drain. */
class TapEngine final : public engine::Engine
{
public:
    void prepare(const engine::EngineConfig& prepared) override
    {
        hop = prepared.hop;
        latency = prepared.latencySamples;
    }

    void reset() override
    {
        a.clear();
        b.clear();
        aPointers.clear();
        bPointers.clear();
    }

    int latencySamples() const override { return latency; }

    void processHop(const float* const* aIn, const float* const* bIn, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            aPointers.push_back(aIn[channel]);
            bPointers.push_back(bIn[channel]);
            for (int i = 0; i < hop; ++i)
            {
                a.push_back(aIn[channel][i]);
                b.push_back(bIn[channel][i]);
            }
        }
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < hop; ++i)
                out[channel][i] = aIn[channel][i];
    }

    /** Slot @p slot of hop @p hopIndex in the stream recorded for one processed channel. */
    const float* hopSlot(int hopIndex, int processed, int slot) const noexcept
    {
        return b.data() + (static_cast<std::size_t>(hopIndex) * static_cast<std::size_t>(processed)
                           + static_cast<std::size_t>(slot))
                              * static_cast<std::size_t>(hop);
    }

    std::vector<float> a;
    std::vector<float> b;
    std::vector<const float*> aPointers;
    std::vector<const float*> bPointers;
    int hop = 0;
    int latency = 0;
};

int hopsFired(const TapEngine& tap, int processed) noexcept
{
    return static_cast<int>(tap.bPointers.size()) / processed;
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

TEST_CASE("a mono sidechain broadcasts its channel 0 to every processed channel", "[scheduler]")
{
    const int hop = 64;
    const std::vector<int> blocks{192, 128, 0, 256, 64, 0};
    const int total = std::accumulate(blocks.begin(), blocks.end(), 0);
    const int fullHops = total / hop;

    for (const int channels : {2, 4})
    {
        TapEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, channels, 1, 2048));

        const auto a = makeStream(channels, total, 0.0f);
        const auto b = makeStream(1, total, 5000.0f);
        const auto run = runWithWidths(scheduler, channels, 1, channels, a, b, blocks);

        INFO(channels << " channels");
        REQUIRE(hopsFired(engine, channels) == fullHops);
        expectEverySampleWritten(run, channels);

        for (int fired = 0; fired < fullHops; ++fired)
            for (int slot = 0; slot < channels; ++slot)
            {
                INFO("hop " << fired << ", slot " << slot);
                const auto first = static_cast<std::size_t>(fired * channels);
                CHECK(engine.bPointers[first]
                      == engine.bPointers[first + static_cast<std::size_t>(slot)]);
                for (int i = 0; i < hop; ++i)
                    CHECK(engine.hopSlot(fired, channels, slot)[i]
                          == b[0][static_cast<std::size_t>(fired * hop + i)]);
            }
    }
}

/** The consolidated side's input patterns, each with the property the fold is judged on: unequal rows
    catch a mean that ignores chB, an antiphase pair catches anything but an exact ascending sum, and a
    hot row against zeros catches a fold that reads one channel and calls it the mean. */
enum class SidePattern
{
    decorrelated,
    unequal,
    antiphase,
    hotChannel
};

std::vector<std::vector<float>> makeSideStream(int rows, int samples, SidePattern kind)
{
    std::vector<std::vector<float>> stream(
        static_cast<std::size_t>(rows), std::vector<float>(static_cast<std::size_t>(samples)));
    std::mt19937 rng(20261103U);
    std::uniform_real_distribution<float> spread(-0.4f, 0.4f);

    for (int row = 0; row < rows; ++row)
    {
        /* An odd antiphase row is the exact negation of the one beneath it rather than a fresh draw:
           that relation is the whole case, and two independent draws would not cancel. Every other
           row keeps its own independent noise, so the decorrelated pattern means what it says. */
        if (kind == SidePattern::antiphase && row % 2 != 0)
        {
            const auto& leader = stream[static_cast<std::size_t>(row - 1)];
            for (int i = 0; i < samples; ++i)
                stream[static_cast<std::size_t>(row)][static_cast<std::size_t>(i)] = -leader[static_cast<std::size_t>(i)];
            continue;
        }

        for (int i = 0; i < samples; ++i)
        {
            float value = spread(rng);
            switch (kind)
            {
                case SidePattern::decorrelated:
                    break;
                case SidePattern::unequal:
                    value *= 1.0f / static_cast<float>(row + 2);
                    break;
                case SidePattern::antiphase:
                    break;
                case SidePattern::hotChannel:
                    if (row != 0)
                        value = 0.0f;
                    break;
            }
            stream[static_cast<std::size_t>(row)][static_cast<std::size_t>(i)] = value;
        }
    }
    return stream;
}

TEST_CASE("the consolidated fold is the arithmetic mean of its sidechain sources", "[scheduler]")
{
    const int hop = 64;
    const std::vector<int> blocks{100, 0, 196, 128, 512, 0};
    const int total = std::accumulate(blocks.begin(), blocks.end(), 0);
    const int fullHops = total / hop;

    for (const int sideSources : {2, 3, 4, 8})
        for (const SidePattern kind : {SidePattern::decorrelated, SidePattern::unequal,
                                       SidePattern::antiphase, SidePattern::hotChannel})
    {
        TapEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfigForOutputs(hop, hop, 1, sideSources, 1, 2048));

        const auto a = makeStream(1, total, 0.0f);
        const auto b = makeSideStream(sideSources, total, kind);
        const auto run = runWithWidths(scheduler, 1, sideSources, 1, a, b, blocks);

        INFO("chB " << sideSources << ", pattern " << static_cast<int>(kind));
        REQUIRE(hopsFired(engine, 1) == fullHops);
        expectEverySampleWritten(run, 1);

        /* The reference mirrors the scheduler's order — ascending source index, the reciprocal applied
           once after the sum — so the comparison is bit-exact rather than a tolerance, including at
           chB = 3 where the reciprocal is not representable. */
        const float scale = 1.0f / static_cast<float>(sideSources);
        for (int fired = 0; fired < fullHops; ++fired)
            for (int i = 0; i < hop; ++i)
            {
                const int position = fired * hop + i;
                float sum = 0.0f;
                for (int row = 0; row < sideSources; ++row)
                    sum += b[static_cast<std::size_t>(row)][static_cast<std::size_t>(position)];
                INFO("hop " << fired << ", sample " << i);
                CHECK(engine.hopSlot(fired, 1, 0)[i] == sum * scale);
            }

        if (kind == SidePattern::antiphase && sideSources % 2 == 0)
            for (const float sample : engine.b)
                CHECK(sample == 0.0f);

        /* The fold writes its own scratch, so the A side of the same hop must arrive untouched. A fold
           that wrote into A scratch instead would show up here and not in the B column, because the fold
           value itself would still be right. */
        for (int fired = 0; fired < fullHops; ++fired)
            for (int i = 0; i < hop; ++i)
            {
                const auto position = static_cast<std::size_t>(fired * hop + i);
                INFO("hop " << fired << ", sample " << i << ", A side");
                CHECK(engine.a[position] == a[0][position]);
            }
    }
}

TEST_CASE("the consolidation marker, not the index sharing its field, selects the fold",
          "[scheduler]")
{
    const int hop = 64;
    const std::vector<int> blocks{100, 0, 156};
    const int total = std::accumulate(blocks.begin(), blocks.end(), 0);

    /* The two shapes the marker exists to separate: a mono sidechain leaves 0 in the same field and
       must read that channel, a consolidated side must read the mean. Row 0 carries the signal and row
       1 is silence, so a fold that fell through to index 0 would hand over the hot row whole — twice
       the mean, and a different number at every sample this leg checks. */
    const auto wide = makeSideStream(2, total, SidePattern::hotChannel);
    const std::vector<std::vector<float>> narrow{wide[0]};
    const auto a = makeStream(1, total, 0.0f);

    REQUIRE(wide[0] != wide[1]);
    CHECK(engine::ChannelMap::resolve(1, 2, 1).map.b[0] == engine::kConsolidatedSource);
    CHECK(engine::ChannelMap::resolve(1, 1, 1).map.b[0] == 0);

    SECTION("a mono main with a wider side reads the mean of it")
    {
        TapEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfigForOutputs(hop, hop, 1, 2, 1, 2048));
        runWithWidths(scheduler, 1, 2, 1, a, wide, blocks);

        REQUIRE(hopsFired(engine, 1) == total / hop);
        for (int i = 0; i < static_cast<int>(engine.b.size()); ++i)
        {
            float sum = 0.0f;
            sum += wide[0][static_cast<std::size_t>(i)];
            sum += wide[1][static_cast<std::size_t>(i)];
            CHECK(engine.b[static_cast<std::size_t>(i)] == sum * 0.5f);
        }
        CHECK(engine.b != wide[0]); ///< the channel-0 read is the shape this rules out
    }

    SECTION("a mono sidechain reads itself, unchanged")
    {
        TapEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfigForOutputs(hop, hop, 1, 1, 1, 2048));
        runWithWidths(scheduler, 1, 1, 1, a, narrow, blocks);

        REQUIRE(hopsFired(engine, 1) == total / hop);
        CHECK(engine.b == narrow[0]);
    }
}

TEST_CASE("an absent sidechain still reads silence on every processed slot", "[scheduler]")
{
    const int hop = 64;
    const std::vector<int> blocks{0, 63, 1, 64, 512, 7, 0, 129};
    const int total = std::accumulate(blocks.begin(), blocks.end(), 0);

    for (const int channels : {1, 2, 4})
    {
        TapEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfig(hop, hop, channels, 0, 2048));

        const auto a = makeStream(channels, total, 0.0f);
        const auto run = runWithWidths(scheduler, channels, 0, channels, a, {}, blocks);

        INFO(channels << " channels");
        expectEverySampleWritten(run, channels);
        CHECK(scheduler.hopCount() == static_cast<std::uint64_t>(total / hop));
        REQUIRE_FALSE(engine.b.empty());
        for (const float sample : engine.b)
            CHECK(sample == 0.0f);
    }
}

TEST_CASE("prepare takes every map the rule resolves and refuses an index outside its side",
          "[scheduler]")
{
    const int hop = 64;

    SECTION("every ok triple prepares and drives one hop")
    {
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);

        for (const int chA : {1, 2, 3, 4, 8})
            for (const int chB : {0, 1, 2, 3, 8})
                for (const int out : {1, 2, 8})
                {
                    const auto resolved = engine::ChannelMap::resolve(chA, chB, out);
                    if (resolved.status != engine::ChannelMapStatus::ok)
                    {
                        CHECK(resolved.map.channels == 0);
                        continue;
                    }

                    INFO("(" << chA << ", " << chB << ", " << out << ")");
                    auto config = makeConfigForOutputs(hop, hop, chA, chB, out, 256);
                    CHECK_NOTHROW(scheduler.prepare(config));

                    const auto a = makeStream(chA, hop, 0.0f);
                    const auto b = makeStream(chB, hop, 9000.0f);
                    const auto run = runWithWidths(scheduler, chA, chB, out, a, b, {hop});
                    for (const auto& row : run.channel)
                        for (const float sample : row)
                            CHECK(sample == sample);
                }
    }

    SECTION("an index at or beyond its side's width is refused at prepare")
    {
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);
        const auto base = makeConfigForOutputs(hop, hop, 2, 2, 2, 256);
        scheduler.prepare(base);

        auto offByA = base;
        offByA.map.a[1] = 2; ///< == chA: there is no ring 2 on the A side
        CHECK_THROWS_AS(scheduler.prepare(offByA), std::invalid_argument);

        auto offByB = base;
        offByB.map.b[1] = 2;
        CHECK_THROWS_AS(scheduler.prepare(offByB), std::invalid_argument);

        auto foldWithoutSources = makeConfigForOutputs(hop, hop, 1, 0, 1, 256);
        foldWithoutSources.map.b[0] = engine::kConsolidatedSource;
        CHECK_THROWS_AS(scheduler.prepare(foldWithoutSources), std::invalid_argument);

        CHECK_THROWS_AS(scheduler.prepare(engine::SchedulerConfig{}), std::invalid_argument);

        /* The rejected prepares leave the last good configuration standing: a host that keeps playing
           after a declined re-prepare must not find a half-written one under the callback. */
        const auto a = makeStream(2, hop, 0.0f);
        const auto b = makeStream(2, hop, 0.0f);
        const auto run = runWithWidths(scheduler, 2, 2, 2, a, b, {hop});
        for (const auto& row : run.channel)
            for (const float sample : row)
                CHECK(sample == sample);
    }

    SECTION("both sentinels are in range whatever the widths")
    {
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);

        auto silent = makeConfigForOutputs(hop, hop, 2, 2, 2, 256);
        silent.map.b[0] = engine::kSilentSource;
        silent.map.b[1] = engine::kSilentSource;
        CHECK_NOTHROW(scheduler.prepare(silent));

        auto folded = makeConfigForOutputs(hop, hop, 1, 4, 1, 256);
        CHECK(folded.map.b[0] == engine::kConsolidatedSource);
        CHECK_NOTHROW(scheduler.prepare(folded));
    }
}

TEST_CASE("broadcast and consolidation are both invariant in the host block size", "[scheduler]")
{
    const int hop = 64;
    const int total = 4096;

    /* Delivered samples per channel and the recorded B stream — content, not addresses: a pointer
       compared across two scheduler instances is a comparison of two heap allocations, so the fold's
       identity is carried by its values here and by the pointer legs in the broadcast case. */
    auto recordedFor = [](int channels, int sideSources, const std::vector<int>& blocks) {
        const int totalOfBlocks = std::accumulate(blocks.begin(), blocks.end(), 0);
        TapEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfigForOutputs(hop, hop, channels, sideSources, channels, 4096));

        const auto a = makeStream(channels, totalOfBlocks, 0.0f);
        const auto b = makeSideStream(sideSources, totalOfBlocks, SidePattern::decorrelated);
        const auto run = runWithWidths(scheduler, channels, sideSources, channels, a, b, blocks);

        /* Stitched per processed channel, not callback by callback: a 4096-sample callback and 4096
           one-sample callbacks hold the same audio in different concatenation orders otherwise. */
        std::vector<std::vector<float>> delivered(static_cast<std::size_t>(channels));
        for (std::size_t callback = 0; callback < run.delivered.size(); ++callback)
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto& samples =
                    run.channel[callback * static_cast<std::size_t>(channels)
                                + static_cast<std::size_t>(channel)];
                delivered[static_cast<std::size_t>(channel)].insert(
                    delivered[static_cast<std::size_t>(channel)].end(), samples.begin(), samples.end());
            }
        return std::make_tuple(delivered, engine.b, scheduler.hopCount());
    };

    std::vector<int> ones(static_cast<std::size_t>(total), 1);
    std::vector<int> big(1, total);
    std::vector<int> mixed{7, 64, 128, 1, 2048, 63, 0};
    mixed.push_back(total - std::accumulate(mixed.begin(), mixed.end(), 0));

    std::mt19937 rng(20261103U);
    std::uniform_int_distribution<int> size(1, 512);
    std::vector<int> random;
    for (int sum = 0; sum < total;)
    {
        const int take = std::min(size(rng), total - sum);
        random.push_back(take);
        sum += take;
    }

    for (const auto& shape : std::vector<std::pair<int, int>>{{2, 1}, {1, 2}, {1, 8}})
    {
        const auto reference = recordedFor(shape.first, shape.second, {total});

        for (const auto& blocks : {ones, big, mixed, random})
        {
            INFO("chA " << shape.first << ", chB " << shape.second << ", " << blocks.size()
                        << " callbacks");
            REQUIRE(std::accumulate(blocks.begin(), blocks.end(), 0) == total);
            const auto other = recordedFor(shape.first, shape.second, blocks);
            CHECK(std::get<0>(other) == std::get<0>(reference));
            CHECK(std::get<1>(other) == std::get<1>(reference));
            CHECK(std::get<2>(other) == std::get<2>(reference));
        }
    }
}

TEST_CASE("neither the broadcast nor the fold allocates on the hop path", "[scheduler]")
{
    for (const auto& shape : std::vector<std::pair<int, int>>{{2, 1}, {1, 2}, {1, 8}, {4, 1}})
    {
        const int channels = shape.first;
        const int sideSources = shape.second;
        const int total = 4096;

        INFO("chA " << channels << ", chB " << sideSources);

        /* A silent engine rather than a recording one: a tap that grows a vector per hop would be
           charged to the watch, and what this leg is about is the scheduler's own fold scratch. */
        engine::PassthroughEngine engine;
        engine::HopScheduler scheduler(engine);
        scheduler.prepare(makeConfigForOutputs(64, 64, channels, sideSources, channels, 512));

        const auto a = makeStream(channels, total, 0.0f);
        const auto b = makeSideStream(sideSources, total, SidePattern::decorrelated);

        std::vector<const float*> aPtrs(static_cast<std::size_t>(channels));
        std::vector<const float*> bPtrs(static_cast<std::size_t>(sideSources));
        std::vector<std::vector<float>> out(
            static_cast<std::size_t>(channels), std::vector<float>(512, 0.0f));
        std::vector<float*> outPtrs(static_cast<std::size_t>(channels));
        for (int channel = 0; channel < channels; ++channel)
        {
            aPtrs[static_cast<std::size_t>(channel)] = a[static_cast<std::size_t>(channel)].data();
            outPtrs[static_cast<std::size_t>(channel)] = out[static_cast<std::size_t>(channel)].data();
        }
        for (int channel = 0; channel < sideSources; ++channel)
            bPtrs[static_cast<std::size_t>(channel)] =
                b[static_cast<std::size_t>(channel)].data();

        const std::vector<int> blocks{512, 0, 1, 63, 64, 129, 7, 512, 256};

        // The buffers are already at their final size, so anything allocated inside the watch
        // belongs to the scheduler — including the per-hop fold.
        for (int pass = 0; pass < 2; ++pass)
        {
            if (pass == 1)
            {
                AllocationWatch watch;
                for (const int samples : blocks)
                    scheduler.process(aPtrs.data(), bPtrs.data(), outPtrs.data(), channels, samples);
                CHECK(allocationCount == 0);
            }
            else
                for (const int samples : blocks)
                    scheduler.process(aPtrs.data(), bPtrs.data(), outPtrs.data(), channels, samples);
        }
    }
}

TEST_CASE("an A side narrower than the processed count broadcasts too", "[scheduler]")
{
    /* One A source feeding two processed channels. `resolve` cannot produce this shape — it caps the
       output count at max(chA, chB), so (1, 0, 2) is `countMismatch` — which means the shape reaches
       the scheduler only as a map somebody built, and the resolver has to honour the map it is handed
       rather than re-derive the rule from the two counts (INDEX §3 rule 2). That is also what item 03
       or 06 would hand it if the rule ever allowed fewer A sources than outputs, so the reading is
       tested now rather than assumed. */
    const int hop = 64;
    const std::vector<int> blocks{100, 0, 156, 64};
    const int total = std::accumulate(blocks.begin(), blocks.end(), 0);

    engine::SchedulerConfig config;
    config.engine.sampleRate = 48000.0;
    config.engine.hop = hop;
    config.engine.maxChannels = 2;
    config.engine.latencySamples = hop;
    config.channels = 1;
    config.sidechainChannels = 0;
    config.maxHostBlock = 2048;
    config.map.channels = 2;
    config.map.a = {0, 0};
    config.map.b.fill(engine::kSilentSource);
    REQUIRE(engine::ChannelMap::resolve(1, 0, 2).status == engine::ChannelMapStatus::countMismatch);

    TapEngine engine;
    engine::HopScheduler scheduler(engine);
    scheduler.prepare(config);

    const auto a = makeStream(1, total, 0.0f);
    const auto run = runWithWidths(scheduler, 1, 0, 2, a, {}, blocks);

    expectEverySampleWritten(run, 2);
    for (int fired = 0; fired < hopsFired(engine, 2); ++fired)
        CHECK(engine.aPointers[static_cast<std::size_t>(fired * 2)]
              == engine.aPointers[static_cast<std::size_t>(fired * 2 + 1)]);

    int streamStart = 0;
    for (std::size_t callback = 0; callback < run.delivered.size(); ++callback)
    {
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto& samples = channelSamples(run, 2, slot, static_cast<int>(callback));
            for (int i = 0; i < static_cast<int>(samples.size()); ++i)
            {
                const int streamIndex = streamStart + i;
                const float expected =
                    streamIndex < hop ? 0.0f : pattern(0, streamIndex - hop);
                INFO("callback " << callback << " slot " << slot << " sample " << i);
                CHECK(samples[static_cast<std::size_t>(i)] == expected);
            }
        }
        streamStart += run.delivered[callback];
    }
}
