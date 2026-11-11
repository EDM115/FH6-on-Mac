"""Run the bounded native memory, msync and HTTP fixtures, including failing controls"""
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    output = root / '.local/tests/source-checks'
    output.mkdir(parents=True, exist_ok=True)
    for arm, expected in [('control', 1), ('patched', 0)]:
        binary = output / ('memory-' + arm)
        source = root / 'experiments/memory-footprint'
        subprocess.run(['/usr/bin/clang', '-O2', '-Wall', '-Wextra', '-DCOUNTER_SOURCE="' + str(source / ('counters-' + arm + '.h')) + '"', str(source / 'counter_semantics.c'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        if result.returncode != expected:
            raise SystemExit('Unexpected memory ' + arm + ' result: ' + result.stdout + result.stderr)
        print('Memory ' + arm + ': expected exit ' + str(expected), flush=True)
    for arm, expected in [('control', 1), ('patched', 0)]:
        binary = output / ('msync-' + arm)
        source = root / 'experiments/msync-registration' / (arm + '-source-fixture.c')
        subprocess.run(['/usr/bin/clang', '-O2', '-Wall', '-Wextra', str(source), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        if result.returncode != expected:
            raise SystemExit('Unexpected msync ' + arm + ' result: ' + result.stdout + result.stderr)
        print('Msync ' + arm + ': expected exit ' + str(expected), flush=True)
    binary = output / 'http-logger'
    subprocess.run(['/usr/bin/clang', '-O0', str(root / 'experiments/online-session/logger_fixture.c'), '-o', str(binary)], check=True)
    for enabled in ('0', '1'):
        subprocess.run([str(binary), enabled], check=True)
    print('Source fixtures passed, no Wine process or account data was used')


if __name__ == '__main__':
    main()
