"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Diagnostic launch of isolated FH6 with Apple's Metal API validation logging."""
from pathlib import Path
import os
import subprocess

base = Path(__file__).resolve().parents[2]
r = base / 'work/gptk4-full-runtime'
b = base / 'work/bottles/FH6-GPTK4'
settings = {
    'SteamDeck': '1',
    'D3DM_MTL4': '0',
    'D3DM_VENDOR_ID': '32902',
    'CX_GRAPHICS_BACKEND': 'd3dmetal',
    'CX_ROOT': str(r),
    'CX_APPLEGPTK_LIBD3DSHARED_PATH': str(r / 'lib64/apple_gptk/external/libd3dshared.dylib'),
    'MTL_DEBUG_LAYER': '1',
    'MTL_DEBUG_LAYER_ERROR_MODE': 'nslog',
}
a = [str(r / 'bin/wine'), '--bottle', str(b),
     '--debugmsg', '-all,err+ole,err+combase,err+seh', '--dll', 'concrt140=n,b',
     '--env', ' '.join(f'{k}={v}' for k, v in settings.items()),
     '--cx-app', r'C:\Program Files (x86)\Steam\steam.exe', '--', '-applaunch', '2483190']
with (base / 'work/menu-rendering/fh6-metal-validation.log').open('w') as log:
    raise SystemExit(subprocess.run(a, env={**os.environ, **settings}, stdout=log, stderr=subprocess.STDOUT).returncode)
