---
name: juce-dsp-processing
description: Build and wire juce::dsp processors (ProcessorChain, AudioBlock, ProcessContext, IIR/FIR filters, DelayLine, Oversampling, Convolution) without breaking their duck-typed lifecycle, channel-count limits, thread boundaries, or latency accounting.
source: learned
---

# JUCE `juce::dsp` Processing Chains

Distilled from `juce/modules/juce_dsp/` (JUCE 9.0.4; `containers/`, `processors/`,
`frequency/`, `maths/`, `widgets/`, `filter_design/`, `juce_dsp.h`). Module depends on
`juce_audio_formats`, needs C++17, links Accelerate on macOS/iOS.

## When to Use

- Building an effect/synth signal path out of `juce::dsp` classes inside an
  `AudioProcessor::processBlock`.
- Chaining processors with `ProcessorChain`, or applying a mono-only filter to stereo
  with `ProcessorDuplicator`.
- Bridging a `juce::AudioBuffer<float>` (host block) to `AudioBlock` + `ProcessContext`.
- Adding oversampling, dry/wet mixing with latency compensation, convolution/IR reverb,
  FFT analysis, or filter design.
- Debugging silent output, "filter does nothing", mono-only assertion failures, clicks on
  parameter change, or missing/doubled latency reporting.

## The Contract (read first)

A "processor" is **duck-typed** — no base class required. Three members, exact spellings:

```cpp
void prepare (const ProcessSpec&);            // allocate + size state; NOT the audio thread
void process (const ProcessContext&) noexcept;// templated on the context type; audio thread
void reset();                                // clear internal state only, keeps coefficients
```

Types (all in `namespace juce::dsp`):

| Type | Contract |
|---|---|
| `ProcessSpec{ double sampleRate; uint32 maximumBlockSize; uint32 numChannels; }` | everything a processor may allocate for. Has `operator==`/`!=` — compare before re-preparing. |
| `ProcessContextReplacing<T>` | one block is both input and output; `getInputBlock() == getOutputBlock()`. `usesSeparateInputAndOutputBlocks()` is `false`. |
| `ProcessContextNonReplacing<T>` | distinct in/out; `jassert (input != output)` in the ctor — if they are the same, use `ProcessContextReplacing`. |
| `AudioBlock<T>` | **non-owning view** over someone else's memory. Never copies. Owns nothing, allocates nothing (except the `HeapBlock<char>&` ctor). |
| `ProcessorState` | ref-counted (`ReferenceCountedObject`) shared parameter block; `Ptr` = `ReferenceCountedObjectPtr`. |
| `Complex<T>` | alias for `std::complex<T>`. |
| `isBypassed` | a **field on every context**, not a processor method. Processors must honour it themselves. |

Concrete constructors you will actually use:

```cpp
AudioBlock<float> block { bufferForBlock };                  // from AudioBuffer<float>
AudioBlock<const float> in  { myInputBuffer };                // read-only view
ProcessContextReplacing<float> ctx { block };                 // holds a REFERENCE — block must outlive ctx
```

## Procedure

1. **Construct processors as members** (they are cheap, non-copyable-safe value types —
   `Gain`, `IIR::Filter`, `DelayLine`, `Oscillator`, `ProcessorChain`). Prefer a chain over
   hand-written sequential calls so bypass and reset stay uniform:

   ```cpp
   dsp::ProcessorChain<dsp::Oscillator<float>,
                       dsp::StateVariableTPTFilter<float>,
                       dsp::Gain<float>,
                       dsp::Oversampling<float>> chain;
   ```
   Reach into it with `chain.get<I>()` or the non-member `get<I> (chain)` (nicer syntax);
   `auto& [osc, filt, gain, overs] = chain;` works — `tuple_size`/`tuple_element` are
   specialised. Per-stage bypass: `chain.setBypassed<I> (true)`.

2. **Call `prepare()` exactly once per spec change**, before any `process()`, with the
   *maximum* block size the host may deliver — every internal buffer is sized from
   `maximumBlockSize`:

   ```cpp
   void prepareToPlay (double sr, int maxBlock) override
   {
       const dsp::ProcessSpec spec { sr,
                                     static_cast<std::uint32_t> (maxBlock),
                                     static_cast<std::uint32_t> (getTotalNumOutputChannels()) };
       chain.prepare (spec);
   }
   ```
   Re-`prepare()` (not just `reset()`) when sample rate, block size, or channel count
   changes; guard with `if (spec == lastSpec) return;` because `ProcessSpec` is comparable.

3. **Wrap the host buffer once per callback** and drive the chain. Do this per call — the
   context and block are stack-sized views, constructing them costs nothing and must not be
   cached across callbacks (the host's pointers move):

   ```cpp
   void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
   {
       juce::ScopedNoDenormals noDenormals;
       const dsp::AudioBlock<float> block { buffer };
       const dsp::ProcessContextReplacing<float> ctx { block };
       chain.process (ctx);
   }
   ```

4. **Push parameter values before `process()`, once per callback**, never mid-sample-loop.
   Read host params through atomics/`SmoothedValue`, then hand the *target* to the processor
   (`gain.setGainDecibels (x)`, `filter.setCutoffFrequency (x)`). Smoothing inside the
   processor (`Gain::setRampDurationSeconds`, `Oscillator`'s fixed 50 ms frequency ramp) only
   runs when `prepare()`/`reset()` gave it a sample rate.

5. **For a filter that must handle >1 channel, check whether it is mono-only** and wrap it
   if so. Mono-only: `IIR::Filter`, `FIR::Filter`, `dsp::StateVariableFilter` (the legacy
   one) — all `jassert (numChannels == 1)`. Give them a shared ref-counted state:

   ```cpp
   using State = dsp::IIR::Coefficients<float>;
   dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, State> stereoFilter;
   ```
   `ProcessorDuplicator` instantiates one clone per channel inside `prepare()` and gives
   each a 1-channel spec. Multichannel-capable by contrast: `StateVariableTPTFilter`
   (`s1`/`s2` resized to `spec.numChannels`), `Gain`, `DelayLine`, `LadderFilter`,
   `Compressor`, `Limiter`.

6. **Update IIR coefficients by replacing the ref-counted pointer**, never by mutating the
   array in place while audio runs:

   ```cpp
   filter.coefficients = dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 1200.0f, 0.707f);
   filter.reset();   // required if the coefficient ORDER changed
   ```
   Use `IIR::ArrayCoefficients<T>` for plain `std::array` returns (4 entries = 1st order,
   6 = biquad), `FilterDesign<FloatType>::designFIRLowpass*Method` for high-order FIR, and
   `coefficients->getMagnitudeForFrequencyArray (...)` / `getPhaseForFrequencyArray (...)`
   for response plots.

7. **Account for latency explicitly.** Anything that delays audio must be reported and
   compensated:
   - `oversampling.getLatencyInSamples()` → `setLatencySeconds (latency / sampleRate)` and
     feed the same figure to `DryWetMixer::setWetLatency`.
   - `Convolution` built with `Latency{ n }` delays by *at least* `n` samples.
   - `DryWetMixer` ordering is fixed: `pushDrySamples (dryBlock)` **then**
     `mixWetSamples (wetBlock)`; the wet block is modified in place into the mix.
   - Non-integer oversampling latency: plugins can only report integer samples, so use
     `useIntegerLatency = true` or round and compensate yourself.

8. **Oversample in the correct three-step order** — the up-sampled block is the only valid
   input to the non-linear stage, and its sample rate is `N ×` the host rate:

   ```cpp
   auto upsampled = oversampling.processSamplesUp (block);        // AudioBlock<float>
   distortion.process (dsp::ProcessContextReplacing<float> { upsampled });
   oversampling.processSamplesDown (block);
   ```
   `initProcessing (maximumBlockSizeBeforeOversampling)` before first use. FIR
   (`filterHalfBandFIREquiripple`) = linear phase, more latency;
   (`filterHalfBandPolyphaseIIR`) = minimal latency, phase smeared near Nyquist.

9. **Use `pushSample`/`popSample` when the delay moves, `process()` when it does not.**
   `DelayLine::process()` is documented for a *fixed* delay; real-time modulation and any
   feedback loop go through `pushSample (ch, s)` / `popSample (ch, delay, updateReadPointer)`.
   Interpolation choice: `None` (constant integer delay / lo-fi), `Linear` (cheap, low-pass
   smearing), `Lagrange3rd` (better, still modulatable), `Thiran` (cheapest, flat magnitude,
   **stateful → unsuitable for fast delay modulation**).

10. **Call `reset()` on stop/start and on any transport or config change**, and nothing
    else on the audio thread. `reset()` clears filter/memory state but keeps coefficients and
    allocations; `ProcessorChain::reset()` does all stages.

## Threading & real-time rules

- `prepare()`, `reset()` (as it re-arms `SmoothedValue`), coefficient *assignment*,
  `DelayLine::setMaximumDelayInSamples`, `Oversampling::initProcessing`,
  `ProcessorDuplicator::prepare` when the channel count grows, and any
  `OwnedArray`/`std::vector` resize **allocate** → message thread only.
- `process()` is `noexcept` and must not allocate, lock, log, or do I/O.
- `Convolution` is the exception the docs state outright: its `loadImpulseResponse()` calls
  are *wait-free and realtime-safe*, but calls to the class must never interleave — in
  practice issue `load()` from the audio thread. IR loads are asynchronous: to have a
  specific IR active on the first `process()`, call `load()` **before** `prepare()`.
- `Reverb::setParameters` is documented as unlocked — calling it in parallel with
  `process()` produces artifacts. Set it on the message thread between callbacks.
- Editor↔processor contact is parameters and atomics only; nothing in a DSP object may hold
  a UI reference.
- `juce::dsp` classes are **not** `JUCE-light`: nothing under a JUCE-free library (e.g. a
  project's own `src/Dsp`/`src/Container` seam) may include them. Keep `juce::dsp` at the
  plugin boundary, where JUCE buffer types meet the host.

## Class selection quick map

| Need | Use | Note |
|---|---|---|
| Static gain / ramped gain | `Gain<T>` | `getGainLinear()` returns the **target**, not the smoothed value |
| Mono/stereo filter, fast cutoff modulation | `StateVariableTPTFilter<T>` | TPT, 12 dB/oct; multichannel |
| Filter with custom/legacy coefficients | `IIR::Filter<T>` | mono; TDF-II; artefacts on hard cutoff jumps |
| Classic SVF with shared `Parameters` | `StateVariableFilter<T>` | mono; needs `ProcessorDuplicator` |
| Moog-style ladder, drive/self-oscillation | `LadderFilter<T>` | `LadderFilterMode` LPF/HPF/BPF 12 & 24 |
| Delay, chorus-style modulated tap | `DelayLine<T, Interp>` | set delay via `setDelay` before `process` |
| FIR filtering ≤ ~128 taps | `FIR::Filter<T>` | mono; **asserts** above 128 (`JUCE_ASSERTION_FIRFILTER`) |
| Long FIR / IR convolution | `Convolution` | float-only; frequency domain |
| Alias-free nonlinearity | `Oversampling<T>` | report `getLatencyInSamples()` |
| Dry/wet with delayed wet path | `DryWetMixer<T>` | 7 mixing rules; default 50 ms ramp |
| Envelope follower (attack/release) | `BallisticsFilter<T>` | ms; <0.001 ms snaps to zero; peak/RMS |
| Waveform generation | `Oscillator<T>` | output-only; adds input to generated sample |
| Transfer curve | `WaveShaper<T, Fn>` | `prepare` is a no-op; `process` is `const` |
| Pan | `Panner` | **silently no-ops** unless output is exactly 2 channels |
| Spectrum analysis / filter design | `FFT`, `WindowingFunction`, `FilterDesign` | cache one `FFT` per size+direction |

## Pitfalls

1. **A default-constructed filter outputs silence, not passthrough.** `IIR::Filter()`
   starts with null coefficients and `FIR::Filter()` builds a null `Coefficients` — both
   documented as "will produce silence". Assign coefficients before the first `process()`.
2. **`IIR::Filter`/`FIR::Filter`/`StateVariableFilter` assert on stereo.** A 2-channel
   `ProcessSpec` trips `jassert (numChannels == 1)` in release-invisible ways (mono path on
   channel 0 data only via the duplicator). Wrap in `ProcessorDuplicator` instead of
   assuming the class loops channels.
3. **Coefficient order change without `reset()` = corruption.** The header is explicit:
   "If you change the order of the coefficients then you must call reset after modifying
   them." Same rule for `coefficients->getRawCoefficients()` poking.
4. **Mutating coefficients in place from the UI thread races the audio thread.** The
   `coefficients` member is a ref-counted `Ptr` precisely so you can swap the pointer;
   mutating the shared array in place gives torn coefficients and clicks.
5. **`AudioBlock` owns nothing.** Views into `AudioBuffer`/raw pointers must not outlive
   them, and `ProcessContextReplacing` stores a *reference* to the block. Never cache either
   across callbacks — host channel pointers change.
6. **Forgetting `isBypassed` in a custom processor.** When bypassed, a processor must either
   leave the output untouched (replacing context, in/out alias the same memory) or
   `outputBlock.copyFrom (inputBlock)` (separate blocks) — and must still advance its
   smoothing (`Gain` calls `gain.skip (len)` for exactly this reason). Skipping the branch
   makes bypass do nothing; copying unconditionally in a replacing context is harmless but
   wasteful.
7. **`Oscillator` is output-only and its input is *added*.** It writes
   `input + generator(...)`, bypass clears the output, and `process()` asserts
   `numSamples <= rampBuffer.size()`, so `prepare()` must see the true maximum block size.
   Call `initialise()` (or use the ctor taking a function over `-pi..pi`) before use, or the
   `isInitialised()` assert fires.
8. **`Reverb` needs `setEnabled (true)`.** The wrapper copies input→output and returns early
   unless `enabled`; forgetting it looks like a broken reverb, not a disabled one. It only
   handles 1 or 2 channels — 3+ channels pass through unreverberated.
9. **`Panner` returns silently for non-stereo output** (`numOutputChannels != 2 ||
   numInputChannels > 2`) — no assert, no audio change. Check your channel layout.
10. **Convolution message queue lifetime.** `Convolution (ConvolutionMessageQueue&)` borrows
    the queue; it "*must* remain alive throughout the lifetime of the Convolution". Declare
    the queue before/outlive the processor, and size it up (`ConvolutionMessageQueue{ n }`)
    when sharing between instances or loading frequently.
11. **`FFT` array sizing.** `performRealOnlyForwardTransform` / `performFrequencyOnlyForwardTransform`
    / `performRealOnlyInverseTransform` all need an array of **`2 * getSize()` floats**, even
    the inverse path that only reads `(size/2)+1` complex values. `getSize()` is the number of
    *points* (2^order), not the array length. `onlyCalculateNonNegativeFrequencies` is only a
    hint — the fallback engine still computes them.
12. **Creating an `FFT` per callback.** It holds lookup tables; the docs say to create and
    cache one object per size/direction and re-use it. Construction on the audio thread is an
    allocation.
13. **`DelayLine::setMaximumDelayInSamples` on the audio thread** — documented to allocate.
    Also `getMaximumDelayInSamples()` may return less than requested (`totalSize - 2`) for
    very short delays, so clamp your own delay values against it.
14. **Long `DelayLine`/`BallisticsFilter` times saturate.** Attack/release below 0.001 ms are
    snapped to zero and very long times saturate at the working precision.
15. **Cutoff/resonance asserts.** `StateVariableTPTFilter::setCutoffFrequency` asserts the
    value is positive and below Nyquist; `setResonance` asserts `> 0`. Clamp against the
    *current* sample rate before calling — a sample-rate change makes a previously valid
    cached Hz value illegal. `LadderFilter::setResonance` up to 1 can self-oscillate.
16. **Reporting no latency for a latency-adding path** desyncs the plugin against the dry
    signal in the host. Oversampling, fixed-latency convolution, and look-ahead partitioning
    must all reach `setLatencySeconds`.
17. **Double denormal handling.** `JUCE_DSP_ENABLE_SNAP_TO_ZERO` defaults to 1 and adds
    overhead per filter call; if the app already uses `ScopedNoDenormals`, turn the flag off.
    Conversely, hand-rolled `processSample()` paths do need `snapToZero()` yourself
    (`IIR::Filter::snapToZero()` exists for exactly that).
18. **`setRampLength` / `setRampDurationSeconds` call `reset()`.** Calling them every
    callback silently wipes filter and smoothing state — set them on change only.
19. **`JUCE_ASSERTION_FIRFILTER` is a design hint, not noise.** It fires in DEBUG above 128
    FIR taps to say "use `Convolution`". Switch class rather than disabling the flag.
20. **Assuming `getLatencyInSamples()` is an integer**, or that `ProcessSpec` defaults are
    initialised — `ProcessSpec` is an aggregate with no defaults, so brace-init all three
    fields.

## Build/Config knobs (from `juce_dsp.h`)

`JUCE_ASSERTION_FIRFILTER` (1), `JUCE_DSP_ENABLE_SNAP_TO_ZERO` (1),
`JUCE_DSP_USE_INTEL_MKL` (0), `JUCE_DSP_USE_SHARED_FFTW` (0, dlopen'd at runtime with
fallback), `JUCE_DSP_USE_STATIC_FFTW` (0), `JUCE_USE_SIMD` (auto: 1 on x86 SSE/AVX, 1 on ARM
with `JUCE_USE_ARM_NEON`). Enabling MKL/FFTW replaces JUCE's FFT and convolution engines —
record it as a dependency decision (licence: FFTW is copyleft-adjacent, MKL proprietary)
before turning either on.
