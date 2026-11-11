# Online session diagnosis, 24 and 25 October 2026

The user could sign in and download liveries/tunes, but shared free roam had no other players and The Eliminator stayed on Matchmaking. Those successful content requests did not establish a working session

## HTTP and connectivity controls

The [optional WinHTTP patch](../../patches/wine/winhttp-metadata-diagnostic.patch) logs constant operation/service labels and numeric results only, capped at 8,192 records per process. It omits headers, bodies, query values and arbitrary host/path strings and preserves LastError. HTTP 200, asynchronous 401/503 and timeout fixtures passed, six synthetic canaries stayed absent from the captured output  
The fixture's response-header timeout was distinct from the data-receive timeout. The recorded vendor result was 10060, the test stopped expecting a different WinHTTP timeout code. That fixture correction was not evidence of a timeout in FH6  
The game capture showed successful login, title data, QoS-server listing, lobby subscription and WebSocket upgrades. Some operations remained `other`, no identified Match route did not prove that no matchmaking request existed. Scratch connectivity probes reported InternetAccess and a non-null profile, the game's own trace established calls but did not print every returned value

## NETRESET was not the failure

[Patch 0024](../../patches/wine/0024-ws2_32-udp-netreset.patch) backports Wine's SIO_UDP_NETRESET support. It changed the unsupported-option results and preserved UDP/TCP behavior, but FH6 still closed the sockets and stayed on Matchmaking  
Inspection of the enclosing live initialization function showed that it ignored both reset-option results. The earlier inference from the warning's proximity to socket close was wrong. The checked failure came later, from IP_TTL=255 on a bound dual-stack IPv6 UDP socket

## TTL255 and the one-byte guard

macOS rejected that IP_TTL request with EINVAL, Wine returned WSAEINVAL 10022 and the game closed the socket. The [active TTL patch](../../patches/wine/0025-fh6-dualstack-ttl255.patch) is gated by `FH6_DUALSTACK_TTL255=1` and admits the observed failed request only after validating the DWORD value, IPv6 family, UDP type and V6ONLY=0  
Darwin's native IPV6_UNICAST_HOPS path sets both IPv6 hops and dual-stack IPv4 TTL. The 18-case matrix covers protocol family, socket type, binding and values 1/64/255. Real loopback packet metadata stayed at 37 with the gate off and changed to 255 with it on. Other UDP/TCP and asynchronous cases remained correct  
The first helper used an uninitialized four-byte `int v6only`. This Wine getter wrote one byte, leaving nonzero upper bytes in the quiet run. The numeric audit saw the returned low byte 0 but an integer value of `0x00006f00`, rejecting all three otherwise valid calls. Relay tracing happened to hide that helper bug  
The final helper uses `BYTE v6only = 1`. [test_ttl_guard_stack.py](../../tests/test_ttl_guard_stack.py) compiles the actual exported helper with nonzero automatic storage and a one-byte getter. Reintroducing the original four-byte local fails the admitted case, the fixed version passes while gate-off and IPv6-only cases still reject  
The active patch already combines the trial and correction. [The historical audit diff](history/0026-fh6-ttl-silent-observations.patch) and [byte correction](history/0027-fh6-ttl-byte-guard.patch) are investigation records for the original trial source, not extra steps for the current series

## Game result and limits

The TTL trial retained its UDP sockets and the user confirmed other players in shared free roam and entry into The Eliminator. After the byte-guard correction, the user also confirmed online connection with relay, HTTP metadata and extra connectivity/socket diagnostics disabled  
The ordinary quiet launcher keeps `FH6_HTTP_DIAG=0`. Relay registry filters were restored after diagnosis. A later transient disconnect recovered, its cause remains unknown. Frame pauses continue online and offline  
The fallback couples both protocol hop limits to 255. It does not implement independent Windows hop-limit semantics or the IPv6 IP_TTL getter. There was no credential reset, packet-content capture, firewall change or game executable patch in this fix

## Exported probes

| Source | Purpose |
| --- | --- |
| [http_probe.c](http_probe.c) | Loopback HTTP/callback canary fixture |
| [logger_fixture.c](logger_fixture.c) | Constant labels, gate, cap and LastError preservation |
| [connectivity_probe.c](connectivity_probe.c) | Explicit connectivity API results |
| [udp_ioctl_probe.c](udp_ioctl_probe.c) | Reset IOCTL, UDP/TCP and asynchronous controls |
| [udp_ttl_probe.c](udp_ttl_probe.c) | Family/type/binding/value matrix |
| [udp_ttl_wire_probe.c](udp_ttl_wire_probe.c) | Sender for native received-hop verification |
| [thread_owners.c](thread_owners.c) | Attribute numeric Wine thread IDs to the selected process |

Build Windows probes with `python3 tools/build_experiment_probes.py`. The native source checks run the logger fixture without Wine. Keep live diagnostics in a separate runtime/test bottle, the exported source does not include raw requests, game disassembly or account state
