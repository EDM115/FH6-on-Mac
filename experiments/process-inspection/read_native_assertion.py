"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read only libsystem_c's bounded crash annotation in a verified FH6 process.

The module base is supplied by a previously saved host vmmap. No debugger,
thread suspension, arbitrary memory dump, or account/HTTP logging is used.
"""
import argparse
import datetime
import json
import struct
from pathlib import Path
from read_process_session import ReadSession

parser = argparse.ArgumentParser()
parser.add_argument('pid', type=int)
parser.add_argument('native_map', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
lines = [line for line in args.native_map.read_text().splitlines()
         if line.startswith('__TEXT ') and line.endswith('/libsystem_c.dylib')]
assert lines, 'Need libsystem_c text mappings'
# macOS can split __TEXT at pages with different protections. Its lowest
# mapping contains the Mach-O header, which is independently checked below.
base = min(int(line.split()[1].split('-')[0], 16) for line in lines)
result = {'time': datetime.datetime.now().astimezone().isoformat(),
          'wine_pid': args.pid, 'module': 'libsystem_c', 'base': hex(base),
          'annotations': []}
with ReadSession(args.pid) as reader:
    assert reader.read(0x142a57f41, 8) == bytes.fromhex('0fb6412088442428'), 'FH6 build mismatch'
    header = reader.read(base, 32)
    assert struct.unpack_from('<I', header)[0] == 0xfeedfacf
    count, size = struct.unpack_from('<II', header, 16)
    assert 0 < count < 512 and 0 < size < 65536
    commands = b''.join(reader.read(base+32+off, min(4096, size-off))
                        for off in range(0, size, 4096))
    slide, sections, offset = None, [], 0
    for _ in range(count):
        command, length = struct.unpack_from('<II', commands, offset)
        assert length >= 8 and offset+length <= size
        if command == 0x19:
            segment = commands[offset+8:offset+24].split(b'\0')[0]
            vmaddr = struct.unpack_from('<Q', commands, offset+24)[0]
            nsects = struct.unpack_from('<I', commands, offset+64)[0]
            assert 72+80*nsects <= length
            if segment == b'__TEXT':
                slide = base-vmaddr
            for index in range(nsects):
                section = offset+72+80*index
                name = commands[section:section+16].split(b'\0')[0]
                if name == b'__crash_info':
                    address, section_size = struct.unpack_from('<QQ', commands, section+32)
                    assert 16 <= section_size <= 4096
                    sections.append(address)
        offset += length
    assert slide is not None and offset == size
    for address in sections:
        version, message = struct.unpack('<QQ', reader.read(address+slide, 16))
        assert 4 <= version <= 9, 'Unexpected crash annotation schema'
        item = {'section': hex(address+slide), 'version': version, 'message': None}
        if message:
            assert 0x10000 <= message < 0x800000000000
            text = bytearray()
            for off in range(0, 2048, 64):
                chunk = reader.read(message+off, 64)
                text.extend(chunk.split(b'\0', 1)[0])
                if b'\0' in chunk:
                    break
            item['message'] = text.decode('utf-8', 'replace')
        result['annotations'].append(item)
args.output.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
