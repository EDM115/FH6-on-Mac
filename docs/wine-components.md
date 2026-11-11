# Rebuild the October components

Use a separate CrossOver 26.3.0 source copy and a stopped private runtime clone. These commands build selected components, they do not install them or start FH6. The source is the [official CodeWeavers archive](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz), linked from [CodeWeavers source downloads](https://www.codeweavers.com/crossover/source)  
Check the archive SHA-256 before extracting it :

```text
ac99c8ca4b3848f3e81784135f023df266b61c2345726ea55a50b3e030dd6872
```

## Prepare and check the source

Extract privately, then copy its `sources/wine` directory to `.local/wine-source`. Configure `.local/config.json` to point at the private runtime, bottle and loader described in [setup](setup.md). The build needs Apple Clang, a current Bison, Flex and the x86-64 mingw-w64 compiler

```zsh
python3 tools/prepare_wine_patches.py --source .local/wine-source
python3 tools/prepare_wine_patches.py --source .local/wine-source --apply
python3 tools/build_wine_components.py --source .local/wine-source --check
python3 tools/build_wine_components.py --source .local/wine-source --jobs 4
```

The patch tool first works on scratch copies, checks every post-image and only then updates the selected source tree. A changed or partially patched source tree is rejected. [The sequence](../patches/wine/README.md) includes the final one-byte TTL correction, the old audit instrumentation is excluded  
The build uses the recorded x86-64 configuration for these targets, retaining CoreAudio, libunwind and the existing runtime's libinotify library. The public libinotify header and MIT notice are under `third_party/libinotify`. The other disabled subsystems are not a recommended configuration for rebuilding and installing the entire Wine distribution  
The script checks the native ntdll exports against the configured runtime, verifies the server's inotify/kqueue imports and fixes the built CoreAudio library's sibling ntdll lookup. It writes logs and `components.json` under `.local/wine-build`. No signing or runtime installation occurs

## Build the timestamp/layout shim

```zsh
python3 tools/build_timestamp_shim.py
python3 tools/build_experiment_probes.py
python3 tools/run_source_checks.py
```

The shim builder preserves the beta 2 `.def` export table and stamps Wine's builtin marker. It writes `.local/build/tsshim/d3d12.dll` and its source/artifact hashes. The Windows probe builder compiles standalone fixtures, it does not execute them  
Before replacing Apple's PE in the private runtime, preserve the original. Use the PE renamer to create a separate backing copy :

```zsh
python3 third_party/highball-tsshim/pe_export_rename.py ORIGINAL_D3D12_DLL .local/build/tsshim/apd12.dll apd12.dll --mark-builtin
```

Replace `ORIGINAL_D3D12_DLL` with the original beta 2 file from your preserved overlay, not the shim. The helper refuses an identical source/destination. The [recorded runtime profile](../provenance/tested-runtime-2026-10-25.json) includes the known renamed-backing hash. No Apple DLL is shipped here

## Assemble a stopped private runtime clone

Keep the original runtime as rollback. Copy only the selected built files into the clone :

| Build output under `.local/wine-build/` | Destination under the private runtime |
| --- | --- |
| `dlls/advapi32/x86_64-windows/advapi32.dll` | `lib/wine/x86_64-windows/advapi32.dll` |
| `dlls/mmdevapi/x86_64-windows/mmdevapi.dll` | `lib/wine/x86_64-windows/mmdevapi.dll` |
| `dlls/winecoreaudio.drv/x86_64-windows/winecoreaudio.drv` | `lib/wine/x86_64-windows/winecoreaudio.drv` |
| `dlls/winecoreaudio.drv/winecoreaudio.so` | `lib/wine/x86_64-unix/winecoreaudio.so` |
| `dlls/ntdll/ntdll.so` | `lib/wine/x86_64-unix/ntdll.so` |
| `server/wineserver` | `bin/wineserver` |
| `dlls/ws2_32/x86_64-windows/ws2_32.dll` | `lib/wine/x86_64-windows/ws2_32.dll` |
| `dlls/winhttp/x86_64-windows/winhttp.dll` | `lib/wine/x86_64-windows/winhttp.dll` |

Ntdll and wineserver are one matched pair. Preserve the runtime's unchanged libinotify library and existing PE ntdll. If the hosted wineserver path is a hard link, verify that both paths still identify the updated server. A mapped-path spelling difference alone does not prove a different executable  
In `lib64/apple_gptk/wine/x86_64-windows`, keep the renamed original `apd12.dll` beside the new shim `d3d12.dll`. In the sibling `x86_64-unix` directory, `apd12.so` must resolve to the original `d3d12.so`. The shim relies on both the PE export-name distinction and that Unix-side module  
Retain the display workaround, native C++ libraries, prepared shaders and guarded depth repair from [setup](setup.md). The current launcher sets `WINEDLLPATH_PREPEND` so the rebuilt loader selects this GPTK overlay, and `HB_TSSHIM_DEPTHRESOLVE=1` so the shim leaves the resolve to the existing depth repair

## Choose the preflight profile

For the preserved lab runtime, the plain wrapper checks the recorded October binary hashes :

```zsh
python3 tools/launch_current.py --check
```

For components rebuilt by this checkout, keep their build outputs, receipts and prepared source tree, install them in the stopped private clone, then select the local-rebuild check explicitly :

```zsh
python3 tools/launch_current.py --rebuilt-components --check
```

This mode checks the recorded patch/source hashes, the eight actual build artifacts, shim sources/artifact and the corresponding installed runtime files. The original game, D3DMetal, backing DLL and unchanged-component guards still apply. It does not relabel a rebuild as a gameplay-verified binary set  
After preflight, the same command without `--check` launches that candidate for the [separate game checks](launch.md). Record sign-in, loading, driving, online and clean relaunch outcomes before replacing a known working runtime. Do not edit the public tested-hash profile to silence a mismatch

## Rollback

Exit the candidate game and Steam, wait for that bottle's server to stop, then select the preserved runtime in the local configuration. Restore paired components together. All generated files, local receipts, logs and copied sources remain under `.local`, they are excluded from publication
