"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read-only file metadata for FH6 game assets, not account/profile paths."""
from pathlib import Path
import datetime
import json
import subprocess
import time

d = Path(__file__).resolve().parent
prefix = str(d.parents[1]/'work/bottles/FH6-GPTK4/drive_c/Program Files (x86)/Steam/steamapps/common/ForzaHorizon6/')+'/'
rows = []
for index in range(3):
    r = subprocess.run(['/usr/sbin/lsof','-a','-p','18965','-o','-Ffnos'],
                       capture_output=True,text=True,timeout=20)
    assert r.returncode == 0, (r.returncode,r.stderr[:200])
    records = []
    current = {}
    for line in r.stdout.splitlines()+['fEND']:
        if not line: continue
        if line[0]=='f':
            if current.get('n','').startswith(prefix):
                records.append(dict(fd=current.get('f'),
                    asset=current['n'][len(prefix):],offset=current.get('o'),size=current.get('s')))
            current={'f':line[1:]}
        elif line[0] in 'nos': current[line[0]]=line[1:]
    rows.append(dict(time=datetime.datetime.now().astimezone().isoformat(),files=records))
    if index<2: time.sleep(10)
(d/'loading-file-offsets-18965.json').write_text(json.dumps(rows,indent=2)+'\n')
changed = []
first = {x['fd']:x for x in rows[0]['files']}
for row in rows[1:]:
    for f in row['files']:
        old=first.get(f['fd'])
        if old and old['asset']==f['asset'] and old['offset']!=f['offset']:
            changed.append(dict(asset=f['asset'],before=old['offset'],after=f['offset']))
print(json.dumps(dict(samples=len(rows),file_counts=[len(r['files']) for r in rows],changed_offsets=changed[:40]),indent=2))
