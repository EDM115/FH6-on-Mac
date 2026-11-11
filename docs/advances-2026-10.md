# October 24 and 25 advances

This update follows the October 4 [startup and rendering investigation](investigation.md). The supplied research bundle provided leads, the results below come from the subsequent local API controls and user game checks. The November export preserves source and conclusions, not private captures

| Step | Local result | Source and limits |
| --- | --- | --- |
| Timestamp queries | Stock beta 2 retained the prefill pattern, the shim returned advancing completed queries. The game progressed without a CPU-selector write | [Timestamp experiment](../experiments/timestamp-queries/README.md), CPU recording-time values remain approximate |
| Game update | Build 440.853 showed MISSING TITLE UPDATE for the profile. The user updated the clone to 6.461.691.0 / Steam 25588841, then reached garage and driving | Old helper rejects the new executable, no save downgrade was attempted |
| Saved login | The prefix lookup failed with count 1 in both original controls. Patch 0019 passed the synthetic test and three clean sign-ins | [Saved-login experiment](../experiments/saved-login/README.md), no Xbox tokens inspected |
| Vertex layout | Per-slot explicit offsets repaired the tested Tacoma livery | [Layout experiment](../experiments/vertex-layout/README.md), black buildings/vegetation remain separate |
| Spatial audio | Channel mapping and surround-to-stereo mixing passed isolated checks, engine and voices initially returned | [Audio experiment](../experiments/spatial-audio/README.md), later car-change muting remains |
| Process memory | Reserved and touched memory became distinguishable, game System Memory changed from 160,146 MB to 9,796 MB | [Memory experiment](../experiments/memory-footprint/README.md), not a physical-memory reduction |
| msync leak | The matching original pair exhausted registrations, the patched pair completed about 6.58 million bounded waits without that growth | [msync experiment](../experiments/msync-registration/README.md), not a demonstrated FPS gain |
| Online | TTL255 fallback restored session setup. A one-byte V6ONLY correction made it work without extra diagnostics | [Online experiment](../experiments/online-session/README.md), protocol hop limits are coupled |
| Frame pauses | Controls narrowed the recurring pattern to time-related activity correlated with one game thread | [Frame experiment](../experiments/frame-pauses/README.md), cause/fix unresolved |

## Corrections worth preserving

The NETRESET warning appeared just before socket close, but the enclosing caller ignored that result. Making it succeed did not restore online play. The later checked TTL failure was the useful boundary  
Relay tracing was not a multiplayer requirement. It hid uninitialized upper bytes in the authored TTL helper. A silent numeric audit and nonzero-stack test exposed that bug, the final active patch includes its correction  
The first memory-patch launch chose the wrong d3d12 backend after rebuilding ntdll. Restoring loader precedence fixed that comparison, the resulting FH203/FH204/FH205 messages did not prove unsupported hardware  
The first audio callback diagnostic used Wine logging on a native thread without a Wine TEB. Bounded native logging corrected the instrumentation hang. Aggregate energy still cannot identify a missing engine voice  
The apparent 64-frame pause cadence did not hold at the higher frame-rate limit. A repeated wall-clock pattern survived, and msync off did not remove it. The final baseline retained msync on

## Preserved baseline

The final quiet stack uses the timestamp/layout shim, 26 shader fallbacks, guarded depth-source repair, credential and paired audio patches, loader precedence, memory accounting, the paired msync repair and corrected TTL helper. Extra HTTP/socket/connectivity/relay diagnostics are disabled  
The user later requested restoration after the msync-off control. The original configuration, video settings and component hashes were rechecked. That restart was still loading at the final check, a fresh free-roam confirmation was not claimed for it  
The source export remains separate from gameplay validation. [The launch guide](launch.md) distinguishes the recorded binary profile from explicitly selected local rebuild candidates
