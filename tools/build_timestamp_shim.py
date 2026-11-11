"""Build the beta 2 timestamp/layout shim under .local without installing it"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='x86_64-w64-mingw32-gcc')
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        raise SystemExit('Install mingw-w64 or pass --cc with its x86-64 Windows compiler')
    root = Path(__file__).resolve().parents[1]
    source = root / 'third_party/highball-tsshim'
    output = root / '.local/build/tsshim'
    output.mkdir(parents=True, exist_ok=True)
    dll = output / 'd3d12.dll'
    command = [compiler, '-shared', '-O2', '-Wall', '-static-libgcc', '-o', str(dll), str(source / 'd3d12.c'), str(source / 'd3d12.def')]
    subprocess.run(command, check=True)
    subprocess.run([sys.executable, str(source / 'mark_builtin.py'), str(dll)], check=True)
    manifest = {'compiler': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0], 'command': command, 'source_sha256': {name: hashlib.sha256((source / name).read_bytes()).hexdigest() for name in ('d3d12.c', 'd3d12.def')}, 'dll_sha256': hashlib.sha256(dll.read_bytes()).hexdigest(), 'installed': False}
    (output / 'build.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Built ' + str(dll) + ', no runtime files changed')


if __name__ == '__main__':
    main()
