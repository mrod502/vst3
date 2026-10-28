#pragma once

#include "RingSpan.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace grain
{
/** Sums successive grain results and hands out the H that become final each hop.

    One instance per output channel. prepare allocates; emitAndAdd allocates nothing, so it is
    audio-thread safe. One instance is driven by one thread.

    FIXED ORDER, and the order is the latency model: emit the H samples at the head and zero
    them, advance the head by H, then add the arriving grain at the new head. The emitted block
    is therefore everything before the next grain's contribution — which is why the block is
    final, why physical delay is L, and why output sample q draws on input samples below q
    alone. Adding before emitting shifts the wet stream one hop early, which breaks the delta
    test's n0+L anchor and makes output q depend on input at and after q. See F-005.

    The accumulator knows nothing about the reported latency R: R is a declaration and a dry-path
    delay, never a change to this arithmetic. */
class OlaAccumulator
{
public:
    /** @param grainLength L.
        @param hop         H, samples emitted per call.
        @param sizeF       F, a power of two >= 2L-1 and the ring capacity. */
    void prepare(int grainLength, int hop, int sizeF);

    void reset() noexcept;

    /** @param y     normalized IFFT output; only y[0, 2L-1] is read, so slots beyond the linear
                    convolution length may hold anything the backend left there.
        @param scale engine gain, folded into the add and applied exactly once.
        @param out   H samples. */
    void emitAndAdd(const float* y, float scale, float* out) noexcept;

    /** Absolute output position of the next sample to emit; 0 at reset, +H per call. */
    std::int64_t head() const noexcept { return headPosition; }

private:
    int grainLengthValue = 0;
    int hopValue = 0;
    int sizeFValue = 0;
    std::size_t mask = 0;
    std::vector<float> ring;
    std::int64_t headPosition = 0;
};
}
