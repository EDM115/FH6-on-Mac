# Dependencies and credits

| Project | Role here |
| --- | --- |
| [CodeWeavers CrossOver](https://www.codeweavers.com/crossover) / [Wine](https://www.winehq.org/) | Windows API compatibility and the bottle/runtime used in the tests |
| [Apple Game Porting Toolkit](https://developer.apple.com/games/game-porting-toolkit/) | D3DMetal, the Metal 4 backend and shader conversion libraries from the evaluation environment |
| Apple Rosetta 2 | x86-64 execution on Apple silicon |
| [Microsoft DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) | Local DXIL disassembly, assembly and validation; two pinned headers used to build our bridge |
| Microsoft gaming and C++ runtimes | The game's local dependencies, including the Xbox account flow |
| [Valve Proton](https://github.com/ValveSoftware/Proton) | Source and issue references during research; no Proton runtime is bundled or installed by these tools |
| [Highball](https://github.com/gauthierpiarrette/highball) / [Highball-engine](https://github.com/gauthierpiarrette/highball-engine) | Timestamp/layout source and the tested credential, audio, memory, loader and msync patches |
| [libinotify-kqueue](https://github.com/libinotify-kqueue/libinotify-kqueue) | Public header for rebuilding the server against the preserved runtime library |
| Steam and Forza Horizon 6 | User-provided game installation |

The MIT license covers the independently authored parts of this repository. The Highball-derived shim retains GPLv3 terms, Wine-derived patches/fixtures retain LGPL terms and the included headers retain their MIT notices. See [third-party notices](../THIRD_PARTY_NOTICES.md). Fetch the game and vendor software from their official sources; this repository doesn't redistribute them.

## Source references

- [Microsoft: variable-rate shading](https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs), for `SV_ShadingRate` and the 1×1 fallback.
- [Apple: DYLD environment entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.allow-dyld-environment-variables), for the private loader's added entitlement.
- [Microsoft DXC headers at the pinned commit](https://github.com/microsoft/DirectXShaderCompiler/tree/4781bc2115d6e64f8f344664de0f0b6ec1d93015/include/dxc). The downloader records their SHA-256 values in code and leaves fetched files in `.local/third-party/`.

The GPTK page describes Metal 4 support and DXIL-to-Metal conversion. It doesn't validate these FH6 workarounds. Local experiments described in [how it works](how-it-works.md) provide that evidence.

## October source references

- [Highball PR 403 at the reviewed head](https://github.com/gauthierpiarrette/highball/tree/4cb329372d30572d29c959655999677764f7983b), for the explicit-input-offset backport. The exported shim keeps beta 2 exports
- [Official CrossOver 26.3.0 source](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz), with archive and affected-file hashes in the [patch manifest](../patches/wine/series.json)
- [Wine NETRESET implementation commit](https://github.com/wine-mirror/wine/commit/19d5fbd382ccef3e5dc929b661dae63c10ec4f82), a tested compatibility backport that did not resolve the game's session failure
- [Apple dual-stack hop handling](https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/bsd/netinet6/ip6_output.c), used to explain the coupled native fallback
- [Microsoft dual-stack socket guidance](https://learn.microsoft.com/en-us/windows/win32/winsock/dual-stack-sockets), for protocol-specific option semantics
- [Apple thread counter implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c), for the CPU counter units. The local SDK and observed clock calibration also informed the frame-capture tool

These source links describe implementation or API contracts. The dated local tests establish the FH6 observations, no current upstream release is being claimed as a drop-in replacement
