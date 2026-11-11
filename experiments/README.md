# Experiment archive

The new experiments below include portable source and dated results from October 24 and 25. The older archive remains further down this page. Historical Python snapshots explicitly stop before running, the new frame-capture tool is an active opt-in diagnostic. Read each experiment before using it, native probes need an explicit build

## October 24 and 25 experiments

| Investigation | Result |
| --- | --- |
| [Timestamp queries](timestamp-queries/) | Advancing compatibility values without a live game-selector write |
| [Saved login](saved-login/) | Credential prefix fix and three clean remembered sign-ins |
| [Vertex layout](vertex-layout/) | Per-slot offset backport and repaired test livery |
| [Spatial audio](spatial-audio/) | Initial engine/voice improvement, residual car-change mute and bounded diagnostics |
| [Memory footprint](memory-footprint/) | Corrected process accounting and reserved-versus-touched controls |
| [msync registration](msync-registration/) | Reproduced and fixed a paired client/server registration leak |
| [Online sessions](online-session/) | Negative NETRESET control, checked TTL failure and final quiet online fix |
| [Frame pauses](frame-pauses/) | Controlled timing/counter observations, no validated pause fix |

Build the Windows probes with `python3 tools/build_experiment_probes.py`. Run synthetic API tests in a separate account-free bottle with the intended component mappings, not the working game bottle. Match the Wine client/server synchronization setting. Native source fixtures run without Wine through `python3 tools/run_source_checks.py`

## October 1 to 4 archive

| Investigation | Result |
| --- | --- |
| [UI clock experiments](clock-causality/) | A one-second accumulator write revealed the sign-in panel. Bounded paced writes then allowed authentication and Settings. The later A/B/A frame-history test identified the CPU timing selector. |
| [Popup transition removal](ui-transition/) | Removing the popup content transition from a private UI archive produced a crash. We restored the original UI.zip. |
| [Rendering comparisons](rendering-comparisons/) | Metal 3, standard DPI, native C++ overrides and graphics validation did not reveal the popup. Shader validation caused severe slowdown and missing video in one test. The first interposed logger reached the parent but missed the FH6 child. |
| [Display API experiments](startup-display-api/) | The missing GetIntegratedDisplaySize export caused an early exception. Editing the game import table failed. The private Wine API-set redirect allowed the original executable to proceed. |
| [Read-only process inspection](process-inspection/) | Bounded ReadProcessMemory probes replaced a disruptive debugger attach. Saved syscall contexts helped locate waits and the native assertion. An already-completed fence and stale frames ruled out premature deadlock claims. |
| [Child-loader test](loader-injection/) | An inert Windows parent/child probe established whether the injected library reached both processes. The game needed the entitlement on the private runtime child loader, not just the hosted launcher. |
| [Memory-counter probe](memory-accounting/) | GlobalMemoryStatusEx reported available RAM while the game warned about impossible usage. The game display closely matched host virtual address-space size. Cross-process Windows counters returned zeros on the inspected Mach path. |
| [Texture-view logging](depth-logging/) | The first logger missed the failing swizzle overload. Version 2 recorded the Depth32Float source, R32Float request and four-sample texture. That led to the guarded source-view repair in src/depth/. |
| [Shader capture attempt](shader-capture/) | The initial converter interposer loaded in the game but captured no shaders. Reading a private D3DMetal cache snapshot recovered the 26 relevant containers. The same capture code worked with the synthetic shader. |

The export excludes game disassembly, UI archives, extracted shader bytecode, vendor binaries, raw captures and credentials. Upstream source excerpts retain their own notices in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). The current launch path is in [docs/launch.md](../docs/launch.md).
