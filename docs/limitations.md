# Known issues

## Coverage

The latest local gameplay evidence in this export is from October 25, 2026, on FH6 6.461.691.0 / Steam build 25588841, GPTK 4 beta 2, CrossOver 26.3-derived Wine and the M5 Max test machine. Later builds, other machines and long-session behavior need their own checks  
The public source export has separate syntax, unit, patch and build checks. We have not replayed installation from an empty checkout and bottle. Keep the game and renderer guards, a different binary needs analysis rather than an edited expected hash

## Remaining playtest problems

| Area | Observation and limit |
| --- | --- |
| Frame pauses | Brief recurring pauses remain in free roam and events, online and Solo. Parking, lower resolution, V-sync off and msync off did not remove the pattern |
| Audio | Initial engine and voices improved. After a car change, engine/car-impact sounds can disappear while tire squeal and some world impacts remain. Camera cycling or releasing the accelerator while stationary restored them in recorded cases |
| Materials | Many Tokyo buildings look glossy black. Black vegetation and props, including some guardrails, remain. Winter can make the vegetation issue less visible. The livery repair does not establish a fix for these surfaces |
| Steam UI | The user reports Steam's window and menu-bar context menu turning black after a while. Cause unknown, investigation deferred |
| Loading | Race/loading transitions can take a long time. No general loading-time fix is established |
| Timing HUD | The shim records CPU command-building time, not GPU execution. The game showed GPU utilization at 0%, but the exact HUD calculation has not been traced |
| Memory budget | The process-memory display improved. The separate roughly 106,168 MB Video Memory budget was not changed |
| Network stability | Shared-session connection and Eliminator entry worked after the TTL workaround. One transient disconnect recovered during a later control, its cause is unknown. This is not proof of indefinite connection stability |

Saved sign-in survived three clean same-day relaunches and later trials. Expired tokens, system restarts and other accounts were not independently covered

## Limits of the fixes

The timestamp shim substitutes QPC values captured during command recording. Command-list replay and upload-ring reuse are limitations, and its values must not be used as GPU execution measurements  
The memory patch uses Mach charged footprint for PagefileUsage and the corresponding PrivateUsage path. PeakPagefileUsage follows the current footprint rather than a retained historical maximum. VirtualSize and working-set reporting remain separate  
The TTL fallback sets both IPv4 TTL and IPv6 hop limit to 255 for the admitted dual-stack request. It is not a general implementation of independent Windows hop-limit settings or an IPv6 IP_TTL getter. The final helper uses an initialized one-byte V6ONLY value, the original four-byte local was wrong for this Wine getter  
The msync patch fixes a reproduced wait-registration leak. It is not a demonstrated FPS optimization, disabling msync did not fix the periodic pauses. Native unwinding and inotify support must survive any replacement client/server build

## Performance evidence

At 60 FPS, many pauses were about 64 displayed frames apart. At the higher limit, the game averaged about 101 FPS and typical spacing changed to about 120 frames, still roughly 1.2 seconds. An exact modulo-64 explanation does not fit both captures  
A periodic game thread becomes busy around the submission stalls while rendering workers become mostly idle. Saved wait/exception records narrow the next investigation, they do not establish the thread's purpose or a causal fix. The 30 FPS control reduced visible pauses but retained the same periodic work  
Native CPU sampling briefly caused much worse audio/frame disruption and produced poor Rosetta/Wine unwinds. Later counter reads avoided thread suspension and recorded their own overhead. The [frame-pause experiment](../experiments/frame-pauses/README.md) describes timing units, clock domains and invalid joins that must not be reused  
These short captures are controls and observations, not a benchmark suite. Scenes, input and connection transitions limit FPS comparisons. We have not compared game speed or input latency against Windows, or established support for other Forza releases
