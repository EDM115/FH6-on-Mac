"""Compile standalone experiment probes, without running Wine or changing a bottle"""
import argparse
from pathlib import Path
import shutil
import subprocess

WINDOWS = {
    'timestamp': ('timestamp-queries/ts_repro.c', ['-ld3d12', '-ldxgi', '-luuid']),
    'layout': ('vertex-layout/layout_test.c', []),
    'credential-filter': ('saved-login/credential_filter.c', ['-ladvapi32']),
    'audio-client': ('spatial-audio/audio_client_probe.c', ['-lole32', '-luuid']),
    'memory-client': ('memory-footprint/memory_client_probe.c', ['-lpsapi']),
    'wait-registration': ('msync-registration/wait_probe.c', []),
    'http-metadata': ('online-session/http_probe.c', ['-lwinhttp']),
    'udp-ioctl': ('online-session/udp_ioctl_probe.c', ['-lws2_32']),
    'udp-ttl': ('online-session/udp_ttl_probe.c', ['-lws2_32']),
    'udp-ttl-wire': ('online-session/udp_ttl_wire_probe.c', ['-lws2_32']),
    'connectivity': ('online-session/connectivity_probe.c', ['-lole32', '-luuid', '-liphlpapi', '-lruntimeobject']),
    'thread-owners': ('online-session/thread_owners.c', []),
    'thread-map': ('frame-pauses/map_threads.c', []),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='x86_64-w64-mingw32-gcc')
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if not compiler:
        raise SystemExit('Install mingw-w64 or select its compiler with --cc')
    root = Path(__file__).resolve().parents[1]
    output = root / '.local/build/probes'
    output.mkdir(parents=True, exist_ok=True)
    for name, (relative, libraries) in WINDOWS.items():
        print('Building ' + name, flush=True)
        subprocess.run([compiler, '-O2', '-Wall', '-Wextra', '-static-libgcc', str(root / 'experiments' / relative), '-o', str(output / (name + '.exe')), *libraries], check=True)
    for arm in ('control', 'patched'):
        subprocess.run([compiler, '-O2', '-Wall', '-static-libgcc', '-DTEST_PATCHED=' + str(int(arm == 'patched')), str(root / 'experiments/spatial-audio/channel_order.c'), '-o', str(output / ('channel-order-' + arm + '.exe'))], check=True)
    print('Probe binaries are private under .local/build/probes, no probe was executed')


if __name__ == '__main__':
    main()
