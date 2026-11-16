# Entangle

The JUCE plugin shell: the `AudioProcessor`, its editor, and the shared chain builder that both the
processor and the test harness use. Everything below the shell is `src/Engine`, `src/Grain`,
`src/FftJuce` and `src/Dsp`.

| File | Role |
|---|---|
| `EntangleProcessor.h/.cpp` | buses, parameters, `processBlock`, layout gate and re-prepare guard, `layoutReport()` |
| `EntangleEditor.h/.cpp` | the kit's shell: seventeen components, three engine-truth readouts, a 100 ms tick |
| `EngineChain.h` | `entangle::makeChain` / `makeShippedChain` — the one place the shipped chain is assembled |
| `Entangle.h.in` | template for the generated `Entangle.h` |

**Never edit `src/Entangle/Entangle.h`** — `configure_file` regenerates it from `Entangle.h.in` on
every configure, silently discarding a hand edit.

## Buses and parameters

| Bus | Default |
|---|---|
| Input (main) | stereo, enabled |
| Sidechain | stereo, **disabled** |
| Output | stereo, enabled |

| Parameter | Range | Default |
|---|---|---|
| Blend | −1 … 1, step 1/128 | 0 |
| Hop size | choice 64 / 128 / 256 / 512 | 128 |
| Grain length | choice 1024 … 65536 | 8192 |
| Output gain | −60 … +12 dB, step 0.125 dB | 0 dB |
| Bypass | bool | off |
| Normalisation amount | 0 … 1, step 1/128 | 1.0 |
| Silence floor | −90 … −20 dB, step 0.125 dB | −60 dB |

The two choice parameters are **indexed by position in a saved preset**, so their declaration order is
part of the file format: append new parameters, never insert. The floats and the bool are looked up by
id. Both quantisations are dyadic because a `juce::NormalisableRange` with a non-dyadic interval
cannot express its own default or endpoints.

## What the shell decides

- **The chain is built once, in one function**, and has three consumers: the processor, and — through
  `tools/harness` — `OfflineRender` and `ShellTests`. A harness must never assemble a chain of its own,
  and must not recompile this directory's sources into its own archive either: a static archive lets a
  recompiled copy of `EngineChain` replace the plugin's, with every leg green.
- **`clampGrainLength(chosen, hop)`** maps several dial positions onto one running length; the CPU
  bound and the buffers follow the running length, not the dial.
- **`maxHostBlockFor(hostBlock)`** is a floor, not a cap, with a documented consequence (ring sizes —
  see `src/Container`).
- **Layout:** `isBusesLayoutSupported` and `configureRings` both call `engine::gateLayout`, so a layout
  the rule rejects is refused with a named reason rather than met with silence. `configureRings` keeps
  an `std::optional<engine::ReconfigureKey>` and re-prepares only when it changes; a refusal clears it.
- **`layoutReport()`** renders what the plugin was *told* as one `key=value` line — bus counts and
  `getDescription()` per bus, the gate's verdict and rule, processed count, `R`, hop, requested and
  running grain length, host block against the floor, sidechain active, effective blend, plus
  `prepares=`/`skipped=`. It goes to the editor's readout and to `juce::Logger` behind a
  `MessageManager::existsAndIsCurrentThread()` test. Host answers collected against it go in
  `docs/host-layout-matrix.md`.

## Editor

Widgets come from `src/UiJuce`; every rect comes from `ui::Grid` plus `BarGeometry` / `PieGeometry`
and the ramp's cap metrics, so the sheet scales as one sheet. The window is resizable within
`ui::kMinimum/MaximumWindowWidthPx` with the aspect locked on the editor's own constrainer, and
`setResizable(true, false)` keeps JUCE's corner grip off the sheet — it paints colours the palette
never named. `resized()` reads the **width** alone, which makes an off-aspect host window harmless
rather than stretched. Parameters reach the DSP through the existing APVTS attachments; the tick
writes only what changed.

## Threading

`processBlock` follows the repo-wide real-time rules (`AGENTS.md`). Editor and processor meet only
through APVTS attachments and atomics. Sample-rate, layout and latency changes are handled in
`prepareToPlay` / `releaseResources` / `setBusesLayout`, never mid-callback.

## Build and test

```bash
cmake --build build/release --target vstplug_VST3 -j4      # also vstplug_Standalone
find build -name '*.vst3'                                   # verify the artefact path, don't guess
```

Shell behaviour is decided by `OfflineRender` (sections §5.19–§5.22), `ShellTests` and
`CrossProductTests` — see `tools/AGENTS.md`.

## Decisions

D-001, D-002, D-011, D-021, D-022, D-023, D-026 — `docs/decisions.md`.
