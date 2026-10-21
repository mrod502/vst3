---
name: juce-apvts-parameters
description: Wire, persist, and read AudioProcessor parameters through JUCE's AudioProcessorValueTreeState (APVTS) without breaking its construction order, thread boundaries, or normalised/denormalised value semantics.
source: learned
---

# JUCE AudioProcessorValueTreeState (APVTS) Parameter Management

Distilled from `juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.{h,cpp}`
(JUCE 9). APVTS owns one `ValueTree` that represents an `AudioProcessor`'s entire state,
mirrors every `RangedAudioParameter` into it, and exposes per-parameter atomics plus GUI
attachments.

## When to Use

- Declaring parameters for a JUCE `AudioProcessor` (new plugin or migrating off
  hand-rolled `AudioParameterFloat` members).
- Reading parameter values from the audio thread (`processBlock`).
- Implementing `getStateInformation` / `setStateInformation` (preset save/restore).
- Connecting sliders, combo boxes, or buttons to parameters in an editor.
- Reacting to a parameter change with non-trivial side effects (re-allocating,
  rebuilding a table, changing channel count) via `APVTS::Listener`.
- Debugging "my slider is dead", "preset doesn't restore", "value is 0.7 not 44.1k",
  or "undo broke".

## Core Model (read before coding)

Three layers, each with its own value representation:

| Layer | Representation | Who owns it |
|---|---|---|
| Host / `AudioProcessor::getParameters()` | normalised `float` in `[0,1]` (`parameter->getValue()`) | the processor (APVTS only borrows) |
| `ParameterAdapter::unnormalisedValue` | `std::atomic<float>` in **engineering units** — what `getRawParameterValue()` returns | the adapter |
| `ValueTree state` | `PARAM` children, `id` + `value` properties, **denormalised** | APVTS, updated lazily on the message thread |

Fixed identifiers (not configurable): child type `PARAM`, value property `value`,
ID property `id`. Persisted XML is therefore `<PARAM id="cutoff" value="1200.0"/>`.

Sync direction and cost:
- **parameter → tree**: asynchronous. A `Timer` (started at 10 Hz in the constructor)
  calls `flushParameterValuesToValueTree()`; it re-arms at 20 ms (`1000/50`) when anything
  changed and backs off to as slow as 500 ms when idle
  (`jlimit (50, 500, getTimerInterval() + 20)`). The tree can lag a change by up to ~0.5 s.
- **tree → parameter**: synchronous. `valueTreePropertyChanged` / `valueTreeChildAdded` →
  `setNewState()` → `setValueNotifyingHost()`. `valueTreeRedirected` re-binds every adapter
  to its child tree via `updateParameterConnectionsToChildTrees()`.

## Procedure

1. **Declare every parameter in the APVTS constructor via `ParameterLayout`.** This is the
   supported path; it registers each parameter with the processor and the adapter table in
   one step.
   ```cpp
   MyProcessor()
       : apvts (*this, &undoManager, "PARAMETERS",
                { std::make_unique<AudioProcessorParameterGroup> ("dynamics", "Dynamics", " | ",
                      std::make_unique<AudioParameterFloat> (
                          ParameterID { "threshold", 1 }, "Threshold",
                          NormalisableRange<float> { -60.0f, 0.0f, 0.1f }, -12.0f),
                      std::make_unique<AudioParameterBool> ("bypass", "Bypass", false)),
                  std::make_unique<AudioParameterChoice> ("mode", "Mode", modeNames, 0) })
   {}
   ```
   `ParameterLayout` accepts `unique_ptr<RangedAudioParameter>` and
   `unique_ptr<AudioProcessorParameterGroup>` interleaved, is move-only, and also supports
   `layout.add(...)` in a loop or `ParameterLayout (begin, end)` for programmatic sets —
   build the layout in a free function and pass it to the member initialiser.

2. **Never assign `state` or add parameters after construction.** The internal assertion is
   `jassert (! state.isValid())` — "All parameters must be created before giving this
   manager a ValueTree state!". The two-step pattern (deprecated
   `APVTS (processor, undoManager)` → `createAndAddParameter(...)` →
   `state = ValueTree (type)`) still exists but is documented as slated for removal; use it
   only when parameters genuinely cannot be known up front, and honour the ordering.

3. **Prefer the dedicated parameter types over `APVTS::Parameter` + flags.** Resulting
   ranges (asserted by JUCE's own unit tests):
   - `AudioParameterBool` → `NormalisableRange (0, 1, 1)`
   - `AudioParameterFloat` → range retained exactly, including `interval`, `skew`,
     `symmetricSkew`
   - `AudioParameterInt (min, max)` → `(float)min, (float)max, interval 1`
   - `AudioParameterChoice (n items)` → `(0, n-1, 1)` and `getNumSteps() == n`
   `AudioProcessorValueTreeStateParameterAttributes().withDiscrete(true)` /
   `.withBoolean(true)` only set the `isDiscrete()` / `isBoolean()` **flags** on
   `APVTS::Parameter` (an `AudioParameterFloat` subclass) — they do not give you the range
   or text behaviour of the real types. Use the attributes builder for label, category,
   `withMeta`, `withAutomatable`, and the `valueToText` / `textToValue` lambdas.

4. **Read parameters on the audio thread only through the atomic, and smooth it.**
   ```cpp
   void processBlock (AudioBuffer<float>& buf, MidiBuffer&) override
   {
       juce::ScopedNoDenormals noDenormals;
       auto* raw = apvts.getRawParameterValue ("threshold"); // denormalised, may be nullptr
       if (raw == nullptr) return;
       smoothed.apply (*raw);                                 // ramp to avoid zipper noise
       ...
   }
   ```
   Cache the `std::atomic<float>*` at `prepareToPlay` / construction time if you want to
   avoid the `std::map` lookup per block (the lookup itself takes no lock, but it is a
   string compare).

5. **React to changes off the audio thread with `APVTS::Listener`.**
   ```cpp
   struct MyListener : AudioProcessorValueTreeState::Listener
   {
       void parameterChanged (const String& id, float newValue) override
       {
           // use newValue; do NOT re-read getRawParameterValue(id) here
       }
   };
   apvts.addParameterListener ("mode", &listener);
   ...
   ~MyProcessor() { apvts.removeParameterListener ("mode", &listener); }
   ```
   Callbacks fire on whichever thread changed the parameter (host automation thread, message
   thread, or your own `setValueNotifyingHost` caller) under the adapter's listener lock —
   no allocation, no `repaint()` storms, no blocking. Registration is keyed by ID, so
   register before changes matter and unregister before the listener dies.

6. **Persist with `copyState()` / `replaceState()`, never by touching `state` directly.**
   ```cpp
   void getStateInformation (MemoryBlock& dest) override
   {
       if (auto xml = apvts.copyState().toXml())
           xml->copyToBinaryData (dest);
   }
   void setStateInformation (const void* data, int size) override
   {
       if (auto xml = std::unique_ptr<XmlElement> (XmlElement::parseFromData (data, size)))
           if (auto vt = ValueTree::fromXml (*xml))
               apvts.replaceState (vt);
   }
   ```
   Both take the `valueTreeChanging` lock; `copyState()` first flushes pending audio updates
   so the snapshot is current. `state` is public — read it on the message thread for live
   inspection, and add your own extra properties/children to it freely; non-parameter
   properties survive and are your place for anything that is not a host parameter.

7. **Attach editor controls to parameters with the attachment classes, declared as members
   that outlive nothing.**
   ```cpp
   struct Editor : AudioProcessorEditor
   {
       Slider cutoff;
       std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> cutoffAtt;
       Editor (MyProcessor& p) : AudioProcessorEditor (&p), cutoffAtt (
           std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "threshold", cutoff))
       { addAndMakeVisible (cutoff); }
   };
   ```
   `SliderAttachment` / `ComboBoxAttachment` / `ButtonAttachment` are non-copyable, hold the
   link only for their lifetime, and require the APVTS and the control to outlive them
   (declare attachments after the controls they bind, and keep the APVTS a member of the
   processor, which is destroyed after the editor).

8. **Use `getParameterAsValue()` for data-bound, undo-aware UI state** (e.g. binding a
   custom component or a `Value`-based property). It returns a `Value` on the tree's `value`
   property *only if the adapter is bound to a child tree*, and it routes through the
   `UndoManager` when one was supplied.

## Pitfalls

- **`getRawParameterValue()` is denormalised, not 0..1.** The adapter stores
  `convertFrom0to1(default)`-style inverse mapping: the atomic holds real units (Hz, dB,
  choice index). Comparing it against a normalised constant, or feeding it straight to a
  0..1 DSP coefficient, is the classic silent-gain bug. Convert explicitly with
  `apvts.getParameterRange(id).convertTo0to1(v)` when you need normalised.
- **Default value is deliberately *not* snapped at construction.** `ParameterAdapter` seeds
  `unnormalisedValue` from `getRange().convertFrom0to1 (getDefaultValue())` with the comment
  "For legacy reasons, the unnormalised value should *not* be snapped on construction", and
  `APVTS::Parameter::getDefaultValue()` returns `unsnappedDefault`. So a default that falls
  between `interval` steps reads back un-snapped until the first real `setValue`. Don't
  "fix" this with a manual snap in the constructor — it emits a spurious host notification
  and an extra undo entry.
- **Wrong parameter ID fails silently.** `getRawParameterValue` / `getParameter` return
  `nullptr`; `getParameterRange` returns a default-constructed `NormalisableRange<float>`
  (0..1); `addParameterListener` does nothing at all; `getParameterAsValue` returns a null
  `Value`; `makeAttachment` hits `jassertfalse` and yields an empty attachment (a dead
  control in release). Always check the pointer from `getRawParameterValue` and centralise
  IDs in `static const String`s or `ParameterID` constants — one typo is an invisible bug.
- **Duplicate IDs are dropped, not merged.** `createAndAddParameter` returns `nullptr` and
  leaves the processor's parameter list untouched when the ID already exists. The adapter
  table is keyed by `StringRef` (`std::map`), so two params with the same ID = the second is
  invisible to the host.
- **Non-`RangedAudioParameter` in a group trips `jassertfalse`.** Only parameters deriving
  from `RangedAudioParameter` get an adapter; `AudioProcessorParameter`-only types cannot be
  managed by APVTS.
- **Never read or write `state` for a live value.** The tree lags the parameter by up to
  ~500 ms when idle (adaptive timer). Audio thread → atomic; authoritative snapshot →
  `copyState()`.
- **`copyState()` / `replaceState()` / `flushParameterValuesToValueTree()` lock.** They are
  thread-safe but explicitly **not** realtime-safe: never call them, `state.getProperty()`,
  or `ValueTree::toXml()` from `processBlock`.
- **`replaceState()` clears the undo history** when an `UndoManager` is attached. Restoring
  a preset is intentionally not undoable — don't wrap it in an undo transaction expecting a
  single undo step.
- **Don't re-read the parameter inside `parameterChanged()`.** The documented contract is
  that `getRawParameterValue()` / `getParameter()` are *not guaranteed up to date* in that
  callback; use the `newValue` argument.
- **`setNewState()` asserts `vt.getParent() == state`.** Don't call APVTS internals with a
  detached tree; hand back a tree you pulled out of `state` and it will assert.
- **Replacing the tree with an already-populated one needs the redirect path.** After an XML
  redirect, `valueTreeRedirected` re-wires adapters (`updateParameterConnectionsToChildTrees`
  clears every `adapter.tree`, re-appends missing `PARAM` children, then flushes). Bypassing
  this (e.g. poking children onto `state` by hand from an odd thread) leaves adapters bound
  to stale child trees and the UI stops following the DSP.
- **Attachments don't own the control or the state.** Deleting the `SliderAttachment`
  severs the link; destroying the APVTS or the `Slider` first leaves a dangling reference.
- **ComboBox items are spaced linearly across the parameter range.** With
  `NormalisableRange (-0.5, 0.5, 0.5)` three items land on -0.5 / 0 / 0.5. For a choice
  parameter that means interval `1` over `(0, n-1)`, or items land on unintended values.
- **APVTS is bound to exactly one processor for its whole lifetime**, and ownership of every
  parameter transfers to the processor (`param.release()` / `addParameterGroup`). The adapter
  holds a bare `RangedAudioParameter&`; keep the APVTS a direct member of the processor so
  the lifetimes cannot diverge.
- **`AudioParameterChoice::getCurrentChoiceName()` after a tree-driven write** is the check
  that the round-trip works — writing `2.0` through `getParameterAsValue` should select the
  third item and the atomic should read `2.0`.

## Verifying a Change

Drive the APVTS directly in a console test (no DAW, matching this repo's JUCE-light test
policy — APVTS itself needs `ScopedJuceInitialiser_GUI`, so such a test does sit in a JUCE
harness):
1. Construct with a layout → `processor.getParameters().size()` equals the declared count,
   and `state.getNumChildren()` equals the parameter count (**groups are flattened** —
   two groups of two parameters give 4 children, each `hasType("PARAM")` with `id` and
   `value`).
2. `param->setValueNotifyingHost (v)` → `getRawParameterValue (id)->load()` reflects it
   (after `copyState()` flushes, if you assert on the tree).
3. `addParameterListener` → change → listener receives the ID and the **denormalised** value.
4. `getParameterAsValue (id) = newValue` → parameter, atomic, and listener all agree.
5. Duplicate-ID add leaves the parameter list unchanged.
