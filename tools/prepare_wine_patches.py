"""Check or apply the recorded patch series to a separate CrossOver 26.3.0 source copy"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'patches/wine'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_series():
    manifest = json.loads((PATCHES / 'series.json').read_text())
    for patch in manifest['patches']:
        if digest(PATCHES / patch['file']) != patch['sha256']:
            raise SystemExit('Patch checksum mismatch: ' + patch['file'])
    return manifest


def check_files(source, expected):
    for name, value in expected.items():
        path = source / name
        if not path.resolve().is_relative_to(source.resolve()) or path.is_symlink():
            raise SystemExit('Source path leaves the selected copy: ' + name)
        if not path.is_file() or digest(path) != value:
            raise SystemExit('Source checksum mismatch: ' + name)


def prepare(source, apply=False):
    source = source.expanduser().resolve()
    manifest = read_series()
    final = manifest['final_files']
    if all((source / name).is_file() and digest(source / name) == value for name, value in final.items()):
        check_files(source, final)
        print('This source copy already matches the complete recorded patch series')
        return
    initial = {}
    for patch in manifest['patches']:
        for name, value in patch['before'].items():
            initial.setdefault(name, value)
    check_files(source, initial)
    with tempfile.TemporaryDirectory(prefix='fh6-patch-check-') as directory:
        scratch = Path(directory)
        for name in initial:
            target = scratch / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / name, target)
        for patch in manifest['patches']:
            check_files(scratch, patch['before'])
            result = subprocess.run(['/usr/bin/patch', '-p1', '-F', '0', '--batch', '--forward', '-i', str(PATCHES / patch['file'])], cwd=scratch, capture_output=True, text=True)
            if result.returncode:
                raise SystemExit(result.stdout + result.stderr)
            check_files(scratch, patch['after'])
        check_files(scratch, final)
        print('All ' + str(len(manifest['patches'])) + ' patches produce the recorded source bytes')
        if not apply:
            print('Check only, pass --apply to update the selected source copy')
            return
        # Recheck after the scratch pass so a concurrent source edit is not overwritten
        check_files(source, initial)
        originals = {name: (source / name).read_bytes() for name in initial}
        written = []
        try:
            for name in initial:
                written.append(name)
                (source / name).write_bytes((scratch / name).read_bytes())
            check_files(source, final)
        except BaseException:
            for name in written:
                (source / name).write_bytes(originals[name])
            raise
        print('Applied to the selected source copy, no runtime or bottle was changed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    prepare(args.source, args.apply)


if __name__ == '__main__':
    main()
