"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read-only comparison of verified UI timing fields in the current FH6 build.

Usage: python3 sample_ui_time.py WINE_PID STATE_LABEL
No debugger, remote function calls, game-memory writes, or account data reads.
Each ReadProcessMemory block is a separate observation, not an atomic snapshot.
Heap addresses are rediscovered, and verified pointer chains are checked again
after sampling. The upstream frame-context route remains a candidate until its
runtime connection to the active main-owner update has been established.
"""
import datetime
import json
import struct
import sys
import time

from read_subscription import read, q, d

label = sys.argv[2]
assert label.replace('-', '').isalnum()

def f32(b, offset=0):
    return struct.unpack_from('<f', b, offset)[0]

def discover():
    app = q(read(0x14aa28ac0, 8), 0)
    assert q(read(app, 8), 0) == 0x146c1ffa0
    dispatcher = q(read(app + 0x28, 8), 0)
    assert q(read(dispatcher, 8), 0) == 0x146d446f0
    media = q(read(dispatcher + 0x60, 8), 0)
    assert q(read(media, 8), 0) == 0x146d8af90
    manager = q(read(media + 0x40, 8), 0)
    assert q(read(manager, 8), 0) == 0x146d595c0
    owner = q(read(0x14a8af088, 8), 0)
    assert q(read(owner, 8), 0) == 0x146559db0
    delta_owner = q(read(owner + 0x320, 8), 0)
    root = q(read(0x14a8b5438, 8), 0)
    context = q(read(root + 0x90, 8), 0)
    assert q(read(context, 8), 0) == 0x14640e230
    timer = q(read(context + 0xf0, 8), 0) + 0x78
    assert q(read(timer, 8), 0) == 0x1468a7978
    clockdata = q(read(timer + 0x10, 8), 0)
    assert q(read(clockdata, 8), 0) == 0x14689f278
    return dict(app=app, dispatcher=dispatcher, media=media,
                manager=manager, owner=owner, delta_owner=delta_owner,
                root=root, context=context, timer=timer, clockdata=clockdata)

chain = discover()
rows = []
for _ in range(4):
    b = read(chain['app'] + 0x90, 16)
    row = dict(monotonic=time.monotonic(), app_delta=f32(b, 4),
               app_updates=q(b, 8))
    b = read(chain['manager'] + 0x68, 64)
    row.update(manager_current=struct.unpack_from('<i', b, 0)[0],
               manager_previous=struct.unpack_from('<i', b, 8)[0],
               manager_accumulator=struct.unpack_from('<i', b, 0x38)[0])
    row['main_owner_delta'] = (f32(read(chain['delta_owner'] + 0xa8, 4))
                               if chain['delta_owner'] else None)
    row['candidate_frame_delta'] = f32(read(chain['clockdata'] + 0xf8, 4))
    rows.append(row)
assert chain == discover(), 'Object chain changed: discard this sample.'
out = dict(pid=int(sys.argv[1], 0), label=label,
           time=datetime.datetime.now().astimezone().isoformat(),
           chain={k: hex(v) for k, v in chain.items()}, samples=rows)
out['duration_seconds'] = rows[-1]['monotonic'] - rows[0]['monotonic']
out['updates_per_second'] = ((rows[-1]['app_updates'] - rows[0]['app_updates'])
                             / out['duration_seconds'])
path = d / f'ui-time-comparison-{label}-{out["pid"]}.json'
path.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))
