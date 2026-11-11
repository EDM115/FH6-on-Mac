# Launch order

This guide describes the quiet online stack tested on October 25 with FH6 6.461.691.0 / Steam build 25588841 and GPTK 4 beta 2. Use the [legacy guide](legacy-launch.md) only for the older 440.853 experiment  
Finish the game and Steam session in the selected bottle before changing runtimes or synchronization settings. The Wine client and server must agree on msync. A running Steam process can keep an earlier environment

## Start the checked stack

From the repository root, after [setup](setup.md) and [component preparation](wine-components.md) :

```zsh
python3 tools/launch_current.py --check
python3 tools/launch_current.py
```

The current wrapper checks the recorded game and component hashes, then calls the shared launcher through the native `--ux-app` loader route. It selects Metal 4, Intel vendor ID, Deck sign-in, native C++ libraries, timestamp/layout support, shader fallbacks, the depth repair, `WINEDLLPATH_PREPEND`, `WINEMSYNC=1` and `FH6_DUALSTACK_TTL255=1`  
`FH6_HTTP_DIAG=0` disables the optional HTTP metadata records. Relay tracing and raw log capture are off. The original bounded graphics/launch filter remains active. `FH6SHIM_DIAG=1` retains the existing limited shim diagnostics, it does not enable per-resource tracing  
The default hash profile identifies the tested lab binaries. A local rebuild can have different hashes. Use the explicit `--rebuilt-components` mode described in the [component guide](wine-components.md) to check its source/build receipts and installed artifacts. Keep the public recorded profile intact. A successful `--check` is a file/configuration check, not a gameplay result

## Let the game load

Leave the launcher terminal open. Loading can take several minutes. Complete the Microsoft sign-in flow yourself if requested. The October tests remembered sign-in across three clean relaunches, which does not guarantee every later token expiry or system restart  
No CPU timing-selector write is part of this launch. The shim supplies advancing compatibility timestamps. The old `frame_timing_session.py` is guarded to build 440.853 and rejects the updated executable before process inspection

## Verify process identity when diagnosing a run

```zsh
python3 tools/window_snapshot.py title --msync 1
```

The visible top-level `App` row contains the decimal Wine PID. macOS tools use a different host PID, identify it from its command and mapped FH6 executable. Rediscover both after every restart  
The window probe uses a Windows `Z:` path and passes the synchronization flag through the wrapper. If testing msync off, pass `--msync 0` to this probe as well. Never launch the other mode while the bottle's original server is alive  
Look for `FH6_TEXTURE_SOURCE_REPAIRED` in the private graphics log after loading. Check that FH6 itself maps the intended DLLs and D3DMetal, parent-only hook markers do not prove the game loaded them. A warm shader cache can produce no new replacement hits

## Online check

Successful Xbox login or livery/tune downloads do not establish a multiplayer session. The local online comparison checked other players in shared free roam and entry into The Eliminator. The final quiet runtime connected without relay or HTTP diagnostics  
The TTL workaround remains opt-in and narrow. It handles the observed failed IP_TTL=255 request on a bound dual-stack IPv6 UDP socket by setting the native IPv6 hop option. Darwin couples that setting to both protocol hop limits. See the [online experiment](../experiments/online-session/README.md) for the guard fix, negative comparisons and tests

## Exit and rollback

Exit FH6 through its menu, then exit Steam in that bottle and let its Wine server stop. If Steam stays in the background, use its normal Exit/Shutdown action. Identify the bottle and remaining processes before stopping anything after a crash  
The failed performance comparison used `--env WINEMSYNC=0`, the recorded baseline was restored to `1`. Keep the matched patched ntdll/server pair in both arms. A synchronization-mode comparison requires a clean client/server restart  
Keep a separate stopped runtime copy before installing rebuilt components. Restore that copy to undo a component change. Do not downgrade a newer save to the old executable, the updated profile rejected build 440.853 in the October test. Shader/depth hooks apply to the process launched with them, the legacy CPU-selector write resets at process exit
