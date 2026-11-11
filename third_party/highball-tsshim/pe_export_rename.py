# SPDX-License-Identifier: GPL-3.0-only
#!/usr/bin/env python3
"""Copy a PE DLL and rename its export-directory name in place (same length or shorter).

Port of Highball's PEExportName.patched (Sources/HighballKit/PEExportName.swift in the Highball repo,
read 2026-10-08). Used to lay D3DMetal's d3d12.dll out as apd12.dll so CrossOver's Wine 11
does not take it for the shim (Wine 11 identifies builtins by export name).
Usage: pe_export_rename.py SRC.dll DST.dll NEWNAME.dll [--mark-builtin]
Prints old/new names and SHA-256 of both files. Never modifies SRC.
"""
import hashlib, struct, sys
from pathlib import Path

def u16(b, o): return struct.unpack_from('<H', b, o)[0]
def u32(b, o): return struct.unpack_from('<I', b, o)[0]

def name_offset(b):
    assert b[:2] == b'MZ', 'not MZ'
    pe = u32(b, 0x3c)
    assert b[pe:pe+4] == b'PE\0\0', 'not PE'
    nsec, optsz = u16(b, pe+6), u16(b, pe+20)
    opt = pe + 24
    magic = u16(b, opt)
    dd = opt + (112 if magic == 0x20b else 96)
    exp_rva, exp_size = u32(b, dd), u32(b, dd+4)
    assert exp_rva and exp_size, 'no export directory'
    secs = []
    for i in range(nsec):
        s = opt + optsz + 40*i
        secs.append((u32(b, s+12), max(u32(b, s+8), u32(b, s+16)), u32(b, s+20)))
    def off(rva):
        for va, size, raw in secs:
            if va <= rva < va + max(size, 1): return rva - va + raw
        raise ValueError('rva not in a section')
    return off(u32(b, off(exp_rva) + 12))

def main():
    src, dst, new = sys.argv[1], sys.argv[2], sys.argv[3].encode()
    if Path(src).resolve() == Path(dst).resolve():
        raise SystemExit('Source and destination must be different files')
    b = bytearray(open(src, 'rb').read())
    o = name_offset(b); end = b.index(b'\0', o)
    old = bytes(b[o:end])
    assert len(new) <= len(old), f'new name longer than {old!r}'
    b[o:end] = new + b'\0' * (len(old) - len(new))
    if '--mark-builtin' in sys.argv:
        assert u32(b, 0x3c) >= 0x40 + 17
        b[0x40:0x40+17] = b'Wine builtin DLL\0'
    open(dst, 'wb').write(b)
    print(f'export name {old.decode()} -> {new.decode()}; builtin marker at 0x40: {bytes(b[0x40:0x50])!r}')
    for p in (src, dst): print(hashlib.sha256(open(p, 'rb').read()).hexdigest(), p)

if __name__ == '__main__': main()
