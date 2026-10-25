#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace container
{
namespace detail
{
inline constexpr std::size_t roundToPowerOfTwo(std::size_t v) noexcept
{
    std::size_t p = 1;
    while (p < v)
        p <<= 1;
    return p;
}
}

/** Fixed-capacity FIFO for the audio path.

    Capacity is fixed at construction and never grows. Read and write cursors run
    monotonically and only their low bits index the storage, so wrap-around is owned here
    and a caller can never index outside the buffer. There is no growth path: a write into
    a full buffer takes what fits and reports how little was accepted.
*/
template <typename T>
class CircularBuffer
{
public:
    /** @param requestedCapacity rounded up to a power of two; 0 becomes 1. */
    explicit CircularBuffer(std::size_t requestedCapacity)
        : stride(detail::roundToPowerOfTwo(requestedCapacity)),
          mask(stride - 1),
          data(stride)
    {
    }

    /** Usable capacity — the rounded-up value the storage was built to. */
    std::size_t capacity() const noexcept { return stride; }

    std::size_t availableRead() const noexcept
    {
        return static_cast<std::size_t>(writeCursor - readCursor);
    }

    std::size_t availableWrite() const noexcept { return stride - availableRead(); }

    /** @return samples taken, <= min(count, availableWrite()). */
    std::size_t write(const T *source, std::size_t count) noexcept
    {
        const std::size_t taken = std::min(count, availableWrite());
        const std::size_t head = std::min(taken, stride - slot(writeCursor));
        copyIn(source, head, slot(writeCursor));
        copyIn(source + head, taken - head, 0);
        writeCursor += taken;
        return taken;
    }

    /** @return samples copied out, <= min(count, availableRead()). */
    std::size_t read(T *destination, std::size_t count) noexcept
    {
        const std::size_t taken = std::min(count, availableRead());
        const std::size_t head = std::min(taken, stride - slot(readCursor));
        copyOut(head, slot(readCursor), destination);
        copyOut(taken - head, 0, destination + head);
        readCursor += taken;
        return taken;
    }

    /** Discards everything unread. */
    void clear() noexcept { readCursor = writeCursor; }

private:
    std::size_t slot(std::uint64_t cursor) const noexcept
    {
        return static_cast<std::size_t>(cursor & mask);
    }

    void copyIn(const T *source, std::size_t count, std::size_t destination) noexcept
    {
        for (std::size_t i = 0; i < count; ++i)
            data[destination + i] = source[i];
    }

    void copyOut(std::size_t count, std::size_t source, T *destination) noexcept
    {
        for (std::size_t i = 0; i < count; ++i)
            destination[i] = data[source + i];
    }

    std::size_t stride;
    std::size_t mask;
    std::vector<T> data;
    std::uint64_t writeCursor = 0;
    std::uint64_t readCursor = 0;
};
}
