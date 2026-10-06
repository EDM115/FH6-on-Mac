"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Export the missing API and forward existing kernelbase APIs unchanged."""
from pathlib import Path
import struct,json
from build_pe import build
root=Path(__file__).resolve().parent
p=root.parent/'gptk4-full-runtime/lib/wine/x86_64-windows/kernelbase.dll'
r=p.read_bytes(); pe=struct.unpack_from('<I',r,0x3c)[0];op=pe+24;sections=[]
for i in range(struct.unpack_from('<H',r,pe+6)[0]):
 s=op+struct.unpack_from('<H',r,pe+20)[0]+40*i
 vs,va,rs,rp=struct.unpack_from('<IIII',r,s+8);sections.append((va,max(vs,rs),rp))
def off(v):
 for va,size,rp in sections:
  if va<=v<va+size:return rp+v-va
 raise ValueError(hex(v))
exp=off(struct.unpack_from('<I',r,op+112)[0]); fields=struct.unpack_from('<IIHHIIIIIII',r,exp); nn,na=fields[7],fields[9]
names=[]
for i in range(nn):
 pos=off(struct.unpack_from('<I',r,off(na)+4*i)[0]);names.append(r[pos:r.index(0,pos)].decode())
assert 'GetIntegratedDisplaySize' not in names
names=sorted(names+['GetIntegratedDisplaySize'])
head=(root/'display_size.s').read_text().split('.p2align 2')[0]
lines=[head,'.p2align 2','.globl exports, exports_end, relocations, relocations_end','exports:',' .long 0,0',' .short 0,0',' .long dll_name-base+0x1000',f' .long 1,{len(names)},{len(names)}',' .long functions-base+0x1000,names-base+0x1000,ordinals-base+0x1000','functions:']
for i,name in enumerate(names):lines.append(f' .long {"get_display_size" if name=="GetIntegratedDisplaySize" else "forward_"+str(i)}-base+0x1000')
lines.append('names:')
for i in range(len(names)):lines.append(f' .long name_{i}-base+0x1000')
lines.append('ordinals:')
for i in range(len(names)):lines.append(f' .short {i}')
lines+=['dll_name:',' .asciz "fh6disp.dll"']
for i,name in enumerate(names):
 lines+=[f'name_{i}:',f' .asciz {json.dumps(name)}']
 if name!='GetIntegratedDisplaySize':lines+=[f'forward_{i}:',f' .asciz {json.dumps("kernelbase."+name)}']
lines+=['exports_end:','.p2align 2','relocations:',' .long 0x1000,12',' .short 0,0','relocations_end:']
(root/'display_proxy.s').write_text('\n'.join(lines)+'\n')
build('display_proxy.s','fh6disp.dll',True)
print(f'Forwarded {len(names)-1} existing kernelbase exports; added one local API')
