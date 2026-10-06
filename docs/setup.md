# Setup

This is the manual reconstruction path for the tested combination. The original lab already had a working CrossOver Steam bottle. We haven't tested a clean installation through this guide from start to finish.

## Prerequisites

- Apple silicon Mac. The tested machine and versions are in the [README](../README.md).
- A legitimate Steam installation of FH6 build 440.853 in CrossOver 26.3.
- Apple's **Evaluation environment for Windows games 4.0 beta 2**, obtained from [Apple Developer Downloads](https://developer.apple.com/download/all/). Obtain it yourself and follow its license terms.
- Rosetta 2 and Apple command-line tools, including `clang` and `clang++`.
- Python 3.11 or newer. The tools use the standard library; no pip packages are needed for the current launch path.
- The bottle's native Microsoft C++ runtime group: `concrt140`, `msvcp140`, `msvcp140_atomic_wait`, `vcruntime140`, `vcruntime140_1`.

## Make private copies

Exit FH6 and Steam in the source bottle first. Duplicate that bottle using CrossOver, or copy it with a method that preserves symlinks and permissions. On APFS, a copy-on-write clone avoids duplicating all game data up front. Keep the original bottle.  
Copy `/Applications/CrossOver.app/Contents/SharedSupport/CrossOver` to a private directory outside the app bundle. Copy the **whole runtime**, including its `bin`, `lib`, `lib64` and hosted-application resources. A minimal collection of libraries didn't work in the investigation.  
In that private runtime, preserve the original `lib64/apple_gptk` directory. From the mounted Apple evaluation environment's `redist/lib/`, copy both `external/` and `wine/` into a new `lib64/apple_gptk/` directory. Keep their internal layout intact. The resulting paths include:

```text
PRIVATE_RUNTIME/bin/wine
PRIVATE_RUNTIME/lib/wine/x86_64-unix/wine
PRIVATE_RUNTIME/lib64/apple_gptk/external/libd3dshared.dylib
PRIVATE_RUNTIME/lib64/apple_gptk/external/D3DMetal.framework/Versions/A/D3DMetal
PRIVATE_RUNTIME/lib64/apple_gptk/wine/
```

Copy `PRIVATE_RUNTIME/CrossOver-Hosted Application/wineloader` to a separate private file named `diagnostic-wineloader`. We'll point the launch helper at that file.  
From the repository root:

```sh
mkdir -p .local
cp config.example.json .local/config.json
```

Edit `.local/config.json` with absolute paths to the private runtime, cloned bottle and diagnostic loader. An optional `probe_runtime` can point at a matching uninstrumented private runtime; otherwise the probes use `runtime`. Keep the configuration local. `FH6_CONFIG` can select another private JSON file.  
The tested D3DMetal SHA-256 is:

```text
f5b56df1b8fe8b364dd9530651a3769c8aed948bd343be3b4510604d503e2bad
```

Check the file with `shasum -a 256`. A different hash needs a new review of the hook, not a replacement hash in the launcher.

## Build the helpers

Use the same Python interpreter for these commands. If your shell's `python3` is older than 3.11, use a newer installation or a uv environment.

```sh
python3 tools/fetch_dxc_headers.py
python3 tools/build.py --tests
```

The first command downloads two Microsoft DXC headers at a pinned commit and checks their hashes. It writes them under `.local/third-party/`. The build uses your private GPTK libraries and writes authored binaries under `.local/build/`. It doesn't install anything into the game or start Wine.

## Allow hooks in the private loaders

The game launches as a Wine child process. Setting `DYLD_INSERT_LIBRARIES` on Steam alone didn't load the hooks in FH6. We needed the additional DYLD entitlement on both the diagnostic loader and the private runtime's `lib/wine/x86_64-unix/wine`.  
Inspect the planned change:

```sh
python3 tools/sign_private_loaders.py
```

Apply it to those two private copies:

```sh
python3 tools/sign_private_loaders.py --apply
```

This replaces their vendor signatures with local ad hoc signatures while preserving their existing entitlements and adding `com.apple.security.cs.allow-dyld-environment-variables`. Recreate the copies from the originals to undo signing. The script refuses loaders inside app bundles. It doesn't change Gatekeeper or SIP.

## Retain the display API workaround

The working bottle kept this earlier workaround. Its necessity under Deck mode hasn't been isolated yet. With the private runtime and bottle stopped:

```sh
python3 tools/build_display_proxy.py
python3 tools/install_display_proxy.py
```

The installer accepts the recorded original CrossOver schema hash, saves a local backup and installs the authored proxy into the cloned bottle. An existing proxy or backup stops it; inspect an earlier installation rather than deleting its backups. Rollback, after stopping that bottle:

```sh
python3 tools/restore_display_proxy.py
```

## Generate shader replacements locally

No game shaders ship in this repo. First collect the bytecode cache from your own copy:

```sh
python3 tools/launch.py --mode shader-control --check
python3 tools/launch.py --mode shader-control
```

Wait for the title, then exit FH6 and that Steam client. This control run uses original shaders and a separate cache under `.local/shaders/control-cache/`. It is for cache collection, not loading into gameplay.  
Find the bytecode cache:

```sh
find .local/shaders/control-cache -type f -name '*bytecode*'
```

Use the actual file path in place of `CACHE_FILE`:

```sh
python3 tools/extract_cache.py CACHE_FILE .local/shaders/extracted
python3 tools/prepare_fallbacks.py .local/shaders/extracted
```

The extractor reads at most 32 MiB and selects pixel shaders with `SV_ShadingRate`. Preparation disassembles, rewrites, rebuilds and validates each candidate. The known cache produced 26 replacements. The launcher refuses a failed or empty preparation manifest; unknown shader bytecode still passes through unchanged.  
If there's no cache or no matching shader, stop and check whether the overlay loaded in the FH6 child. Don't download someone else's shader cache or invent replacements. Cache locations and contents can change between builds.  
Prepared bytecode, disassembly and manifests remain under `.local/`. Keep them out of commits and attachments. Next: [launch order](launch.md).
