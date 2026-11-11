# Third-party notices

The root MIT license applies to independently authored code and documentation. The paths below retain their upstream terms. Source copies, patches and extracted test functions are attributed here and in the source map, no vendor runtime or game binary is included

| Paths | Origin and terms |
| --- | --- |
| `third_party/highball-tsshim/` | Highball timestamp shim, export table and PE helper adaptations, GPLv3, see [GPL-3.0](LICENSES/GPL-3.0) |
| `experiments/vertex-layout/layout_test.c` | Tests that compile the Highball-derived shim, GPLv3 |
| `patches/wine/`, `experiments/online-session/history/`, `experiments/spatial-audio/audio-statistics.patch` | Wine/CrossOver and Highball-engine patches, including local changes against those sources, LGPL 2.1 or later, see [LGPL-2.1](LICENSES/LGPL-2.1) |
| `experiments/spatial-audio/map-*.h`, `experiments/memory-footprint/counters-*.h`, `experiments/msync-registration/*-source-fixture.c`, `experiments/online-session/logger_fixture.c`, `experiments/spatial-audio/spatial_mixer_probe.c` | Wine/Highball-derived functions or mixer logic used in bounded fixtures, LGPL 2.1 or later |
| `third_party/directx/sdk_abi_excerpt.h` | Microsoft DirectX-Headers test declarations, MIT, see [Microsoft's notice](LICENSES/DirectX-Headers-MIT) |
| `third_party/libinotify/` | libinotify-kqueue header, MIT, [original copyright and license](third_party/libinotify/LICENSE) |

The Highball source reference for the vertex-layout backport is [PR 403 head 4cb3293](https://github.com/gauthierpiarrette/highball/tree/4cb329372d30572d29c959655999677764f7983b). That revision's [LICENSE](https://github.com/gauthierpiarrette/highball/blob/4cb329372d30572d29c959655999677764f7983b/LICENSE) carries GPLv3. The local shim preserves beta 2 exports and the reviewed lab additions, it is not a verbatim copy of the later upstream tree  
Wine patches originate from [Highball-engine](https://github.com/gauthierpiarrette/highball-engine), Wine and the locally tested changes. [series.json](patches/wine/series.json) records their bytes and the exact affected source hashes. The source base is the official CrossOver 26.3.0 archive, whose Wine files carry their own copyright notices and LGPL terms. The [patch README](patches/wine/README.md) records patch-specific origins  
Microsoft's declaration excerpt records Git blob `99562647170a141907183fa90809cfe61444bec4`. The libinotify header is blob `519bc8ef500d6a2688bd5f71cb1bd7ddf9c04857`, its license is blob `3c462f79445751bc9410fc1b8dc7ebdd7aac92af`  
The November 11 export adds portable paths, build helpers, guards and documentation around the October sources. The root license does not relabel upstream-derived code as MIT. Local modifications to the separately licensed components retain their component's terms
