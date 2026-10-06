# How it works

The successful run used all the changes below. We haven't removed each earlier workaround in turn to establish the smallest required set.

## Startup

The early game crash dereferenced a null pointer inside an AMD-specific path. Setting `D3DM_VENDOR_ID=32902` reports Intel's vendor ID and avoided that branch in our tests. This doesn't emulate an Intel GPU.  
The launch also uses the bottle's native Microsoft C++ libraries: `concrt140`, `msvcp140`, `msvcp140_atomic_wait`, `vcruntime140` and `vcruntime140_1`. They're prerequisites from the local installation, not files supplied here.  
Wine lacked `GetIntegratedDisplaySize`. Our proxy exports that function, returns `E_NOTIMPL` with a zero output, and forwards the existing kernelbase exports. A change to the private Wine API-set schema routes the sysinfo API family through it. We restored the game's original executable after an earlier import-table experiment failed.  
`SteamDeck=1` selects the game's built-in remote sign-in flow. That path uses a seven-inch display assumption, so the display API proxy's necessity in the final Deck-mode setup still needs a separate test. We retained it in the working bottle.

## The invisible sign-in panel

The title background kept moving, but opening Settings or Start Game left only a blur. An early theory blamed missing Xbox components or rendering. A Windows comparison showed that Settings also requests sign-in before login, so it wasn't an independent menu test.  
Read-only probes found the popup and its controls. Their animated opacity stayed at zero while the UI received zero time deltas. Advancing the UI accumulator by one second revealed the Xbox QR panel. A paced clock then allowed sign-in and Settings to work.  
We traced the zero delta further. Frame-history source `1` contained identical `0x8000000000000000` timestamps. Source `0` used advancing CPU elapsed samples. Switching to `0` restored frame and UI time; restoring `1` reproduced the problem.  
[frame_timing_session.py](../tools/frame_timing_session.py) selects source `0` with one live data-byte write. It finds the current heap objects from build-specific globals and checks code bytes, types, pointers and configuration before writing. It samples the result and provides a read-only mode and rollback. Restarting the game resets the selection. The producer of the bad timestamps remains unresolved.

## Shading-rate fallback

D3DMetal's shader converter produced an unmatched `SV_ShadingRate` fragment input. Metal rejected 148 pipeline states. A self-authored shader reproduced the error outside FH6.  
Our private cache contained 26 affected pixel shaders, all shadow-mask variants. The fallback uses the ordinary 1×1 shading value, removes the corresponding input and feature flag, then asks DXC to rebuild and validate the container. Microsoft's [VRS documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs) describes the shading-rate semantic and rates.  
[shader_trial.cpp](../src/shaders/shader_trial.cpp) hashes incoming bytecode and substitutes only a matching, locally prepared replacement. Other shaders pass through. The overlay uses a separate cache. The tools preserve root and output signatures when preparing replacements.  
The fallback reduced those 148 pipeline failures to zero. **It did not reveal the hidden sign-in popup.** Both the unchanged-shader control and fallback run still showed the blur until we addressed timing.  
Extracted shaders and rewritten game bytecode stay on your machine. This repo includes the preparation tools and a synthetic shader test.

## Depth-view repair

The final loading screen stopped at a Metal assertion: D3DMetal requested an `R32Float` view of a four-sample `Depth32Float` texture. This is a view-format mismatch, not a missing color channel.  
[depth_source_trial.mm](../src/depth/depth_source_trial.mm) keeps the legal depth format for that source view. It checks the GPTK UUID, instruction layout, calling site, texture type, sample count, ranges and swizzle before acting. It leaves the destination and resolve shader alone. Unmatched requests retain the original behavior, including assertions.  
A synthetic fragment test reads four distinct depth samples, averages them, and checks for `0.25` in all 64 output pixels. It passed with Metal API validation. The game then logged a matching source repair, reached the garage and allowed driving.  
This hook targets one GPTK build and an observed driver path. Extending it to other formats, sample counts or versions needs new evidence.

## Memory and timing statistics

The game displayed about 151 GB of system-memory use on a 64 GB Mac. At that point, macOS showed normal memory pressure. The game's number closely matched the process's virtual address-space size, while resident memory was much lower.  
The inspected CrossOver memory-counter code maps Mach virtual size into Windows counters that can look like committed/private usage. We haven't traced which counter FH6 consumes. The game's zero FPS and huge timing values are also unreliable; neither proves the actual frame rate.
