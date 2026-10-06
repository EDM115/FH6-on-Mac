"""Launch the configured isolated Steam/FH6 session. Apply timing separately at title."""
import argparse
import datetime
import hashlib
import json
import os
import shlex
import subprocess
import time
from local_config import load_config, METAL_SHA256

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('game', 'shader-control'), default='game',
                        help='shader-control prepares an original-bytecode cache without the depth repair')
    parser.add_argument('--check', action='store_true', help='Check files and print command; do not start Wine')
    args = parser.parse_args()
    cfg = load_config()
    runtime, bottle, state = cfg['runtime'], cfg['bottle'], cfg['state']
    if hashlib.sha256(cfg['metal'].read_bytes()).hexdigest() != METAL_SHA256:
        raise SystemExit('Unsupported D3DMetal build. Do not bypass the hash guard; see docs/limitations.md.')
    build = state / 'build'
    required = [runtime/'bin/wine', cfg['loader'], cfg['probe_runtime']/'bin/wine',
                runtime/'lib/wine/x86_64-unix/wine', build/'pipeline_log_capture.dylib', build/'shader_trial.dylib']
    if args.mode == 'game':
        required.append(build/'depth_source_trial.dylib')
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
    for path in required:
        if not path.is_file():
            raise SystemExit(f'Missing prerequisite: {path}')
    # Fresh helper environment; do not inherit expensive validation from a prior shell.
    env = dict(os.environ)
    for name in list(env):
        if name.startswith(('MTL_', 'D3DM_', 'DYLD_', 'FH6_SHADER_', 'CX_', 'WINE')) or name in ('SteamDeck', 'OS_ACTIVITY_DT_MODE'):
            env.pop(name)
    libraries = [build/'pipeline_log_capture.dylib', build/'shader_trial.dylib']
    if args.mode == 'game':
        libraries.append(build/'depth_source_trial.dylib')
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
    command = [str(runtime/'bin/wine'), '--bottle', str(bottle), '--debugmsg',
               '-all,+timestamp,err+all,fixme+gamingtcui,fixme+combase,fixme+ole', '--dll',
               'concrt140,msvcp140,msvcp140_atomic_wait,vcruntime140,vcruntime140_1=n,b',
               '--env', shlex.join([f'{k}={v}' for k,v in values.items()]),
               '--cx-app', cfg['steam_exe'], '--', '-applaunch', '2483190']
    if args.check:
        print(shlex.join(command))
        print('File checks passed. This does not verify signatures, a fresh process tree, DLL loading, or gameplay.')
        return
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    logs = state/'logs'/stamp
    logs.mkdir(parents=True, mode=0o700)
    print(f'Private log directory: {logs}\nWait for title, then use window_snapshot.py and frame_timing_session.py in another terminal.', flush=True)
    started = time.monotonic()
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    written = 0
    limit = 8*1024*1024
    with (logs/'graphics.log').open('wb') as log:
        for line in iter(process.stdout.readline, b''):
            if any(word in line for word in (b'FH6_PIPELINE_',b'FH6_SHADER_',b'FH6_TEXTURE_',b'D3DMetal',b'D3DM',b'err:',b'gamingtcui',b'unimplemented function',b'failed assertion',b'Texture Creation',b'pixelFormat')):
                data = (f'[{time.monotonic()-started:.3f}s] '.encode()+line)[:max(0,limit-written)]
                log.write(data); log.flush(); written += len(data)
    result = process.wait()
    print(f'Launcher exited {result}; inspect the private log. Logs may contain personal data: do not upload them unreviewed.')
    raise SystemExit(result)

if __name__ == '__main__':
    main()
