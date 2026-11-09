/* Item 04's JUCE-light half: the reconfigure key's equality, and the scheduler's memory as allocated bytes.

   Two things live here because each needs what this binary provides. The key is a plain Engine type, so its
   per-field equality (C4) and its never-match-a-real-configuration sentinel (C5) are pure-function legs. The
   memory legs (C1, C2, C8, C10) measure allocation, and a program may replace global `operator new` exactly
   once — which is why `EngineTests`, `MixEngineTests`, `ChannelMapTests` and `LayoutGateTests` are four
   binaries and this is the fifth.

   Nothing here holds a byte *limit* (04 §0 C). The legs assert relationships — how many bytes a prepare takes,
   how many a narrowing gives back, how many a callback takes — so a sizing change reddens a measurement rather
   than a documented ceiling drifting quietly. The whole-plugin figure, through the real transform, is
   `tools/PrepareFootprint`'s: it is not in CTest because a figure measured on one host is not a pass/fail
   signal. */

#include <HopScheduler.hpp>
#include <PassthroughEngine.hpp>
#include <ReconfigureKey.hpp>

#include <Container.hpp>

#include <catch_amalgamated.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <optional>

namespace
{
/** Byte-exact live accounting. Every block carries its size and its raw base in the 16 bytes below the
    payload, so `operator delete(void*)` — which is not given the size — can still retire it. The counter is
    armed for the call-count legs; the byte total runs always, so a leg snapshots before and after. */
bool armAllocationCount = false;
long allocationCount = 0;
std::ptrdiff_t liveBytes = 0;

constexpr std::size_t maxAlignment(std::size_t requested) noexcept
{
    return std::max(requested, alignof(std::max_align_t));
}

void* allocateCounted(std::size_t size, std::size_t alignment)
{
    const std::size_t span = maxAlignment(alignment);
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
    if (armAllocationCount)
        ++allocationCount;
    return payload;
}

void releaseCounted(void* payload) noexcept
{
    if (payload == nullptr)
        return;
    char* at = static_cast<char*>(payload);
    liveBytes -= static_cast<std::ptrdiff_t>(*reinterpret_cast<std::size_t*>(at - 8));
    std::free(*reinterpret_cast<void**>(at - 16));
}
}

void* operator new(std::size_t size) { return allocateCounted(size, alignof(std::max_align_t)); }
void* operator new[](std::size_t size) { return allocateCounted(size, alignof(std::max_align_t)); }
void operator delete(void* payload) noexcept { releaseCounted(payload); }
void operator delete(void* payload, std::size_t) noexcept { releaseCounted(payload); }
void operator delete[](void* payload) noexcept { releaseCounted(payload); }
void operator delete[](void* payload, std::size_t) noexcept { releaseCounted(payload); }

#ifdef __cpp_aligned_new
void* operator new(std::size_t size, std::align_val_t alignment)
{
    return allocateCounted(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return allocateCounted(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* payload, std::align_val_t) noexcept { releaseCounted(payload); }
void operator delete[](void* payload, std::align_val_t) noexcept { releaseCounted(payload); }
#endif

namespace
{
struct AllocationWatch
{
    AllocationWatch()
    {
        allocationCount = 0;
        armAllocationCount = true;
    }
    ~AllocationWatch() { armAllocationCount = false; }
};
}

namespace
{
using engine::ChannelMap;
using engine::ChannelMapStatus;
using engine::HopScheduler;
using engine::kMaxChannels;
using engine::PassthroughEngine;
using engine::ReconfigureKey;
using engine::SchedulerConfig;
using Ring = container::CircularBuffer<float>;

/** The shell's floor (`EntangleAudioProcessor::maxHostBlockSamples`) and the rate and default hop the shipped
    instance prepares with. Restated as inputs, deliberately: these are the arguments `configureRings` passes,
    not the sizes this file is measuring. */
constexpr double kRate = 48000.0;
constexpr int kHostBlock = 4096;

/** The shipped default configuration, as a key: 48 kHz, a 4096-callback host, stereo main and sidechain, the
    default hop and the default grain length after the clamp — which at H = 128 is the dial position itself. */
constexpr ReconfigureKey kDefault{48000.0, kHostBlock, 2, 2, 2, 128, 8192};

constexpr int pow2AtLeast(int value) noexcept
{
    int size = 1;
    while (size < value)
        size <<= 1;
    return size;
}

/** One prepare's configuration, resolved the way the shipped builder resolves it — the rule stays in
    `ChannelMap::resolve` and this file only dereferences it (INDEX §3 rule 2). R is one hop (D-004), which is
    what makes the input and output ring capacities the same number. */
SchedulerConfig configFor(int hop, int chA, int chB, int chOut)
{
    const auto resolved = ChannelMap::resolve(chA, chB, chOut);
    REQUIRE(resolved.status == ChannelMapStatus::ok);

    SchedulerConfig config;
    config.engine.sampleRate = kRate;
    config.engine.hop = hop;
    config.engine.maxChannels = resolved.map.channels;
    config.engine.latencySamples = hop;
    config.engine.grainLength = 2 * hop;
    config.channels = chA;
    config.sidechainChannels = chB;
    config.maxHostBlock = kHostBlock;
    config.map = resolved.map;
    return config;
}

int processedFor(int chA, int chB, int chOut)
{
    const auto resolved = ChannelMap::resolve(chA, chB, chOut);
    REQUIRE(resolved.status == ChannelMapStatus::ok);
    return resolved.map.channels;
}

std::ptrdiff_t prepareAndMeasureBytes(HopScheduler& scheduler, const SchedulerConfig& config)
{
    const std::ptrdiff_t before = liveBytes;
    scheduler.prepare(config);
    return liveBytes - before;
}

/** The bytes one `prepare` is expected to hold: the capacity bounds of `HopScheduler`'s own capacity section,
    the container's power-of-two rounding, and the vectors that own them. Written as an oracle the *measured*
    allocation is compared against — the distinction `04` §1.5 draws about §5.18's figure, which printed this
    sum of expressions without ever allocating it. */
std::size_t expectedPrepareBytes(int hop, int chA, int chB, int processed)
{
    const std::size_t ring = static_cast<std::size_t>(pow2AtLeast(kHostBlock + hop)) * sizeof(float);
    std::size_t bytes = static_cast<std::size_t>(kHostBlock) * sizeof(float);
    bytes += static_cast<std::size_t>(chA + chB + processed) * ring;
    bytes += static_cast<std::size_t>(chA + chB + processed) * sizeof(Ring);
    bytes += static_cast<std::size_t>(hop) * static_cast<std::size_t>(chA + chB + processed + 1)
             * sizeof(float);
    bytes += 3 * static_cast<std::size_t>(processed) * sizeof(float*);
    return bytes;
}

/** The ring capacity every callback is sized against, at this hop — the figure `04` §1.2's table claims. */
std::size_t ringCapacityBytes(int hop)
{
    return static_cast<std::size_t>(Ring(static_cast<std::size_t>(kHostBlock + hop)).capacity())
           * sizeof(float);
}

/** A callback's pointer arrays, at final size, so nothing inside a watched loop belongs to the harness. */
struct CallbackBuffers
{
    CallbackBuffers(int sources, int sideSources, int processed, int samples)
        : a(static_cast<std::size_t>(sources), std::vector<float>(static_cast<std::size_t>(samples), 0.25f)),
          b(static_cast<std::size_t>(std::max(sideSources, 1)),
            std::vector<float>(static_cast<std::size_t>(samples), -0.5f)),
          out(static_cast<std::size_t>(processed), std::vector<float>(static_cast<std::size_t>(samples), 0.0f)),
          aPtrs(static_cast<std::size_t>(sources)),
          bPtrs(static_cast<std::size_t>(std::max(sideSources, 1))),
          outPtrs(static_cast<std::size_t>(processed))
    {
        for (std::size_t channel = 0; channel < a.size(); ++channel)
            aPtrs[channel] = a[channel].data();
        for (std::size_t channel = 0; channel < b.size(); ++channel)
            bPtrs[channel] = b[channel].data();
        for (std::size_t channel = 0; channel < out.size(); ++channel)
            outPtrs[channel] = out[channel].data();
    }

    std::vector<std::vector<float>> a;
    std::vector<std::vector<float>> b;
    std::vector<std::vector<float>> out;
    std::vector<const float*> aPtrs;
    std::vector<const float*> bPtrs;
    std::vector<float*> outPtrs;
};

long runCallbacksAllocations(HopScheduler& scheduler, CallbackBuffers& buffers, int processed,
                             const std::vector<int>& blocks)
{
    AllocationWatch watch;
    for (const int samples : blocks)
        scheduler.process(buffers.aPtrs.data(), buffers.bPtrs.data(), buffers.outPtrs.data(), processed,
                          samples);
    return allocationCount;
}

/** A transcription of `configureRings`'s key handling — the three arrows of `04` §2.2 — so the ordering the
    shell documents has a leg at the key level. Production keeps the one decider: the guard lives in
    `configureRings` alone and this oracle is a test, not a second copy of the rule in the build. */
class GuardTranscription
{
public:
    /** @return whether this configure rebuilt, which is what the shell's early return decides. */
    bool configure(const ReconfigureKey& key, bool gateAccepted)
    {
        if (!gateAccepted)
        {
            stored.reset();
            return false;
        }
        if (stored.has_value() && *stored == key)
            return false;
        stored = key;
        return true;
    }

    bool holdsAKey() const noexcept { return stored.has_value(); }

private:
    std::optional<ReconfigureKey> stored;
};

/** A configuration per field: each one differs from `kDefault` in exactly one field and is otherwise a
    configuration the shell really can be asked to prepare. The values are all ones the dials, the buses and
    the hosts actually produce, so a comparison that ignores a field cannot pass by coincidence. */
ReconfigureKey changedRate(ReconfigureKey key)
{
    key.sampleRate = 96000.0;
    return key;
}

ReconfigureKey changedHostBlock(ReconfigureKey key)
{
    key.hostBlock = 512;
    return key;
}

ReconfigureKey changedChannels(ReconfigureKey key)
{
    key.channels = 1;
    return key;
}

ReconfigureKey changedSidechain(ReconfigureKey key)
{
    key.sidechainChannels = 0;
    return key;
}

ReconfigureKey changedOutputs(ReconfigureKey key)
{
    key.outputChannels = 1;
    return key;
}

ReconfigureKey changedHop(ReconfigureKey key)
{
    key.hop = 256;
    return key;
}

ReconfigureKey changedGrainLength(ReconfigureKey key)
{
    key.grainLength = 4096;
    return key;
}

/** One field's leg: the default equals a byte-identical copy of itself and differs from that same key with
    exactly this one field moved to each of `values`. An `operator==` that drops a field reddens this leg and
    no other, which is the diagnosability shape `04` §5 arms 3, 4 and 5 probe. */
template <class Mutate, class Value>
void expectOneFieldDistinguishes(ReconfigureKey base, Mutate mutate, std::initializer_list<Value> values)
{
    CHECK(base == ReconfigureKey{base});
    for (const Value value : values)
    {
        auto moved = base;
        mutate(moved, value);
        CHECK(moved != base);

        auto sameAsMoved = base;
        mutate(sameAsMoved, value);
        CHECK(moved == sameAsMoved);
    }
}
}

TEST_CASE("reconfigure: sampleRate alone forces a re-prepare", "[reconfigure]")
{
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, double value) {
                                         key.sampleRate = value;
                                     },
                                     {44100.0, 96000.0, 192000.0});
}

TEST_CASE("reconfigure: hostBlock alone forces a re-prepare", "[reconfigure]")
{
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, int value) { key.hostBlock = value; },
                                {1, 64, 512, 8192, 65536});
}

TEST_CASE("reconfigure: chA alone forces a re-prepare", "[reconfigure]")
{
    /* Widths the gate refuses are legitimate key values: the key records what `configureRings` was handed and
       the gate decides separately (INDEX §3 rule 2), and `LayoutGateTests` owns the width rule. */
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, int value) { key.channels = value; },
                                {1, 4, 8});
}

TEST_CASE("reconfigure: chB alone forces a re-prepare", "[reconfigure]")
{
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, int value) {
                                         key.sidechainChannels = value;
                                     },
                                {0, 1, 8});
}

TEST_CASE("reconfigure: chOut alone forces a re-prepare", "[reconfigure]")
{
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, int value) {
                                         key.outputChannels = value;
                                     },
                                {1, 4, 8});
}

TEST_CASE("reconfigure: hop alone forces a re-prepare", "[reconfigure]")
{
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, int value) { key.hop = value; },
                                {64, 256, 512});
}

TEST_CASE("reconfigure: grainLength alone forces a re-prepare", "[reconfigure]")
{
    expectOneFieldDistinguishes(kDefault, [](ReconfigureKey& key, int value) {
                                         key.grainLength = value;
                                     },
                                {1024, 4096, 16384, 65536});
}

TEST_CASE("reconfigure: seven fields equal means the same configuration", "[reconfigure]")
{
    const ReconfigureKey same{48000.0, kHostBlock, 2, 2, 2, 128, 8192};
    CHECK(kDefault == same);
    CHECK(ReconfigureKey{48000.0, kHostBlock, 1, 8, 1, 256, 32768}
          == ReconfigureKey{48000.0, kHostBlock, 1, 8, 1, 256, 32768});

    /* The seven legs above each move one field, so together they are a complete cover: a comparison that stops
       reading one of them reddens this count *and* exactly one named case, which is what makes `04` §5 arms 3-6
       diagnosable rather than a wall of red. Measured: each dropped field reddens 2 cases of 14 — its own, and
       this one — and comparing only (hop, grainLength) reddens 6. */
    int unchanged = 0;
    for (const ReconfigureKey variant : {changedRate(kDefault), changedHostBlock(kDefault),
                                         changedChannels(kDefault), changedSidechain(kDefault),
                                         changedOutputs(kDefault), changedHop(kDefault),
                                         changedGrainLength(kDefault)})
        if (variant == kDefault)
            ++unchanged;
    CHECK(unchanged == 0);
}

TEST_CASE("reconfigure: the initial key never matches a real configuration", "[reconfigure]")
{
    /* What the shell relies on for its first prepare: the key lives in a `std::optional`, so there is no
       stored value to compare against and nothing can be skipped. Should that member ever become a plain
       `ReconfigureKey`, the value-initialised one is the fallback sentinel — and it matches none of the
       configurations below either, which is the property that makes a first prepare unskippable however the
       sentinel is spelled. */
    std::optional<ReconfigureKey> nothingToCompareAgainst;
    CHECK_FALSE(nothingToCompareAgainst.has_value());

    const ReconfigureKey blank{};
    int matched = 0;
    for (const int hop : {64, 128, 256, 512})
        for (const auto& triple : {std::pair<int, int>{1, 0}, {1, 1}, {1, 8}, {2, 0}, {2, 1}, {2, 2}})
        {
            const auto [chA, chB] = triple;
            for (const int grainLength : {1024, 8192, 65536})
                for (const int hostBlock : {1, 64, kHostBlock, 8192})
                    for (const double rate : {44100.0, 48000.0, 96000.0})
                        if (blank == ReconfigureKey{rate, hostBlock, chA, chB, chA, hop, grainLength})
                            ++matched;
        }
    CHECK(matched == 0);
}

TEST_CASE("reconfigure: a refused triple clears the reconfigure key", "[reconfigure]")
{
    const ReconfigureKey accepted{48000.0, kHostBlock, 2, 2, 2, 128, 8192};
    const ReconfigureKey refused{48000.0, kHostBlock, 4, 4, 4, 128, 8192};

    GuardTranscription guard;
    CHECK(guard.configure(accepted, true));

    /* Two configures in a row on the refused triple: both take the refusal path, and neither rebuilds
       anything, so the accepted layout that preceded them is untouched between them. */
    CHECK_FALSE(guard.configure(refused, false));
    CHECK_FALSE(guard.holdsAKey());
    CHECK_FALSE(guard.configure(refused, false));
    CHECK_FALSE(guard.holdsAKey());

    /* And because the refusal left nothing behind, returning to the accepted triple is a prepare rather than
       a skip. Storing the refused key instead would make the comparison below succeed on a configuration that
       was never built — silence with no reason attached, which is the state AGENTS §13 carried. */
    CHECK(guard.configure(accepted, true));
    CHECK_FALSE(guard.configure(accepted, true));
}

TEST_CASE("reconfigure: prepare sizes by source in and processed out", "[reconfigure]")
{
    /* C1: the figures of `04` §1.2 and §1.3, taken from allocated bytes rather than from the expression at
       `HopScheduler.cpp:53`. Every triple the shipped gate accepts at its two ends — mono with no sidechain,
       the widest layout that shipped before D-021, and both halves of the consolidated family — over the four
       hops the UI can select. */
    for (const int hop : {64, 128, 256, 512})
    {
        for (const auto& triple : {std::tuple<int, int, int>{1, 0, 1}, {2, 2, 2}, {1, 2, 1}, {1, 8, 1}})
        {
            const auto [chA, chB, chOut] = triple;
            const int processed = processedFor(chA, chB, chOut);
            PassthroughEngine engine;
            HopScheduler scheduler(engine);

            const std::ptrdiff_t measured = prepareAndMeasureBytes(scheduler, configFor(hop, chA, chB, chOut));

            INFO("H=" << hop << " triple (" << chA << "," << chB << "," << chOut
                      << ") processed " << processed);
            CHECK(measured == static_cast<std::ptrdiff_t>(expectedPrepareBytes(hop, chA, chB, processed)));

            /* The ceiling and the consolidation, stated as counts the allocation has to back up: eight B
               rings for a side that reaches the Engine as one channel, and a ring capacity that is the power
               of two above floor + hop whatever the host's block would have been. */
            if (chB == 8)
                CHECK(processed == 1);
            CHECK(ringCapacityBytes(hop) >= static_cast<std::size_t>(kHostBlock + hop) * sizeof(float));
            CHECK(ringCapacityBytes(hop) < static_cast<std::size_t>(2 * (kHostBlock + hop)) * sizeof(float));
        }
    }

    /* The consolidated side is the expensive one and the Engine never sees it: eight input rings for one
       processed channel. `04` §1.3 draws the consequence from these numbers — the ceiling triple costs four
       rings more than the widest layout that shipped before D-021, because that one spends its extra channel
       on the output side while the fold spends its seven on the way in. Measured, and exact. */
    PassthroughEngine engine;
    HopScheduler wide(engine);
    const std::ptrdiff_t ceiling = prepareAndMeasureBytes(wide, configFor(128, 1, kMaxChannels, 1));
    HopScheduler stereo(engine);
    const std::ptrdiff_t widestLegacy = prepareAndMeasureBytes(stereo, configFor(128, 2, 2, 2));
    const std::ptrdiff_t rings = static_cast<std::ptrdiff_t>(
        (1 + kMaxChannels + 1 - (2 + 2 + 2)) * ringCapacityBytes(128));
    const std::ptrdiff_t scratch = static_cast<std::ptrdiff_t>(
        (1 + kMaxChannels + 1 + 1 - (2 + 2 + 2 + 1)) * 128 * sizeof(float));
    const std::ptrdiff_t ringObjects = static_cast<std::ptrdiff_t>(4 * sizeof(Ring));
    const std::ptrdiff_t pointers = -static_cast<std::ptrdiff_t>(3 * sizeof(float*));
    CHECK(ceiling > widestLegacy);
    CHECK(ceiling - widestLegacy == rings + scratch + ringObjects + pointers);
}

TEST_CASE("reconfigure: a narrower re-prepare releases the sidechain rings", "[reconfigure]")
{
    /* C2: behaviour, not a limit. `inputA`/`inputB`/`output` are cleared and rebuilt, so a narrowing frees the
       rings it no longer needs — exactly the sidechain rings, measured against the ring capacity a prepare
       allocates at this hop — and the way back costs the same number of bytes. */
    for (const int hop : {128, 512})
    {
        PassthroughEngine engine;
        HopScheduler scheduler(engine);

        const std::ptrdiff_t wide = prepareAndMeasureBytes(scheduler, configFor(hop, 1, kMaxChannels, 1));
        const std::ptrdiff_t narrowing = prepareAndMeasureBytes(scheduler, configFor(hop, 1, 1, 1));
        INFO("H=" << hop);
        CHECK(narrowing < 0);
        CHECK(narrowing == -static_cast<std::ptrdiff_t>((kMaxChannels - 1) * ringCapacityBytes(hop)));

        const std::ptrdiff_t backToWide = prepareAndMeasureBytes(scheduler, configFor(hop, 1, kMaxChannels, 1));
        CHECK(backToWide > 0);
        CHECK(backToWide == -narrowing);
    }
}

TEST_CASE("reconfigure: hop scratch keeps its capacity across a narrowing", "[reconfigure]")
{
    /* C10: the other half of `04` §1.6, recorded rather than assumed away. The ring vectors are cleared and
       rebuilt, so their storage comes back; every hop scratch goes through `assign`, which does not shrink
       capacity, so the high-water of chB x hop floats stays allocated whatever comes next — and so does the
       ring-object array, whose capacity `clear` likewise leaves alone. */
    const int hop = 256;
    PassthroughEngine engine;
    HopScheduler reused(engine);

    const std::ptrdiff_t wide = prepareAndMeasureBytes(reused, configFor(hop, 1, kMaxChannels, 1));
    const std::ptrdiff_t narrowing = prepareAndMeasureBytes(reused, configFor(hop, 1, 1, 1));
    const std::ptrdiff_t heldAtNarrow = wide + narrowing;

    PassthroughEngine freshEngine;
    HopScheduler fresh(freshEngine);
    const std::ptrdiff_t preparedNarrow = prepareAndMeasureBytes(fresh, configFor(hop, 1, 1, 1));

    const std::ptrdiff_t retained = heldAtNarrow - preparedNarrow;
    CHECK(retained == static_cast<std::ptrdiff_t>(kMaxChannels - 1)
                          * static_cast<std::ptrdiff_t>(hop * sizeof(float) + sizeof(Ring)));
}

TEST_CASE("reconfigure: the callback after a re-prepare does not allocate", "[reconfigure]")
{
    /* C8: the ceiling triple, including the fold's first hop — the one path that had no reason to have been
       warm before, since the fold's mono scratch is written between the B hop-reads and processHop. Harness
       buffers are at their final size before the watch, so anything counted inside belongs to the scheduler. */
    const int hop = 256;
    const int processed = processedFor(1, kMaxChannels, 1);
    const std::vector<int> blocks{1, 64, 255, 256, 4096, 0, 1000};

    PassthroughEngine engine;
    HopScheduler scheduler(engine);
    scheduler.prepare(configFor(hop, 1, kMaxChannels, 1));

    CallbackBuffers buffers(1, kMaxChannels, processed, 4096);
    CHECK(runCallbacksAllocations(scheduler, buffers, processed, blocks) == 0);

    /* Again after a narrowing and back, which is the sequence a host poking layouts on session load drives.
       The narrow shape does not fold, so the way back is the first fold hop since the buffer was resized. */
    scheduler.prepare(configFor(hop, 1, 1, 1));
    CallbackBuffers narrowBuffers(1, 1, processedFor(1, 1, 1), 4096);
    CHECK(runCallbacksAllocations(scheduler, narrowBuffers, processedFor(1, 1, 1), blocks) == 0);

    scheduler.prepare(configFor(hop, 1, kMaxChannels, 1));
    CHECK(runCallbacksAllocations(scheduler, buffers, processed, blocks) == 0);
}
