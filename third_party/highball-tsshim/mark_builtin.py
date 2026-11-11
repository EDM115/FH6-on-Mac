# SPDX-License-Identifier: GPL-3.0-only
#!/usr/bin/env python3
"""Stamp Wine's builtin marker into a PE DLL (copy of Highball Scripts/mark-builtin.py logic)."""
import struct, sys
for path in sys.argv[1:]:
    b = bytearray(open(path, 'rb').read())
    assert b[:2] == b'MZ', path
    e_lfanew = struct.unpack_from('<I', b, 0x3C)[0]
    assert e_lfanew >= 0x40 + 17, (path, e_lfanew)
    b[0x40:0x40 + 17] = b'Wine builtin DLL\0'
    open(path, 'wb').write(b); print('marked', path, 'e_lfanew=0x%x' % e_lfanew)
