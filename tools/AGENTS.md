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
| `UiProbe` | Catch2, in CTest | `"ui: probe: "` — the kit's pixels, sampled from an offscreen `paintEntireComponent` |
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
cmake --build build/release --target OfflineRender ShellTests CrossProductTests -j4
ctest --test-dir build/release -R OfflineRender --output-on-failure
ctest --test-dir build/release -E "OfflineRender|cross product long" --output-on-failure
./build/release/tools/PrepareFootprint_artefacts/Release/PrepareFootprint
```

`OfflineRender`'s sections: §5.1–§5.18 the DSP and lifecycle anchors, §5.19 the re-prepare guard inside
a real `setBusesLayout`, §5.20 the channel matrix, §5.21 the layout report, §5.22 the editor shell.
§5.21 swaps the installed `juce::Logger` for a counting one, so it runs last and `main()` filters the
log to keep the other sections' output byte-identical between runs.
