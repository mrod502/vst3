#pragma once

#include "RingSpan.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace grain
{
/** The last L samples of one signal on one channel, handed out windowed and zero-padded.

    One instance per (signal, channel). The engine owns these; the scheduler's FIFO is never
    asked to retain L samples, since its capacity bound is one host callback plus a hop.

    prepare allocates; push and extractWindowed allocate nothing, so both are audio-thread safe.
    One instance is driven by one thread. */
class GrainHistory
{
public:
    /** @param grainLength L, a multiple of hop.
        @param hop         H, the step between successive extractions.
        @param sizeF       F, a power of two >= 2L-1; only the [L, F) tail of the destination is
                           sized by it, so the transform that consumes the result never reuses
                           the previous hop's padding. */
    void prepare(int grainLength, int hop, int sizeF);

    /** Zeroes the history and restarts the counter, so a re-prepared engine cannot leak the
        previous take into its first grain. */
    void reset() noexcept;

    /** @param count must be H. */
    void push(const float* x, int count) noexcept;

    /** dst[0, L) = window[i] · x[t-L+i]; dst[L, F) = 0.

        Absolute indices below zero read as silence, which is what makes the first grain after
        reset (start H-L < 0) fall out of the same code path instead of needing a special case. */
    void extractWindowed(const float* window, float* dst) const noexcept;

    int grainLength() const noexcept { return grainLengthValue; }

    int hop() const noexcept { return hopValue; }

    int sizeF() const noexcept { return sizeFValue; }

    std::int64_t written() const noexcept { return writtenCount; }

private:
    int grainLengthValue = 0;
    int hopValue = 0;
    int sizeFValue = 0;
    std::size_t mask = 0;
    std::vector<float> ring;
    std::int64_t writtenCount = 0;
};
}
