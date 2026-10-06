"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Capture D3DMetal pipeline errors locally with per-process stderr mirroring."""
from pathlib import Path
import subprocess, os

base = Path(__file__).resolve().parents[2]
r = base / 'work/gptk4-full-runtime'
b = base / 'work/bottles/FH6-GPTK4'
debug = '-all,+timestamp,err+all,fixme+d3d12,fixme+dxgi,fixme+d3dcompiler,fixme+dwrite,fixme+d2d,fixme+font'
hook = base / 'work/menu-rendering/pipeline_log_capture.dylib'
loader = base / 'work/menu-rendering/diagnostic-loader/wineloader'
environment = f'WINELOADER={loader} CX_WINELOADER={loader} DYLD_INSERT_LIBRARIES={hook} SteamDeck=1 D3DM_MTL4=0 D3DM_VENDOR_ID=32902 CX_GRAPHICS_BACKEND=d3dmetal CX_ROOT={r} CX_APPLEGPTK_LIBD3DSHARED_PATH={r}/lib64/apple_gptk/external/libd3dshared.dylib'
a = [str(r / 'bin/wine'), '--bottle', str(b), '--debugmsg', debug,
     '--dll', 'concrt140,msvcp140,msvcp140_atomic_wait,vcruntime140,vcruntime140_1=n,b',
     '--env', environment,
     '--cx-app', r'C:\Program Files (x86)\Steam\steam.exe', '--', '-applaunch', '2483190']
# Mirror only this process tree's native logs; retain graphics messages only.
with (base / 'work/menu-rendering/fh6-pipeline-interposed.log').open('wb', buffering=0) as log:
    native_env = os.environ.copy()
    native_env.pop('OS_ACTIVITY_DT_MODE', None)
    process = subprocess.Popen(a, env=native_env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    written = 0
    for line in iter(process.stdout.readline, b''):
        if not any(token in line for token in (b'D3DMetal', b'D3DM', b'GetRenderPipelineState', b'marking PSO', b'FH6_PIPELINE_DIAGNOSTIC', b'FH6_PIPELINE_CAPTURE_LOADED')):
            continue
        keep = line[:max(0, 8 * 1024 * 1024 - written)]
        log.write(keep)
        written += len(keep)
    raise SystemExit(process.wait())
