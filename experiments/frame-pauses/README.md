# Recurring frame pauses, 25 October 2026

The quiet online fix left frequent brief pauses in free roam and events. These tests kept the working runtime and changed one relevant setting at a time

| Control | Observation |
| --- | --- |
| V-sync off, still 60 FPS | Drawable waiting fell close to zero, pauses remained |
| Parked, camera still | Pauses remained, driving into new scenery was not necessary |
| Horizon Solo at the same location | Pauses remained, online play was not necessary |
| 1280 × 800, still 60 FPS | Roughly the same recurring pattern despite fewer pixels |
| 120 FPS limit | About 101 FPS average, typical gap spacing changed from about 64 to about 120 frames, still roughly 1.2 seconds |
| 30 FPS limit | Fewer large visible gaps, the periodic game-thread activity remained |
| msync disabled | Pauses remained. After reconnection, one repeat recorded 18 gaps over 100 ms in 19 seconds |

These are short controls, not a benchmark suite. Variable scenes/input and a transient network event limit comparisons of average FPS or individual maxima. The original 1920 × 1200, 60 FPS, V-sync On, msync On baseline was restored

## CPU and frame observations

A periodic game thread becomes busy in roughly 140 to 200 ms bursts around submission gaps, while many rendering/game workers become mostly idle. Rosetta's exception service also becomes busier. The thread retained similar CPU work at 30 FPS and without msync. This is correlation, its operation and causal role remain unidentified  
The saved syscall series included NtWaitForSingleObject and NtContinue states. Those are last-saved records, not current execution contexts or call counts. Normal Wine syscall entry does not save volatile R8 into that frame, so it cannot be treated as the current third argument. The entry bytes did not decode as a conventional linear function, no protection-system attribution or bypass follows from this evidence  
A native `sample` run temporarily caused much worse audio/frame disruption and poor Wine/Rosetta unwinds. The later counter reader did not suspend threads and measured its own overhead, about 0.37 to 0.45 CPU seconds over each roughly 20-second capture. That does not mean zero measurement cost

## Reuse the capture carefully

[capture_cpu_and_frames.py](capture_cpu_and_frames.py) uses the configured bottle for video settings and verifies that the selected native PID maps its FH6 executable. It temporarily enables process-scoped per-frame metrics, collects four known graphics event types and reads thread CPU counters every 20 ms. It attempts to disable metrics even if enabling or log-reader cleanup fails, and a normal capture requires a successful disable result. Output must stay under `.local`

```zsh
python3 experiments/frame-pauses/capture_cpu_and_frames.py --pid HOST_PID --expected-frame-rate 60 --output .local/performance/capture.json
```

Use the freshly verified macOS host PID, not a Wine PID. Keep the scene and camera fixed. Use a distinct output filename, the tool refuses overwrite. Build the optional [thread metadata helper](map_threads.c) through `tools/build_experiment_probes.py`. Its TEB offsets are Wine 11-specific and its saved-state output needs separate interpretation

## Timing traps

- OnGlassDrawableInterval begin/end values are raw mach ticks. DrawableLifetime values are already nanoseconds
- The first CPU sampler used mach_absolute_time, while the graphics records used mach_continuous_time. The sleep offset had to be measured and applied before comparison. The exported sampler now records continuous time directly
- A lifetime-end/on-glass-end join produced partial matches and negative latencies. It was rejected. Compare independent sequences unless frame identity is established
- Aggregate interval maxima are not counts of individual pauses. Sparse or implausible aggregate GPU fields cannot establish a GPU bottleneck
- Apple telemetry can label the verified game PID as steamwebhelper. Check the mapped executable rather than trusting that label
- Native cthread addresses and Wine pthread IDs differed by 0xe0 in the recorded run, verified against 36 named threads. Rediscover and validate that relationship instead of hardcoding old pointers

No performance runtime patch resulted from this investigation. The next useful work is at the periodic thread's exception/wait boundary. The repository does not distribute the captured game instructions, raw stacks or session-specific addresses
