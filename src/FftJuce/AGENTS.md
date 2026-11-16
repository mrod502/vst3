# FftJuce

The one JUCE-aware implementation of the `fft::IFft` contract. It exists as its own library so that
`src/Fft`, `src/Engine` and `src/Grain` never include a JUCE header.

## Contract

`fft::JuceFft final : fft::IFft`, constructed with the transform size (`explicit JuceFft(int sizeF)`),
non-copyable. It satisfies `src/Fft`'s layout, never-read slots and normalisation exactly as
documented there, so a grain engine cannot tell it from `fft::Dft`.

- **Gain is calibrated, not assumed.** `juce::dsp::FFT` scales differently per backend and per size,
  so the adapter carries the measured correction that makes forward-then-inverse round-trip at unity.
  Changing the backend, the size rule or the JUCE version means re-measuring that figure — it is not
  portable arithmetic.
- **The fallback path locks.** Where JUCE cannot honour a request the adapter falls back to an
  implementation that takes a lock, which puts it outside the real-time guarantee; the shipped
  configuration avoids that path, and adding a size that reaches it is a design change, not a
  constructor argument.
- **Never-read slots are zeroed on the way into the engine**, which is what makes `Fft`'s promise
  hold for any backend rather than only for the oracle.

## Threading

Construction allocates and may plan the transform; it belongs off the audio thread. `forward` and
`inverse` allocate nothing and touch only pre-built state. One instance is driven by one thread.

## Build and test

```bash
cmake --build build/release --target FftJuce -j4
ctest --test-dir build/release -R JuceFft --output-on-failure
```

Links `Fft` and `juce::juce_dsp`. `tests/JuceFftTests.cpp` links JUCE; `src/Fft`'s own suite does not,
and `GrainEngineJuceTests` runs the grain engine's assertions against this transform.

## Decisions

D-006 (the backend choice and its calibration), F-003 (per-engine scaling, the locking fallback,
measured vDSP behaviour), F-009 (the archive's JUCE closure and PIC) — `docs/decisions.md`.
