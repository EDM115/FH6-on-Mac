"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read-only unwind of a stable Wine saved syscall frame, with PE x64 metadata.

No attach/suspend/target writes. Stack bytes stay in memory; only frame addresses
and nonvolatile register values are saved. Stops on unknown modules/opcodes.
This is a saved syscall context, not an atomic sample of a running thread.
Reference: Microsoft x64 exception handling, Wine 11 signal_x86_64.c.
"""
import bisect, datetime, json, mmap, struct, sys
from pathlib import Path
from read_subscription import read,q,d
pid=int(sys.argv[1]);tid=int(sys.argv[2]);label=sys.argv[3]
import atexit
from read_process_session import ReadSession
reader=ReadSession(pid);atexit.register(reader.close);read=reader.read
assert label.replace('-','').isalnum()
root=d.parents[1];system=root/'work/bottles/FH6-GPTK4/drive_c/windows/system32'
class PE:
 def __init__(self,path,base):
  self.path=path;self.base=base;self.f=path.open('rb');self.b=mmap.mmap(self.f.fileno(),0,access=mmap.ACCESS_READ)
  h=struct.unpack_from('<I',self.b,0x3c)[0];o=h+24;n=struct.unpack_from('<H',self.b,h+6)[0];sz=struct.unpack_from('<H',self.b,h+20)[0]
  self.size=struct.unpack_from('<I',self.b,o+56)[0];self.sections=[];self.executable=[]
  for i in range(n):
   vs,va,rs,rp=struct.unpack_from('<IIII',self.b,o+sz+40*i+8);self.sections.append((va,rs,rp))
   flags=struct.unpack_from('<I',self.b,o+sz+40*i+36)[0]
   if flags&0x20000000:self.executable.append((va,va+max(vs,rs)))
  va,n=struct.unpack_from('<II',self.b,o+112+24);self.table=list(struct.iter_unpack('<III',self.data(va,n))) if n else [];self.starts=[x[0] for x in self.table]
 def data(self,r,n):
  for a,sz,p in self.sections:
   if a<=r and r+n<=a+sz:return self.b[p+r-a:p+r-a+n]
  raise ValueError('metadata outside raw sections '+hex(r))
 def function(self,r):
  i=bisect.bisect_right(self.starts,r)-1
  return self.table[i] if i>=0 and self.table[i][0]<=r<self.table[i][1] else None
mods=[PE(system/n,b) for n,b in [('ntdll.dll',0x6ffffff40000),('kernelbase.dll',0x6fffffc10000),('kernel32.dll',0x6fffffec0000)]]
mods.append(PE(root/'work/bottles/FH6-GPTK4/drive_c/Program Files (x86)/Steam/steamapps/common/ForzaHorizon6/forzahorizon6.exe',0x140000000))
source=json.loads((d/f'saved-waits-named-{pid}-loading.json').read_text())
t=next(x for x in source['threads'] if x['tid']==tid);teb=int(t['teb'],16);tb=read(teb,0x50)
assert q(tb,0x40)==pid and q(tb,0x48)==tid
base=q(tb,8);limit=q(tb,16);fp=q(read(teb+0x378,8));h=read(fp,0xa0);rsp=q(h,0x88);rip=q(h,0x70)
assert limit<=rsp<base and base-rsp<=0x10000
stack=b''.join(read(a,min(4096,base-a)) for a in range(rsp,base,4096))
h2=read(fp,0xa0);assert h==h2,'Saved syscall frame changed; no result accepted'
regs={i:q(h,j) for i,j in [(0,0),(1,0x10),(2,0x18),(3,8),(4,0x88),(5,0x98),(6,0x20),(7,0x28)]+[(i,0x30+(i-8)*8) for i in range(8,16)]}
names={3:'rbx',4:'rsp',5:'rbp',6:'rsi',7:'rdi',12:'r12',13:'r13',14:'r14',15:'r15'}
def sq(a):
 assert rsp<=a and a+8<=base,'stack read out of bounds '+hex(a)
 return q(stack,a-rsp)
rows=[];reason='frame bound';verified=set()
try:
 for step in range(48):
  m=next((m for m in mods if m.base<=rip<m.base+m.size),None)
  if m is None:reason='unknown module '+hex(rip);break
  r=rip-m.base
  if not any(a<=r<b for a,b in m.executable):reason='non-executable address '+hex(rip);break
  f=m.function(r);row={'rip':hex(rip),'module':m.path.name,'rsp':hex(regs[4]),'registers':{names[k]:hex(regs[k]) for k in names},'function':hex(m.base+f[0]) if f else None};rows.append(row)
  if not f:row['method']='leaf';rip=sq(regs[4]);regs[4]+=8;continue
  row['method']='PE unwind';chain=0
  while True:
   start,end,ui=f;u=m.data(ui,4);v,prolog,count,fr=u;version=v&7;flags=v>>3;assert version==1,'unsupported unwind version '+str(version)
   raw=m.data(ui,4+2*((count+1)&~1)+(12 if flags&4 else 0))
   key=(m.base,ui)
   if key not in verified:
    assert read(m.base+ui,len(raw))==raw,'live/disk unwind metadata mismatch';verified.add(key)
   codes=raw[4:4+2*count];i=0;inprolog=r-start<prolog and chain==0
   frame_reg=fr&15;frame_off=(fr>>4)*16
   fixed=regs[frame_reg]-frame_off if frame_reg else regs[4]
   while i<count:
    off,opinfo=codes[i*2:i*2+2];op=opinfo&15;info=opinfo>>4;i+=1
    extra={0:0,1:1 if info==0 else 2,2:0,3:0,4:1,5:2,8:1,9:2}.get(op)
    assert extra is not None,'unsupported unwind opcode '+str(op)
    val=int.from_bytes(codes[i*2:(i+extra)*2],'little');i+=extra;assert i<=count
    if inprolog and off>r-start:continue
    if op==0:regs[info]=sq(regs[4]);regs[4]+=8
    elif op==1:regs[4]+=val*(8 if info==0 else 1)
    elif op==2:regs[4]+=info*8+8
    elif op==3:regs[4]=regs[frame_reg]-frame_off
    elif op in (4,5):regs[info]=sq(fixed+val*(8 if op==4 else 1))
    # XMM restoration doesn't affect this integer-address unwind.
   if flags&4:
    f=struct.unpack_from('<III',raw,4+2*((count+1)&~1));chain+=1;assert chain<8
   else:break
  rip=sq(regs[4]);regs[4]+=8
  if not rip:reason='null return';break
except (AssertionError,ValueError) as e:reason=str(e)
out={'time':datetime.datetime.now().astimezone().isoformat(),'pid':pid,'tid':tid,'saved_frame_stable':True,'caveat':'Saved syscall stack; no thread suspension; limited v1 PE unwinder, epilog decoding not implemented. Validate call sites before causal claims.','frames':rows,'stop_reason':reason,'live_metadata_blocks_verified':len(verified)}
p=d/f'loading-unwind-{tid}-{label}.json';p.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'tid':tid,'frames':[{k:v for k,v in x.items() if k!='registers'} for x in rows],'stop_reason':reason,'output':p.name},indent=2))
