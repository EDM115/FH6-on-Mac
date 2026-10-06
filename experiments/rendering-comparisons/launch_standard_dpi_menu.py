"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Launch the native C++ baseline after changing only cloned-bottle display scaling."""
from pathlib import Path
import subprocess

base = Path(__file__).resolve().parents[2]
r = base / 'work/gptk4-full-runtime'
b = base / 'work/bottles/FH6-GPTK4'
debug = '-all,+timestamp,err+all,fixme+d3d12,fixme+dxgi,fixme+d3dcompiler,fixme+dwrite,fixme+d2d,fixme+font'
environment = f'SteamDeck=1 D3DM_MTL4=0 D3DM_VENDOR_ID=32902 CX_GRAPHICS_BACKEND=d3dmetal CX_ROOT={r} CX_APPLEGPTK_LIBD3DSHARED_PATH={r}/lib64/apple_gptk/external/libd3dshared.dylib'
a = [str(r / 'bin/wine'), '--bottle', str(b), '--debugmsg', debug,
     '--dll', 'concrt140,msvcp140,msvcp140_atomic_wait,vcruntime140,vcruntime140_1=n,b',
     '--env', environment,
     '--cx-app', r'C:\Program Files (x86)\Steam\steam.exe', '--', '-applaunch', '2483190']
with (base / 'work/menu-rendering/fh6-standard-dpi-menu.log').open('wb', buffering=0) as log:
    process = subprocess.Popen(a, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    written = 0
    for chunk in iter(lambda: process.stdout.read1(16384), b''):
        keep = chunk[:max(0, 32 * 1024 * 1024 - written)]
        log.write(keep)
        written += len(keep)
    raise SystemExit(process.wait())
