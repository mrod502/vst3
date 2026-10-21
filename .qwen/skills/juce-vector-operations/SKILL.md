---
name: juce-vector-operations
description: Speed up block-level audio math with JUCE's SIMD layer — FloatVectorOperations for whole-buffer arithmetic, dsp::SIMDRegister for hand-written vector loops — without breaking alignment, lane-count portability, mask semantics, FMA/denormal handling, or real-time rules.
source: learned
---

# JUCE Vector / SIMD Operations (the performance layer)

Distilled from `juce/modules/juce_audio_basics/buffers/juce_FloatVectorOperations.{h,cpp}`,
`juce/modules/juce_dsp/containers/juce_SIMDRegister{,_Impl}.h`,
`juce/modules/juce_dsp/native/juce_SIMDNativeOps_{sse,avx,neon,fallback}.h`,
`juce/modules/juce_dsp/juce_dsp.h` and `containers/juce_AudioBlock.h` (JUCE 9.0.4).

Use this skill whenever the goal is **making existing per-sample audio math faster**, not
designing a signal path.

## When to Use

- Replacing a hand-written `for (i …) dest[i] += src[i] * g;` loop over an
  `AudioBuffer` channel with a block operation (gain, mixing, DC offset, muting, metering,
  clipping, buffer copies).
- Writing an inner DSP loop that wants explicit vector registers (filters, grain/overlap-add
  kernels, envelope/energy accumulators, complex spectrum arithmetic).
- Denormal (subnormal) performance spikes — CPU jumps when signals fade to silence.
- Portability questions: "why 4 lanes here and 8 there?", "is this buffer aligned?",
  "does this still work on Apple Silicon?"
- Debugging: `SIGSEGV`/`#GP` inside `_mm_load_ps`, garbage in the last few samples of a
  block, an ambiguous `FloatVectorOperations::add` overload, or vector code that is *slower*
  than the scalar loop it replaced.

**Not for:** choosing or wiring processors (`ProcessorChain`, filters, `Convolution`) — that is
the `juce-dsp-processing` skill. Parameters/persistence → `juce-apvts-parameters`.

## The three layers — always use the highest one that fits

| Layer | Header | Scope | Notes |
|---|---|---|---|
| `juce::FloatVectorOperations` | `juce_audio_basics/buffers/…` | whole arrays of `float`/`double` | Needs only `juce_audio_basics`. Handles unaligned pointers **and** non-multiple-of-lane counts for you. First choice. |
| `juce::dsp::SIMDRegister<T>` | `juce_dsp/containers/juce_SIMDRegister{,_Impl}.h` | one register of elementwise math + masks | Needs `juce_dsp` and `JUCE_USE_SIMD`. You own alignment, head/tail, and lane bookkeeping. |
| `juce::dsp::SIMDNativeOps<T>` / `SIMDFallbackOps` | `juce_dsp/native/juce_SIMDNativeOps_*.h` | raw intrinsics per ISA | Dispatched to *by* `SIMDRegister`. Never call directly; specialisations exist per (ISA × type) and are `@cond`. |

`FloatVectorOperations` is `detail::NameForwarder< Base<float,int>, Base<float,size_t>,
Base<double,int>, Base<double,size_t> >` — four instantiations of
`FloatVectorOperationsBase<FloatType, CountType>`, all `static … JUCE_CALLTYPE … noexcept`.

Operations available (all elementwise; `num` is the element count):

| Need | Call |
|---|---|
| zero / constant fill | `clear(dest,n)`, `fill(dest,value,n)` |
| copy, copy scaled | `copy(dest,src,n)`, `copyWithMultiply(dest,src,mult,n)` |
| add scalar / vector / two sources | `add(dest,k,n)`, `add(dest,src,n)`, `add(dest,src1,src2,n)` |
| subtract | `subtract(dest,src,n)`, `subtract(dest,src1,src2,n)` |
| **mix one tap** | `addWithMultiply(dest,src,gain,n)` (+ `…(dest,src1,src2,n)`) |
| subtract a product | `subtractWithMultiply(...)` |
| gain in place / into new buffer | `multiply(dest,src,n)`, `multiply(dest,k,n)`, `multiply(dest,src,k,n)` |
| polarity / magnitude | `negate(dest,src,n)`, `abs(dest,src,n)` |
| clamp / limit | `min(dest,src,comp,n)`, `max(dest,src,comp,n)`, `clip(dest,src,lo,hi,n)` |
| **peak / RMS metering** | `findMinAndMax(src,n)` → `Range<T>`, `findMinimum`, `findMaximum` |
| 24-bit int → float | `convertFixedToFloat(dest,intSrc,multiplier,n)` |
| denormals | `enableFlushToZeroMode(bool)`, `disableDenormalisedNumberSupport(bool = true)`, `areDenormalsDisabled()` |

There is **no** transcendental, complex, FFT, or interleaving op in this class.

### Widths and masks (memorise the shape, not the numbers)

| Build | `vSIMDType` (float) | lanes | `vSIMDType` (double) | lanes | align to |
|---|---|---|---|---|---|
| SSE (x86, no `__AVX2__`) | `__m128` | 4 | `__m128d` | 2 | 16 B |
| AVX (`__AVX2__`) | `__m256` | 8 | `__m256d` | 4 | 32 B |
| NEON (ARM) | `float32x4_t` | 4 | (1-wide in the FVO helper) | 4/1 | 16 B |

Query at compile time, never hardcode:
`SIMDRegister<T>::SIMDRegisterSize`, `SIMDNumElements`, `size()`.

`MaskType`: `float→uint32_t`, `double→uint64_t`, `int8_t→uint8_t`, `int16_t→uint16_t`,
`int32_t→uint32_t`, `int64_t→uint64_t`, `std::complex<float>→uint32_t`.
A mask lane is **all-ones or all-zero**; combine with `& | ^ ~` for branchless select:
`(m & a) | (~m & b)`.

## Procedure

1. **First, replace the loop with a `FloatVectorOperations` call.** This is the single biggest
   win and costs no correctness risk — it is already alignment- and tail-safe:

   ```cpp
   using FVO = juce::FloatVectorOperations;
   FVO::clear (buf.getWritePointer (ch), numSamples);
   FVO::multiply (buf.getWritePointer (ch), gain, numSamples);          // gain in place
   FVO::addWithMultiply (dest, src, tapGain, numSamples);               // one mixer tap
   const auto [lo, hi] = FVO::findMinAndMax (buf.getReadPointer (ch), numSamples);
   ```

   Safe on the audio thread: every entry point is `noexcept`, allocates nothing, locks nothing.

2. **Match the overload's `CountType` exactly.** The four instantiations differ in
   `int` vs `size_t`. Pass `int` (host API type) or `size_t` explicitly; an `unsigned`,
   `long`, or `uint32_t` argument converts equally well to both and fails to compile as an
   ambiguous call. Likewise let `float`/`double` come from the pointer type — do not cast
   sample values to widen the operation.

3. **Choose the in-place vs. separate-dest form on purpose.** Two/one-source forms
   (`add(dest,src,n)`) read *and* write `dest`; the three-source forms (`add(dest,src1,src2,n)`)
   never read `dest`, so they are the ones to use when `dest` must start undefined. `dest` may
   alias `src` in the two-pointer forms; never let `src1`/`src2`/`dest` overlap partially —
   the vector loops assume non-overlapping strides.

4. **Only drop to `SIMDRegister` when the algorithm needs lanes**, and then write the loop in
   three parts: scalar **head** to the first aligned sample, vector **body**, scalar **tail**:

   ```cpp
   using V = juce::dsp::SIMDRegister<float>;
   constexpr int N = (int) V::size();

   float* p = dest;
   int i = 0;
   for (; i < num; ++i)                  p[i] = a[i] * g;        // scalar head/tail only
   auto* av = V::fromRawArray (a + i);                            // requires aligned a+i
   auto* pv = reinterpret_cast<V*> (p + i);
   for (int k = 0; k < (num - i) / N; ++k) pv[k] = av[k] * g;      // vector body
   for (i += ((num - i) / N) * N; i < num; ++i) p[i] = a[i] * g;   // scalar epilogue
   ```

   `fromRawArray` / `copyToRawArray` `jassert (isSIMDAligned (ptr))`; `getNextSIMDAlignedPtr`
   gives the first legal body start. `operator[]` (non-const) returns an `ElementAccess`
   proxy — usable as an lvalue, converts to `T`; the const overload returns a value.
   `get(i)`/`set(i,v)`/`operator[]` do **not** bounds-check outside a debug `jassert`.

5. **Express conditionals as masks, never as a per-lane branch.**

   ```cpp
   using V = juce::dsp::SIMDRegister<float>;
   auto m = V::greaterThan (x, V::expand (0.0f));   // all-ones lanes where x > 0
   auto out = (m & limited) | (~m & other);         // branchless select
   ```

   Comparators: `equal`, `notEqual`, `lessThan`, `lessThanOrEqual`, `greaterThan`,
   `greaterThanOrEqual` — all `static`, all return `vMaskType`. Elementwise `min`, `max`,
   `abs`, `truncate`, `multiplyAdd` are also static. `jmin`/`jmax` are overloaded for
   `SIMDRegister` in `juce_SIMDRegister_Impl.h`, so generic code keeps working.

6. **Accumulate in registers, reduce once at the end.** `sum()` is a horizontal reduction
   (`_mm_dp_ps`/permute on AVX, a scalar loop on the fallback path) — hoist it out of the loop
   and sum per-lane, then call `sum()` once. Same rule for `findMinAndMax`: one call over the
   block, not one per sample.

7. **Use `multiplyAdd (a, b, c)` for `a += b*c`** (one FMA on AVX when the build defines
   `__FMA__`, otherwise mul+add). For coefficient-heavy kernels this is where AVX actually
   pays; on SSE the op is literally `add (a, mul (b, c))` and buys you nothing but fewer
   temporaries.

8. **Complex spectra:** `SIMDRegister<std::complex<float>>` holds **half** as many elements as
   the float register and stores them interleaved re/im. `CmplxSIMDOps` rewrites `mul` →
   `cmplxmul`, `expand` → re/im interleave, `sum` → `oddevensum` (returns a complex). So
   `size()` and every index are in *complex* elements; do not mix float-shaped and
   complex-shaped indexing on the same buffer.

9. **Handle denormals once per callback, with RAII:**

   ```cpp
   void processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) override
   {
       juce::ScopedNoDenormals noDenormals;   // saves + restores the FP status register
       ...
   }
   ```

   `ScopedNoDenormals` reads the FP status register (`_mm_getcsr` on x86, `mrs fpcr`/`vmrs
   fpscr` on ARM), sets the mask, and restores it in the destructor. The x86 mask is
   `0x8040` (DAZ|FTZ); ARM gets only flush-to-zero (bit 24, FZ). Prefer this over
   `disableDenormalisedNumberSupport()`, which mutates the mode globally and never restores it.
   Check state with `FloatVectorOperations::areDenormalsDisabled()`.

10. **Then measure with the real host block size.** Vector width only pays once `num` exceeds a
    few lanes; for a 32-sample block on AVX (8 lanes) you get 4 iterations plus prologue, and a
    compiler already auto-vectorising your loop. A/B it: build with `JUCE_USE_SIMD=0` for a
    scalar reference, and diff numerics against a naive time-domain implementation within an
    explicit tolerance. Reference behaviour:
    `juce/modules/juce_dsp/containers/juce_SIMDRegister_test.cpp` and the
    `FloatVectorOperationsTests` unit test inside `juce_FloatVectorOperations.cpp`.

## Pitfalls

1. **Aligned loads/stores only at the `SIMDRegister` layer.** `SIMDNativeOps::load/store` map
   to `_mm_load_ps` / `_mm256_load_ps` (not the `u` variants). An unaligned pointer is a debug
   `jassert` at best and a general-protection fault on `movaps` at worst.
   FloatVectorOperations avoids this by branching on `isAligned` and selecting
   `loadA/storeA` vs `loadU/storeU`; your hand-written loop gets no such rescue.
2. **The alignment requirement itself is not portable.** It is `sizeof(vSIMDType)` — 16 B under
   SSE, **32 B under AVX**. A buffer hand-aligned to 16 B in an SSE-tuned build is *not* legal
   input to the same code compiled with `__AVX2__`. Use `isSIMDAligned`,
   `getNextSIMDAlignedPtr`, or `snapPointerToAlignment`, and size `AudioBlock`'s
   `defaultAlignment` from `sizeof (SIMDRegister<NumericType>)`.
3. **Never hardcode the lane count.** `4` is wrong on AVX, `8` is wrong on SSE and NEON —
   silently dropping or double-processing samples at a block boundary. Use
   `SIMDRegister<T>::size()`.
4. **`operator==` on `SIMDRegister` is a bool, not a mask.** It is `allEqual` (all lanes equal),
   and `operator!=` is `!allEqual`. Using it as an elementwise mask gives one bool instead of a
   vector. And equality is NaN-poisoned: SSE uses `_CMP_*_OQ` and the fallback uses
   `exactlyEqual`, so a single NaN lane makes `allEqual` false.
5. **`lessThan`/`lessThanOrEqual` do not exist in `SIMDNativeOps`.** `SIMDRegister::lessThan (a,b)`
   is implemented as `NativeOps::greaterThan (b, a)`. If you ever call the native layer, swap
   the operands yourself — otherwise you get the inverted predicate.
6. **The vector body reads a full register width**, so `fromRawArray` on a pointer near the end
   of an allocation reads out of bounds (ASan/valgrind hit, and garbage in the surplus lanes
   that a later `copyToRawArray` would write back). Bound the body by `(num - head) / N` and
   leave the rest to the scalar epilogue. FloatVectorOperations does exactly this:
   `num / numParallel` vector steps, then `num &= numParallel - 1` scalar steps.
7. **`truncate` is round-to-zero via a 32-bit integer round-trip** (`cvttps_epi32`), so
   magnitudes ≥ 2³¹ return the integer indefinite value, not a clamp; and it is an identity
   function on the integer specialisations. Do not use it as `std::floor` (it rounds toward
   zero, so `-0.5 → -0`, not `-1`), and do not use it on unbounded signal levels.
8. **`min`/`max` are the IEEE intrinsics, not comparisons, and `clip` is the only real clamp.**
   `min`/`max` have a fixed NaN operand preference and do not bound a value into an interval —
   for limiting use `clip (dest, src, low, high, n)`.
9. **`juce::dsp::util::snapToZero (SIMDRegister<Type>&)` is an empty stub** in
   `juce_SIMDRegister_Impl.h`. Vector code gets **no** denormal protection from it — only FTZ/DAZ
   (see step 9) does. Likewise `JUCE_SNAP_TO_ZERO` is a no-op on non-Intel targets.
10. **Denormal flags are architectural state, not object state.** The MXCSR/FPCR bits are
    per-thread, so `ScopedNoDenormals` in a shared helper affects everything else on that
    thread for its scope, and `disableDenormalisedNumberSupport (true)` leaks into the host and
    sibling plugins. On non-x86/non-ARM platforms these calls hit `jassertfalse`.
11. **No transcendentals here.** `FloatVectorOperations` is add/sub/mul/min/max/clip/abs/negate/
    find only. For `exp`/`log`/`pow`/trig look at `juce::dsp::FastMathApproximations`; do not
    invent a "vector exp" or reach into the native layer for one.
12. **ISA is fixed at compile time — there is no runtime dispatch.** `juce_dsp.h` includes the
    AVX header only when `__AVX2__` is defined (else SSE), and NEON on ARM; forcing
    `JUCE_USE_SIMD=1` on an unsupported target is a hard `#error`. Compiling with `-mavx2` and
    shipping to a machine without AVX2 is a `SIGILL`, not a graceful degradation.
13. **Calling-convention mismatch on MSVC.** `JUCE_VECTOR_CALLTYPE` expands to `__vectorcall`
    only for 32-bit MSVC with SIMD enabled, and to nothing elsewhere. Tag your own inline
    by-value-SIMD helpers with `JUCE_VECTOR_CALLTYPE` and keep the definition visible in a
    header; mixing TU's with different `/arch:` settings produces ODR/linking surprises.
14. **Integer SIMD is the poorly-optimised corner.** On SSE, `int8_t`/`int16_t` `mul` is
    emulated by unpacking to 16-bit multiplies, and pre-SSE4 `min`/`max` degrade to
    mask-select sequences; `uint8_t` comparisons xor a sign bit first. Float/double is the
    well-trodden path — measure before vectorising byte- or word-wide work this way.
15. **`AudioBlock<SIMDRegister<T>>` index-unit confusion:** with a register-shaped
    `SampleType` every position is in **vector** units (`sizeFactor = sizeof (SampleType) /
    sizeof (NumericType)`), so mixing a plain sample index into a `copyFrom`/`copyTo` position
    argument is an `N×` offset error. Keep one unit convention per buffer and multiply at the
    boundary.
16. **Don't hand-roll register type-punning.** JUCE converts float↔mask registers with internal
    unions and `fromNative`; the fallback ops are union-based. Re-implementing that with
    `reinterpret_cast` on sample buffers is exactly what the project's memory rules forbid, and
    it is UB outside the two JUCE buffer-pointer idioms.
17. **A vectorised loop is still bound by the real-time contract:** no allocation for scratch
    vectors in `processBlock`, size everything in `prepareToPlay`, and keep
    `JUCE_USE_SIMD`/`-march` choices out of the audio path as runtime decisions.

## Build / config knobs

| Knob | Default | Effect |
|---|---|---|
| `JUCE_USE_SIMD` | 1 on x86; 1 on ARM iff `JUCE_USE_ARM_NEON`; else 0 | Gates `SIMDRegister`/`SIMDNativeOps` entirely; `SIMDRegister` does not exist when 0. Useful as the scalar reference build. |
| `JUCE_VECTOR_CALLTYPE` | `__vectorcall` on 32-bit MSVC+SIMD, else empty | Annotate your own inline vector helpers with it. |
| `JUCE_INTEL` / `JUCE_ARM` + `__AVX2__` | compiler-derived | Selects the AVX vs SSE vs NEON header at configure time. |
| `JUCE_SNAP_TO_ZERO` | Intel-only macro | Per-value denormal snapping; a no-op elsewhere. |

Dependency placement: `FloatVectorOperations` needs `juce_audio_basics`; `SIMDRegister` needs
`juce_dsp`. Both are JUCE types, so neither may appear in a JUCE-light library (this repo's
`src/Dsp`, `src/Container` seams) — the vectorisation belongs at the plugin boundary where JUCE
buffers live, or in a project-owned primitive that gets its own hand-written, ISA-free loop.
