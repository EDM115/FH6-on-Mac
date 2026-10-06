"""Run read-only metadata probe in the existing isolated Wine bottle."""
from pathlib import Path
import datetime, json, struct, subprocess, sys

from local_config import load_config
cfg=load_config()
root=cfg["state"]/"probes"
root.mkdir(parents=True,exist_ok=True)
label = sys.argv[1]
assert label.replace('-','').replace('_','').isalnum()
command = [str(cfg['probe_runtime']/'bin/wine'), '--bottle',
           str(cfg['bottle']), '--debugmsg', '-all',
           str(root/'window_inventory.exe')]
started = datetime.datetime.now(datetime.timezone.utc).isoformat()
result = subprocess.run(command, capture_output=True, timeout=30)
(root/f'{label}.stderr.log').write_bytes(result.stderr)
if result.returncode != 0:
 raise RuntimeError(f'Probe exit {result.returncode}; inspect local stderr log')
assert len(result.stdout)%224 == 0, f'Unexpected framing: {len(result.stdout)} bytes'
rows=[]
for offset in range(0,len(result.stdout),224):
 record=result.stdout[offset:offset+224]
 magic,kind,hwnd,pid,tid,parent,owner,style,exstyle,fg = struct.unpack_from('<IIQIIQQQQQ',record)
 assert magic==0x36485746
 rectangle=struct.unpack_from('<4i',record,64)
 visible,iconic,tick=struct.unpack_from('<IIQ',record,80)
 rows.append(dict(kind='child' if kind else 'top',hwnd=hex(hwnd),pid=pid,tid=tid,
                  parent=hex(parent),owner=hex(owner),style=hex(style),exstyle=hex(exstyle),
                  foreground=hwnd==fg,rect=rectangle,visible=bool(visible),iconic=bool(iconic),
                  tick_ms=tick,window_class=record[96:224].split(b'\0')[0].decode('ascii','replace')))
out=dict(started_utc=started,finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         label=label,windows=rows)
(root/f'{label}.windows.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(label=label,count=len(rows),visible=[r for r in rows if r['visible']]),indent=2))
