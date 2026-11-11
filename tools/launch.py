"""Launch the configured isolated Steam/FH6 session

Based on the reviewed v2 handoff launcher and EDM115's tools/launch.py at 92c58cc
This local revision validates effective shader mode, isolates trial variables and supports the native loader entry point
Use launch_current.py for the October 25 tested stack
Use FH6_CONFIG=<json> to select other local runtime and bottle paths
"""
import argparse
import contextlib
import datetime
import hashlib
import json
import os
import shlex
import subprocess
import time
from pathlib import Path
from local_config import load_config, METAL_SHA256

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('game', 'shader-control'), default='game',
                        help='shader-control prepares an original-bytecode cache without the depth repair')
    parser.add_argument('--check', action='store_true', help='Check files and print command; do not start Wine')
    parser.add_argument('--env', action='append', default=[], metavar='K=V', help='add or override one launch variable (repeatable)')
    parser.add_argument('--unset', action='append', default=[], metavar='K', help='drop one default launch variable (repeatable)')
    parser.add_argument('--shaders', choices=('fallback', 'control'), default=None,
                        help='game mode only: keep the depth repair but choose original (control) or fallback shaders')
    parser.add_argument('--no-depth-repair', action='store_true', help='game mode only: leave depth_source_trial.dylib out')
    parser.add_argument('--dylib', action='append', default=[], help='extra dylib for DYLD_INSERT_LIBRARIES (repeatable)')
    parser.add_argument('--debugmsg-extra', default='', help='appended to the --debugmsg string, e.g. +loaddll,warn+module')
    parser.add_argument('--steam-silent', action='store_true', help='add -silent before -applaunch')
    parser.add_argument('--direct', action='store_true', help='invoke the configured native Wine loader directly after environment setup')
    parser.add_argument('--raw-log', action='store_true', help='retain a bounded private raw log for DLL-load diagnostics')
    parser.add_argument('--tag', default='', help='label stored in run.json')
    args = parser.parse_args()
    cfg = load_config()
    runtime, bottle, state = cfg['runtime'], cfg['bottle'], cfg['state']
    if hashlib.sha256(cfg['metal'].read_bytes()).hexdigest() != METAL_SHA256:
        raise SystemExit('Unsupported D3DMetal build. Do not bypass the hash guard; see docs/limitations.md.')
    build = state / 'build'
    required = [runtime/'bin/wine', cfg['loader'], cfg['probe_runtime']/'bin/wine',
                runtime/'lib/wine/x86_64-unix/wine', build/'pipeline_log_capture.dylib', build/'shader_trial.dylib']
    if args.mode == 'game' and not args.no_depth_repair:
        required.append(build/'depth_source_trial.dylib')
    for path in required:
        if not path.is_file():
            raise SystemExit(f'Missing prerequisite: {path}')
    # Clear inherited runtime and experiment settings before constructing this trial
    env = dict(os.environ)
    for name in list(env):
        if name.startswith(('MTL_', 'D3DM_', 'DYLD_', 'FH6_', 'FH6SHIM_', 'HB_', 'CX_', 'WINE')) or name in ('SteamDeck', 'OS_ACTIVITY_DT_MODE'):
            env.pop(name)
    libraries = [build/'pipeline_log_capture.dylib', build/'shader_trial.dylib']
    if args.mode == 'game' and not args.no_depth_repair:
        libraries.append(build/'depth_source_trial.dylib')
    for extra in args.dylib:
        path = Path(extra).expanduser().resolve()
        if not path.is_file():
            raise SystemExit(f'Missing extra dylib: {path}')
        libraries.append(path)
    if any(':' in str(p) for p in libraries):
        raise SystemExit('DYLD library paths cannot contain a colon')
    values = {
        'SteamDeck':'1', 'D3DM_MTL4':'1', 'D3DM_VENDOR_ID':'32902',
        'CX_GRAPHICS_BACKEND':'d3dmetal', 'CX_ROOT':str(runtime),
        'CX_APPLEGPTK_LIBD3DSHARED_PATH':str(runtime/'lib64/apple_gptk/external/libd3dshared.dylib'),
        'WINELOADER':str(cfg['loader']), 'CX_WINELOADER':str(cfg['loader']),
        'DYLD_INSERT_LIBRARIES':':'.join(map(str,libraries)),
        'FH6_SHADER_TRIAL':'control' if args.mode == 'shader-control' else 'fallback',
    }
    if args.shaders:
        if args.mode != 'game':
            raise SystemExit('--shaders applies to --mode game only')
        values['FH6_SHADER_TRIAL'] = args.shaders
    for name in args.unset:
        env.pop(name, None)
        values.pop(name, None)
    for item in args.env:
        name, sep, value = item.partition('=')
        if not sep or not name or ' ' in name:
            raise SystemExit(f'--env expects K=V, got {item!r}')
        values[name] = value
    if values.get('FH6_SHADER_TRIAL') == 'fallback':
        folder = state/'shaders/prepared-fallbacks'
        manifest_path = folder/'manifest.json'
        if not manifest_path.is_file():
            raise SystemExit('Generate local shader replacements first; see docs/setup.md.')
        records = json.loads(manifest_path.read_text())
        if not records or any(not item.get('success') for item in records):
            raise SystemExit('Shader preparation did not pass for every input.')
        for item in records:
            digest = item['original_sha256']
            if len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
                raise SystemExit('Invalid shader manifest key')
            replacement = folder/'replacements'/(digest+'.dxil')
            if hashlib.sha256(replacement.read_bytes()).hexdigest() != item['replacement_sha256']:
                raise SystemExit('Local shader replacement checksum mismatch')
        print(f'Validated {len(records)} local replacements (historical checkpoint: 26).')
    debugmsg = '-all,+timestamp,err+all,fixme+gamingtcui,fixme+combase,fixme+ole'
    if args.debugmsg_extra:
        debugmsg += ',' + args.debugmsg_extra.lstrip(',')
    command = [str(runtime/'bin/wine'), '--bottle', str(bottle), '--debugmsg',
               debugmsg, '--dll',
               'concrt140,msvcp140,msvcp140_atomic_wait,vcruntime140,vcruntime140_1=n,b',
               '--env', shlex.join([f'{k}={v}' for k,v in values.items()])]
    command += ['--ux-app', str(cfg['loader']), '--', cfg['steam_exe']] if args.direct else ['--cx-app', cfg['steam_exe'], '--']
    command += (['-silent'] if args.steam_silent else []) + ['-applaunch', '2483190']
    if args.check:
        print(shlex.join(command))
        print('File checks passed. This does not verify signatures, a fresh process tree, DLL loading, or gameplay.')
        return
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    logs = state/'logs'/stamp
    logs.mkdir(parents=True, mode=0o700)
    (logs/'run.json').write_text(json.dumps(dict(tag=args.tag, started=datetime.datetime.now().astimezone().isoformat(),
        argv=os.sys.argv[1:], raw_log=args.raw_log, runtime=str(runtime), bottle=str(bottle), values=values, command=command), indent=2)+'\n')
    print(f'Private log directory: {logs}\nWait for title, then use window_snapshot.py to identify this run. The old frame_timing_session.py addresses do not support this game build.', flush=True)
    started = time.monotonic()
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    written = 0
    limit = 8*1024*1024
    raw_written = 0
    with contextlib.ExitStack() as stack:
        log = stack.enter_context((logs/'graphics.log').open('wb'))
        raw = stack.enter_context((logs/'raw.log').open('wb')) if args.raw_log else None
        for line in iter(lambda: process.stdout.readline(65536), b''):
            if raw is not None and raw_written < limit:
                data = line[:limit-raw_written]
                raw.write(data)
                raw.flush()
                raw_written += len(data)
            if any(word in line for word in (b'FH6_PIPELINE_',b'FH6_SHADER_',b'FH6_TEXTURE_',b'D3DMetal',b'D3DM',b'err:',b'gamingtcui',b'unimplemented function',b'failed assertion',b'Texture Creation',b'pixelFormat',
                                                 b'tsshim',b'msync',b'FH6_MEMORY',b'no-exec',b'loaddll',b'apd12',b':cred:',b'winecoreaudio:')):
                data = (f'[{time.monotonic()-started:.3f}s] '.encode()+line)[:max(0,limit-written)]
                log.write(data); log.flush(); written += len(data)
    result = process.wait()
    print(f'Launcher exited {result}; inspect the private log. Logs may contain personal data: do not upload them unreviewed.')
    raise SystemExit(result)

if __name__ == '__main__':
    main()
