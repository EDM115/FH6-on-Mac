"""Offline, fail-closed rewrite of the observed last SV_ShadingRate PS input.

Does not edit the game, its cache, or any loaded runtime. The exact original
SHA-256 must match at deployment. Only validated results enter replacements/.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import argparse
from local_config import load_config

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('source',type=Path,help='Private extract_cache.py output directory')
args=parser.parse_args()
cfg=load_config()
base=cfg['state']/'build'
source=args.source.resolve()
dest=cfg['state']/'shaders'/'prepared-fallbacks'
if dest.exists(): raise SystemExit('Existing prepared-fallbacks directory: preserve or move it before regenerating')
dest.mkdir(parents=True,mode=0o700)
replacements=dest/'replacements'
replacements.mkdir(mode=0o700)
lib=cfg['resources']/'libdxcompiler.dylib'


def chunks(data):
    u = lambda off: struct.unpack_from('<I', data, off)[0]
    assert data[:4] == b'DXBC' and u(24) == len(data)
    parts = {}
    for i in range(u(28)):
        off = u(32 + i * 4)
        parts[data[off:off+4]] = data[off+8:off+8+u(off+4)]
    return parts


def rewrite(text):
    text = text.rstrip('\0')
    nodes = dict(re.findall(r'^(!\d+) = (.*)$', text, re.M))
    assert re.search(r'!\{!"ps", i32 6, i32 \d+\}', text)
    entry_ref = re.search(r'^!dx.entryPoints = !\{(!\d+)\}$', text, re.M).group(1)
    entry = re.fullmatch(r'!\{void \(\)\* @main, !"main", (!\d+), (?:!\d+|null), (!\d+)\}', nodes[entry_ref])
    assert entry
    sig_ref, flags_ref = entry.groups()
    input_list = re.fullmatch(r'!\{(!\d+), (?:!\d+|null), null\}', nodes[sig_ref]).group(1)
    inputs = re.findall(r'!\d+', nodes[input_list])
    matches = [n for n in inputs if '!"SV_ShadingRate"' in nodes[n]]
    assert len(matches) == 1 and matches[0] == inputs[-1], 'Only the observed final scalar input is supported'
    target = matches[0]
    m = re.fullmatch(r'!\{i32 (\d+), !"SV_ShadingRate", i8 5, i8 29, !\d+, i8 1, i32 1, i8 1, i32 \d+, i8 0, (?:!\d+|null)\}', nodes[target])
    assert m
    input_id = int(m.group(1))
    assert input_id == len(inputs)-1
    pattern = rf'^(\s*%[\w.]+ = )call i32 @dx\.op\.loadInput\.i32\(i32 4, i32 {input_id}, i32 0, i8 0, i32 undef\).*$'
    transformed, count = re.subn(pattern, r'\1add i32 0, 0 ; 1x1 shading fallback', text, flags=re.M)
    assert count >= 1
    assert not re.search(rf'call .*@dx\.op\.loadInput\.\w+\(i32 4, i32 {input_id},', transformed)
    if not re.search(r'call .*@dx\.op\.loadInput\.i32\(', transformed):
        transformed, n = re.subn(r'^declare i32 @dx\.op\.loadInput\.i32\([^\n]+\n', '', transformed, flags=re.M)
        assert n == 1
    transformed = transformed.replace(f'{input_list} = {nodes[input_list]}', f'{input_list} = !{{'+', '.join(inputs[:-1])+'}')
    flags = re.fullmatch(r'!\{i32 0, i64 (\d+)\}', nodes[flags_ref])
    assert flags
    old_flags = int(flags.group(1)); assert old_flags & (1 << 24)
    transformed = transformed.replace(f'{flags_ref} = {nodes[flags_ref]}', f'{flags_ref} = !{{i32 0, i64 {old_flags & ~(1 << 24)}}}')
    return transformed, dict(input_id=input_id, replaced_loads=count, old_flags=old_flags, new_flags=old_flags & ~(1 << 24))


def tool(mode, inp, out, report, original=None):
    args = [str(base/'dxil_tool'), str(lib), mode, str(inp), str(out), str(report)]
    if original: args.append(str(original))
    p = subprocess.run(args, capture_output=True)
    if p.returncode:
        raise RuntimeError(f'{mode} exit {p.returncode}: '+(report.read_text() if report.exists() else p.stderr.decode()))


records = []
for original in sorted(source.glob('*.dxil')):
    sha = hashlib.sha256(original.read_bytes()).hexdigest()
    assert original.stem == sha
    record = dict(original_sha256=sha)
    try:
        original_ll=dest/(sha+'.original.ll')
        tool('disassemble',original,original_ll,dest/(sha+'.original-disassembly.txt'))
        ll, edits = rewrite(original_ll.read_text())
        record.update(edits)
        output_ll = dest / (sha+'.ll'); output_ll.write_text(ll)
        assembled = dest / (sha+'.assembled.dxil')
        tool('validate', original, dest/(sha+'.original-validated.dxil'), dest/(sha+'.original-validation.txt'))
        tool('assemble', output_ll, assembled, dest/(sha+'.assembly.txt'), original)
        validated = dest/(sha+'.validated.dxil')
        tool('validate', assembled, validated, dest/(sha+'.validation.txt'))
        old, new = chunks(original.read_bytes()), chunks(validated.read_bytes())
        for tag in (b'RTS0', b'OSG1'):
            assert old.get(tag) == new.get(tag), f'{tag!r} unexpectedly changed'
        assert b'SV_ShadingRate' not in new[b'ISG1']
        tool('disassemble', validated, dest/(sha+'.validated.ll'), dest/(sha+'.disassembly.txt'))
        data = validated.read_bytes()
        (replacements/(sha+'.dxil')).write_bytes(data)
        record.update(success=True, replacement_sha256=hashlib.sha256(data).hexdigest(), root_and_output_signature_identical=True, bytes=len(data))
    except Exception as exc:
        record.update(success=False, error=str(exc))
    records.append(record)
(dest/'manifest.json').write_text(json.dumps(records, indent=2))
print(json.dumps(dict(total=len(records),passed=sum(r['success'] for r in records),failures=[r for r in records if not r['success']]),indent=2))

if not records or any(not r["success"] for r in records):
    raise SystemExit("Not all shaders passed; do not deploy this replacement directory")
