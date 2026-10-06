# Child-loader test

An inert Windows parent/child probe established whether the injected library reached both processes. The game needed the entitlement on the private runtime child loader, not just the hosted launcher.  
The builder embeds a local path in the generated parent program. The snapshot stops before running; adapt it to tools/build_pe.py and an output directory under .local/. It uses no game assets or accounts.
