# Checks

From the repository root, using Python 3.11 or newer:

```sh
python3 tools/audit_public_tree.py
python3 -m unittest discover -s tests -v
```

These check Python syntax, local documentation links, common data leaks, shader rewrite guards and bounded cache extraction using authored fixtures. They don't start Wine or inspect your account.

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
