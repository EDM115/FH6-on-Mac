"""Redirect sysinfo-l1-2 API-set only in the standalone cloned Wine runtime.

Host names are shared by many entries: append a new name rather than overwrite
kernelbase.dll's shared string. Hash/name tables stay unchanged.
"""
from pathlib import Path
import hashlib,json,shutil,struct
from local_config import load_config
cfg=load_config()
root=cfg['state']/'display'
schema=cfg['runtime']/'lib/wine/x86_64-windows/apisetschema.dll'
bottle=cfg['bottle']/'drive_c/windows/system32/fh6disp.dll'
assert '/Applications/' not in str(schema), 'Use a private runtime copy'
assert not bottle.exists(), 'Existing proxy; refusing to replace it'
assert (root/'fh6disp.dll').is_file(), 'Build the proxy first'
assert hashlib.sha256(schema.read_bytes()).hexdigest()=='9d20ab9c93439309b0325467e45b80b70e00ad5f484d8dd522c05e8a94b4a0fb', 'Unrecognized original API schema'

backup=root/'apisetschema.dll.original'
raw=bytearray(schema.read_bytes());pe=struct.unpack_from('<I',raw,0x3c)[0];op=pe+24
for i in range(struct.unpack_from('<H',raw,pe+6)[0]):
 sec=op+struct.unpack_from('<H',raw,pe+20)[0]+40*i
 if raw[sec:sec+8].rstrip(b'\0')==b'.apiset':break
else:raise ValueError('No namespace section')
vs,va,rs,rp=struct.unpack_from('<IIII',raw,sec+8)
version,size,_,count,ent,_,_=struct.unpack_from('<7I',raw,rp)
assert version==6
matches=[]
for i in range(count):
 flags,no,nl,hl,vo,vc=struct.unpack_from('<6I',raw,rp+ent+24*i)
 name=raw[rp+no:rp+no+nl].decode('utf-16le')
 if name.startswith('api-ms-win-core-sysinfo-l1-2-'):matches.append((name,vo,vc))
assert len(matches)==1 and matches[0][2]==1,matches
name,vo,_=matches[0]
flags,no,nl,ho,hln=struct.unpack_from('<5I',raw,rp+vo)
assert bytes(raw[rp+ho:rp+ho+hln]).decode('utf-16le')=='kernelbase.dll'
new='fh6disp.dll'.encode('utf-16le');extra=new+b'\0\0'
assert size+len(extra)<=rs,'Not enough reserved section space'
assert not backup.exists(),'Refusing to overwrite schema backup'
shutil.copy2(schema,backup)
raw[rp+size:rp+size+len(extra)]=extra
struct.pack_into('<II',raw,rp+vo+12,size,len(new))
struct.pack_into('<I',raw,rp+4,size+len(extra))
struct.pack_into('<I',raw,sec+8,max(vs,size+len(extra)))
schema.write_bytes(raw)

assert not bottle.exists(),'Existing file at DLL destination'
shutil.copy2(root/'fh6disp.dll',bottle)
record={'schema':str(schema),'backup':str(backup),'namespace':name,'host_before':'kernelbase.dll','host_after':'fh6disp.dll','game_executable_modified':False,'original_sha256':hashlib.sha256(backup.read_bytes()).hexdigest(),'modified_sha256':hashlib.sha256(raw).hexdigest(),'proxy':str(bottle)}
(root/'schema-change.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
