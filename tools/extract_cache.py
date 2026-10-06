"""Read a private cache snapshot; extract validated-container candidates only.

Usage: extract_cache.py bytecode_cache.bin output-directory
No cache mutations. Extracted shaders are private diagnostic material.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys

source, output = Path(sys.argv[1]), Path(sys.argv[2])
output.mkdir(mode=0o700, parents=True, exist_ok=True)
limit = 32*1024*1024
assert source.stat().st_size <= limit, 'Cache exceeds the reviewed 32 MiB limit'
with source.open('rb') as stream:
    data = stream.read(limit + 1)
assert len(data) <= limit, 'Cache grew during the read; use a stopped-process snapshot'
u = lambda b, off: struct.unpack_from('<I', b, off)[0]
records, invalid, pos = [], 0, 0
while True:
    start = data.find(b'DXBC', pos)
    if start < 0: break
    pos = start + 4
    try:
        size, count = u(data, start+24), u(data, start+28)
        assert 32 <= size <= 8*1024*1024 and start+size <= len(data)
        assert count <= 64 and 32+count*4 <= size
        blob = data[start:start+size]
        tags, relevant, stage = [], False, None
        for i in range(count):
            off = u(blob, 32+i*4); assert off+8 <= size
            tag, length = blob[off:off+4].decode('ascii'), u(blob, off+4)
            assert off+8+length <= size
            payload = blob[off+8:off+8+length]; tags.append(tag)
            if tag == 'DXIL': stage = u(payload, 0) >> 16
            if tag in ('ISG1', 'ISGN') and b'sv_shadingrate' in payload.lower(): relevant = True
        sha = hashlib.sha256(blob).hexdigest()
        records.append(dict(offset=start, bytes=size, sha256=sha, chunks=tags, stage=stage, shading_rate_input=relevant))
        if relevant and stage == 0: (output/(sha+'.dxil')).write_bytes(blob)
        pos = start+size
    except (AssertionError, struct.error, UnicodeDecodeError): invalid += 1
manifest = dict(source=str(source), bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), containers=len(records), invalid_candidates=invalid, relevant=sum(r['shading_rate_input'] for r in records), records=records)
(output/'index.json').write_text(json.dumps(manifest, indent=2))
print(json.dumps({k:v for k,v in manifest.items() if k != 'records'}, indent=2))
