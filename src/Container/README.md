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
