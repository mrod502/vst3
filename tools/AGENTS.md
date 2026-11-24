# tools

Harnesses, test programs and measurement instruments. Nothing here ships.

| Target | Kind | What it is |
|---|---|---|
| `EntangleHarnessCore` | static lib, JUCE-light | `Stream`, the sample-rate/block/tolerance constants, the three `Shape`s, 11 seeded generators, the block-plan helpers, the eight metrics |
| `EntangleHarnessShell` | static lib, links `vstplug` | the `BusSetup` layouts, the drivers, the render routes, the three expectations with their optional `FailDump`, `writeWav`, `ReportLogFilter`, and **the one accepted-layout table** (`acceptedLayouts()` plus the comparison helpers) |
| `EntangleAllocWatch` | INTERFACE | `AllocWatch.hpp` + `entangle_enable_alloc_watch(<target>)`, which appends the one `AllocWatchShim.cpp` to one binary |
| `OfflineRender` | console app, in CTest | renders through the plugin's shared code and prints a numbered section log — one CTest case, not a suite |
| `ShellTests` | Catch2, in CTest | `"harness: "` cases — shell identity, block-plan invariance, the dry path delayed by `R`, generator reproducibility, the failure dump, the tripwire controls |
| `CrossProductTests` | Catch2, in CTest | the accepted-layout × callback-plan matrix; the quick arm at H = 512 / `L` = 1024 and the long arm carrying the `"cross product long: "` prefix at the shipped H = 128 / `L` = 8192 |
| `UiProbe` | Catch2, in CTest | `"ui: probe: "` — the kit's and the display panels' pixels, sampled from an offscreen `paintEntireComponent` |
| `UiAllocTests` | Catch2, in CTest | `"ui: alloc: "` — allocations inside one paint, as equalities against the same drawing without the kit |
| `CpuBaseline` | console app, **not** in CTest | per-callback cost figures |
| `PrepareFootprint` | console app, **not** in CTest | live bytes per configuration, printed after a host/build-type line |

## Rules

- **The harness is the only place a chain is assembled besides `EngineChain.h`.**
  `EntangleHarnessShell` links `vstplug` and adds no shell `.cpp` of its own, so the shell a leg renders
  and the plugin a VST3 bundle loads are the same object code. Adding a shell source to this archive
  compiles and links green while the harness's copy silently serves instead of the plugin's.
- **One `operator new` replacement per binary.** An allocation-counting program owns the hook alone;
  `entangle_enable_alloc_watch` applied twice to a target is a silent no-op, and what stays a
  duplicate-symbol error is a *second, different* replacement reaching the same link line.
- **New shell legs go in `ShellTests` or `CrossProductTests`, not into `OfflineRender`.** A section may
  be migrated out of the monolith only by its own item, with its own mutation proof.
- **Cost and footprint programs are not assertions.** `CpuBaseline` and `PrepareFootprint` print figures
  and exit 0; wall-clock and byte counts on a shared host are not pass/fail signals, which is why they
  stay out of CTest.
- **A second instrument for a second reader goes in `tools/harness/` as a header.** `FontNameTable.hpp`
  reads a face's `name`, `head`, `hhea` and `cmap` tables out of raw bytes, so a leg can weigh what JUCE
  reports (and which glyphs it thinks exist) against what the file says; header-only means it rides the
  harness include path with no archive to update.
- **A filtered run that matched nothing exits 0.** Filter on case-name prefixes, never target names:
  `ctest -R OfflineRender` and `ctest -R ShellTests` select zero tests and pass. The two run sets are
  quick = `ctest --test-dir build/<config> -E "OfflineRender|cross product long"` and long =
  `OfflineRender` plus `"cross product long: "`.
- **Artefact paths follow the target kind.** `OfflineRender` and `PrepareFootprint` are
  `juce_add_console_app`, so they land in `tools/<Target>_artefacts/Release/<Target>`; `CpuBaseline` is
  `build/<config>/tools/CpuBaseline` and `ShellTests` `build/<config>/tools/harness/tests/ShellTests`.
  Prefer `ctest`, which knows where each went. `ShellTests --dump-wavs` only runs its dump leg when the
  binary is invoked directly, so a `Skipped` line in a CTest run is the honest state.

## Build and test

```bash
R="${BUILD_ROOT:-build}/release"
cmake --build "$R" --target OfflineRender ShellTests CrossProductTests -j4
ctest --test-dir "$R" -R OfflineRender --output-on-failure
ctest --test-dir "$R" -E "OfflineRender|cross product long" --output-on-failure
"$R/tools/PrepareFootprint_artefacts/Release/PrepareFootprint"
```

`OfflineRender`'s sections: §5.1–§5.18 the DSP and lifecycle anchors, §5.19 the re-prepare guard inside
a real `setBusesLayout`, §5.20 the channel matrix, §5.21 the layout report, §5.22 the editor shell,
§5.23 the two display panels and the publication behind them — a snapshot against the level stage's own
getters on a transport that has stopped, the window the shell sized at prepare, the derived axes, the
spread figure recorded rather than judged, `idle`, the counted half of a quiet tick, and the three
components in the cells `03` left empty.

**§5.22 is where the sheet's words are decided**, in three of its legs. *C1 measures every word against the room
the `paint()` gives it* — 75 rows at 595, 1190 and 2380 px: the six `ui::Text`, four bar labels and their values,
three rotated captions, two choice-bar items, the meter's number — through `ui::Theme::measureText`, the same
`GlyphArrangement` route the paint takes, gated at **zero** deficit. Room is never the component's own rect: a
rotated caption's is `ui::RotatedPlacement::extentPx`, a bar label's is the label column, a value's is the body less
the inset; vertically it is `ui::stackReservePx` (first line's ink plus one unrounded pitch per extra line) against
the reserve the placer handed the box. The same section prints the squeeze each role asked for against the scale the
font carries, and the digit pairs (`1024` against `8192`) both pitched and unpitched. *C9 walks the words*: the
blend, grain and layout sentences against the strings that write them, then the naming table — every control's own
word, and the rule that no control ships unnamed. *The face row* prints each embedded face's PostScript name
(`nameID 6`, read out of the bytes with `tools/harness/FontNameTable.hpp`, because JUCE reports the typographic pair
instead), cap/x/hhea per em against what draws, the ten digit advances, and per role the cap against the figure the
previous face produced (F-030, D-037).

*The dial's legs are the blend's, and D-038 re-asked all four against a `proportional` dial.* C2 asserts the
`blend` control is the proportional one; C3 counts the pixels no *pair* of tokens can make and requires every one
of them on the live dial — the wedge's antialiased edge over the face over the sheet — with none on the inert one;
C5 walks `q ∈ {0, 0.25, 0.5, 0.75, 1}` requiring a sweep within 0.001 turn of `q`, fill strictly growing, **zero**
`ink` inside the disc at every position and a plain `track` face at `q = 0`; C9 compares the readout's sentences
verbatim, the no-sidechain pin clause included. The blend value any leg writes goes through
`harness::setBlendDial`, which fails the run on a figure outside `[0, 1]` rather than letting a leftover `p` read
as a legal `q`.
§5.21 swaps the installed `juce::Logger` for a counting one, so it runs last and `main()` filters the
log to keep the other sections' output byte-identical between runs.
