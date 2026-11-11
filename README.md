# FH6 on Mac

Forza Horizon 6 running through CrossOver and Apple's Game Porting Toolkit, with fixes for startup, timing, graphics, saved sign-in and online sessions. The October 24 and 25 tests reached driving, populated shared free roam and The Eliminator on build 461.691. Several audio and rendering problems remain, along with recurring frame pauses  
This repo contains the source, patches, setup notes and experiments from that work. You'll need your own copies of FH6, CrossOver and GPTK. **Setup is manual and the fixes target specific builds.** The lab runs establish the gameplay results. The exported tools have separate checks, but setup from an empty checkout and bottle has not been replayed end to end

## Demo

The [October 4 demo](https://www.youtube.com/watch?v=akHFtDcVHV4) shows the earlier build 440.853 setup. It predates the improvements below

## What changed on October 24 and 25

| Area | Result |
| --- | --- |
| Timing and title update | The timestamp shim restored advancing menu/frame time without the old per-launch CPU-selector write. Build 461.691 reached the garage and driving |
| Saved Xbox sign-in | A credential prefix-lookup fix preserved the registry matches. Three clean relaunches remembered sign-in |
| Liveries | Explicit vertex input offsets repaired the tested Tacoma's broken metallic-looking artwork |
| Audio | Channel ordering and the native spatial mixer restored engine sound and voices initially. Engine/car-impact audio can still disappear after a car change |
| Memory display | System Memory changed from 160,146 MB to 9,796 MB in the recorded game comparison. The patch corrects accounting, it does not free 150 GB of RAM |
| msync | A paired client/server patch fixed a reproduced wait-registration leak. It did not remove the recurring game pauses |
| Online | A narrow dual-stack TTL workaround restored session setup. Its final one-byte guard correction made online work with extra diagnostics disabled |

[The October investigation](docs/advances-2026-10.md) explains the evidence and failed comparisons. [Known issues](docs/limitations.md) records the remaining limits

## Tested setup

| Component | October 24 and 25 test version |
| --- | --- |
| Mac | MacBook Pro M5 Max, 64 GB unified memory |
| macOS | 27.0.1 |
| CrossOver source/runtime | 26.3, Wine 11-derived engine |
| Apple runtime | Game Porting Toolkit 4.0 beta 2, Metal 4 backend |
| Game | Windows Steam build 25588841, executable version 6.461.691.0 |

These are dated test results, not a compatibility claim for later game or GPTK releases. The guarded launcher checks the game, renderer and tested component hashes. The old address-based timing helper rejects this updated executable

## Get started

1. [Prepare private runtime and bottle copies](docs/setup.md)
2. [Build the newer Wine components and timestamp/layout shim](docs/wine-components.md), keeping the shader and depth repairs
3. [Launch the tested current stack](docs/launch.md)

## What's here

| Path | Contents |
| --- | --- |
| [tools/](tools/) | Build, patch verification, shader preparation and guarded launch helpers |
| [src/](src/) | Authored depth-view repair, shader overlay, display stub and graphics logger |
| [patches/wine/](patches/wine/) | The checked Wine patch sequence, including the final online workaround |
| [third_party/](third_party/) | Attributed timestamp/layout shim and the small headers needed by its tests/build |
| [experiments/](experiments/) | New probes and dated results alongside the original investigation archive |
| [tests/](tests/) | Export, launch, shader and one-byte socket-guard checks |
| [docs/next-steps.md](docs/next-steps.md) | Remaining investigation work |

## Under the hood

CrossOver/Wine handles Windows APIs, Rosetta 2 translates x86-64 code and Apple's D3DMetal translates Direct3D graphics to Metal. The newer work uses Highball's timestamp shim and Wine patches, with a beta 2 layout backport and the local TTL guard correction. [Dependencies and credits](docs/dependencies.md) records the sources  
The [MIT license](LICENSE) covers the independently authored parts of this repo. The Highball-derived shim retains GPLv3 terms, Wine-derived patches and fixtures retain LGPL terms, and the header notices are preserved. See [third-party notices](THIRD_PARTY_NOTICES.md). Game files, vendor runtimes, account bottles, extracted shaders and private captures are not included

## Contributing

Include the game build, macOS version, chip, CrossOver/GPTK versions and the step that fails. Review diagnostic files for account details and local paths before sharing them. Development continues in the private lab, the maintainer handles Git operations for these curated exports
