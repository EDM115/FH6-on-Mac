# msync registration leak, 24 and 25 October 2026

[Patch 0014](../../patches/wine/0014-msync-unregister-early-return.patch) changes both the native client and wineserver. An early-return wait failed to unregister its server objects, and the server's index-1 signaled case missed earlier registrations. The patch also limits the exhausted-pool warning to one report  
The extracted control source fails seven fixture checks, the patched source passes. `python3 tools/run_source_checks.py` compiles both fixtures and requires those different outcomes. [wait_probe.c](wait_probe.c) is the separate bounded Windows integration probe

| Original lab stress | Result |
| --- | --- |
| Existing memory-fixed client and vendor server | Pool exhaustion and growing server memory, stopped at the 2 MiB diagnostic limit |
| Matching unpatched source pair | Same failure, stopped at the same limit |
| Patched client and server | 6,582,279 successful waits in 15.012 seconds, no timeouts/wait failures/pool warnings, sampled server RSS stayed at 19.17 MiB |

The patched probe also checked timeout, queued APC, wait-all and final quit-event behavior. This is a bounded leak reproduction, not proof of unlimited-session stability  
The server rebuild preserves the vendor's inotify/kqueue imports, and a directory-notification probe received the expected added-file event. Ntdll retains its native exports and unwinding support. Both source pairs passed the 120-frame renderer probe  
The user confirmed remembered sign-in and normal driving with the patched pair. A later explicit WINEMSYNC=0 comparison did not remove the recurring pauses, even after reconnection. The original WINEMSYNC=1 baseline was restored. Do not present the leak fix as a measured FH6 FPS gain  
Use a separate scratch bottle for wait stress, limit time, log size and server memory. Never switch mode against an already-running server, and never combine unmatched ntdll/server builds. The build recipe compiles both components together
