# Container

Fixed-capacity containers for the audio path. JUCE-light: nothing here includes a JUCE
header, so anything built on it compiles and tests as a plain console program.

## Public contract

`container::CircularBuffer<T>` — a FIFO whose capacity is fixed at construction:

| Member | Contract |
|---|---|
| `CircularBuffer(n)` | capacity rounded up to a power of two; `0` becomes `1`. Allocates once. |
| `capacity()` | the rounded-up, usable capacity. |
| `availableRead()` / `availableWrite()` | samples readable / writable right now; sum to `capacity()`. |
| `write(src, n)` | takes `min(n, availableWrite())`, returns what it took. |
| `read(dst, n)` | delivers `min(n, availableRead())`, returns what it delivered. |
| `clear()` | drops everything unread. |

There is deliberately **no growth path**. A full buffer discards the excess and reports the
short count, so an undersized ring shows up as a wrong return value the caller can assert on,
never as a reallocation on the audio thread. Wrap-around is internal: cursors run
monotonically and only their low bits index storage, so callers cannot get the wrap wrong.

Single-producer / single-consumer in practice (one writer, one reader per ring), but nothing
is atomic — see `src/Engine` for the thread story.

## The cost of the power of two

Stated once here because every caller inherits it and none of them can avoid it. The scheduler sizes each of its
rings at `maxHostBlock + hop` (`src/Engine`), where `maxHostBlock` is the host's callback size floored at 4096
(D-022 A), so what is *requested* barely moves while what is *allocated* jumps a power of two:

| host block | requested at H = 64 / 128 / 256 / 512 | `capacity()` | waste at H = 128 |
|---|---|---|---|
| 64 … 4096 | 4160 / 4224 / 4352 / 4608 | **8192 floats = 32 KiB** | 1.94× |
| 8192 | 8256 / 8320 / 8448 / 8704 | 16384 floats = 64 KiB | 1.97× |
| 65536 | 65600 / 65664 / 65792 / 66048 | 131072 floats = 512 KiB | 2.00× |

Measured per configuration by `tools/PrepareFootprint` (Linux aarch64, gcc 13.3, Release), not derived, and it is
the rounding rather than the hop that owns the byte: at any hop the shell can dial with a host block at or below
the floor, an input ring is 32 KiB whether it needed 4160 samples or 4608. The rounding is not negotiable — mask
indexing requires the power of two — and D-022 C decided against a tighter requested capacity for the same reason:
the only alternative is exact sizing, which is a different trade against a different set of callers, not this
item's edit. `assign`-style reuse has the same shape one level up: a `CircularBuffer` that is destroyed releases
its storage, and one that is only re-`assign`ed keeps its high-water.

## Threading

Construction allocates, so it belongs in `prepareToPlay` or earlier. `read`, `write`,
`available*` and `clear` touch no allocator and take no lock.

## Dependencies

None. `<algorithm>`, `<cstddef>`, `<cstdint>`, `<vector>`.

## Build and test

```bash
cmake --build build/release --target Container -j4      # registered in src/CMakeLists.txt
ctest --test-dir build/release -R CircularBuffer --output-on-failure
```

Tests live in `tests/` and link no JUCE.
