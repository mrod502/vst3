#include "GrainHistory.hpp"

#include <algorithm>
#include <cmath>

namespace grain
{
void GrainHistory::prepare(int grainLength, int hop, int sizeF)
{
    assert(grainLength > 0 && hop > 0 && sizeF > 0);
    assert(grainLength % hop == 0);
    assert(sizeF >= 2 * grainLength - 1);
    assert((sizeF & (sizeF - 1)) == 0);

    grainLengthValue = grainLength;
    hopValue = hop;
    sizeFValue = sizeF;

    const std::size_t capacity = powerOfTwoAtLeast(static_cast<std::size_t>(grainLength));
    mask = capacity - 1;
    ring.assign(capacity, 0.0f);
    writtenCount = 0;
}

void GrainHistory::reset() noexcept
{
    std::fill(ring.begin(), ring.end(), 0.0f);
    writtenCount = 0;
}

void GrainHistory::push(const float* x, int count) noexcept
{
    assert(count == hopValue);

    const RingSegments span = segmentsFor(ring.size(), writtenCount, count);
    std::copy_n(x, span.firstCount, ring.begin() + static_cast<std::ptrdiff_t>(span.firstOffset));
    if (span.secondCount > 0)
        std::copy_n(x + span.firstCount,
                    span.secondCount,
                    ring.begin());

    writtenCount += count;
}

void GrainHistory::extractWindowed(const float* window, float* dst) const noexcept
{
    const int unavailable =
        static_cast<int>(std::max<std::int64_t>(0, grainLengthValue - writtenCount));
    const int available = grainLengthValue - unavailable;
    const std::int64_t firstSample = writtenCount - available;

    std::fill_n(dst, unavailable, 0.0f);

    const RingSegments span = segmentsFor(ring.size(), firstSample, available);
    for (int i = 0; i < span.firstCount; ++i)
        dst[unavailable + i] =
            window[unavailable + i] * ring[static_cast<std::size_t>(span.firstOffset)
                                           + static_cast<std::size_t>(i)];
    for (int i = 0; i < span.secondCount; ++i)
        dst[unavailable + span.firstCount + i] =
            window[unavailable + span.firstCount + i] * ring[static_cast<std::size_t>(i)];

    std::fill_n(dst + grainLengthValue, sizeFValue - grainLengthValue, 0.0f);
}
}
