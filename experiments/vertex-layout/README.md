# Explicit vertex input offsets, 24 October 2026

The beta 2-compatible backport resolves appended input-element offsets before pipeline creation, pipeline-state streams and pipeline-library loads. It preserves separate input-slot cursors and format alignment, unknown formats and unsupported streams pass through  
The source comes from [Highball PR 403](https://github.com/gauthierpiarrette/highball/pull/403), head `4cb329372d30572d29c959655999677764f7983b`. The local backport retains the beta 2 export table, without the PR's unrelated DLSS bridge or engine upgrades

[layout_test.c](layout_test.c) checks :

- Mixed byte, halfword and float alignment, explicit offsets, overlaps and interleaved input slots
- The independently expected nine-element livery layout, ending at 36 bytes
- Preserved stream bytes and passthrough for unknown/truncated input
- Descriptor sizes/alignments against the installed SDK and [six Microsoft declaration excerpts](../../third_party/directx/sdk_abi_excerpt.h)

A deliberately incorrect global cursor failed three assertions in the lab, the per-slot implementation passed. The real Metal-backed timestamp probe still passed 120 frames  
The user compared the Toyota Tacoma TRD Pro FE's Fuji/red-sun/cherry-blossom livery before and after. The broken metallic facets changed to the expected artwork. Lighting and viewpoint differed, so this was a livery correctness check rather than a color or performance comparison. Later black buildings, vegetation and props remain unresolved  
`HB_TSSHIM_APPEND_OFFSETS=1` selects the tested behavior. The compiled layout test includes the GPL shim, see [third-party notices](../../THIRD_PARTY_NOTICES.md). Build it with `python3 tools/build_experiment_probes.py`, the output is `.local/build/probes/layout.exe`
