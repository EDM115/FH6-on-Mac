# Next steps

1. Reproduce the full setup from this checkout, then test a clean exit and relaunch. Check account persistence and shader-cache reuse.
2. Automate timing selection after game initialization, keeping the existing build and object checks. Trace why the original timestamp history doesn't advance.
3. Reduce the required startup changes one at a time, starting with the display API proxy under `SteamDeck=1`.
4. Compare game time, car speed and loading duration against Windows before tuning performance.
5. Diagnose engine audio and the black materials with small reproductions where possible.
6. Trace the game's memory-counter reads. Correct the affected accounting path rather than hiding the warning or inventing a RAM total.
7. Investigate online loading separately from account authentication.
8. Test another machine/build only after recording its differences. Keep unsupported versions out of the current offset-based hooks.

Keep a working runtime and bottle while testing. Change one relevant variable, record the result, and restore failed trials before the next comparison.
