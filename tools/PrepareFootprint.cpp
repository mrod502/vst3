/** Prepare-time footprint of the shipped chain, measured from allocation rather than restated from the
    sizing expressions (M4 item 04, criteria C1 and C10; the figures D-022 A and C were taken with).

    `OfflineRender` §5.18 printed a footprint built by re-evaluating `maxHostBlockSamples + hop` in the harness
    — a second copy of the scheduler's expression, so a change to that expression would have left it printing
    the old number with the suite green (F-016). This program allocates instead: every figure below comes out
    of a replaced global `operator new`/`delete` pair that tracks live bytes, over a region that builds the
    real thing.

    Three instruments, because they see different scopes:

      1  `EntangleAudioProcessor::maxHostBlockFor` against `container::CircularBuffer`'s rounding — what the
         floor of D-022 A costs a ring at every host block a host may deliver.
      2  `engine::HopScheduler` alone, on a `PassthroughEngine`, per (chA, chB, chOut) triple and hop: the
         scheduler's live bytes, and what a narrowing gives back against what it keeps (04 §1.6).
      3  the shipped builder and the real shell — `entangle::makeShippedChain` with `fft::JuceFft` behind it,
         then `createPluginFilter()` through `setBusesLayout` and `prepareToPlay` — for the whole-plugin figure
         at the default grain length and the cap, and for the cost of a re-prepare that the key skips.

    Not registered with CTest, on `CpuBaseline`'s stated grounds: a footprint measured on one host through one
    transform backend is a measurement to record, not a pass/fail signal. `juce::dsp::FFT` picks its backend at
    runtime (vDSP, IPP, MKL, FFTW or the fallback), so its tables are reported as the residue beside the
    project's own `10.5F + 6` / `12.5F + 6` bound rather than folded into it.

    Always quote these numbers with the build type and host on the first line. Exits 0 whatever it finds. */

#include <juce_audio_utils/juce_audio_utils.h>

#include <Container.hpp>
#include <HopScheduler.hpp>
#include <PassthroughEngine.hpp>

#include "EngineChain.h"
#include "EntangleProcessor.h"
#include "JuceTransformFactory.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>

/** Defined by the plugin's shared code, which this harness links rather than recompiling. */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
/** Live-byte accounting. Each block carries its size and the raw pointer malloc returned in the 16 bytes
    below the payload, which is what lets `operator delete(void*)` — not given the size — retire it. */
std::ptrdiff_t liveBytes = 0;
long allocationCalls = 0;

void* allocateTracked(std::size_t size, std::size_t alignment)
{
    const std::size_t span = std::max(alignment, alignof(std::max_align_t));
    const std::size_t total = size + span;
    void* base = span == alignof(std::max_align_t)
                     ? std::malloc(total)
                     : std::aligned_alloc(span, ((total + span - 1) / span) * span);
    if (base == nullptr)
        throw std::bad_alloc();

    char* payload = static_cast<char*>(base) + span;
    *reinterpret_cast<std::size_t*>(payload - 8) = size;
    *reinterpret_cast<void**>(payload - 16) = base;

    liveBytes += static_cast<std::ptrdiff_t>(size);
    ++allocationCalls;
    return payload;
}

void releaseTracked(void* payload) noexcept
{
    if (payload == nullptr)
        return;
    char* at = static_cast<char*>(payload);
    liveBytes -= static_cast<std::ptrdiff_t>(*reinterpret_cast<std::size_t*>(at - 8));
    std::free(*reinterpret_cast<void**>(at - 16));
}
}

void* operator new(std::size_t size) { return allocateTracked(size, alignof(std::max_align_t)); }
void* operator new[](std::size_t size) { return allocateTracked(size, alignof(std::max_align_t)); }
void operator delete(void* payload) noexcept { releaseTracked(payload); }
void operator delete(void* payload, std::size_t) noexcept { releaseTracked(payload); }
void operator delete[](void* payload) noexcept { releaseTracked(payload); }
void operator delete[](void* payload, std::size_t) noexcept { releaseTracked(payload); }

#ifdef __cpp_aligned_new
void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocateTracked(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocateTracked(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* payload, std::align_val_t) noexcept { releaseTracked(payload); }
void operator delete[](void* payload, std::align_val_t) noexcept { releaseTracked(payload); }
#endif

namespace
{
constexpr double kRate = 48000.0;
constexpr int kHostBlock = EntangleAudioProcessor::maxHostBlockSamples;
constexpr int kMaxChannels = engine::kMaxChannels;

double kib(std::ptrdiff_t bytes)
{
    return static_cast<double>(bytes) / 1024.0;
}

/** Runs `body` and reports the live bytes it leaves behind — what the region allocated minus what it freed. */
template <class Body>
std::ptrdiff_t measureLiveBytes(Body&& body)
{
    const std::ptrdiff_t before = liveBytes;
    body();
    return liveBytes - before;
}

int pow2AtLeast(int value) noexcept
{
    int size = 1;
    while (size < value)
        size <<= 1;
    return size;
}

struct Triple
{
    const char* name;
    int chA;
    int chB;
    int chOut;
};

const Triple kTriples[] = {
    {"(1,0,1) mono, no sidechain", 1, 0, 1},
    {"(2,2,2) stereo, widest pre-D-021", 2, 2, 2},
    {"(1,2,1) mono main, stereo side", 1, 2, 1},
    {"(1,8,1) the ceiling", 1, 8, 1}};

engine::SchedulerConfig schedulerConfigFor(int hop, const Triple& triple, int grainLength)
{
    entangle::ChainConfig chain;
    chain.sampleRate = kRate;
    chain.hop = hop;
    chain.grainLength = grainLength;
    chain.channels = triple.chA;
    chain.sidechainChannels = triple.chB;
    chain.outputChannels = triple.chOut;
    chain.maxHostBlock = EntangleAudioProcessor::maxHostBlockSamples;
    return entangle::makeSchedulerConfig(chain);
}

void reportEnvironment()
{
    std::cout << "host: " << juce::SystemStats::getOperatingSystemName().toRawUTF8() << ", "
              << juce::SystemStats::getCpuModel().toRawUTF8() << ", "
              << juce::SystemStats::getNumCpus() << " CPUs, "
              << juce::SystemStats::getMemorySizeInMegabytes() / 1024 << " GiB RAM";
#if defined(__aarch64__)
    std::cout << ", aarch64";
#elif defined(__x86_64__)
    std::cout << ", x86-64";
#else
    std::cout << ", unknown arch";
#endif
    std::cout << ", gcc " << __VERSION__;

#ifdef NDEBUG
    std::cout << ", build type: NDEBUG defined (Release-like)";
#else
    std::cout << ", build type: NDEBUG not defined";
#endif

#ifdef ENTANGLE_ASSERTS_CONFIGURED
    std::cout << ", ENTANGLE_ASSERTS=ON";
#else
    std::cout << ", ENTANGLE_ASSERTS=OFF for this target";
#endif
    std::cout << "\n\n";
}

void reportRingFloor()
{
    std::cout << "[1] the floor of D-022 A, per host block, through the container's rounding\n"
                 "    maxHostBlockFor() is the shell's own function, so this row cannot drift from what\n"
                 "    configureRings sizes against; capacity is the ring's live storage, not the ask.\n";
    for (const int hostBlock : {1, 64, 512, 4095, 4096, 4097, 8192, 65536})
    {
        const int sized = EntangleAudioProcessor::maxHostBlockFor(hostBlock);
        std::cout << "    hostBlock " << hostBlock << " -> sized " << sized << ":";
        for (const int hop : EntangleAudioProcessor::hopValues)
        {
            const container::CircularBuffer<float> ring(static_cast<std::size_t>(sized + hop));
            std::cout << "  H=" << hop << " " << ring.capacity() << "f/"
                      << kib(static_cast<std::ptrdiff_t>(ring.capacity()) * static_cast<std::ptrdiff_t>(sizeof(float)))
                      << "KiB";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void reportSchedulerFootprint()
{
    std::cout << "[2] HopScheduler live bytes after one prepare, measured from allocation\n"
                 "    (PassthroughEngine, so nothing below the seam is sized; R = hop, D-004).\n";
    for (const Triple& triple : kTriples)
    {
        std::cout << "    " << triple.name << ":";
        for (const int hop : EntangleAudioProcessor::hopValues)
        {
            engine::PassthroughEngine engine;
            engine::HopScheduler scheduler(engine);
            const auto config = schedulerConfigFor(hop, triple, 2 * hop);
            const std::ptrdiff_t bytes = measureLiveBytes([&] { scheduler.prepare(config); });
            std::cout << "  H=" << hop << " " << kib(bytes) << "KiB";
        }
        const auto config = schedulerConfigFor(128, triple, 256);
        std::cout << "   [processed " << config.map.channels << ", rings " << triple.chA << "+"
                  << triple.chB << "+" << config.map.channels << "]\n";
    }
    std::cout << "\n";
}

void reportRetention()
{
    std::cout << "[3] what a narrowing gives back and what it keeps (04 §1.6), at H = 256\n";
    const int hop = 256;
    const Triple ceiling{"(1,8,1)", 1, 8, 1};
    const Triple monoSide{"(1,1,1)", 1, 1, 1};

    engine::PassthroughEngine engine;
    engine::HopScheduler reused(engine);
    const auto wide = schedulerConfigFor(hop, ceiling, 2 * hop);
    const auto narrow = schedulerConfigFor(hop, monoSide, 2 * hop);

    const std::ptrdiff_t wideBytes = measureLiveBytes([&] { reused.prepare(wide); });
    const std::ptrdiff_t narrowed = measureLiveBytes([&] { reused.prepare(narrow); });
    const std::ptrdiff_t backWide = measureLiveBytes([&] { reused.prepare(wide); });

    engine::PassthroughEngine freshEngine;
    engine::HopScheduler fresh(freshEngine);
    const std::ptrdiff_t preparedNarrow = measureLiveBytes([&] { fresh.prepare(narrow); });

    const std::ptrdiff_t ringBytes = static_cast<std::ptrdiff_t>(
        container::CircularBuffer<float>(static_cast<std::size_t>(kHostBlock + hop)).capacity())
                                    * static_cast<std::ptrdiff_t>(sizeof(float));
    const std::ptrdiff_t releasedRings = static_cast<std::ptrdiff_t>(kMaxChannels - 1) * ringBytes;
    const std::ptrdiff_t keptScratch = static_cast<std::ptrdiff_t>(kMaxChannels - 1)
                                       * static_cast<std::ptrdiff_t>(hop * sizeof(float));
    const std::ptrdiff_t keptObjects = static_cast<std::ptrdiff_t>(kMaxChannels - 1)
                                       * static_cast<std::ptrdiff_t>(
                                             sizeof(container::CircularBuffer<float>));

    std::cout << "    (1,8,1) prepared          " << kib(wideBytes) << " KiB\n"
              << "    -> (1,1,1) re-prepare    " << kib(narrowed) << " KiB, against " << kib(releasedRings)
              << " KiB of " << (kMaxChannels - 1) << " sidechain rings\n"
              << "    -> (1,8,1) re-prepare    " << kib(backWide) << " KiB\n"
              << "    (1,1,1) prepared fresh   " << kib(preparedNarrow) << " KiB, so the reused instance holds "
              << kib(wideBytes + narrowed - preparedNarrow) << " KiB it no longer needs\n"
              << "    which is hop scratch at the chB high-water (" << kib(keptScratch) << " KiB) plus the\n"
              << "    ring-object array (" << kib(keptObjects)
              << " KiB): `assign` and `clear` keep capacity, so neither comes back.\n\n";
}

void reportWholeChain()
{
    std::cout << "[4] the shipped chain with the real transform behind it, per prepare\n"
                 "    engine = entangle::makeShippedChain(...)->prepare(); full = the same chain with a\n"
                 "    HopScheduler over it. The documented bound is 10.5F + 6 floats for one processed\n"
                 "    channel and 12.5F + 6 for two; the residue is juce::dsp::FFT's own tables, the three\n"
                 "    safety stages' per-channel state, and the vectors that own them.\n";
    const JuceTransformFactory factory;
    /* The default dial at the default hop, the cap dial where the CPU clamp pulls it back, and the cap
       dial at the only hop that lets it through — the configuration the cap's footprint comment quotes. */
    struct Shape
    {
        const char* name;
        int dial;
        int hop;
    };
    const Shape shapes[] = {
        {"default", EntangleAudioProcessor::grainLengthValues[EntangleAudioProcessor::defaultGrainLengthIndex],
         EntangleAudioProcessor::hopValues[EntangleAudioProcessor::defaultHopIndex]},
        {"cap dial, clamped", EntangleAudioProcessor::maxGrainLengthSamples,
         EntangleAudioProcessor::hopValues[EntangleAudioProcessor::defaultHopIndex]},
        {"cap dial, unclamped", EntangleAudioProcessor::maxGrainLengthSamples,
         EntangleAudioProcessor::hopValues.back()}};
    for (const Triple& triple : {Triple{"mono", 1, 0, 1}, Triple{"stereo", 2, 2, 2},
                                 Triple{"(1,8,1) ceiling", 1, 8, 1}})
    {
        for (const Shape& shape : shapes)
        {
            const int length = EntangleAudioProcessor::clampGrainLength(shape.dial, shape.hop);
            const auto config = schedulerConfigFor(shape.hop, triple, length);
            const int sizeF = 2 * length;

            auto engineOnly = entangle::makeShippedChain(factory);
            const std::ptrdiff_t engineBytes =
                measureLiveBytes([&] { engineOnly->prepare(config.engine); });

            auto chain = entangle::makeShippedChain(factory);
            engine::HopScheduler scheduler(*chain);
            const std::ptrdiff_t fullBytes = measureLiveBytes([&] { scheduler.prepare(config); });

            const double boundFloats = config.engine.maxChannels == 1 ? 10.5 * static_cast<double>(sizeF) + 6.0
                                                                     : 12.5 * static_cast<double>(sizeF) + 6.0;
            const double boundKiB = kib(static_cast<std::ptrdiff_t>(
                boundFloats * static_cast<double>(sizeof(float))));
            std::cout << "    " << triple.name << " " << shape.name << " (dial " << shape.dial << " at H="
                      << shape.hop << ") -> L=" << length << " F=" << sizeF << " processed "
                      << config.engine.maxChannels << ":\n"
                      << "      engine " << kib(engineBytes) << " KiB | scheduler " << kib(fullBytes - engineBytes)
                      << " KiB | full " << kib(fullBytes) << " KiB\n"
                      << "      documented " << boundKiB << " KiB (10.5F+6 / 12.5F+6) | residue "
                      << kib(engineBytes) - boundKiB << " KiB\n";
        }
    }
    std::cout << "\n";
}

bool applyLayout(juce::AudioProcessor& processor, const juce::AudioChannelSet& mainSet,
                 const juce::AudioChannelSet& side)
{
    auto layout = processor.getBusesLayout();
    layout.getChannelSet(true, 0) = mainSet;
    layout.getChannelSet(false, 0) = mainSet;
    layout.getChannelSet(true, 1) = side;
    return processor.setBusesLayout(layout);
}

void setDials(juce::AudioProcessor& processor, int hopIndex, int grainIndex)
{
    auto& shell = static_cast<EntangleAudioProcessor&>(processor);
    if (auto* hop = dynamic_cast<juce::AudioParameterChoice*>(
            shell.apvts.getParameter(EntangleAudioProcessor::hopParameterId)))
        *hop = hopIndex;
    if (auto* grain = dynamic_cast<juce::AudioParameterChoice*>(
            shell.apvts.getParameter(EntangleAudioProcessor::grainLengthParameterId)))
        *grain = grainIndex;
}

void reportShell()
{
    std::cout << "[5] the shell through createPluginFilter(): construction, first prepare, and what a\n"
                 "    re-prepare costs when the key says nothing changed (D-022 B).\n";
    struct Shell
    {
        const char* name;
        juce::AudioChannelSet mainSet;
        juce::AudioChannelSet side;
    };
    const std::vector<Shell> shapes = {
        {"stereo + stereo sidechain", juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()},
        {"stereo, no sidechain", juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled()},
        {"mono, no sidechain", juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled()},
        {"mono + stereo sidechain (fold)", juce::AudioChannelSet::mono(),
         juce::AudioChannelSet::stereo()},
        {"mono + 8-wide sidechain (fold, ceiling)", juce::AudioChannelSet::mono(),
         juce::AudioChannelSet::discreteChannels(kMaxChannels)}};

    for (const Shell& shape : shapes)
    {
        std::unique_ptr<juce::AudioProcessor> processor;
        const std::ptrdiff_t constructed =
            measureLiveBytes([&] { processor.reset(createPluginFilter()); });
        if (!applyLayout(*processor, shape.mainSet, shape.side))
        {
            std::cout << "    " << shape.name
                      << ": setBusesLayout refused it — the gate's answer, not this tool's\n";
            continue;
        }
        setDials(*processor, 1, 3);

        const std::ptrdiff_t prepared =
            measureLiveBytes([&] { processor->prepareToPlay(kRate, kHostBlock); });
        const int latency = processor->getLatencySamples();
        const std::ptrdiff_t repeat =
            measureLiveBytes([&] { processor->prepareToPlay(kRate, kHostBlock); });
        setDials(*processor, 2, 3);
        const std::ptrdiff_t hopChange =
            measureLiveBytes([&] { processor->prepareToPlay(kRate, kHostBlock); });

        std::cout << "    " << shape.name << ": " << processor->getChannelCountOfBus(true, 0) << "/"
                  << processor->getChannelCountOfBus(true, 1) << " -> "
                  << processor->getTotalNumOutputChannels() << " out, latency " << latency
                  << " -> " << processor->getLatencySamples() << " when the hop moves\n"
                  << "      constructed " << kib(constructed) << " KiB | first prepare " << kib(prepared)
                  << " KiB | same key again " << kib(repeat) << " KiB (skipped) | hop 128->256 "
                  << kib(hopChange) << " KiB (re-prepared)\n";
    }
    std::cout << "\n";
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;

    reportEnvironment();
    reportRingFloor();
    reportSchedulerFootprint();
    reportRetention();
    reportWholeChain();
    reportShell();

    std::cout << "Recorded, not asserted: no figure above is a limit and none of them fails this program.\n";
    return 0;
}
