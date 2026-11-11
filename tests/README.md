# Checks

From the repository root, using Python 3.11 or newer:

```sh
python3 tools/audit_public_tree.py
python3 -m unittest discover -s tests -v
```

These check Python syntax, documentation links, common data leaks, shader/cache guards, current-launch profiles, patch integrity, the one-byte TTL regression and cleanup/override failure paths. They do not start Wine or inspect accounts. The TTL test invokes Clang on an extracted helper and a deliberately broken control.

## Native probes

After configuring the private runtime and running `python3 tools/build.py --tests`, run `python3 tools/run_native_tests.py`. It checks the expected failure as well as the passing controls. To run individual probes:

```sh
MTL_DEBUG_LAYER=1 .local/build/depth_read_probe
MTL_DEBUG_LAYER=1 .local/build/depth_fragment_probe
MTL_DEBUG_LAYER=1 DYLD_INSERT_LIBRARIES="$PWD/.local/build/depth_source_trial.dylib" .local/build/depth_fragment_probe test-hook
DYLD_INSERT_LIBRARIES="$PWD/.local/build/pipeline_log_capture.dylib" .local/build/pipeline_log_probe
```

Both depth probes should report a passing readback with 64 pixels averaging to `0.25`. The hooked fragment test checks the repair's legal view helper; it doesn't simulate the game caller's UUID/stack guard. The logging probe should capture the synthetic pipeline error and omit its unrelated message from the custom capture.  
The shading-rate reproducer takes your GPTK resource directory and a private output directory:

```sh
mkdir -p .local/tests/shading-original .local/tests/shading-fallback
.local/build/shading_rate_repro GPTK_RESOURCES .local/tests/shading-original
.local/build/shading_rate_repro GPTK_RESOURCES .local/tests/shading-fallback tests/native/shading_rate_fallback.ll
```

Replace `GPTK_RESOURCES` with `PRIVATE_RUNTIME/lib64/apple_gptk/external/D3DMetal.framework/Versions/A/Resources`. The JSON should show the original shading-rate pipeline failing and the fallback passing with red-pixel readback. The reproducer itself can exit zero after reporting a pipeline failure; inspect its JSON or use the test runner. Both use an authored shader, not an extracted FH6 asset.  
Gameplay verification remains separate: fresh launch, advancing timing, sign-in, garage, Drive, then a clean exit and relaunch. See [launch order](../docs/launch.md). Don't run archived invalid-view tests in your working game session.

## Export check, 6 October 2026

The source build, six Python checks, native depth readbacks, logger filter and shader control/fallback tests passed on the lab Mac. The exported preparation tool also rebuilt and validated all 26 private shader replacements. We kept those generated files in the lab. We did not re-sign loaders, install the API proxy, restart FH6 or replay setup from an empty bottle during this export.

## October experiment checks

```zsh
python3 tools/build_timestamp_shim.py
python3 tools/build_experiment_probes.py
python3 tools/run_source_checks.py
```

The source fixtures require the original memory and msync controls to fail and the patched cases to pass. They also check the HTTP diagnostic gate, cap and LastError preservation. The Windows probe build does not execute the resulting programs  
The patch preparer checks all before/after hashes in a scratch tree, then writes source only when `--apply` is requested. The component builder compiles the selected PE/native files and checks native unwinding exports and inotify imports. See [the rebuild guide](../docs/wine-components.md) for that separate path  
The frame-capture cleanup tests simulate an enable timeout and a log process that cannot be reaped. Disabling metrics must still be attempted. They do not touch a live process or invoke metalperftrace  
The updated launch tests exercise explicit unsets through the current wrapper as well as the shared launcher. Quiet defaults keep the TTL gate and depth repair while leaving extra network tracing/raw capture off

## November export validation

This update is checked independently of the October game runs. Local unit tests, exact patch post-images, selected component builds, probe compilation and native source fixtures are part of the export validation. New binaries and captures stay in `.local`  
A source build or preflight is not a fresh FH6 playtest. The public export does not claim a new end-to-end install, updated-game support beyond the recorded build, a complete audio repair or a frame-pause fix

On November 11 the staged export passed the public-tree audit and 27 Python tests. All nine patches reproduced the recorded post-images for 13 Wine files. The eight selected Wine components and timestamp/layout shim rebuilt, their checked PE/native exports matched the preserved baseline and the server retained its inotify/kqueue imports. Fifteen Windows probes compiled, they were not rerun as integration tests during this export  
The native memory/msync fixtures produced their expected failing controls and passing patched cases, the bounded HTTP fixture passed with diagnostics off and on, and all six native depth/logger/shader checks passed. Preflight accepted both the recorded runtime and an explicitly selected local rebuild clone, and rejected that clone under the recorded-binary profile. No game session was launched for these export checks, the working runtime and account bottle were unchanged
