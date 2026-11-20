#pragma once

#include "Container.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace container
{
/** Fixed-capacity single-producer / single-consumer ring of small fixed-size records, for publishing a
    per-step reading to a reader that runs slower than the writer.

    Two differences from `CircularBuffer`, both of them what a trace needs. A full ring **overwrites the
    oldest** unread record rather than refusing the new one: the newest record is the current state of the
    signal, and a publication that drops it stops meaning anything, while an unread old one is history the
    reader has simply missed. And every slot carries a sequence tag, so a record the writer was
    mid-overwrite on is detected and never delivered half-written.

    One writer, one reader. No locks, no waiting, no allocation after construction. The writer owns
    `produced` and the reader owns `consumed`; `published` is the only index either side reads from the
    other, and it is released after the slot's tag, so a record the reader can see has finished being
    written. The tags make the record reads safe; nothing relies on a multi-word index being atomic.
*/
template <typename T>
class TraceRing
{
public:
    /** @param requestedCapacity rounded up to a power of two; `0` becomes `1`. Allocates here, so
        construction belongs in `prepare` and never on the step path. */
    explicit TraceRing(std::size_t requestedCapacity)
        : stride(detail::roundToPowerOfTwo(requestedCapacity)),
          mask(stride - 1),
          slots(stride)
    {
    }

    /** Usable capacity — the rounded-up value the storage was built to. */
    [[nodiscard]] std::size_t capacity() const noexcept { return stride; }

    /** Records written since construction, including those overwritten before anyone read them. */
    [[nodiscard]] std::size_t writes() const noexcept
    {
        return static_cast<std::size_t>(published.load(std::memory_order_acquire));
    }

    /** Records overwritten while unread since construction — the count `write` charges itself for, and
        the figure a leg compares against an deliberate overflow. */
    [[nodiscard]] std::size_t dropped() const noexcept
    {
        return static_cast<std::size_t>(droppedCount.load(std::memory_order_relaxed));
    }

    /** The writer's side: publishes `record`, lapping the oldest unread record when the ring is full.
        Never fails, never blocks, never allocates. */
    void write(const T& record) noexcept
    {
        const std::uint64_t index = produced;
        Slot& slot = slots[static_cast<std::size_t>(index & mask)];

        slot.tag.store(tagWriting(index), std::memory_order_relaxed);
        slot.value = record;
        slot.tag.store(tagWritten(index), std::memory_order_release);

        ++produced;
        published.store(produced, std::memory_order_release);

        /* Unread depth past the capacity is what the reader can no longer reach. `consumed` is read for
           the count only; a stale value can only under-report a drop, never invent one. */
        if (produced - consumed.load(std::memory_order_relaxed) > stride)
            droppedCount.fetch_add(1, std::memory_order_relaxed);
    }

    /** The reader's side: takes the newest records that fit in `out`, oldest first, and consumes them.
        @return how many it delivered, `<= out.size()`.

        A record whose tag moves under the copy is retried `retries` times and then skipped rather than
        guessed at, and delivery stops there so what came back is a contiguous run in time. A short return
        is therefore "the writer was lapping this slot", never "half a record". */
    std::size_t drainNewestInto(std::span<T> out) noexcept
    {
        std::uint64_t first = 0;
        const std::size_t delivered = copyNewest(out, first);
        consumed.store(first + delivered, std::memory_order_release);
        return delivered;
    }

    /** The reader's side, without consuming: the newest records that fit in `out`, oldest first.

        The same copy and the same no-torn-read guarantee as `drainNewestInto`, and nothing else happens —
        the window stays whole, so two readers of one ring see the same history instead of dividing it. That
        is what a display that polls a rolling window needs, and why the two panels and the processor's own
        snapshot getter can share one ring. */
    std::size_t peekNewestInto(std::span<T> out) const noexcept
    {
        std::uint64_t ignored = 0;
        return copyNewest(out, ignored);
    }

    /** The reader's side, without consuming: the newest published record, or `false` when there is none.
        A record caught mid-overwrite is retried and then reported absent rather than half-read. */
    bool newest(T& destination) const noexcept
    {
        const std::uint64_t head = published.load(std::memory_order_acquire);
        return head > 0 && takeAt(head - 1, destination);
    }

    /** How many records the reader has not consumed yet, capped at capacity — what a full-capacity drain
        would deliver. */
    [[nodiscard]] std::size_t size() const noexcept
    {
        const std::uint64_t head = published.load(std::memory_order_acquire);
        const std::uint64_t oldest = oldestReadable(head);
        return head > oldest ? static_cast<std::size_t>(head - oldest) : 0;
    }

    /** The reader's side: discards everything unread. */
    void clear() noexcept
    {
        consumed.store(published.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    static constexpr std::uint64_t tagWritten(std::uint64_t index) noexcept
    {
        return 2 * index + 2;
    }
    static constexpr std::uint64_t tagWriting(std::uint64_t index) noexcept
    {
        return 2 * index + 1;
    }

    /** Copies the newest published records that fit in `out`, oldest first, and reports the index of the
        first one copied — `head` itself when nothing was copyable, so a caller advancing past `[first,
        first + delivered)` is correct on the empty path too. */
    std::size_t copyNewest(std::span<T> out, std::uint64_t& firstOut) const noexcept
    {
        const std::uint64_t head = published.load(std::memory_order_acquire);
        firstOut = head;

        if (out.empty())
            return 0;

        const std::uint64_t oldest = oldestReadable(head);
        if (head <= oldest)
            return 0;

        const std::size_t readable = static_cast<std::size_t>(head - oldest);
        const std::size_t count = readable < out.size() ? readable : out.size();
        const std::uint64_t start = head - count;

        std::size_t delivered = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!takeAt(start + i, out[i]))
                break;
            ++delivered;
        }

        firstOut = start;
        return delivered;
    }

    struct Slot
    {
        std::atomic<std::uint64_t> tag{0};
        T value{};
    };

    /** Reads slot `index` only if its tag says the record at this lap is complete and unmoved. */
    [[nodiscard]] bool takeAt(std::uint64_t index, T& destination) const noexcept
    {
        const Slot& slot = slots[static_cast<std::size_t>(index & mask)];
        const std::uint64_t expected = tagWritten(index);

        for (int attempt = 0; attempt < retries; ++attempt)
        {
            if (slot.tag.load(std::memory_order_acquire) != expected)
                return false;
            destination = slot.value;
            if (slot.tag.load(std::memory_order_acquire) == expected)
                return true;
        }
        return false;
    }

    /** The oldest record still physically present: everything earlier has been overwritten. */
    [[nodiscard]] std::uint64_t oldestReadable(std::uint64_t head) const noexcept
    {
        const std::uint64_t reader = consumed.load(std::memory_order_acquire);
        return head - reader > stride ? head - stride : reader;
    }

    /** A slot caught being lapped is re-read this many times before it is skipped. The window in which a
        slot is unstable is one assignment, so one pass is normally enough; more buys nothing on a ring the
        reader visits every step and bounds the wait when it does not. */
    static constexpr int retries = 3;

    std::size_t stride;
    std::size_t mask;
    std::vector<Slot> slots;

    std::uint64_t produced = 0;
    std::atomic<std::uint64_t> published{0};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<std::uint64_t> droppedCount{0};
};
}
