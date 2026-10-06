# Experiment archive

Selected authored scripts from the investigation. Historical Python files stop before running; several depend on old lab helpers, fixed process identities or one test session. Read and port them rather than treating this folder as a second launcher. Native probes need an explicit build.

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

The archive excludes game disassembly, UI archives, shader bytecode, vendor code, raw results and credentials. The current launch path is in [docs/launch.md](../docs/launch.md).
