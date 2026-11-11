# Next steps

1. Identify the periodic game thread's exception/wait activity and its relation to stalled rendering workers. The existing controls rule out an exact 64-frame trigger and did not support msync, resolution or V-sync as a fix
2. Trace the residual engine/car-impact mute across car changes, camera recovery and accelerator release. Keep the confirmed initial spatial-audio improvement separate
3. Reproduce the black Tokyo buildings, vegetation and props at known locations and seasons. The input-layout fix repaired the tested livery, it did not settle this issue
4. Investigate Steam's black window/context menu when that deferred work resumes
5. Reconstruct the current stack from this checkout in a fresh private bottle. Recheck saved sign-in, online free roam, an event and clean shutdown/relaunch. Export checks do not replace this integration run
6. Compare game speed, input latency and loading duration against Windows before tuning performance. Use native measurements instead of the shim-backed GPU HUD
7. Test long sessions and token expiry. The existing clean sign-in and connection checks cover a limited window
8. Reduce retained startup workarounds one at a time. Test the display proxy under Deck mode separately from disabling Deck mode
9. Review newer game/GPTK builds in separate copies. Recheck export tables, shader inputs and depth-hook guards, a new hash alone is not a compatibility port

Keep a working runtime and bottle while testing. The 30 FPS cap reduces visible pauses but leaves the periodic work in place. The tested msync default remains on, its leak fix and the unresolved frame pause are separate issues
