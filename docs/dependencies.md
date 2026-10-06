# Dependencies and credits

| Project | Role here |
| --- | --- |
| [CodeWeavers CrossOver](https://www.codeweavers.com/crossover) / [Wine](https://www.winehq.org/) | Windows API compatibility and the bottle/runtime used in the tests |
| [Apple Game Porting Toolkit](https://developer.apple.com/games/game-porting-toolkit/) | D3DMetal, the Metal 4 backend and shader conversion libraries from the evaluation environment |
| Apple Rosetta 2 | x86-64 execution on Apple silicon |
| [Microsoft DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) | Local DXIL disassembly, assembly and validation; two pinned headers used to build our bridge |
| Microsoft gaming and C++ runtimes | The game's local dependencies, including the Xbox account flow |
| [Valve Proton](https://github.com/ValveSoftware/Proton) | Source and issue references during research; no Proton runtime is bundled or installed by these tools |
| Steam and Forza Horizon 6 | User-provided game installation |

The MIT license covers this repository's authored scripts, probes and documentation. External projects retain their own licenses. Fetch the game and vendor software from their official sources; this repository doesn't redistribute them.

## Source references

- [Microsoft: variable-rate shading](https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs), for `SV_ShadingRate` and the 1×1 fallback.
- [Apple: DYLD environment entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.allow-dyld-environment-variables), for the private loader's added entitlement.
- [Microsoft DXC headers at the pinned commit](https://github.com/microsoft/DirectXShaderCompiler/tree/4781bc2115d6e64f8f344664de0f0b6ec1d93015/include/dxc). The downloader records their SHA-256 values in code and leaves fetched files in `.local/third-party/`.

The GPTK page describes Metal 4 support and DXIL-to-Metal conversion. It doesn't validate these FH6 workarounds. Local experiments described in [how it works](how-it-works.md) provide that evidence.
