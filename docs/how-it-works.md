# How it works

The October 25 stack keeps the startup, shader and depth changes from the [October 4 investigation](legacy-how-it-works.md), then adds the checked components below. We have not removed every older workaround separately to establish a minimal set

## Startup and rendering foundation

`D3DM_VENDOR_ID=32902` avoids the observed AMD-specific startup path. It does not emulate an Intel GPU. `SteamDeck=1` selects the remote sign-in flow. The private bottle retains the display API proxy and native Microsoft C++ libraries  
The shader overlay replaces only hashes for the 26 locally prepared `SV_ShadingRate` fallbacks, leaving other bytecode unchanged. The depth hook preserves a legal depth source view for the observed MSAA resolve path. It still checks beta 2's UUID, instructions, caller, format and sample count  
The timestamp shim normally has a depth-resolve bypass of its own. This setup uses `HB_TSSHIM_DEPTHRESOLVE=1` so the resolve reaches the separately verified source-view repair

## Timestamp queries and vertex layouts

The [Highball-derived shim](../third_party/highball-tsshim/README.md) sits in front of Apple's d3d12 PE module. It serves timestamp heaps with QPC values at command recording, copies those values through an upload ring and reports the matching QPC frequency. It also retries unsupported shader-model enum queries with lower versions  
Wine 11 distinguishes builtin modules by their internal export names. The original Apple PE is copied as `apd12.dll` with its export-directory name changed, and the corresponding Unix-side link points at Apple's original implementation. The shim retains the beta 2 export table, the later beta 3 table is incompatible with that assumption  
The layout backport resolves `D3D12_APPEND_ALIGNED_ELEMENT` to explicit offsets before relevant pipeline creation/library calls. It keeps a cursor per input slot and respects format alignment. Unknown formats and unsupported streams pass through. The independent layout tests catch a single-cursor mutation, and the same Tacoma livery visibly improved in the game

## Saved sign-in

Patch 0019 keeps registry credential matches when the separate host-Keychain query returns no matches. The original code returned failure even with one matching registry item. A synthetic probe reproduced the failure without reading Xbox tokens, then three clean patched relaunches remembered the user's sign-in

## Spatial audio

Patches 0022 and 0023 are a pair. One orders static spatial objects into the Windows channel layout, the other uses AUSpatialMixer when an 8- or 12-channel stream targets a device with fewer output channels  
The offline probe checks every input channel at 44.1 and 48 kHz. The game initially regained engine sound and voices. Car-change muting remains a separate problem, aggregate mixer energy does not identify a particular engine voice

## Memory and synchronization

Patch 0018 changes the affected process-memory accounting to Mach footprint while retaining separate virtual/resident counters. A reserved-versus-touched allocation probe distinguishes those values, the game then reports a plausible System Memory figure  
Patch 0014 unregisters early-return msync waits, fixes the server's index-1 cleanup case and bounds the exhausted-pool warning. Build ntdll and wineserver together. The patched bounded stress completed about 6.58 million waits without the reproduced pool growth  
Patch 0003 adds `WINEDLLPATH_PREPEND` before builtins. This matters after rebuilding ntdll, otherwise the default Wine renderer can win over the GPTK overlay even when the desired graphics files are present

## Online sessions

The caller ignored the unsupported NETRESET result, so that warning did not explain the socket close. The checked failure was a later IP_TTL=255 request on a bound dual-stack IPv6 UDP socket. macOS returned EINVAL, and FH6 closed the socket  
The opt-in fallback sends the admitted request through the native IPv6 hop setting, which Darwin applies to both protocol hop limits. Actual loopback packet metadata verified 37→255 with the gate on and unchanged values with it off  
The first helper used an uninitialized four-byte V6ONLY local, while this Wine getter wrote one byte. Extra tracing happened to mask that bug. The final patch uses `BYTE v6only = 1`, and a nonzero-stack regression reproduces the original rejection before the corrected helper passes. Quiet online play then worked without relay tracing or HTTP metadata logging  
The [patch sequence](../patches/wine/README.md) consolidates that correction into the active TTL patch. The [online experiment](../experiments/online-session/README.md) keeps the negative results and diagnostic limits
