"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Launch the isolated, verified FH6 setup with the documented Metal 3 backend comparison."""
from pathlib import Path
import subprocess
base=Path(__file__).resolve().parents[2]
r=base/'work/gptk4-full-runtime'
b=base/'work/bottles/FH6-GPTK4'
a=[str(r/'bin/wine'),'--bottle',str(b),'--debugmsg','-all,err+ole,err+combase,err+seh','--dll','concrt140=n,b','--env',f'SteamDeck=1 D3DM_MTL4=0 D3DM_VENDOR_ID=32902 CX_GRAPHICS_BACKEND=d3dmetal CX_ROOT={r} CX_APPLEGPTK_LIBD3DSHARED_PATH={r}/lib64/apple_gptk/external/libd3dshared.dylib','--cx-app',r'C:\Program Files (x86)\Steam\steam.exe','--','-applaunch','2483190']
with (base/'work/menu-rendering/fh6-metal3-launch.log').open('w') as f:
 raise SystemExit(subprocess.run(a,stdout=f,stderr=subprocess.STDOUT).returncode)
