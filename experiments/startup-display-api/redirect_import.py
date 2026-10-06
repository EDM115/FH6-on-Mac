"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Redirect exactly one import descriptor in the isolated FH6 executable.

Keeps an untouched backup outside the Steam game directory and a change record.
No opcode, function pointer, graphics feature, or game validation check is edited.
"""
from pathlib import Path
import hashlib, json, shutil, struct
root=Path(__file__).resolve().parent
exe=root.parent/'bottles/FH6-GPTK4/drive_c/Program Files (x86)/Steam/steamapps/common/ForzaHorizon6/forzahorizon6.exe'
backup=root/'forzahorizon6.exe.before-display-shim'
old=b'api-ms-win-core-sysinfo-l1-2-3.dll'; new=b'fh6-display-size.dll'
raw=bytearray(exe.read_bytes()); pe=struct.unpack_from('<I',raw,0x3c)[0]; op=pe+24
assert raw[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',raw,op)[0]==0x20b
sections=[]
for i in range(struct.unpack_from('<H',raw,pe+6)[0]):
 s=op+struct.unpack_from('<H',raw,pe+20)[0]+40*i
 vsize,va,rsize,rptr=struct.unpack_from('<IIII',raw,s+8)
 sections.append((va,max(vsize,rsize),rptr))
def offset(rva):
 for va,size,ptr in sections:
  if va<=rva<va+size:return ptr+rva-va
 raise ValueError(f'Unmapped RVA {rva:x}')
def string(pos):return bytes(raw[pos:raw.index(0,pos)])
imp=offset(struct.unpack_from('<I',raw,op+120)[0]); matches=[]
while True:
 ilt,_,_,name,iat=struct.unpack_from('<IIIII',raw,imp)
 if not name:break
 if string(offset(name)) in (old,new):
  symbols=[]; p=offset(ilt or iat)
  while (thunk:=struct.unpack_from('<Q',raw,p)[0]):
   assert not thunk>>63,'Unexpected ordinal import'
   symbols.append(string(offset(thunk)+2).decode());p+=8
  matches.append((offset(name),symbols))
 imp+=20
assert len(matches)==1 and matches[0][1]==['GetIntegratedDisplaySize'],matches
pos,_=matches[0]
if string(pos)==new:
 assert backup.exists(), 'Patched file lacks backup'
 print('Already redirected; preserving backup')
else:
 assert not backup.exists(), 'Refusing to overwrite a previous backup'
 original_hash=hashlib.sha256(raw).hexdigest()
 shutil.copy2(exe,backup)
 assert hashlib.sha256(backup.read_bytes()).hexdigest()==original_hash
 raw[pos:pos+len(old)+1]=new+b'\0'*(len(old)+1-len(new))
 exe.write_bytes(raw)
 changed=[i for i,(a,b) in enumerate(zip(backup.read_bytes(),raw)) if a!=b]
 assert changed and all(pos<=i<pos+len(old)+1 for i in changed)
 record={'executable':str(exe),'backup':str(backup),'original_sha256':original_hash,'patched_sha256':hashlib.sha256(raw).hexdigest(),'import_name_file_offset':hex(pos),'original_import':old.decode(),'replacement_import':new.decode(),'only_export':'GetIntegratedDisplaySize','changed_bytes':len(changed),'authenticode_directory':list(struct.unpack_from('<II',raw,op+112+8*4))}
 (root/'import-change.json').write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps(record,indent=2))
target=exe.parent/new.decode()
assert not target.exists(),'Refusing to replace an existing game DLL'
shutil.copy2(root/new.decode(),target)
print('Installed only in cloned game directory:',target)
