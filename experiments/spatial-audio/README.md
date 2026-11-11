# Spatial audio and residual muting

The October 24 change pairs [0022 channel ordering](../../patches/wine/0022-mmdevapi-spatial-static-object-order.patch) with [0023 native spatial mixing](../../patches/wine/0023-winecoreaudio-spatial-mixer-for-surround-beds.patch). Rebuild mmdevapi.dll, winecoreaudio.drv and winecoreaudio.so together, preserving the existing ntdll until the separate client/server rebuild

## What passed

The original channel mapper failed 14 assertions across 7.1, 7.1.4, back-centre and asymmetric subsets. The patched mapper passed. [channel_order.c](channel_order.c) uses the extracted original/patched functions and independent expected channel positions  
The silent [spatial_mixer_probe.c](spatial_mixer_probe.c) produced finite nonzero stereo output for each input channel in 8- and 12-channel beds at 44.1 and 48 kHz, 40 checks passed. It renders to memory, not the speakers. LFE uses the patch's -6 dB front-pair fold for built-in speakers  
[audio_client_probe.c](audio_client_probe.c) initialized stereo, 8-channel and 12-channel clients in both controls. The patched path activated the mixer for the expected surround streams. Initialization alone did not establish audible output, the user then confirmed initial engine sound and voices in the game

## What remains

After a car change, engine and car-impact audio could disappear while tire squeal and some world-impact sounds remained. The user observed recovery by cycling cameras and returning to chase view, or by releasing the accelerator while stationary. Repeating the rev in the same car/camera then stayed audible  
[audio-statistics.patch](audio-statistics.patch) is a separate diagnostic against the already spatial-patched CoreAudio source. `HB_AUDIO_STATS=1` records bounded mean-square levels, shortages and error counts at two-second intervals, capped at 600 reports per stream. It captures no audio samples or microphone input  
The first diagnostic used Wine's MESSAGE logger from the native HAL callback and hung. That callback has no Wine TEB. The corrected patch uses native bounded snprintf/write, the silent callback test passed. Game summaries showed continuing mixer output without render errors, but aggregate channel energy cannot identify a missing individual voice. This did not establish a complete audio fix

## Build

```zsh
python3 tools/build_experiment_probes.py
mkdir -p .local/build/probes
clang -arch x86_64 -O2 experiments/spatial-audio/spatial_mixer_probe.c -framework AudioUnit -framework CoreAudio -o .local/build/probes/spatial-mixer
.local/build/probes/spatial-mixer
```

Windows outputs include `audio-client.exe` and `channel-order-control.exe` / `channel-order-patched.exe`. The unpatched mapper is expected to fail. The diagnostic patch is not in the normal runtime series. Its Wine-derived portions retain LGPL terms
