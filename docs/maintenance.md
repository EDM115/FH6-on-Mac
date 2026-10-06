# Updating this repository

Development continues in the private investigation workspace. This repository receives curated updates when the maintainer asks for them. It isn't a mirror, and nothing here runs an automatic sync.  
For an update:

1. Choose authored source files and the conclusions worth keeping. Preserve the working lab setup.
2. Port reusable tools to the local configuration and `.local/` output paths. Put obsolete probes under `experiments/` with their result and remaining assumptions.
3. Review dependencies and provenance. Don't copy third-party source under this repo's MIT license without its own license and attribution.
4. Update setup, launch and known-issue docs only for behavior the tests support. Mark hypotheses and user reports as such.
5. Run the [checks](../tests/README.md), inspect the diff, then let the maintainer handle Git operations.

Keep runtime trees, bottles, game assets, shaders extracted from the game, cache files, binaries, logs, dumps and screenshots out of the export. Those can contain proprietary content or account details. Filtered logs can still reveal usernames, paths or tokens.  
[The source map](../provenance/source-map.json) lists selected lab source identifiers and hashes. It contains no local absolute paths or diagnostic results. Export adapters, documentation and tests live alongside the selected sources. A copied source hash describes the lab input, not the adapted output.  
`.gitignore` is a backstop. Review the actual files before publishing, including files that Git hasn't tracked yet.
