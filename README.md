# FH6 on Mac

Forza Horizon 6 running through CrossOver and Apple's Game Porting Toolkit, with a few fixes for startup, UI timing and graphics. We reached Xbox sign-in, the garage and controllable driving on October 4th 2026. There's still plenty to fix.  
This repo contains the source, setup notes and experiments from that work. You'll need your own copies of FH6, CrossOver and GPTK. **Setup is manual, and the fixes target specific builds.** The packaged tools haven't had a fresh end-to-end game run yet; the working result came from the original lab scripts.

## Demo

You can check a full 16min+ video showcasing how the game runs so far (October 4th) and what the issues are : https://www.youtube.com/watch?v=akHFtDcVHV4

## Tested setup

| Component | Version |
| --- | --- |
| Mac | MacBook Pro M5 Max, 64 GB unified memory |
| macOS | 27.0.1 |
| CrossOver | 26.3, Wine 11-derived engine |
| Apple runtime | Game Porting Toolkit 4.0 beta 2, Metal 4 backend |
| Game | Windows Steam release, build 440.853 |

Sign-in, Settings, loading and driving worked in the tested session. Engine audio, some materials and texture transitions still have problems. Race loading can take a long time, and we've seen stutter and input lag. Online gameplay features tried so far haven't loaded. The game also reports nonsense memory and FPS values. See [known issues](docs/limitations.md).

## Get started

1. [Prepare a private runtime and bottle](docs/setup.md).
2. [Build the helpers and generate shaders locally](docs/setup.md#build-the-helpers).
3. [Launch the game and apply the timing fix](docs/launch.md).

The timing helper selects the game's CPU timing source once per launch. The earlier experiment that advanced UI time at a steady cadence is in the archive; you don't need that loop to play.

## What's here

| Path | Contents |
| --- | --- |
| [tools/](tools/) | Build, shader preparation, launch and timing helpers |
| [src/](src/) | Depth-view repair, shader overlay, display API stub and graphics logger |
| [tests/](tests/) | Synthetic graphics probes and packaging checks |
| [docs/how-it-works.md](docs/how-it-works.md) | The fixes and the evidence behind them |
| [docs/investigation.md](docs/investigation.md) | What we tried, what failed and what changed our diagnosis |
| [experiments/](experiments/) | Selected old scripts, grouped by investigation |
| [docs/next-steps.md](docs/next-steps.md) | Work still to do |

## Under the hood

CrossOver/Wine handles Windows APIs, Rosetta 2 translates x86-64 code, and Apple's D3DMetal translates Direct3D graphics to Metal. We added small compatibility hooks and diagnostic tools. Proton was a research reference; this setup doesn't install Proton or a custom renderer. [Dependencies and credits](docs/dependencies.md) list the projects involved.  
We used AI assistance during the investigation, with repeated local tests and manual checks in the game. The docs distinguish those results from theories and later playtest observations.

## Contributing

Include your game build, macOS version, chip, CrossOver/GPTK versions and the step that fails. A short description of the failure is more useful than an entire bottle or crash dump. Review anything you attach for account details, sign-in codes and local paths.  
[LICENSE](LICENSE) covers the authored code in this repo. It doesn't cover the game, vendor runtimes or shaders extracted from your copy. We keep those files out of the repository.
