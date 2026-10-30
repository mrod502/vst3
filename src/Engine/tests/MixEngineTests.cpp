#include <Engine.hpp>
#include <HopScheduler.hpp>
#include <MixEngine.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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
