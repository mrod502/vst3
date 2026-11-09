#include <DcBlocker.hpp>
#include <Engine.hpp>
#include <HopScheduler.hpp>
#include <LevelDetector.hpp>
#include <LevelNormalizer.hpp>
#include <MixEngine.hpp>
#include <SoftClipper.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <random>
#include <vector>

namespace
{
/** processHop must not allocate. Replacing global operator new is allowed once per program, which
    is why this file is its own test binary; the shape follows HopSchedulerTests.cpp. */
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

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kHop = 128;
constexpr int kHostBlock = 32;

/** Blend 20 ms, output gain 20 ms and bypass 10 ms at the rate every case below prepares with, so
    the landing indices the assertions use are derived from the millisecond figures rather than
    repeated as literals. */
constexpr int rampSamples(double milliseconds, double sampleRate) noexcept
{
    return static_cast<int>(milliseconds * sampleRate * 0.001 + 0.5);
}

constexpr int kBlendRamp = rampSamples(engine::MixEngine::blendRampMs, kSampleRate);
constexpr int kBypassRamp = rampSamples(engine::MixEngine::bypassRampMs, kSampleRate);
static_assert(kBlendRamp == 960 && kBypassRamp == 480,
              "ramp lengths drifted from the millisecond figures D-011 records");

using Streams = std::vector<std::vector<float>>;

float sampleOf(int channel, int index)
{
    return static_cast<float>(index * 4 + channel) - 1000.0f;
}

Streams makeInputs(int channels, int samples, int offset)
{
    Streams data(static_cast<std::size_t>(channels),
                 std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] =
                sampleOf(channel, i + offset);
    return data;
}

Streams constantStream(int channels, int samples, float value)
{
    return Streams(static_cast<std::size_t>(channels),
                   std::vector<float>(static_cast<std::size_t>(samples), value));
}

Streams zerosLike(const Streams& other)
{
    return Streams(other.size(),
                   std::vector<float>(other.empty() ? 0 : other[0].size(), 0.0f));
}

/** Content with real high-frequency energy: the second difference of a static render is then a
    level a click has to beat rather than the curvature of a tone nobody would call smooth. */
Streams contentStream(int channels, int samples, double phase)
{
    Streams data(static_cast<std::size_t>(channels),
                 std::vector<float>(static_cast<std::size_t>(samples)));
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
        {
            const double n = static_cast<double>(i + channel * 61);
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] =
                static_cast<float>(0.5 * std::sin(2.0 * M_PI * 0.004 * n + phase)
                                   + 0.3 * std::sin(2.0 * M_PI * 0.02 * n + 2.4 * phase));
        }
    return data;
}

/** Power-of-two coefficients only, so every expectation is exactly representable and the assertions
    can compare floats with ==.

    It depends on the call index, so a scratch slot read without being rewritten fails instead of
    looking plausible, and on the channel, so two channels sharing one slot fails too. */
float wetSample(const float* a, const float* b, int call, int channel, int i)
{
    return 0.5f * a[i] + 0.25f * b[i] + 0.0625f * static_cast<float>(channel)
           + 0.03125f * static_cast<float>(i) + 0.015625f * static_cast<float>(call % 5);
}

class StubEngine final : public engine::Engine
{
public:
    void prepare(const engine::EngineConfig& configToUse) override
    {
        forwarded = configToUse;
        ++prepareCalls;
    }

    void reset() override
    {
        ++resetCalls;
        hopCalls = 0;
    }

    int latencySamples() const override { return declaredLatency; }

    int wetDelaySamples() const override { return wetDelay; }

    void processHop(const float* const* a, const float* const* b, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < forwarded.hop; ++i)
                out[channel][i] = wetSample(a[channel], b[channel], hopCalls, channel, i);
        ++hopCalls;
    }

    int declaredLatency = 96;
    int wetDelay = 8 * kHop;
    engine::EngineConfig forwarded;
    int prepareCalls = 0;
    int resetCalls = 0;
    int hopCalls = 0;
};

class SilentEngine final : public engine::Engine
{
public:
    void prepare(const engine::EngineConfig& configToUse) override { config = configToUse; }
    void reset() override {}
    int latencySamples() const override { return config.latencySamples; }
    int wetDelaySamples() const override { return config.grainLength; }

    void processHop(const float* const*, const float* const*, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < config.hop; ++i)
                out[channel][i] = 0.0f;
    }

private:
    engine::EngineConfig config;
};

/** A wet stream that is smooth across hop boundaries and distinct from both dry buses, so the only
    steps a render can contain come from the mix coefficients themselves. */
class SmoothWetEngine final : public engine::Engine
{
public:
    void prepare(const engine::EngineConfig& configToUse) override
    {
        config = configToUse;
        position = 0;
    }

    void reset() override { position = 0; }

    int latencySamples() const override { return config.latencySamples; }
    int wetDelaySamples() const override { return 8 * config.hop; }

    void processHop(const float* const*, const float* const*, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < config.hop; ++i)
            {
                const double n = static_cast<double>(position + i + channel * 37);
                out[channel][i] =
                    static_cast<float>(0.4 * std::sin(2.0 * M_PI * 0.006 * n)
                                       + 0.3 * std::sin(2.0 * M_PI * 0.03 * n + 0.7)
                                       + 0.15 * std::sin(2.0 * M_PI * 0.07 * n + 1.9));
            }
        position += config.hop;
    }

private:
    engine::EngineConfig config;
    int position = 0;
};

/** The known-bad for the click metric: the same law applied once per hop with no ramp. A fixture
    rather than a knob on the shipped class, and pinned to the real law by the case that renders the
    two of them bit-identically while the parameters are static. */
class SteppedMixEngine final : public engine::Engine
{
public:
    explicit SteppedMixEngine(std::unique_ptr<engine::Engine> innerToUse) noexcept
        : inner(std::move(innerToUse))
    {
    }

    void prepare(const engine::EngineConfig& configToUse) override
    {
        config = configToUse;
        inner->prepare(configToUse);
        wet.assign(static_cast<std::size_t>(config.maxChannels),
                   std::vector<float>(static_cast<std::size_t>(config.hop), 0.0f));
        wetPtrs.assign(static_cast<std::size_t>(config.maxChannels), nullptr);
        for (std::size_t channel = 0; channel < wetPtrs.size(); ++channel)
            wetPtrs[channel] = wet[channel].data();
    }

    void reset() override { inner->reset(); }

    int latencySamples() const override { return inner->latencySamples(); }
    int wetDelaySamples() const override { return inner->wetDelaySamples(); }

    void setBlendTarget(float dialPosition) noexcept { blend = dialPosition; }

    void setOutputGainTarget(float decibels) noexcept
    {
        gain = static_cast<float>(std::pow(10.0, static_cast<double>(decibels) / 20.0));
    }

    void setBypassTarget(bool bypassed) noexcept { beta = bypassed ? 0.0f : 1.0f; }

    void processHop(const float* const* a, const float* const* b, float* const* out,
                    int numChannels) override
    {
        inner->processHop(a, b, wetPtrs.data(), numChannels);

        const engine::MixWeights weights = engine::mixLawWeights(blend);
        const float dryToA = blend < 0.0f ? 1.0f : 0.0f;
        const float coefA = gain * (beta * weights.dry * dryToA + (1.0f - beta));
        const float coefB = gain * (beta * weights.dry * (1.0f - dryToA));
        const float coefWet = gain * (beta * weights.wet);

        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < config.hop; ++i)
                out[channel][i] = coefA * a[channel][i] + coefB * b[channel][i]
                                  + coefWet * wet[static_cast<std::size_t>(channel)]
                                                    [static_cast<std::size_t>(i)];
    }

private:
    std::unique_ptr<engine::Engine> inner;
    engine::EngineConfig config;
    float blend = 0.0f;
    float gain = 1.0f;
    float beta = 1.0f;
    std::vector<std::vector<float>> wet;
    std::vector<float*> wetPtrs;
};

engine::EngineConfig makeConfig(int channels, int latency, double sampleRate = kSampleRate)
{
    engine::EngineConfig config;
    config.sampleRate = sampleRate;
    config.hop = kHop;
    config.maxChannels = channels;
    config.latencySamples = latency;
    config.grainLength = 8 * kHop;
    return config;
}

struct Pointers
{
    std::vector<const float*> a;
    std::vector<const float*> b;
    std::vector<float*> out;
};

Pointers pointersFor(const Streams& a, const Streams& b, Streams& out)
{
    Pointers p;
    for (std::size_t channel = 0; channel < a.size(); ++channel)
    {
        p.a.push_back(a[channel].data());
        p.b.push_back(b[channel].data());
        p.out.push_back(out[channel].data());
    }
    return p;
}

Streams hopWindow(const Streams& stream, int position, int hop)
{
    Streams window(stream.size(), std::vector<float>(static_cast<std::size_t>(hop), 0.0f));
    for (std::size_t channel = 0; channel < stream.size(); ++channel)
        for (int i = 0; i < hop; ++i)
            window[channel][static_cast<std::size_t>(i)] =
                stream[channel][static_cast<std::size_t>(position + i)];
    return window;
}

void storeWindow(const Streams& window, Streams& stream, int position, int hop)
{
    for (std::size_t channel = 0; channel < window.size(); ++channel)
        for (int i = 0; i < hop; ++i)
            stream[channel][static_cast<std::size_t>(position + i)] =
                window[channel][static_cast<std::size_t>(i)];
}

/** Runs `hops` hops with dry A at exactly 1.0, dry B at zero and a silent wet path. Under those,
    out = coefA * 1 + coefB * 0 + coefWet * 0 = coefA, so the returned stream is the dry coefficient
    trajectory itself and a ramp's landing is read off it rather than inferred. */
Streams probeTrajectory(engine::MixEngine& engine, int hops)
{
    const Streams a = constantStream(1, kHop, 1.0f);
    const Streams b = constantStream(1, kHop, 0.0f);
    Streams trajectory(1, std::vector<float>(static_cast<std::size_t>(hops) * kHop, 0.0f));
    Streams out = zerosLike(a);
    const Pointers p = pointersFor(a, b, out);

    for (int hop = 0; hop < hops; ++hop)
    {
        engine.processHop(p.a.data(), p.b.data(), p.out.data(), 1);
        storeWindow(out, trajectory, hop * kHop, kHop);
    }
    return trajectory;
}

/** Drives one mix stage over `a[0].size()` samples in callbacks of `hostBlock`, poking the three
    targets at each callback's absolute start position — the way the shell reads its parameters. */
template <typename Mix, typename Blend, typename Gain, typename Bypass>
Streams renderMix(Mix& mix, const Streams& a, const Streams& b, int hostBlock,
                  const Blend& blendAt, const Gain& gainDbAt, const Bypass& bypassAt)
{
    const int channels = static_cast<int>(a.size());
    const int total = static_cast<int>(a.empty() ? 0 : a[0].size());

    engine::SchedulerConfig schedulerConfig;
    schedulerConfig.engine = makeConfig(channels, kHop);
    schedulerConfig.channels = channels;
    schedulerConfig.sidechainChannels = channels;
    schedulerConfig.maxHostBlock = hostBlock;
    schedulerConfig.map = engine::ChannelMap::resolve(channels, channels, channels).map;

    /* Poked before prepare so the coefficients snap onto the curve's starting value: what the ramp
       legs measure is the automation, not a move away from the class's own defaults. */
    mix.setOutputGainTarget(gainDbAt(0));
    mix.setBypassTarget(bypassAt(0));
    mix.setBlendTarget(blendAt(0));

    engine::HopScheduler scheduler(mix);
    scheduler.prepare(schedulerConfig);

    Streams out(static_cast<std::size_t>(channels),
                std::vector<float>(static_cast<std::size_t>(total), 0.0f));

    for (int position = 0; position < total; position += hostBlock)
    {
        mix.setOutputGainTarget(gainDbAt(position));
        mix.setBypassTarget(bypassAt(position));
        mix.setBlendTarget(blendAt(position));

        const Streams aWindow = hopWindow(a, position, hostBlock);
        const Streams bWindow = hopWindow(b, position, hostBlock);
        Streams outWindow = zerosLike(aWindow);
        const Pointers p = pointersFor(aWindow, bWindow, outWindow);
        scheduler.process(p.a.data(), p.b.data(), p.out.data(), channels, hostBlock);
        storeWindow(outWindow, out, position, hostBlock);
    }
    return out;
}

/** d2(x) = max |x[n] - 2x[n-1] + x[n-2]| over n >= `startFrom`, reported as a ratio against the
    same content rendered with the parameters static. The click prototype M3 §7 allows until M5 §4.7
    defines the metric.

    `startFrom` skips the scheduler's latency pre-fill: the step from that silence onto the first real
    sample is the same in every render, so leaving it in the scan pins every ratio to 1.0 and the
    metric measures the harness instead of the automation. */
double maxSecondDifference(const Streams& stream, int startFrom = 0)
{
    double worst = 0.0;
    for (const auto& channel : stream)
        for (std::size_t i = std::max<std::size_t>(2, static_cast<std::size_t>(startFrom));
             i < channel.size(); ++i)
            worst = std::max(
                worst, std::abs(static_cast<double>(channel[i])
                                - 2.0 * static_cast<double>(channel[i - 1])
                                + static_cast<double>(channel[i - 2])));
    return worst;
}

double peakOf(const Streams& stream)
{
    double peak = 0.0;
    for (const auto& channel : stream)
        for (const float sample : channel)
            peak = std::max(peak, static_cast<double>(std::abs(sample)));
    return peak;
}

/** The first sample whose value is bit-identical to `target`; -1 if the trajectory never lands. */
int landingIndex(const Streams& trajectory, float target)
{
    for (std::size_t i = 0; i < trajectory[0].size(); ++i)
        if (trajectory[0][i] == target)
            return static_cast<int>(i);
    return -1;
}

bool monotoneNonDecreasing(const Streams& trajectory)
{
    for (const auto& channel : trajectory)
        for (std::size_t i = 1; i < channel.size(); ++i)
            if (channel[i] < channel[i - 1])
                return false;
    return true;
}

bool monotoneNonIncreasing(const Streams& trajectory)
{
    for (const auto& channel : trajectory)
        for (std::size_t i = 1; i < channel.size(); ++i)
            if (channel[i] > channel[i - 1])
                return false;
    return true;
}
}

TEST_CASE("the law's weight table holds over every dial position the parameter can express")
{
    /* The dial's own quantum, 1/128 — EntangleAudioProcessor::blendStep, restated here because the
       engine library may not read a JUCE-side header. It is dyadic for a reason that this sweep is
       the guard for: the 0.01 step this replaced could not express its own default or its own
       topmost position (JUCE's snap returned -2.23517418e-08 and 0.99999994 for them), so a table
       walked on decimals never reaches the rows that have to be exact. */
    constexpr float kDialStep = 1.0f / 128.0f;
    constexpr int kSteps = 257;

    CHECK(-1.0f + static_cast<float>(kSteps - 1) * kDialStep == 1.0f);
    CHECK(-1.0f + static_cast<float>((kSteps - 1) / 2) * kDialStep == 0.0f);

    for (int index = 0; index < kSteps; ++index)
    {
        const float p = -1.0f + static_cast<float>(index) * kDialStep;
        const engine::MixWeights w = engine::mixLawWeights(p);

        INFO("p=" << p);
        CHECK(w.dry >= 0.0f);
        CHECK(w.dry <= 1.0f);
        CHECK(w.wet >= 0.0f);
        CHECK(w.wet <= 1.0f);
        CHECK(std::abs(static_cast<double>(w.dry) * w.dry + static_cast<double>(w.wet) * w.wet - 1.0)
              <= 1e-6);
    }

    /* And on that grid the endpoints are reachable, which is what criteria 1 and 2 ride on: the
       first and last position the dial can hold are the law's snapped rows, not values that come
       back a few ULP inside them. */
    CHECK(engine::mixLawWeights(-1.0f + 0.0f * kDialStep).wet == 0.0f);
    CHECK(engine::mixLawWeights(-1.0f + static_cast<float>(kSteps - 1) * kDialStep).wet == 0.0f);

    ///< The three snapped positions hold exact values, which is the whole reason the snap exists.
    CHECK(engine::mixLawWeights(0.0f).dry == 0.0f);
    CHECK(engine::mixLawWeights(0.0f).wet == 1.0f);
    CHECK(engine::mixLawWeights(-1.0f).dry == 1.0f);
    CHECK(engine::mixLawWeights(-1.0f).wet == 0.0f);
    CHECK(engine::mixLawWeights(1.0f).dry == 1.0f);
    CHECK(engine::mixLawWeights(1.0f).wet == 0.0f);

    /* What the snap replaces: cosf(pi/2) is not zero but -4.37e-08, so a literal law leaves that
       fraction of the wet stream in the p = -1 sum — 8.8e-08 at the 2.012 correlated peak of D-009
       and 7.3e-07 at the 16.77 full-scale one, against a half-ULP of 5.96e-08 near unity. */
    const float literalWetWeightAtFullTravel = std::cos(1.0f * static_cast<float>(M_PI_2));
    CHECK(literalWetWeightAtFullTravel < 0.0f);
    CHECK(std::abs(literalWetWeightAtFullTravel) > 1e-8f);
}

TEST_CASE("the settled dial positions hand back dry A, the wet stream and dry B, bit-exactly")
{
    const int channels = 2;
    const Streams a = makeInputs(channels, kHop, 0);
    const Streams b = makeInputs(channels, kHop, 5000);

    struct Dial
    {
        float position;
        const char* what;
    };

    for (const Dial dial : {Dial{-1.0f, "p = -1 is dry A"}, Dial{0.0f, "p = 0 is the wet stream"},
                            Dial{1.0f, "p = +1 is dry B"}})
    {
        auto stub = std::make_unique<StubEngine>();
        StubEngine* observed = stub.get();
        engine::MixEngine engine(std::move(stub));
        engine.setBlendTarget(dial.position);
        engine.prepare(makeConfig(channels, 96));

        Streams out = zerosLike(a);
        const Pointers p = pointersFor(a, b, out);

        for (int call = 0; call < 6; ++call)
        {
            engine.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
            for (int channel = 0; channel < channels; ++channel)
                for (int i = 0; i < kHop; ++i)
                {
                    const float wet = wetSample(a[static_cast<std::size_t>(channel)].data(),
                                                b[static_cast<std::size_t>(channel)].data(), call,
                                                channel, i);
                    const float want =
                        dial.position < 0.0f
                            ? a[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]
                            : (dial.position > 0.0f
                                   ? b[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]
                                   : wet);
                    INFO(dial.what << " call=" << call << " ch=" << channel << " i=" << i);
                    CHECK(out[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]
                          == want);
                }
        }
        CHECK(observed->hopCalls == 6);
    }
}

TEST_CASE("with a silent wet path the stage outputs exactly the dry bus the dial selects")
{
    const int channels = 2;
    const Streams content = makeInputs(channels, kHop, 0);
    const Streams silence = constantStream(channels, kHop, 0.0f);

    struct Case
    {
        const Streams* a;
        const Streams* b;
        const char* what;
    };

    for (const Case c : {Case{&silence, &content, "A silent"},
                         Case{&content, &silence, "B silent"},
                         Case{&silence, &silence, "both silent"}})
    {
        for (const float dial : {-1.0f, -0.75f, 0.0f, 0.5f, 1.0f})
        {
            engine::MixEngine engine(std::make_unique<SilentEngine>());
            engine.setBlendTarget(dial);
            engine.prepare(makeConfig(channels, 96));

            Streams out = zerosLike(*c.a);
            const Pointers p = pointersFor(*c.a, *c.b, out);
            engine.processHop(p.a.data(), p.b.data(), p.out.data(), channels);

            /* The wet hop of a silent pair is silence by the inner engine's own contract, so what is
               left is the selected dry bus at its law weight — exact zeros where that bus is silent,
               never a noise floor and never a NaN. One multiply, so the expectation is bit-exact. */
            const engine::MixWeights weights = engine::mixLawWeights(dial);
            for (int channel = 0; channel < channels; ++channel)
                for (int i = 0; i < kHop; ++i)
                {
                    const float dry = dial < 0.0f
                                          ? (*c.a)[static_cast<std::size_t>(channel)]
                                                 [static_cast<std::size_t>(i)]
                                          : (dial > 0.0f
                                                 ? (*c.b)[static_cast<std::size_t>(channel)]
                                                        [static_cast<std::size_t>(i)]
                                                 : 0.0f);
                    INFO(c.what << " dial=" << dial << " ch=" << channel << " i=" << i);
                    CHECK(std::isfinite(out[static_cast<std::size_t>(channel)]
                                             [static_cast<std::size_t>(i)]));
                    CHECK(out[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]
                          == weights.dry * dry);
                }
        }
    }
}

TEST_CASE("prepare, reset and both delay figures forward unchanged, and both snap the ramps")
{
    auto stub = std::make_unique<StubEngine>();
    StubEngine* observed = stub.get();
    engine::MixEngine engine(std::move(stub));
    observed->declaredLatency = 96;
    observed->wetDelay = 8 * kHop;

    const engine::EngineConfig config = makeConfig(2, 96);
    engine.setBlendTarget(-1.0f);
    engine.prepare(config);

    CHECK(observed->prepareCalls == 1);
    CHECK(observed->forwarded.sampleRate == config.sampleRate);
    CHECK(observed->forwarded.hop == config.hop);
    CHECK(observed->forwarded.maxChannels == config.maxChannels);
    CHECK(observed->forwarded.latencySamples == config.latencySamples);
    CHECK(observed->forwarded.grainLength == config.grainLength);

    ///< R is the declaration, L the physical wet delay: neither may be conflated on the way through.
    CHECK(engine.latencySamples() == 96);
    CHECK(engine.wetDelaySamples() == 8 * kHop);

    /* prepare snaps onto the target rather than ramping from the class's own defaults, which is what
       lets a fresh transport start settled; the probe stream is the dry coefficient, so a value of
       1.0 on every sample says the coefficient never left p = -1. */
    auto probe = probeTrajectory(engine, 2);
    for (std::size_t i = 0; i < probe[0].size(); ++i)
    {
        INFO("after prepare i=" << i);
        CHECK(probe[0][i] == 1.0f);
    }

    observed->resetCalls = 0;
    engine.setBlendTarget(1.0f); ///< starts a ramp away from -1
    engine.reset();              ///< the transport drops the buffered stream, and the ramp with it
    CHECK(observed->resetCalls == 1);
    CHECK(engine.latencySamples() == 96);

    /* Snapped onto p = +1, where dry A carries nothing at all. A ramp would show A still
       contributing here, and the probe's dry B is silence, so 0.0 on every sample is the snap. */
    probe = probeTrajectory(engine, 2);
    for (std::size_t i = 0; i < probe[0].size(); ++i)
    {
        INFO("snapped onto +1 after reset i=" << i);
        CHECK(probe[0][i] == 0.0f);
    }

    engine.setBlendTarget(-1.0f);
    engine.reset();
    probe = probeTrajectory(engine, 2);
    for (std::size_t i = 0; i < probe[0].size(); ++i)
    {
        INFO("snapped back onto -1 i=" << i);
        CHECK(probe[0][i] == 1.0f);
    }
}

TEST_CASE("a ramp lands on its target bit-exactly, at a sample count from its own milliseconds")
{
    /* Blend, 0 -> -1 with a silent wet path: the probe stream is w_dry, so the landing is the first
       sample that is exactly 1.0 and the ramp's length is the millisecond figure in samples. */
    auto blendLanding = [](double sampleRate) {
        engine::MixEngine engine(std::make_unique<SilentEngine>());
        engine.setBlendTarget(0.0f);
        engine.prepare(makeConfig(1, 96, sampleRate));
        engine.setBlendTarget(-1.0f);

        const int expected = rampSamples(engine::MixEngine::blendRampMs, sampleRate);
        const int hops = (expected + kHop - 1) / kHop + 1;
        const auto trajectory = probeTrajectory(engine, hops);

        INFO("rate=" << sampleRate << " landing=" << landingIndex(trajectory, 1.0f) << " expected "
                     << expected - 1);
        return landingIndex(trajectory, 1.0f);
    };

    CHECK(blendLanding(48000.0) == kBlendRamp - 1);
    CHECK(blendLanding(96000.0) == 2 * kBlendRamp - 1);
    CHECK(blendLanding(44100.0) == rampSamples(engine::MixEngine::blendRampMs, 44100.0) - 1);

    {
        engine::MixEngine engine(std::make_unique<SilentEngine>());
        engine.setBlendTarget(0.0f);
        engine.prepare(makeConfig(1, 96));
        engine.setBlendTarget(-1.0f);
        const auto trajectory = probeTrajectory(engine, kBlendRamp / kHop + 1);
        CHECK(monotoneNonDecreasing(trajectory));
        CHECK(trajectory[0][static_cast<std::size_t>(kBlendRamp - 2)] < 1.0f);
    }

    /* Output gain, 0 dB -> -60 dB with the dial settled at -1, where the probe stream is the linear
       gain. Monotone all the way down, and it ends on the target rather than near it. */
    {
        const float wantGain = static_cast<float>(std::pow(10.0, -60.0 / 20.0));
        engine::MixEngine engine(std::make_unique<SilentEngine>());
        engine.setBlendTarget(-1.0f);
        engine.setOutputGainTarget(0.0f);
        engine.prepare(makeConfig(1, 96));
        engine.setOutputGainTarget(-60.0f);

        const auto trajectory = probeTrajectory(engine, kBlendRamp / kHop + 1);
        INFO("gain ramp landing=" << landingIndex(trajectory, wantGain));
        CHECK(landingIndex(trajectory, wantGain) == kBlendRamp - 1);
        CHECK(trajectory[0][0] < 1.0f);
        CHECK(monotoneNonIncreasing(trajectory));
    }

    /* Bypass, 10 ms, measured at the law's centre where the probe stream is 1 - beta: settled
       bypassed is exactly 1.0, which is the wet and dry-B terms sitting at exactly 0.0f rather than
       asymptotically near it. */
    {
        engine::MixEngine engine(std::make_unique<SilentEngine>());
        engine.setBlendTarget(0.0f);
        engine.prepare(makeConfig(1, 96));
        engine.setBypassTarget(true);

        const auto trajectory = probeTrajectory(engine, kBypassRamp / kHop + 1);
        INFO("bypass ramp landing=" << landingIndex(trajectory, 1.0f));
        CHECK(landingIndex(trajectory, 1.0f) == kBypassRamp - 1);
        CHECK(trajectory[0][0] < 0.01f);
        CHECK(monotoneNonDecreasing(trajectory));
    }
}

TEST_CASE("output gain: 0 dB is bit-exact unity, the rest matches 10^(dB/20), all of it monotone")
{
    const int channels = 1;
    const Streams a = makeInputs(channels, kHop, 0);
    const Streams zeros = constantStream(channels, kHop, 0.0f);

    double previousMeasured = -1.0;
    int strictlyGreater = 0;

    for (double db = -60.0; db <= 12.0001; db += 0.5)
    {
        engine::MixEngine engine(std::make_unique<SilentEngine>());
        engine.setBlendTarget(-1.0f);
        engine.setOutputGainTarget(static_cast<float>(db));
        engine.prepare(makeConfig(channels, 96));

        Streams out = zerosLike(a);
        const Pointers p = pointersFor(a, zeros, out);
        engine.processHop(p.a.data(), p.b.data(), p.out.data(), channels);

        const double want = std::pow(10.0, db / 20.0);
        for (int i = 0; i < kHop; ++i)
        {
            const double got = out[0][static_cast<std::size_t>(i)];
            const double expected = want * static_cast<double>(a[0][static_cast<std::size_t>(i)]);
            INFO("dB=" << db << " i=" << i << " got " << got << " want " << expected);
            CHECK(std::isfinite(got));
            CHECK(std::abs(got - expected) <= 1e-6 * std::abs(expected) + 1e-12);
        }

        const double measured = peakOf(out) / peakOf(a);
        INFO("sweep dB=" << db << " measured gain " << measured);
        CHECK(measured >= previousMeasured);
        if (measured > previousMeasured)
            ++strictlyGreater;
        previousMeasured = measured;
    }

    CHECK(strictlyGreater > 100); ///< not a staircase that never moves
    CHECK(std::abs(previousMeasured - std::pow(10.0, 12.0 / 20.0)) <= 1e-6 * previousMeasured);

    engine::MixEngine unity(std::make_unique<SilentEngine>());
    unity.setBlendTarget(-1.0f);
    unity.prepare(makeConfig(channels, 96)); ///< no gain parameter touched anywhere
    Streams out = zerosLike(a);
    const Pointers p = pointersFor(a, zeros, out);
    unity.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
    for (int i = 0; i < kHop; ++i)
    {
        INFO("0 dB i=" << i);
        CHECK(out[0][static_cast<std::size_t>(i)] == a[0][static_cast<std::size_t>(i)]);
    }
}

/** A wet path that depends only on the samples it is handed — no call counter, no internal position
    — so feeding the same input window twice renders the same numbers. That is what lets the bypass
    case below re-render one span after the cycle and compare it bit for bit with the render from
    before it. */
class StatelessWetEngine final : public engine::Engine
{
public:
    void prepare(const engine::EngineConfig& configToUse) override { config = configToUse; }
    void reset() override {}
    int latencySamples() const override { return config.latencySamples; }
    int wetDelaySamples() const override { return 8 * config.hop; }

    void processHop(const float* const* a, const float* const* b, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < config.hop; ++i)
                out[channel][i] = 0.5f * a[channel][i] + 0.25f * b[channel][i]
                                  + 0.03125f * static_cast<float>(i);
    }

private:
    engine::EngineConfig config;
};

TEST_CASE("bypass is a crossfade onto dry A: bit-exact when settled, and the dial survives it")
{
    const int channels = 2;
    const int blockHops = 4;
    const int blockSamples = blockHops * kHop;
    const Streams a = makeInputs(channels, blockSamples, 0);
    const Streams b = makeInputs(channels, blockSamples, 9000);

    engine::MixEngine engine(std::make_unique<StatelessWetEngine>());
    engine.setBlendTarget(-0.4f);
    engine.prepare(makeConfig(channels, 96));

    auto renderBlock = [&](int hops) {
        Streams out(static_cast<std::size_t>(channels),
                    std::vector<float>(static_cast<std::size_t>(hops) * kHop, 0.0f));
        int feedPosition = 0;
        for (int hop = 0; hop < hops; ++hop)
        {
            const Streams aWindow = hopWindow(a, feedPosition, kHop);
            const Streams bWindow = hopWindow(b, feedPosition, kHop);
            Streams outWindow = zerosLike(aWindow);
            const Pointers p = pointersFor(aWindow, bWindow, outWindow);
            engine.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
            storeWindow(outWindow, out, hop * kHop, kHop);
            feedPosition += kHop;
            if (feedPosition == blockSamples)
                feedPosition = 0; ///< the span wraps, so every block is the same input
        }
        return out;
    };

    const auto inEffect = renderBlock(blockHops);

    engine.setBypassTarget(true);
    const auto fading = renderBlock(blockHops); ///< 4 hops: the fade lands partway through
    const auto bypassed = renderBlock(blockHops);

    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < blockSamples; ++i)
        {
            INFO("bypassed ch=" << channel << " i=" << i);
            ///< The wet and dry-B contributions reach exactly 0.0f, not asymptotically.
            CHECK(bypassed[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]
                  == a[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]);
        }

    bool fadingIsMidFade = false;
    for (std::size_t channel = 0; channel < fading.size() && !fadingIsMidFade; ++channel)
        for (std::size_t i = 0; i < fading[channel].size() && !fadingIsMidFade; ++i)
            fadingIsMidFade = fading[channel][i] != bypassed[channel][i];
    CHECK(fadingIsMidFade);

    engine.setBypassTarget(false);
    renderBlock(blockHops); ///< unbypass takes the same 10 ms

    const auto returned = renderBlock(blockHops);
    /* Nothing in a bypass cycle moves the dial, and the wet path here depends only on its input, so
       re-rendering the same span after the cycle comes back to the same numbers bit for bit. */
    for (std::size_t channel = 0; channel < returned.size(); ++channel)
        for (std::size_t i = 0; i < returned[channel].size(); ++i)
        {
            INFO("after the bypass cycle ch=" << channel << " i=" << i);
            CHECK(returned[channel][i] == inEffect[channel][i]);
        }
}

/** Automation that is constant over a whole span of samples, so a callback boundary of any size
    that divides the span observes the same value at the same absolute position. */
struct Segment
{
    float blend;
    float gainDb;
    bool bypass;
};

constexpr int kSegmentSamples = 4 * kHop;

constexpr std::array<Segment, 16> kSegments{
    Segment{-1.0f, 0.0f, false},  Segment{-0.75f, 0.0f, false}, Segment{-0.5f, 0.0f, false},
    Segment{-0.25f, 0.0f, false}, Segment{0.0f, 0.0f, false},   Segment{0.25f, 0.0f, false},
    Segment{0.5f, -6.0f, false},  Segment{0.75f, -12.0f, false},
    Segment{1.0f, -12.0f, false}, Segment{0.5f, -12.0f, true},
    Segment{0.0f, 0.0f, true},    Segment{-0.5f, 0.0f, true},
    Segment{-1.0f, 6.0f, false},  Segment{-0.5f, 12.0f, false},
    Segment{0.0f, 0.0f, false},   Segment{1.0f, 0.0f, false}};

TEST_CASE("the automation ramp is a function of sample position, not of the host callback")
{
    const int channels = 2;
    const int total = static_cast<int>(kSegments.size()) * kSegmentSamples;
    const Streams a = contentStream(channels, total, 0.3);
    const Streams b = contentStream(channels, total, 1.1);

    auto segmentAt = [](int position) -> const Segment& {
        return kSegments[static_cast<std::size_t>(position / kSegmentSamples)];
    };
    auto blendAt = [&](int position) { return segmentAt(position).blend; };
    auto gainAt = [&](int position) { return segmentAt(position).gainDb; };
    auto bypassAt = [&](int position) { return segmentAt(position).bypass; };

    /* What differs between the passes is how many samples each callback carries — 1, 2 and 4 hops —
       which is exactly what a per-callback time constant cannot survive: the one-pole this replaced
       ran 49.5 callbacks to 63 % of the way, i.e. 6.3 ms at hostBlock 128 and 25.4 ms at 512. */
    engine::MixEngine byHop(std::make_unique<SmoothWetEngine>());
    const auto first = renderMix(byHop, a, b, kHop, blendAt, gainAt, bypassAt);

    engine::MixEngine byTwoHops(std::make_unique<SmoothWetEngine>());
    const auto second = renderMix(byTwoHops, a, b, 2 * kHop, blendAt, gainAt, bypassAt);

    engine::MixEngine byFourHops(std::make_unique<SmoothWetEngine>());
    const auto third = renderMix(byFourHops, a, b, 4 * kHop, blendAt, gainAt, bypassAt);

    for (std::size_t channel = 0; channel < first.size(); ++channel)
        for (std::size_t i = 0; i < first[channel].size(); ++i)
        {
            INFO("ch=" << channel << " n" << i);
            CHECK(first[channel][i] == second[channel][i]);
            CHECK(first[channel][i] == third[channel][i]);
        }

    /* And the equality above is not three renders of one static coefficient set: the stepped
       known-bad, fed the same curve, produces a different stream. */
    SteppedMixEngine stepped(std::make_unique<SmoothWetEngine>());
    const auto steppedOut = renderMix(stepped, a, b, kHop, blendAt, gainAt, bypassAt);
    bool differs = false;
    for (std::size_t channel = 0; channel < first.size() && !differs; ++channel)
        for (std::size_t i = 0; i < first[channel].size() && !differs; ++i)
            differs = first[channel][i] != steppedOut[channel][i];
    CHECK(differs);
}

TEST_CASE("the stepped known-bad is the same law as the shipped one while the parameters are static")
{
    const int channels = 2;
    const int total = 4 * kHop;
    const Streams a = contentStream(channels, total, 0.0);
    const Streams b = contentStream(channels, total, 2.0);

    for (const float dial : {-1.0f, -0.55f, 0.0f, 0.31f, 1.0f})
        for (const float db : {-60.0f, -7.5f, 0.0f, 12.0f})
            for (const bool bypassed : {false, true})
            {
                engine::MixEngine shipped(std::make_unique<SmoothWetEngine>());
                const auto ramped = renderMix(shipped, a, b, kHop,
                                              [dial](int) { return dial; },
                                              [db](int) { return db; },
                                              [bypassed](int) { return bypassed; });

                SteppedMixEngine stepped(std::make_unique<SmoothWetEngine>());
                const auto steppedOut = renderMix(stepped, a, b, kHop,
                                                  [dial](int) { return dial; },
                                                  [db](int) { return db; },
                                                  [bypassed](int) { return bypassed; });

                for (std::size_t channel = 0; channel < ramped.size(); ++channel)
                    for (std::size_t i = static_cast<std::size_t>(kHop); i < ramped[channel].size();
                         ++i)
                    {
                        INFO("dial=" << dial << " dB=" << db << " bypassed=" << bypassed
                                     << " ch=" << channel << " n" << i);
                        CHECK(ramped[channel][i] == steppedOut[channel][i]);
                    }
            }
}

TEST_CASE("the click metric discriminates the ramped coefficients from the stepped known-bad")
{
    /* Measured 2026-10-29 at 48 kHz with hop 128 and a 512-sample host callback, on the six
       automations below, each a ratio of d2 against the same content rendered with the parameters
       static:

         automation                                  ramped   stepped known-bad
         blend jump +0.5 -> +1.0                     1.00     15.0
         blend jump -0.5 -> -1.0                     1.00     20.0
         jump between every adjacent pair of steps    1.00     1.00
         seeded random automation at audio rate       2.01     39.6
         output gain jump 0 -> -60 dB                1.00     35.4
         bypass in and out                           1.00     21.7

       The line at 8.0 sits between the two populations — a factor of 4 above the worst shipped
       render, half an order of magnitude below the mildest known-bad that moves anything. The
       adjacent-pair case is reported but not required to trip: one dial position apart moves the
       weights by 8.6e-03, and at hop 128 that step is smaller than the material's own second
       difference in both renders, so nothing discriminates it. What that case does prove is the
       half of the criterion that matters for shipping — the ramped render stays at the material's
       own level. */
    constexpr double kClickRatioLimit = 8.0;
    constexpr double kKnownBadOrderOfMagnitude = 10.0;
    constexpr float kAdjacentDialStep = 1.0f / 128.0f; ///< one position apart, per blendStep

    const int channels = 2;
    constexpr int kHops = 200; ///< long enough for five moves in the jump cases and a full random walk
    const int total = kHops * kHop;
    constexpr int kJumpPeriodHops = 40; ///< five moves per render for the jump cases
    const Streams a = contentStream(channels, total, 0.3);
    const Streams b = contentStream(channels, total, 1.1);

    std::mt19937 rng(20261029U);
    std::uniform_real_distribution<float> dialSpread(0.05f, 1.0f);
    std::vector<float> randomDial(static_cast<std::size_t>(kHops));
    for (float& value : randomDial)
        value = dialSpread(rng);

    enum Kind
    {
        positiveJump,
        negativeJump,
        adjacentSteps,
        randomAutomation,
        gainJump,
        bypassCycle
    };

    struct Case
    {
        Kind kind;
        const char* what;
        float staticBlend;
        bool knownBadMustTrip;
    };

    /* Every automation stays on one side of the dial. A travel that crosses zero carries the dry-bus
       hand-over the law mandates (§4), which the ramp cannot remove and which the stepped variant
       shares, so such a case measures the mandated jump in both renders and discriminates nothing. */
    const std::vector<Case> cases{{positiveJump, "blend jump +0.5 -> +1.0", 0.5f, true},
                                  {negativeJump, "blend jump -0.5 -> -1.0", -0.5f, true},
                                  {adjacentSteps,
                                   "jump between every adjacent pair of dial steps", 0.5f, false},
                                  {randomAutomation, "seeded random automation at audio rate",
                                   randomDial[0], true},
                                  {gainJump, "output gain jump 0 -> -60 dB", 0.5f, true},
                                  {bypassCycle, "bypass in and out", 0.5f, true}};

    for (const Case& c : cases)
    {
        /* Each jump case repeats rather than firing once: a single hop-boundary step lands at one
           sample of the dry bus, and if the signal happens to be small there the known-bad looks no
           worse than the ramp. Five moves over the render sample the material properly. */
        const auto toggles = [&](int position, float low, float high) {
            const int index = position / (kJumpPeriodHops * kHop);
            return index % 2 == 0 ? low : high;
        };
        auto blendAt = [&](int position) -> float {
            switch (c.kind)
            {
                case positiveJump:
                    return toggles(position, 0.5f, 1.0f);
                case negativeJump:
                    return toggles(position, -0.5f, -1.0f);
                case adjacentSteps:
                    return toggles(position, 0.5f, 0.5f + kAdjacentDialStep);
                case randomAutomation:
                    return randomDial[static_cast<std::size_t>(position / kHop)];
                case gainJump:
                case bypassCycle:
                    return 0.5f;
            }
            return 0.0f;
        };
        auto gainAt = [&](int position) {
            return c.kind == gainJump ? toggles(position, 0.0f, -60.0f) : 0.0f;
        };
        auto bypassAt = [&](int position) {
            return c.kind == bypassCycle && position >= total / 4 && position < 3 * total / 4;
        };
        auto noneBlend = [blend = c.staticBlend](int) { return blend; };
        auto noneGain = [](int) { return 0.0f; };
        auto noneBypass = [](int) { return false; };

        /* The baseline is the same content with the parameters static, which is what the criterion
           is stated as a ratio against, so busier material cannot make a step look smooth. */
        const int scanFrom = kHop + 4; ///< past the scheduler's latency pre-fill (see the metric)
        engine::MixEngine rampedStatic(std::make_unique<SmoothWetEngine>());
        const double staticRamped = maxSecondDifference(
            renderMix(rampedStatic, a, b, 512, noneBlend, noneGain, noneBypass), scanFrom);

        engine::MixEngine ramped(std::make_unique<SmoothWetEngine>());
        const double automatedRamped = maxSecondDifference(
            renderMix(ramped, a, b, 512, blendAt, gainAt, bypassAt), scanFrom);

        SteppedMixEngine steppedStatic(std::make_unique<SmoothWetEngine>());
        const double staticStepped = maxSecondDifference(
            renderMix(steppedStatic, a, b, 512, noneBlend, noneGain, noneBypass), scanFrom);

        SteppedMixEngine stepped(std::make_unique<SmoothWetEngine>());
        const double automatedStepped = maxSecondDifference(
            renderMix(stepped, a, b, 512, blendAt, gainAt, bypassAt), scanFrom);

        const double rampedRatio = automatedRamped / staticRamped;
        const double steppedRatio = automatedStepped / staticStepped;
        if (std::getenv("MIXEVIDENCE") != nullptr)
            std::printf("%s: ramped %.6g, stepped %.6g\n", c.what, rampedRatio, steppedRatio);

        INFO(c.what << ": ramped d2 ratio " << rampedRatio << ", stepped known-bad ratio "
                    << steppedRatio);
        CHECK(rampedRatio <= kClickRatioLimit);
        if (c.knownBadMustTrip)
            CHECK(steppedRatio >= kKnownBadOrderOfMagnitude);
    }
}

TEST_CASE("processHop allocates nothing, even while the parameters are moving")
{
    auto stub = std::make_unique<StubEngine>();
    StubEngine* observed = stub.get();
    engine::MixEngine engine(std::move(stub));

    const int channels = 2;
    engine.prepare(makeConfig(channels, 96));

    const Streams a = makeInputs(channels, kHop, 0);
    const Streams b = makeInputs(channels, kHop, 5000);
    Streams out = zerosLike(a);
    const Pointers p = pointersFor(a, b, out);

    engine.processHop(p.a.data(), p.b.data(), p.out.data(), channels);

    std::mt19937 rng(7U);
    std::uniform_real_distribution<float> dial(-1.0f, 1.0f);
    std::uniform_real_distribution<float> gainDb(-60.0f, 12.0f);

    long allocations = -1;
    {
        AllocationWatch watch;
        for (int call = 0; call < 56; ++call)
        {
            engine.setBlendTarget(dial(rng));
            engine.setOutputGainTarget(gainDb(rng));
            engine.setBypassTarget(call % 7 == 0);
            engine.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
        }
        allocations = allocationCount;
    }
    CHECK(allocations == 0);
    CHECK(observed->hopCalls == 57);
}

/** §2's claim that neither dry path needs a delay line: the scheduler's pre-fill alone places them
    at host time +R, so a silent wet path has to come back as the selected dry bus delayed by
    exactly R.
    The R values all sit at or above the 96 samples the three pre-hop callbacks of H = 128 at
    hostBlock = 32 drain before the first hop refills the ring. Below that the scheduler's
    short-delivery assert is what fires — F-006, not the mix stage. */
TEST_CASE("under HopScheduler both dry paths arrive at exactly the declared R")
{
    for (const int latency : {3 * kHostBlock, kHop, 4096})
        for (const float dial : {-1.0f, 1.0f})
        {
            engine::MixEngine engine(std::make_unique<SilentEngine>());
            engine.setBlendTarget(dial);

            engine::SchedulerConfig schedulerConfig;
            schedulerConfig.engine = makeConfig(1, latency);
            schedulerConfig.channels = 1;
            schedulerConfig.sidechainChannels = 1;
            schedulerConfig.maxHostBlock = kHostBlock;
            schedulerConfig.map = engine::ChannelMap::resolve(1, 1, 1).map;

            engine::HopScheduler scheduler(engine);
            scheduler.prepare(schedulerConfig);

            const int total = 40 * kHostBlock;
            const Streams a = makeInputs(1, total, 0);
            const Streams b = makeInputs(1, total, 777);
            const Streams& dry = dial < 0.0f ? a : b;
            Streams out(1, std::vector<float>(static_cast<std::size_t>(total), 0.0f));

            for (int position = 0; position < total; position += kHostBlock)
            {
                const float* aIn = a[0].data() + position;
                const float* bIn = b[0].data() + position;
                float* outPtr = out[0].data() + position;
                scheduler.process(&aIn, &bIn, &outPtr, 1, kHostBlock);
            }

            INFO("R=" << latency << " dial=" << dial);
            for (int i = 0; i < total; ++i)
            {
                const float want =
                    i < latency ? 0.0f : dry[0][static_cast<std::size_t>(i - latency)];
                INFO("i=" << i);
                CHECK(out[0][static_cast<std::size_t>(i)] == want);
            }
        }
}

/** A wet stream of one constant: the convolved path's DC component, which is the only thing the DC
    blocker is allowed to remove and the thing neither dry bus may be filtered for. */
class ConstantWetEngine final : public engine::Engine
{
public:
    explicit ConstantWetEngine(float levelToUse) noexcept : level(levelToUse) {}

    void prepare(const engine::EngineConfig& configToUse) override { config = configToUse; }
    void reset() override {}

    int latencySamples() const override { return config.latencySamples; }
    int wetDelaySamples() const override { return config.grainLength; }

    void processHop(const float* const*, const float* const*, float* const* out,
                    int numChannels) override
    {
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < config.hop; ++i)
                out[channel][i] = level;
    }

private:
    engine::EngineConfig config;
    float level;
};

/** A wet stream of deliberate DC on a tone: the fixture for the chain-ordering leg, where the DC is what a
    level reading taken *before* the blocker inflates by. Both amplitudes arrive through the constructor
    because the leg's whole point is the ratio between them, which has to be visible at the call site.

    The sample counter continues across hops, so the tone has no boundary discontinuity. A per-hop restart
    would give the wet stream a third ripple shape where the dry buses have one, and the follower bias that
    cancels between the three readings (D-014) would stop cancelling for a reason that has nothing to do
    with the ordering under proof. */
class DcPlusToneWetEngine final : public engine::Engine
{
public:
    /** 1 kHz at 48 kHz is exactly 48 samples per period — the fact that lets a DC residue be averaged out
        of the render below, since a mean over a whole number of periods cancels the tone. */
    static constexpr double kToneHz = 1000.0;

    /** Distinct samples on each channel at identical levels, on the `SmoothWetEngine` precedent. */
    static constexpr int kChannelOffset = 37;

    DcPlusToneWetEngine(float dcToUse, float toneToUse) noexcept : dc(dcToUse), tone(toneToUse) {}

    void prepare(const engine::EngineConfig& configToUse) override
    {
        config = configToUse;
        position = 0;
    }

    void reset() override { position = 0; }

    int latencySamples() const override { return config.latencySamples; }
    int wetDelaySamples() const override { return config.grainLength; }

    void processHop(const float* const*, const float* const*, float* const* out,
                    int numChannels) override
    {
        const double step = 2.0 * M_PI * kToneHz / config.sampleRate;
        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < config.hop; ++i)
                out[channel][i] = static_cast<float>(
                    dc + tone * std::sin(step
                                         * static_cast<double>(position + i
                                                               + channel * kChannelOffset)));
        position += config.hop;
    }

private:
    engine::EngineConfig config;
    float dc;
    float tone;
    int position = 0;
};

TEST_CASE("the DC blocker empties the convolved stream and never reaches a dry bus")
{
    /* Decision C, first half. The dry buses carry DC here on purpose: if the blocker reached A, the
       dial's -1 would stop being A and 01's bit-exact endpoint criteria would go red — which is the
       §5 discriminator "put the DC blocker on the dry path instead". */
    const int channels = 2;
    const Streams a = constantStream(channels, kHop, 0.5f);
    const Streams b = constantStream(channels, kHop, -0.25f);

    {
        /* The bound is only meaningful once the stage knows its rate, so it is read from a probe
           prepared at the config's rate rather than from the injected stage, which MixEngine has not
           prepared yet at the point the loop bound is needed. */
        dsp::DcBlocker probe;
        probe.prepare(kSampleRate, channels);
        const int bound = probe.settlingSamples();
        REQUIRE(bound > 0);
        engine::MixEngine mix(std::make_unique<ConstantWetEngine>(1.0f),
                              std::make_unique<dsp::DcBlocker>(), nullptr);
        mix.setBlendTarget(0.0f);
        mix.prepare(makeConfig(channels, 96));

        Streams out = zerosLike(a);
        const Pointers p = pointersFor(a, b, out);
        for (int call = 0; call < bound / kHop + 1; ++call)
            mix.processHop(p.a.data(), p.b.data(), p.out.data(), channels);

        /* At the centre the law hands through the wet stream alone, so a constant wet input has to
           vanish inside the bound the stage itself documents. */
        INFO("settling bound " << bound << " samples");
        CHECK(std::abs(out[0][kHop - 1]) <= 1e-6f);
        CHECK(std::abs(out[1][kHop - 1]) <= 1e-6f);
    }

    for (const float dial : {-1.0f, 1.0f})
    {
        engine::MixEngine mix(std::make_unique<ConstantWetEngine>(1.0f),
                              std::make_unique<dsp::DcBlocker>(),
                              std::make_unique<dsp::SoftClipper>());
        mix.setBlendTarget(dial);
        mix.prepare(makeConfig(channels, 96));

        Streams out = zerosLike(a);
        const Pointers p = pointersFor(a, b, out);
        mix.processHop(p.a.data(), p.b.data(), p.out.data(), channels);

        const Streams& want = dial < 0.0f ? a : b;
        for (int channel = 0; channel < channels; ++channel)
            for (int i = 0; i < kHop; ++i)
            {
                INFO("dial=" << dial << " ch=" << channel << " i=" << i);
                CHECK(out[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]
                      == want[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)]);
            }
    }
}

TEST_CASE("the clipper bounds the summed output, and reaches it through the gain")
{
    /* Decision C, second half: the gain is applied first and the clip last, so what the ceiling
        bounds is the gained sum — driving the output gain harder is limited rather than unbounded.
        The expectation is the law's own coefficient times the wet sample, put through the stage's
        transfer function, so this pins the order and not just the bound. */
    const int channels = 1;
    const Streams a = constantStream(channels, kHop, 0.0f);
    const Streams b = constantStream(channels, kHop, 0.0f);

    for (const float decibels : {0.0f, 6.0f, 12.0f})
    {
        engine::MixEngine mix(std::make_unique<ConstantWetEngine>(1.0f), nullptr,
                              std::make_unique<dsp::SoftClipper>());
        mix.setBlendTarget(0.0f);
        mix.setOutputGainTarget(decibels);
        mix.prepare(makeConfig(channels, 96));

        Streams out = zerosLike(a);
        const Pointers p = pointersFor(a, b, out);
        mix.processHop(p.a.data(), p.b.data(), p.out.data(), channels);

        const float linear = static_cast<float>(std::pow(10.0, static_cast<double>(decibels) / 20.0));
        const float want = dsp::SoftClipper::shape(linear);
        for (int i = 0; i < kHop; ++i)
        {
            INFO(decibels << " dB: want " << want << " (pre-clip " << linear << ")");
            CHECK(out[0][static_cast<std::size_t>(i)] == want);
            CHECK(std::abs(out[0][static_cast<std::size_t>(i)]) <= dsp::SoftClipper::ceiling);
        }
    }

    /* And a stage installed above material that never reaches the knee changes nothing at all, which
       is what lets 01's endpoint criteria stay bit-exact with the shipped chain. */
    engine::MixEngine quiet(std::make_unique<ConstantWetEngine>(0.4f), nullptr,
                            std::make_unique<dsp::SoftClipper>());
    quiet.setBlendTarget(0.0f);
    quiet.prepare(makeConfig(channels, 96));
    Streams out = zerosLike(a);
    const Pointers p = pointersFor(a, b, out);
    quiet.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
    for (int i = 0; i < kHop; ++i)
        CHECK(out[0][static_cast<std::size_t>(i)] == 0.4f);
}

TEST_CASE("the level stage moves neither delay figure, allocates nothing, and cannot reach a dry bus")
{
    /* Criteria C1, C2 and C6's engine-level halves, and the order proof for item 04's decision B. The same
       programme runs through the shipped chain and through the shipped chain plus the stage, at each settled
       dial position: bit-equal at p = -1 and p = +1, different at p = 0. The endpoints are the load-bearing
       pair — `coefWet` is exactly zero there, so any difference means the correction leaked onto a bus the
       dial hands straight through, which is what installing the stage below the law rather than in the shell
       is supposed to make impossible. The midpoint leg keeps that claim from being vacuous. */
    const int channels = 2;
    const Streams a = makeInputs(channels, kHop, 0);
    const Streams b = makeInputs(channels, kHop, 5000);

    struct Run
    {
        Streams out;
        float peakGainDb = 0.0f;
    };

    const auto run = [channels, &a, &b](float dial, bool withStage) {
        engine::MixEngine mix(std::make_unique<StubEngine>(), std::make_unique<dsp::DcBlocker>(),
                              std::make_unique<dsp::SoftClipper>());
        dsp::LevelNormalizer* stage = nullptr;
        if (withStage)
        {
            auto installed = std::make_unique<dsp::LevelNormalizer>();
            stage = installed.get();
            mix.installLevelNormalizer(std::move(installed));
        }
        mix.setBlendTarget(dial);
        mix.prepare(makeConfig(channels, 0));

        Run result{zerosLike(a), 0.0f};
        const Pointers p = pointersFor(a, b, result.out);
        for (int call = 0; call < 400; ++call)
        {
            mix.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
            if (stage != nullptr)
                result.peakGainDb = std::max(result.peakGainDb, std::abs(stage->gainDb(0)));
        }
        return result;
    };

    for (const float dial : {-1.0f, 1.0f})
    {
        const Run bare = run(dial, false);
        const Run staged = run(dial, true);
        INFO("dial " << dial << " with the stage at its defaults, peak |gain| " << staged.peakGainDb << " dB");
        CHECK(staged.out == bare.out);
    }

    const Run wetBare = run(0.0f, false);
    const Run wetStaged = run(0.0f, true);
    bool anyDifference = false;
    double loudestRatio = 1.0;
    for (std::size_t channel = 0; channel < a.size(); ++channel)
        for (std::size_t i = 0; i < a[channel].size(); ++i)
        {
            const float before = wetBare.out[channel][i];
            const float after = wetStaged.out[channel][i];
            anyDifference = anyDifference || before != after;
            if (std::abs(before) > 1.0e-6f)
                loudestRatio = std::max(loudestRatio, static_cast<double>(after) / before);
        }
    INFO("at p = 0 the wet bus moved by a peak ratio of " << loudestRatio << " with the hop gain reaching "
                                                          << wetStaged.peakGainDb
                                                          << " dB, so the endpoint claim above is not vacuous");
    CHECK(anyDifference);
    CHECK(wetStaged.peakGainDb > 0.0f);

    auto stub = std::make_unique<StubEngine>();
    StubEngine* observed = stub.get();
    observed->declaredLatency = 96;
    observed->wetDelay = 8 * kHop;
    engine::MixEngine mix(std::move(stub), std::make_unique<dsp::DcBlocker>(),
                          std::make_unique<dsp::SoftClipper>());
    auto stage = std::make_unique<dsp::LevelNormalizer>();
    dsp::LevelNormalizer* observedStage = stage.get();
    mix.installLevelNormalizer(std::move(stage));
    mix.prepare(makeConfig(channels, 96));

    CHECK(mix.latencySamples() == 96);                              ///< C1: R is still the inner engine's
    CHECK(mix.wetDelaySamples() == 8 * kHop);                       ///< C1: so is W
    CHECK(observedStage->hopSamples() == kHop);                     ///< E: sized from the prepare-time H
    CHECK(observedStage->windowSamples() == static_cast<std::size_t>(kHop));

    const Streams streamsA = makeInputs(channels, kHop, 0);
    const Streams streamsB = makeInputs(channels, kHop, 5000);
    Streams out = zerosLike(streamsA);
    const Pointers p = pointersFor(streamsA, streamsB, out);
    long allocations = -1;
    {
        AllocationWatch watch;
        for (int call = 0; call < 64; ++call)
        {
            mix.setNormalisationAmountTarget(static_cast<float>(call % 5) / 4.0f);
            mix.setSilenceFloorTarget(-60.0f + static_cast<float>(call % 3));
            mix.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
        }
        allocations = allocationCount;
    }
    INFO(allocations << " allocations over 64 hops with the stage installed and both of its controls moving "
                        "every hop; delay figures "
                     << mix.latencySamples() << " and " << mix.wetDelaySamples()
                     << " against the inner engine's 96 and " << 8 * kHop);
    CHECK(allocations == 0);
    CHECK(observed->hopCalls == 64);
}

namespace
{
/** The chain-ordering fixture's levels. Read *after* the blocker the stage asks for
    20·log10(0.05 / 0.02) = 7.9588 dB, which `M4-impl-plan/07` §0 C rounds to +8 dB. Read *before* it the
    conv reading carries the DC, and the same arithmetic asks for
    20·log10(0.05 / √(0.08² + 0.01414²)) = −7.23 dB. The two orders are therefore 15.2 dB apart, which is
    far outside any tolerance a reviewer would sign off on, and both of them sit inside the ±36 dB range,
    so neither the clamp nor the slew cap can be the reason the leg goes red. */
constexpr double kWetDcOffset = 0.08;
constexpr double kWetToneAmplitude = 0.02;
constexpr double kDryToneAmplitude = 0.05;

/** The separation between the two orders above, which the run length has to outlast on top of the
    blocker's own bound and the follower's release time. */
constexpr double kOrderingSeparationDb = 15.0;

/** One hop of the stage's window, in the millisecond unit `LevelDetector`'s constructor takes — the same
    figure `LevelNormalizer::prepare` derives for each of its three detectors. */
constexpr double kOrderingWindowMs = 1000.0 * static_cast<double>(kHop) / kSampleRate;

/** Samples per period of the fixture's tone. A mean over a whole number of them cancels the tone, which
    is the only way its DC residue is measurable at all: the tone does not fit the 128-sample hop, and a
    one-hop mean of it is of order 0.1 — five orders of magnitude above the residue. */
constexpr int kTonePeriodSamples = 48;
static_assert(kSampleRate / DcPlusToneWetEngine::kToneHz == static_cast<double>(kTonePeriodSamples),
              "the DC-residue window is only tone-free while the fixture's tone is a whole number of "
              "samples per period at the rate the leg runs at");

/** `samples` of the dry fixture: phase-continuous from sample 0 and carrying the wet stub's per-channel
    offset, so all three of the stage's readings see the same ripple content and D-014's follower bias
    cancels in the ratio instead of having to be allowed for. */
Streams dryToneStream(int channels, int samples, double amplitude, double phase)
{
    Streams data(static_cast<std::size_t>(channels),
                 std::vector<float>(static_cast<std::size_t>(samples)));
    const double step = 2.0 * M_PI * DcPlusToneWetEngine::kToneHz / kSampleRate;
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            data[static_cast<std::size_t>(channel)][static_cast<std::size_t>(i)] =
                static_cast<float>(amplitude
                                   * std::sin(step
                                              * static_cast<double>(
                                                  i + channel * DcPlusToneWetEngine::kChannelOffset)
                                              + phase));
    return data;
}

/** The pointer set `pointersFor` takes, lifted `offset` samples into each stream, so one long
    phase-continuous fixture stream can be fed hop by hop instead of a fresh one-hop buffer per hop. */
Pointers pointersAt(const Streams& a, const Streams& b, Streams& out, int offset)
{
    Pointers p;
    for (std::size_t channel = 0; channel < a.size(); ++channel)
    {
        p.a.push_back(a[channel].data() + offset);
        p.b.push_back(b[channel].data() + offset);
        p.out.push_back(out[channel].data() + offset);
    }
    return p;
}

/** One variant of the chain: the DC-plus-tone stub behind the blocker, dial and gain settled before the
    prepare so they are already at target on hop 0, and the level stage present only when asked. The
    variants therefore differ by the stage and by nothing else. The stage is installed before the prepare it
    has to precede and returned alongside, because its three readings are the evidence. */
struct OrderingChain
{
    std::unique_ptr<engine::MixEngine> mix;
    dsp::LevelNormalizer* stage = nullptr;
};

OrderingChain makeOrderingChain(int channels, float wetDc, bool withStage, float dial,
                                float amount = 1.0f)
{
    OrderingChain built{std::make_unique<engine::MixEngine>(
                            std::make_unique<DcPlusToneWetEngine>(wetDc, kWetToneAmplitude),
                            std::make_unique<dsp::DcBlocker>(), nullptr),
                        nullptr};
    if (withStage)
    {
        auto stage = std::make_unique<dsp::LevelNormalizer>();
        built.stage = stage.get();
        built.mix->installLevelNormalizer(std::move(stage));
    }
    built.mix->setBlendTarget(dial);
    built.mix->setOutputGainTarget(0.0f);
    built.mix->prepare(makeConfig(channels, 96));
    built.mix->setNormalisationAmountTarget(amount);
    built.mix->setSilenceFloorTarget(-90.0f);
    return built;
}

/** The DC residue of a render, averaged over a whole number of periods of the fixture's tone so the tone
    cancels and only the offset is left: the tone does not fit the 128-sample hop, and a one-hop mean of it
    is of order 0.1 — five orders of magnitude above the residue being looked for. */
double dcResidueOverWholeTonePeriods(const Streams& stream, std::size_t channel, int samples,
                                     int periods)
{
    double sum = 0.0;
    for (int i = samples - periods * kTonePeriodSamples; i < samples; ++i)
        sum += static_cast<double>(stream[channel][static_cast<std::size_t>(i)]);
    return std::abs(sum / static_cast<double>(periods * kTonePeriodSamples));
}

/** Criteria 1 to 5 of `M4-impl-plan/07`, for one DC level on the wet path.

    Variant A is the same chain with no level stage and the dial at the centre, where the law hands through
    `coefWet · conv` with `coefWet` exactly 1.0f — so its render *is* the blocked stream. That is the
    reference: the chain under test minus the stage, never a second implementation of the arithmetic being
    proved. A bare `LevelDetector` carrying the stage's own constants is driven off variant A's render, hop
    for hop, against the installed stage's `envelopeConv`.

    `toleranceDb` is 1 dB with the DC in place and a tenth of that without it. The DC is what stops the
    three readings sharing ripple content, and the follower bias cancels only while they do (D-016 finding
    1), so the zero-DC control run is what proves the slack belongs to the fixture rather than to the leg. */
void checkOrderingOfBlockerAndLevelStage(float wetDc, double toleranceDb)
{
    const int channels = 2;

    /* Criterion 1. An assertion that lands mid-transient measures a step response instead of an ordering,
       so the run is pinned against the three bounds it has to clear. The blocker's bound comes from a probe
       prepared at the config's rate, because the injected stage is not prepared yet where the bound is
       needed. */
    dsp::DcBlocker blockerProbe;
    blockerProbe.prepare(kSampleRate, channels);
    const int blockerBound = blockerProbe.settlingSamples();
    REQUIRE(blockerBound > 0);
    const int releaseBound
        = static_cast<int>(dsp::LevelNormalizer::releaseMs * kSampleRate * 0.001 + 0.5);
    const double slewDbPerSecond
        = 1000.0 * 2.0 * dsp::LevelNormalizer::gainRangeDb / dsp::LevelNormalizer::fullTravelMs;
    const int slewBound = static_cast<int>(kOrderingSeparationDb / slewDbPerSecond * kSampleRate + 0.5);
    const int runSamples = std::max(blockerBound, static_cast<int>(0.25 * kSampleRate + 0.5))
                           + static_cast<int>(0.1 * kSampleRate + 0.5);
    const int hops = (runSamples + kHop - 1) / kHop;

    INFO("run " << hops << " hops = " << hops * kHop << " samples, which has to outlast the blocker's "
                << blockerBound << ", the follower's release at " << releaseBound << ", and "
                << kOrderingSeparationDb << " dB of gain travel at " << slewDbPerSecond
                << " dB/s = " << slewBound);
    REQUIRE(runSamples > blockerBound);
    REQUIRE(runSamples > releaseBound);
    REQUIRE(runSamples > slewBound);
    std::cout << "  ordering leg, wet DC " << wetDc << ": " << hops << " hops / " << hops * kHop
              << " samples against bounds blocker " << blockerBound << ", release " << releaseBound
              << ", slew " << slewBound << ", tolerance " << toleranceDb << " dB\n";

    /* Every chain gets its own copy of the same two fixtures. `processHop` may not write A or B — that is
       the Engine contract — but a mutation that breaks that rule and filters the caller's buffers in place
       is one of the things the endpoint assertions below are for, and a buffer shared between the chains
       would have those assertions compare a mutated reference against itself and call it a pass. */
    const auto fixtureA = [channels, samples = hops * kHop] {
        return dryToneStream(channels, samples, kDryToneAmplitude, 0.0);
    };
    const auto fixtureB = [channels, samples = hops * kHop] {
        return dryToneStream(channels, samples, kDryToneAmplitude, M_PI / 2.0);
    };

    const Streams a = fixtureA();
    const Streams b = fixtureB();

    /* The pair no chain is ever handed, which is what the endpoint comparison below is against. */
    const Streams masterA = fixtureA();
    const Streams masterB = fixtureB();

    OrderingChain bare = makeOrderingChain(channels, wetDc, false, 0.0f);
    Streams blocked = zerosLike(a);
    for (int hop = 0; hop < hops; ++hop)
    {
        const Pointers p = pointersAt(a, b, blocked, hop * kHop);
        bare.mix->processHop(p.a.data(), p.b.data(), p.out.data(), channels);
    }

    OrderingChain staged = makeOrderingChain(channels, wetDc, true, 0.0f);
    REQUIRE(staged.stage != nullptr);

    /* Criterion 6's halves: neither the stage nor the blocker is a delay line, and the stub declares both
       figures, so the install cannot move either. */
    REQUIRE(bare.mix->latencySamples() == 96);
    REQUIRE(bare.mix->wetDelaySamples() == 8 * kHop);
    CHECK(staged.mix->latencySamples() == 96);
    CHECK(staged.mix->wetDelaySamples() == 8 * kHop);

    dsp::LevelDetector reference(kOrderingWindowMs, dsp::LevelNormalizer::attackMs,
                                 dsp::LevelNormalizer::releaseMs);
    reference.prepare(kSampleRate, channels);

    Streams out = zerosLike(a);
    const Streams stagedA = fixtureA();
    const Streams stagedB = fixtureB();
    double worstReadingGap = 0.0;
    for (int hop = 0; hop < hops; ++hop)
    {
        const Pointers p = pointersAt(stagedA, stagedB, out, hop * kHop);
        staged.mix->processHop(p.a.data(), p.b.data(), p.out.data(), channels);

        /* The reference consumes variant A's hop in the same order and the same chunk the stage's conv
           detector consumed its own, so the two readings are comparable from here on. */
        for (int channel = 0; channel < channels; ++channel)
            reference.processChannel(static_cast<std::size_t>(channel),
                                     blocked[channel].data() + hop * kHop,
                                     static_cast<std::size_t>(kHop));
        if (hop == 0)
            continue;

        /* Criterion 2 — the tight half of the leg. In this order the stage and the reference are the same
           samples through the same detector class, so anything above float noise means the stage is being
           fed something other than the blocked stream. */
        for (int channel = 0; channel < channels; ++channel)
        {
            const std::size_t ch = static_cast<std::size_t>(channel);
            const float stageReading = staged.stage->envelopeConv(ch);
            const float referenceReading = reference.envelope(ch);
            worstReadingGap = std::max(worstReadingGap,
                                       static_cast<double>(std::abs(stageReading - referenceReading)));
            INFO("hop " << hop << " ch " << channel << ": the stage reads " << stageReading
                        << " on conv, a bare detector on variant A's hop reads " << referenceReading);
            CHECK(std::abs(stageReading - referenceReading) <= 1.0e-6f);
        }
    }
    std::cout << "    worst gap between the stage's conv reading and the bare detector over hops 1 to "
              << hops - 1 << ": " << worstReadingGap << '\n';

    const double predictedDb = 20.0 * std::log10(kDryToneAmplitude / kWetToneAmplitude);
    for (int channel = 0; channel < channels; ++channel)
    {
        const std::size_t ch = static_cast<std::size_t>(channel);
        const double envA = staged.stage->envelopeA(ch);
        const double envB = staged.stage->envelopeB(ch);
        const double envConv = staged.stage->envelopeConv(ch);
        const double impliedDb = 20.0 * std::log10(std::sqrt(envA * envB) / envConv);
        const double appliedDb = staged.stage->gainDb(ch);

        /* Criterion 3, in two halves so a failure names which one moved: the applied gain against the one
           the stage's own final readings imply — which is also the proof the slew cap has finished
           travelling — and that figure against the one the blocked stream implies. The allowance is
           D-014's +0.65 dB follower bias plus margin, which is what the control run tightens. */
        INFO("ch " << channel << ": applied " << appliedDb << " dB, implied " << impliedDb
                   << " dB from envA " << envA << ", envB " << envB << ", envConv " << envConv
                   << "; the fixture predicts " << predictedDb << " dB (M4 07 §0 C's +8 dB), allowance "
                   << toleranceDb << " dB for the follower bias where the three readings do not share "
                   << "ripple content");
        CHECK(std::abs(appliedDb - impliedDb) <= toleranceDb);
        CHECK(std::abs(impliedDb - predictedDb) <= toleranceDb);
        std::cout << "    ch " << channel << ": applied " << appliedDb << " dB, implied " << impliedDb
                  << " dB, fixture predicts " << predictedDb << " dB\n";
    }

    /* Criterion 4's first half: the blocker still has to be doing its job underneath the correction.

       The residue is measured on the staged chain with `amount` at 0, which is bit-exact identity through
       the stage, and not on the corrected render — a correction to §4 criterion 4, recorded in the plan's
       §11. Any time-varying gain under the average leaks the fixture's own tone into it: the same
       96-sample mean taken on the corrected stream measures 2.2e-02 at hop 5 of the DC run and 6.1e-07 at
       hop 80, against 1.3e-10 here. That is the follower's settling tail rather than DC, and it sits over
       the 1e-6 bound on correct code, so a residue leg on the corrected stream would be a function of when
       the run ends rather than of whether the blocker works. At amount 0 the hop gain is exactly 1.0f at
       every sample, so what is left in the mean is the blocker's own residue. */
    OrderingChain identity = makeOrderingChain(channels, wetDc, true, 0.0f, 0.0f);
    const Streams identityA = fixtureA();
    const Streams identityB = fixtureB();
    Streams identityOut = zerosLike(identityA);
    for (int hop = 0; hop < hops; ++hop)
    {
        const Pointers p = pointersAt(identityA, identityB, identityOut, hop * kHop);
        identity.mix->processHop(p.a.data(), p.b.data(), p.out.data(), channels);
    }
    for (int channel = 0; channel < channels; ++channel)
    {
        const std::size_t ch = static_cast<std::size_t>(channel);
        const double stagedResidue = dcResidueOverWholeTonePeriods(identityOut, ch, hops * kHop, 3);
        const double referenceResidue = dcResidueOverWholeTonePeriods(blocked, ch, hops * kHop, 3);
        INFO("ch " << channel << ": DC residue " << stagedResidue << " through the chain with the stage at"
                   << " amount 0 and " << referenceResidue << " on the blocked reference, both averaged "
                   << "over the last " << 3 * kTonePeriodSamples << " samples");
        CHECK(stagedResidue <= 1.0e-6);
        CHECK(referenceResidue <= 1.0e-6);
        std::cout << "    ch " << channel << ": DC residue " << stagedResidue
                  << " staged at unity, " << referenceResidue << " on the blocked reference\n";
    }

    /* Criterion 4's second half: D-010's endpoints under a stage that a later M4 rewrite might be tempted
       to move. `coefWet` is exactly zero at either endpoint, so the dry bus arrives bit for bit — against
       the master copy, which no chain ever saw, because a chain that filtered the caller's A bus in place
       would otherwise be compared with its own handiwork and called exact. */
    for (const float dial : {-1.0f, 1.0f})
    {
        OrderingChain endpoint = makeOrderingChain(channels, wetDc, true, dial);
        const Streams endpointA = fixtureA();
        const Streams endpointB = fixtureB();
        Streams endpointOut = zerosLike(endpointA);
        for (int hop = 0; hop < 4; ++hop)
        {
            const Pointers p = pointersAt(endpointA, endpointB, endpointOut, hop * kHop);
            endpoint.mix->processHop(p.a.data(), p.b.data(), p.out.data(), channels);
        }
        const Streams& want = dial < 0.0f ? masterA : masterB;
        for (int channel = 0; channel < channels; ++channel)
            for (int i = 0; i < 4 * kHop; ++i)
            {
                INFO("dial " << dial << " ch " << channel << " i " << i << ": staged endpoint "
                             << endpointOut[channel][static_cast<std::size_t>(i)] << " against "
                             << want[channel][static_cast<std::size_t>(i)]);
                CHECK(endpointOut[channel][static_cast<std::size_t>(i)]
                      == want[channel][static_cast<std::size_t>(i)]);
            }
    }
}
}

TEST_CASE("level stage runs after the DC blocker")
{
    /* The one M3 invariant no leg held: inside `MixEngine::processHop` the DC blocker has to run on the
       convolved stream *before* the level stage reads it. M3's mutation 10 swapped the two loops and found
       nothing red anywhere, because the blocker still removes the DC before the host sees it either way —
       the residue is zero in both orders, so what differs is the gain the stage computes against a
       DC-inflated reading, not what reaches the output. The witness is therefore the reading and the
       applied gain, on a wet stream carrying DC at exactly the ratio that turns the two orders into a
       15 dB disagreement — a wiring proof, not program material, and nobody should read a pass here as
       "the normalizer sounds right on DC-heavy input". */
    checkOrderingOfBlockerAndLevelStage(static_cast<float>(kWetDcOffset), 1.0);

    /* Criterion 5's control: no DC, everything else identical. The three readings now share ripple
       content, the follower bias cancels, and both comparisons above hold to a tenth of a dB — which is
       what keeps the DC run's 1 dB allowance from being the leg's own imprecision. */
    checkOrderingOfBlockerAndLevelStage(0.0f, 0.1);
}

TEST_CASE("the safety stages move neither delay figure and allocate nothing on the hop path")
{
    /* Criterion B6's engine-level half: neither stage is a delay line, so R and W are the inner
       engine's own declarations, and criterion B8's: the shipped chain's hop path still allocates
       nothing with both of them installed. */
    auto stub = std::make_unique<StubEngine>();
    StubEngine* observed = stub.get();
    observed->declaredLatency = 96;
    observed->wetDelay = 8 * kHop;

    engine::MixEngine mix(std::move(stub), std::make_unique<dsp::DcBlocker>(),
                          std::make_unique<dsp::SoftClipper>());
    mix.prepare(makeConfig(2, 96));

    CHECK(mix.latencySamples() == 96);
    CHECK(mix.wetDelaySamples() == 8 * kHop);

    const int channels = 2;
    const Streams a = makeInputs(channels, kHop, 0);
    const Streams b = makeInputs(channels, kHop, 5000);
    Streams out = zerosLike(a);
    const Pointers p = pointersFor(a, b, out);

    long allocations = -1;
    {
        AllocationWatch watch;
        for (int call = 0; call < 64; ++call)
            mix.processHop(p.a.data(), p.b.data(), p.out.data(), channels);
        allocations = allocationCount;
    }
    CHECK(allocations == 0);
    CHECK(observed->hopCalls == 64);
}
