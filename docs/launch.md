# Launch order

Run commands from the repository root after [setup](setup.md). Finish any existing session in the cloned bottle first, including its Steam client. A running Steam process can keep the old environment.

## 1. Start FH6

```sh
python3 tools/launch.py --check
python3 tools/launch.py
```

Leave this terminal open. The helper selects Metal 4, Intel vendor ID, Deck sign-in, native C++ overrides and the shader/depth hooks. It writes a new private log directory for each launch. `--check` validates paths, D3DMetal and prepared shader hashes; it doesn't establish that the game will launch.

## 2. Find the current Wine PID

Wait until FH6 shows its title screen. In a second terminal, from the same repository:

```sh
python3 tools/window_snapshot.py title
```

Find the visible top-level window with class `App`. Its `pid` is a decimal **Wine PID**, which differs from macOS's PID. Confirm it's the FH6 window in this bottle. Don't reuse a PID from an earlier launch or pick Steam's window.

## 3. Select CPU timing

Replace `WINE_PID` below with that decimal number:

```sh
python3 tools/frame_timing_session.py WINE_PID
python3 tools/frame_timing_session.py WINE_PID --cpu
python3 tools/frame_timing_session.py WINE_PID
```

The first and last commands only read. The middle command changes one checked data byte and keeps it set for this game process. A successful result has `exit_code: 0` and `final_selector: 0`. In the last read-only result, check that `frame_tick` and `ui_updates` advance and `frame_delta`/`ui_delta` become positive while the window is active.  
If the first read already shows selector `0` with advancing time, skip `--cpu`. Reapplying it isn't necessary and its starting-value guard will reject that state.  
A null pointer during startup means the expected objects aren't ready. Wait for title and retry the read-only command. A build or object mismatch needs investigation. Don't remove an assertion or run Python with `-O`.

## 4. Start the game

Choose Start Game. Complete the Microsoft QR/code flow yourself if prompted. The tested run then passed optimization, the loading screens, the garage and Drive.  
The graphics log should show hooks loaded in the **game's host process**, shader replacements, and later `source_match=1` / `FH6_TEXTURE_SOURCE_REPAIRED`. Parent-only markers don't prove the game loaded the hooks. You can inspect a known host PID with `lsof -p HOST_PID`; don't confuse that PID with the Wine PID used above.  
The launch helper doesn't apply timing for you. It also doesn't manage save data, login persistence, Steam shutdown or game updates.

## Exit and rollback

Exit through the game, then exit Steam in that bottle. Keep the terminal open until the launcher finishes. If a crash leaves Wine windows behind, identify the affected bottle and process before stopping anything; don't kill all Wine processes on the Mac.  
The CPU selector resets when FH6 exits. To restore it in a verified live process:

```sh
python3 tools/frame_timing_session.py WINE_PID --original
```

That brings back the original timing behavior, including the known stall. The shader and depth hooks apply only to a process launched with them. `--mode shader-control` omits the depth repair and uses original shaders; use it only for comparison or cache collection. Restore the private API schema with the command in [setup](setup.md).  
Useful keys from the test sessions: Command+Enter toggled fullscreen; Esc then Enter cancelled the hidden sign-in flow. These aren't fixes for a stopped loading screen.
