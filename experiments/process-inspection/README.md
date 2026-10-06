# Read-only process inspection

Bounded ReadProcessMemory probes replaced a disruptive debugger attach. Saved syscall contexts helped locate waits and the native assertion. An already-completed fence and stale frames ruled out premature deadlock claims.  
These are selected lab snapshots, not a standalone debugger. Some imports refer to other lab probes. Reconstruct their dependencies and rediscover process/object identities before porting them. Stack words are not an unwind; the small PE unwinder also lacks epilog decoding. Do not signal game waits from these observations.
