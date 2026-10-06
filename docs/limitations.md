# Known issues

## Build and machine coverage

The fixes target FH6 build 440.853 and GPTK 4.0 beta 2 on the M5 Max test machine. Other game updates, GPTK builds, chips and driver classes need testing. The launch tool checks D3DMetal's hash; the timing and depth hooks check their own assumptions. Don't remove those guards to make an unsupported build run.  
The source export changes local paths and adds packaging checks. We tested those pieces separately; we haven't replayed the full game setup from an empty checkout. The original lab run remains the gameplay reference.

## Playtest reports

| Issue | Current observation |
| --- | --- |
| Audio | Engine sound is mostly missing except when looking behind; turbo and other sounds remain. |
| Rendering | Some buildings appear glossy black. Flag edges and some other elements distort with view angle. Texture/LOD transitions can be abrupt. |
| Loading | Race loading can take a long time. The bottleneck is unknown. |
| Responsiveness | Stutter and noticeable input lag. No measured latency yet. |
| Online | Online gameplay features tried so far didn't load, and the game didn't switch to Horizon Solo. Xbox sign-in itself worked. |
| Sign-in persistence | Several launches requested login again. Clean-exit persistence still needs testing; earlier runs often crashed or needed forced shutdown. |
| Memory warning | Game figures can exceed physical RAM and appear to track virtual address space. Host memory pressure was normal during the recorded comparison. |
| FPS/timing panel | Zero FPS and invalid-looking timing values despite an updating game. Use independent measurement. |

These are user observations unless the technical docs describe a specific diagnostic test. We haven't identified causes for the audio, materials, online or latency issues.

## Performance notes

The Metal 3 process sample reached roughly 41–42 GiB; a Metal 4 sample at a similar failure stage used roughly 9–10 GiB. Those processes had different lifetimes. This isn't a controlled memory benchmark.  
Later Low/1920×1200 playtests reportedly stayed at or below roughly 16 GB, with GPU usage around 55% at a 60 FPS target and 75% at a 120 FPS target. A frame-rate target isn't measured FPS, and utilization alone doesn't establish spare performance.  
One shader-optimization pass took about a minute; a later run was almost instant. The local cache persisted. Cold-cache behavior on other machines remains unknown.  
The user also suspected a change in driving speed. We haven't compared physics or elapsed time against Windows. The lasting workaround selects an existing CPU time source; it doesn't force a fixed 60 Hz game clock.
