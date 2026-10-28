#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace grain
{
/** @return the smallest power of two >= value; 0 and 1 become 1. */
[[nodiscard]] constexpr std::size_t powerOfTwoAtLeast(std::size_t value) noexcept
{
    std::size_t p = 1;
    while (p < value)
        p <<= 1;
    return p;
}

/** A run of `count` ring slots starting at an absolute index, cut at most once by the mask
    boundary. `secondCount` elements live at offsets [0, secondCount).

    The cut is the one place either ring user could get wrap-around wrong, so it is computed
    here and nowhere else. */
struct RingSegments
{
    std::size_t firstOffset;
    int firstCount; ///< >= 1 when count > 0
    int secondCount;
};

/** @param capacity slot count, a power of two.
    @param first    absolute index of the first element; any int64 value, including one below
                    this epoch — only its low bits matter.
    @param count    elements, <= capacity, so no slot is visited twice. */
[[nodiscard]] inline RingSegments segmentsFor(std::size_t capacity, std::int64_t first,
                                              int count) noexcept
{
    assert(capacity >= 1 && (capacity & (capacity - 1)) == 0);
    assert(count >= 0 && static_cast<std::size_t>(count) <= capacity);

    const std::size_t mask = capacity - 1;
    const std::size_t offset =
        static_cast<std::size_t>(first & static_cast<std::int64_t>(mask));
    const int head = static_cast<int>(capacity - offset);

    if (count <= head)
        return {offset, count, 0};
    return {offset, head, count - head};
}
}
