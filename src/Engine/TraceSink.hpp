#pragma once

#include "ChannelMap.hpp"

#include <TraceRing.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace engine
{
/** One channel's four level-stage readings for one hop, in the order the two display panels draw them.

    Plain floats rather than the stage's own types because the snapshot crosses a thread boundary by value
    and is copied into and out of the ring: whatever travels has to be trivially copyable and fixed-size, so
    the reader never dereferences a live `LevelDetector`.

    `envelopeConv` is the level of the convolved stream *before* the stage scaled it, which is what
    `LevelNormalizer` publishes and what the envelope panel's third trace is (D-015's upstream reading). */
struct TraceReading
{
    float envelopeA = 0.0f;
    float envelopeB = 0.0f;
    float envelopeConv = 0.0f;
    float gainDb = 0.0f;
};

/** Everything the level stage decided for one hop, for every processed channel.

    Sized by `kMaxChannels` rather than by the hop's own count, so one snapshot type fits every layout the
    gate accepts and the ring's slots stay interchangeable. `channels` says how many of them were written:
    the rest hold the zero with which the snapshot was constructed, never a previous hop's leftover, because
    the writer builds a fresh snapshot per hop rather than editing a member.

    144 bytes, and the static_assert below is what keeps the ring's capacity arithmetic honest about it. */
struct TraceSnapshot
{
    std::uint64_t hopIndex = 0;
    std::uint32_t channels = 0;
    std::array<TraceReading, static_cast<std::size_t>(kMaxChannels)> reading{};
};

static_assert(sizeof(TraceSnapshot) <= 8 + 4 + static_cast<std::size_t>(kMaxChannels) * 4 * sizeof(float) + 8,
              "TraceSnapshot outgrew the bound its ring capacity is budgeted against");

/** The hop path's side of the publication: a non-owning borrow of the ring a display is reading.

    Injected beside `installLevelNormalizer` rather than reached through an accessor into the stage, so
    nothing below the shell learns that anything is watching: the engine's only act is
    `sink.publish (snapshot)`, which on an empty sink is one null test and nothing else. The ring is owned
    above the seam — the processor owns it, the editor reads it, the engine writes through this. */
class TraceSink
{
public:
    using Ring = container::TraceRing<TraceSnapshot>;

    TraceSink() noexcept = default;

    /** @param ringToUse a borrow, never an ownership transfer; null is the parked state, and the state a
        chain that renders for the harness runs with. */
    explicit TraceSink(Ring* ringToUse) noexcept
        : ring(ringToUse)
    {
    }

    /** One relaxed publish per hop. Nothing here can fail, block or allocate: the ring's own capacity is
        fixed at construction and it laps its oldest snapshot rather than refusing this one. */
    void publish(const TraceSnapshot& snapshot) const noexcept
    {
        if (ring != nullptr)
            ring->write(snapshot);
    }

    [[nodiscard]] bool empty() const noexcept { return ring == nullptr; }

private:
    Ring* ring = nullptr;
};
}
