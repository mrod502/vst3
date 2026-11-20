# Container

Fixed-capacity containers for the audio path. JUCE-light: no JUCE header, no dependencies, so
anything built on it compiles and tests as a plain console program.

## `container::CircularBuffer<T>`

A FIFO whose capacity is fixed at construction.

| Member | Contract |
|---|---|
| `CircularBuffer(n)` | capacity rounded up to a power of two; `0` becomes `1`; allocates once |
| `capacity()` | the rounded-up, usable capacity |
| `availableRead()` / `availableWrite()` | samples readable / writable now; sum to `capacity()` |
| `write(src, n)` | takes `min(n, availableWrite())`, returns what it took |
| `read(dst, n)` | delivers `min(n, availableRead())`, returns what it delivered |
| `clear()` | drops everything unread |

- **No growth path.** A full buffer discards the excess and reports the short count, so an
  undersized ring surfaces as a wrong return value, never as a reallocation on the audio thread.
- **Wrap-around is owned here.** Cursors run monotonically and only their low bits index storage.
- **Single-producer / single-consumer in practice** (one writer, one reader per ring). Nothing is
  atomic; the thread story is `src/Engine`.

## `container::TraceRing<T>`

A fixed-capacity ring of small fixed-size records, published by a fast writer to a slower reader that
wants the *recent past*, not a queue. No locks, no waiting, no allocation after construction.

| Member | Contract |
|---|---|
| `TraceRing(n)` | capacity rounded up to a power of two; `0` becomes `1`; allocates here, so construction belongs in `prepare` |
| `capacity()` | the rounded-up, usable capacity — what a caller sizes its scratch from |
| `write(record)` | publishes, **lapping the oldest unread record** when full; never fails, blocks or allocates |
| `drainNewestInto(span)` | the newest records that fit, oldest first, **consuming**; returns the count |
| `peekNewestInto(span)` | the same copy, **not consuming**, so two readers of one ring see the same history instead of dividing it |
| `newest(dst)` | the newest record, not consuming; `false` when nothing was ever published |
| `size()` | unread depth, capped at capacity — what a full-capacity read would deliver |
| `writes()` / `dropped()` | records published since construction / records lapped while unread |
| `clear()` | discards everything unread |

- **Drop-oldest is the deliberate opposite of `CircularBuffer`.** The newest record is the signal's current
  state and losing it stops the publication meaning anything, while an unread old one is history the reader
  simply missed. That difference, not a missing flag, is why this is a second type.
- **No torn read.** Each slot carries a sequence tag — odd while being written, even once written — so a
  record the writer is lapping mid-copy is detected, retried `retries` times, then skipped, and delivery stops
  there. A short return means "the writer was overtaking this slot", never "half a record".
- **One writer, one reader.** The writer owns `produced`, the reader owns `consumed`; `published` is the only
  index either side reads from the other, and it is released after the slot's tag — a record a reader can see
  has finished being written. The tags make the payloads safe; nothing relies on a multi-word index being atomic.
- **Payloads travel by value.** `T` is fixed-size and trivially copyable, so the reader never dereferences
  anything the writer's side owns.

## Cost of the power of two

The scheduler sizes each ring at `maxHostBlock + hop`, with `maxHostBlock` floored at 4096, so what
is requested barely moves while what is allocated jumps a power of two:

| host block | requested at H = 64 / 128 / 256 / 512 | `capacity()` | waste at H = 128 |
|---|---|---|---|
| 64 … 4096 | 4160 / 4224 / 4352 / 4608 | **8192 floats = 32 KiB** | 1.94× |
| 8192 | 8256 / 8320 / 8448 / 8704 | 16384 floats = 64 KiB | 1.97× |
| 65536 | 65600 / 65664 / 65792 / 66048 | 131072 floats = 512 KiB | 2.00× |

The rounding, not the hop, owns the byte. Mask indexing requires the power of two; exact sizing is
a different trade against a different set of callers. A buffer that is destroyed releases its
storage — one that is only re-assigned keeps its high-water.

## Threading

Construction allocates, so it belongs in `prepareToPlay` or earlier. `read`, `write`, `available*`
and `clear` touch no allocator and take no lock. `TraceRing`'s `write` and its three read paths are the
same, and are safe to run concurrently *as one writer against one reader* — the pairing the tags and the
two owned indices assume.

## Build and test

```bash
cmake --build build/release --target Container -j4
ctest --test-dir build/release -R CircularBuffer --output-on-failure
ctest --test-dir build/release -R "trace ring" --output-on-failure
```

Tests live in `tests/` and link no JUCE. `-R` matches Catch2 case names, not target names.

`TraceRingAllocTests` is a second binary on the rule that one program replaces `operator new` exactly once:
it counts allocations across write, peek, drain, `newest` and `clear`, including an overflow the test asks
for. Both binaries link `EntangleCatch2` and both tag their cases `[trace ring]`, so the filter above
selects the numeric and the allocation legs together.

## Decisions

D-022 (the host-block floor), D-027 (the display publication: why drop-oldest, why a peek window) — see
`docs/decisions.md`.
