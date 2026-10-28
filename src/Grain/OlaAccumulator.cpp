#include "OlaAccumulator.hpp"

#include <algorithm>

namespace grain
{
void OlaAccumulator::prepare(int grainLength, int hop, int sizeF)
{
    assert(grainLength > 0 && hop > 0 && sizeF > 0);
    assert(grainLength % hop == 0);
    assert(sizeF >= 2 * grainLength - 1);
    assert((sizeF & (sizeF - 1)) == 0);

    grainLengthValue = grainLength;
    hopValue = hop;
    sizeFValue = sizeF;

    const std::size_t capacity = powerOfTwoAtLeast(static_cast<std::size_t>(sizeF));
    mask = capacity - 1;
    ring.assign(capacity, 0.0f);
    headPosition = 0;
}

void OlaAccumulator::reset() noexcept
{
    std::fill(ring.begin(), ring.end(), 0.0f);
    headPosition = 0;
}

void OlaAccumulator::emitAndAdd(const float* y, float scale, float* out) noexcept
{
    const int liveLength = 2 * grainLengthValue - 1;

    const RingSegments emitted = segmentsFor(ring.size(), headPosition, hopValue);
    const auto* first = ring.data() + emitted.firstOffset;
    std::copy_n(first, emitted.firstCount, out);
    std::copy_n(ring.data(), emitted.secondCount, out + emitted.firstCount);

    std::fill_n(ring.data() + emitted.firstOffset, emitted.firstCount, 0.0f);
    std::fill_n(ring.data(), emitted.secondCount, 0.0f);

    headPosition += hopValue;

    const RingSegments added = segmentsFor(ring.size(), headPosition, liveLength);
    auto* destination = ring.data();
    for (int i = 0; i < added.firstCount; ++i)
        destination[added.firstOffset + static_cast<std::size_t>(i)]
            += scale * y[i];
    for (int i = 0; i < added.secondCount; ++i)
        destination[static_cast<std::size_t>(i)] += scale * y[added.firstCount + i];
}
}
