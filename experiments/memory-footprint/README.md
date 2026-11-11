# Process-memory accounting, 24 October 2026

[Patch 0018](../../patches/wine/0018-macos-commit-charge-is-the-footprint.patch) uses Mach task footprint for the affected process-memory counter. It preserves the separate VirtualSize and working-set paths. The game comparison changed System Memory from 160,146 MB / 65,536 MB to 9,796 MB / 65,536 MB, this corrected reporting rather than freeing that amount of physical memory  
The [source fixture](counter_semantics.c) distinguishes reserved, resident and charged memory. The original function failed three assertions, the patched function passed the normal, fallback and unsupported cross-process cases. Run these native controls through `python3 tools/run_source_checks.py`

| Allocation probe, patched runtime | Reported charge |
| --- | --- |
| Initial | 10.579 MiB |
| Reserve 2 GiB without touching it | 11.111 MiB |
| Touch 256 MiB | 267.611 MiB |
| Decommit/release | 11.111 MiB |

The two unpatched variants reported roughly 41 GiB in the tiny probe and did not change that value after touching 256 MiB. All variants retained the same physical total and a separate high VirtualSize  
[memory_client_probe.c](memory_client_probe.c) queries the native extended structure because this source's GetProcessMemoryInfo wrapper only filled the basic structure. Its unused PrivateUsage field was not valid evidence. [host_memory.c](host_memory.c) provides a bounded native comparison

## Integration and limits

The first rebuilt-ntdll game run selected Wine's default d3d12 backend and showed FH203/FH204/FH205. [Patch 0003](../../patches/wine/0003-ntdll-winedllpath-prepend.patch) plus explicit `WINEDLLPATH_PREPEND` restored the preserved GPTK overlay. The corrected renderer probes and game run then passed. The initial failure did not establish unsupported GPU hardware  
PeakPagefileUsage follows the current footprint rather than a retained historical maximum. The footprint is macOS charged memory, not a complete implementation of every Windows commit semantic. The separate Video Memory budget was not fixed  
The current build recipe also retains native unwinding and inotify for the later paired msync build. Do not use an earlier reduced audio-only configuration to replace the whole native runtime
