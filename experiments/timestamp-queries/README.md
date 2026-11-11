# Timestamp queries, 24 October 2026

The original beta 2 probe resolved 120 completed queries but retained the `0xA5` prefill pattern. The shim returned advancing results with zero probe flags for both ClearRenderTargetView and CopyBufferRegion workloads. Both workloads use a DIRECT queue, the copy test does not cover a COPY queue  
The original queue reported 60 Hz, the shim reported the QPC frequency of 10,000,000 Hz. These are API observations, not measured rendering rates. The probe [ts_repro.c](ts_repro.c) checks reset/close, signal, event creation and fence completion before reading results. An earlier never-written-query test was removed because D3D12 leaves that resolve undefined  
On build 440.853 the game retained timing selector 1 while frame and UI counters advanced, without a live selector write. The subsequent title update to 461.691 reached the garage and driving with the same shim approach. The old address-based timing helper now rejects that updated executable

```zsh
python3 tools/build_timestamp_shim.py
python3 tools/build_experiment_probes.py
```

The timestamp probe is `.local/build/probes/timestamp.exe`, with `--frames N`, `--sleep MS` and `--copy` options. Run it in an account-free test bottle with the intended timestamp shim/backing DLL environment and verify the mapped files. A Wine-reported module path alone can be misleading  
The shim records CPU time while command lists are built. It does not implement GPU execution timing. Replay and upload-ring reuse remain limitations. Shader-model fallback was exercised by the newer game's 0x6a request, the unrelated prefill/sampler/RT/VRS/clamp trials remain off in the tested launch  
See the [shim source and notices](../../third_party/highball-tsshim/README.md) and [component guide](../../docs/wine-components.md)
