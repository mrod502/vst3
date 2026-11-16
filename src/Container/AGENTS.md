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
and `clear` touch no allocator and take no lock.

## Build and test

```bash
cmake --build build/release --target Container -j4
ctest --test-dir build/release -R CircularBuffer --output-on-failure
```

Tests live in `tests/` and link no JUCE. `-R` matches Catch2 case names, not target names.

## Decisions

D-022 (the host-block floor) — see `docs/decisions.md`.
