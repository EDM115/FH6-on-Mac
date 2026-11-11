# Timestamp and input-layout shim

This is the locally tested GPLv3 Highball-derived shim for GPTK 4 beta 2. It includes the October 24 explicit-input-offset backport, the earlier timestamp compatibility work and local diagnostic switches. The input-layout source reference is [Highball PR 403](https://github.com/gauthierpiarrette/highball/pull/403), head `4cb329372d30572d29c959655999677764f7983b`  
The beta 2 export table keeps GetBehaviorValue at 100 and the serialization exports at 109/110. The later beta 3 table inserts another export and cannot replace this file without a new review  
Local changes are preserved from the tested October sources. The November 11 export adds notices and rejects aliasing the input and output of the PE renamer. [The source map](../../provenance/source-map.json) records the lab input hashes, [GPL-3.0](../../LICENSES/GPL-3.0) preserves the license

```zsh
python3 tools/build_timestamp_shim.py
```

The build writes `.local/build/tsshim/d3d12.dll` and a private build record. It does not replace a runtime file. The [component guide](../../docs/wine-components.md) describes the backing DLL and Unix-side link  
Use `HB_TSSHIM_DEPTHRESOLVE=1` with this repository's depth-source repair. `HB_TSSHIM_APPEND_OFFSETS=1` enables the tested layout rewrite. Leave the unrelated prefill, sampler, RT-barrier, VRS and shader-model-clamp experiments at their defaults  
QPC values are captured during command recording. They are compatibility timestamps, not measurements of GPU execution, and command-list replay/upload-ring reuse remain limitations
