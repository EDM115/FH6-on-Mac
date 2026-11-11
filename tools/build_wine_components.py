"""Rebuild the selected Wine components from checked sources, without installing or signing them"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from local_config import load_config
from prepare_wine_patches import check_files, read_series

TARGETS = {
    'dlls/advapi32/x86_64-windows/advapi32.dll': 'lib/wine/x86_64-windows/advapi32.dll',
    'dlls/mmdevapi/x86_64-windows/mmdevapi.dll': 'lib/wine/x86_64-windows/mmdevapi.dll',
    'dlls/winecoreaudio.drv/x86_64-windows/winecoreaudio.drv': 'lib/wine/x86_64-windows/winecoreaudio.drv',
    'dlls/winecoreaudio.drv/winecoreaudio.so': 'lib/wine/x86_64-unix/winecoreaudio.so',
    'dlls/ntdll/ntdll.so': 'lib/wine/x86_64-unix/ntdll.so',
    'server/wineserver': 'bin/wineserver',
    'dlls/ws2_32/x86_64-windows/ws2_32.dll': 'lib/wine/x86_64-windows/ws2_32.dll',
    'dlls/winhttp/x86_64-windows/winhttp.dll': 'lib/wine/x86_64-windows/winhttp.dll',
}
WITHOUT = ('x', 'freetype', 'alsa', 'capi', 'cups', 'dbus', 'fontconfig', 'gnutls', 'gphoto', 'gstreamer', 'krb5', 'netapi', 'oss', 'pulse', 'sane', 'sdl', 'udev', 'usb', 'v4l2', 'vulkan', 'opencl', 'pcap', 'wayland', 'xinerama', 'opengl', 'ffmpeg')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 32:
        raise SystemExit('--jobs must be between 1 and 32')
    source = args.source.expanduser().resolve()
    series = read_series()
    check_files(source, series['final_files'])
    if not (source / 'configure').is_file():
        raise SystemExit('Select the complete extracted Wine source tree')
    cfg = load_config()
    inotify = cfg['runtime'] / 'lib64/libinotify.0.dylib'
    if not inotify.is_file():
        raise SystemExit('Configured runtime lacks its original libinotify.0.dylib')
    build = cfg['state'] / 'wine-build'
    build.mkdir(parents=True, exist_ok=True)
    options = ['--host=x86_64-apple-darwin', '--enable-archs=x86_64', '--with-mingw', '--with-coreaudio', '--with-unwind', '--with-inotify', '--disable-tests', 'CC=clang -arch x86_64', 'CXX=clang++ -arch x86_64', 'CROSSCC=x86_64-w64-mingw32-gcc', 'INOTIFY_CFLAGS=-I' + str(cfg['repo'] / 'third_party/libinotify/include'), 'INOTIFY_LIBS=-L' + str(cfg['runtime'] / 'lib64') + ' -linotify', 'LDFLAGS=-Wl,-rpath,@loader_path/../lib64']
    options += ['--without-' + name for name in WITHOUT]
    if args.check:
        print(json.dumps({'source': str(source), 'build': str(build), 'configure': options, 'targets': TARGETS, 'installs_files': False}, indent=2))
        return
    env = dict(os.environ)
    bison = Path('/opt/homebrew/opt/bison/bin')
    if bison.is_dir():
        env['PATH'] = str(bison) + os.pathsep + env.get('PATH', '')
    if not shutil.which('x86_64-w64-mingw32-gcc', path=env.get('PATH')):
        raise SystemExit('mingw-w64 is required')
    with (build / 'configure-output.log').open('w') as log:
        subprocess.run([str(source / 'configure'), *options], cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    configuration = (build / 'include/config.h').read_text()
    for required in ('HAVE_LIBUNWIND', 'HAVE_SYS_INOTIFY_H'):
        if '#define ' + required + ' 1' not in configuration:
            raise SystemExit('Required build feature missing: ' + required)
    makefile = (build / 'Makefile').read_text()
    if '-framework CoreAudio' not in makefile or 'dlls/winecoreaudio.drv/winecoreaudio.so:' not in makefile:
        raise SystemExit('Required native CoreAudio target is missing')
    for name, targets in [('tools', ['__tooldeps__']), ('components', list(TARGETS))]:
        print('Building ' + name, flush=True)
        with (build / (name + '-output.log')).open('w') as log:
            subprocess.run(['make', '-j' + str(args.jobs), *targets], cwd=build, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    audio = build / 'dlls/winecoreaudio.drv/winecoreaudio.so'
    dependencies = subprocess.check_output(['/usr/bin/otool', '-L', str(audio)], text=True)
    for line in dependencies.splitlines()[1:]:
        name = line.strip().split(' (', 1)[0]
        if name.endswith('/ntdll.so') and name != '@rpath/ntdll.so':
            subprocess.run(['/usr/bin/install_name_tool', '-change', name, '@rpath/ntdll.so', str(audio)], check=True)
    load_commands = subprocess.check_output(['/usr/bin/otool', '-l', str(audio)], text=True)
    if 'path @loader_path/ (offset' not in load_commands:
        subprocess.run(['/usr/bin/install_name_tool', '-add_rpath', '@loader_path/', str(audio)], check=True)
    client_symbols = subprocess.check_output(['nm', '-gjU', str(build / 'dlls/ntdll/ntdll.so')], text=True).splitlines()
    existing_symbols = subprocess.check_output(['nm', '-gjU', str(cfg['runtime'] / 'lib/wine/x86_64-unix/ntdll.so')], text=True).splitlines()
    if sorted(client_symbols) != sorted(existing_symbols):
        raise SystemExit('Rebuilt ntdll native exports differ from the configured runtime')
    imports = subprocess.check_output(['nm', '-u', str(build / 'server/wineserver')], text=True).splitlines()
    if not all(name in imports for name in ('_inotify_init', '_inotify_add_watch', '_inotify_rm_watch', '_kevent', '_kqueue')):
        raise SystemExit('Rebuilt server lost required inotify/kqueue imports')
    records = {destination: {'build_file': name, 'sha256': hashlib.sha256((build / name).read_bytes()).hexdigest()} for name, destination in TARGETS.items()}
    (build / 'components.json').write_text(json.dumps({'components': records, 'source_files': series['final_files'], 'source_root': str(source), 'installed': False}, indent=2) + '\n')
    print('Build and native-linkage checks passed, no runtime or bottle files were installed')


if __name__ == '__main__':
    main()
