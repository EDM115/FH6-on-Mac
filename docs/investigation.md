# Investigation log

This is a summary of the local tests from 1–4 October 2026. The repository keeps selected source files, not raw dumps, account state or game assets.

| Stage | Test and result | What we learned |
| --- | --- | --- |
| Startup | Tried native C++ libraries and GPTK 4. The game still crashed in the AMD path. | Changing the graphics runtime alone wasn't enough. |
| macOS launch | Tried quarantine removal and ad hoc signing on a modified CrossOver app. macOS still refused to open it. | A valid signature check didn't establish that the app would launch. We moved to a complete private runtime outside the app bundle. |
| GPU detection | Tried vendor overrides. One trial reached FH205; the Intel vendor path got further. | The chosen vendor affects game startup branches. |
| Missing API | Implemented a display-size stub. Editing the game import table failed; redirecting the private Wine API-set schema worked. | Keep the original game executable. |
| Title | Reached the intro and title with GPTK 4 and startup workarounds. | The next failure occurred after entering the UI flow. |
| Sign-in | Both Start and Settings showed a moving blurred background. Windows also opened sign-in from Settings. | Settings wasn't a control that bypassed authentication. |
| UI comparisons | Tried Metal 3, DPI/Retina changes, full native C++ overrides, graphics validation and broader logs. The blur remained. Shader validation made the run much slower. | These changes didn't fix popup visibility. |
| Debugging | WineDbg attachment disrupted the target. Switched to bounded read-only process probes and window metadata. | Collect state without stopping the game; saved syscall frames can still be stale. |
| Loader | The first interposer reached the launcher but not FH6. Signing the private runtime's child Wine executable enabled capture in the game. | Check the actual child process, not only Steam or the parent. |
| Shader linkage | Captured 148 failures involving `SV_ShadingRate`. Reproduced the failure with an authored shader and built a fallback for 26 local shader variants. | The fallback removed the errors. A controlled comparison still showed the blur. |
| UI assets | Removed one popup transition from a private UI archive. The run crashed. Restored the original archive. | Asset removal wasn't a usable fix. |
| UI time | Measured zero incoming delta, zero animation progress and hidden popup controls. A one-second accumulator step revealed the QR panel. | Timing caused popup invisibility in this setup. |
| Authentication | A bounded paced clock allowed account linking and Settings. | The remote sign-in implementation worked; installing Xbox Services wasn't the next step. |
| Frame time | An A/B/A test switched the frame-history selector to CPU timing, restored the original, then selected CPU timing again. | A single selector write replaced the temporary clock loop. |
| Loading | CPU timing moved loading forward, then the final screen stopped. Native diagnostics found an invalid depth texture view. | This was a separate graphics failure. More UI clock writes didn't solve it. |
| Depth capture | The first texture logger missed the call. The second covered the swizzle overload and captured it. | Zero captured calls had meant incomplete instrumentation. |
| Gameplay | Tested the guarded source-view repair under Metal 4. The game reached the garage and accepted acceleration, steering and braking. | Controllable driving worked in the tested session. |
| Playtesting | Longer user runs exposed audio, materials, loading and online issues. | Reaching gameplay wasn't the end of compatibility work. |

## Dead ends worth keeping

The [experiment archive](../experiments/README.md) includes the clock interventions, failed UI transition edit, rendering comparisons, API-set work and process probes. Historical Python snapshots exit before running. They contain assumptions tied to the old lab layout or a specific process; port them before reusing them.  
Two external AI-assisted research archives also informed the investigation. We treated their proposals as leads. Reports about native Xbox login on Windows didn't establish which flow Wine selected, and graphics errors didn't establish which UI draws failed. The window, shader and timing comparisons settled those questions in the local run.

## Evidence limits

The initial driving check covered acceleration, steering, braking and ongoing scene/HUD updates. Later reports include longer play sessions and a recorded run. We haven't benchmarked performance or compared game speed against Windows. We haven't established a first-ever result or support for other Forza releases.
