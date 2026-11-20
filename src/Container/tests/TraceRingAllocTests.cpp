#include <TraceRing.hpp>

#include <catch_amalgamated.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <span>

using container::TraceRing;

namespace
{
/** The hop path must not allocate. Replacing global operator new is allowed once per program, which is why
    this file is its own binary rather than a second TU of ContainerTests — the shape follows
    Engine/tests/MixEngineTests.cpp. */
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

/** The shape the shell actually publishes: a position and a block of per-channel readings, copied by value.
    Container cannot include the engine's snapshot type — the dependency runs the other way — so the payload
    here matches it in kind (trivially copyable, a few hundred bytes) rather than in name. */
struct Snapshot
{
    std::uint64_t hopIndex = 0;
    std::array<float, 32> reading {};
};

constexpr std::size_t kCapacity = 256;
constexpr int kIterations = 4000;
}

TEST_CASE("trace ring: a constructed ring allocates nothing on either side of the seam", "[trace ring]")
{
    /* The ring is built inside prepareToPlay and read on the audio path for the life of the playback
       session, so the only two rules are that construction owns the memory and nothing after it does. */
    Snapshot sample {};
    std::array<Snapshot, 8> window {};

    TraceRing<Snapshot> ring(kCapacity);
    ring.write(sample);

    const AllocationWatch watch;

    for (int i = 0; i < kIterations; ++i)
    {
        ring.write(sample);
        ring.peekNewestInto({window.data(), window.size()});
        ring.drainNewestInto({window.data(), window.size()});
        ring.newest(sample);
        ring.clear();
    }

    const long counted = allocationCount;
    armAllocations = false;

    CHECK(counted == 0);
}

TEST_CASE("trace ring: an overflow allocates nothing either — drop-oldest never grows", "[trace ring]")
{
    /* The arm that would otherwise pass criterion 3 unnoticed: a ring that reallocates to survive overflow is
       correct on every equality test and allocates on the audio thread forever. */
    TraceRing<Snapshot> ring(4);
    Snapshot sample {};

    for (int i = 0; i < 4; ++i)
        ring.write(sample);

    const AllocationWatch watch;

    for (int i = 0; i < kIterations; ++i)
        ring.write(sample);

    const long counted = allocationCount;
    armAllocations = false;

    CHECK(counted == 0);
    CHECK(ring.capacity() == 4);
    CHECK(ring.size() == 4);
}

TEST_CASE("trace ring: a span wider than the window allocates nothing on the read side", "[trace ring]")
{
    /* The editor's window buffer is sized to the ring, so every read is at capacity. A read that resized the
       caller's span would show up here rather than as a message-thread allocation. */
    TraceRing<Snapshot> ring(kCapacity);
    Snapshot sample {};
    std::array<Snapshot, kCapacity> window {};

    for (int i = 0; i < static_cast<int> (kCapacity); ++i)
        ring.write(sample);

    const AllocationWatch watch;

    for (int i = 0; i < kIterations; ++i)
    {
        ring.peekNewestInto({window.data(), window.size()});
        ring.drainNewestInto({window.data(), window.size()});
    }

    const long counted = allocationCount;
    armAllocations = false;

    CHECK(counted == 0);
}
