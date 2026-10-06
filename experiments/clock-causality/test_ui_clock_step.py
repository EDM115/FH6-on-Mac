"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""One-shot causal test: advance only AVUI TimeManager's accumulator by 1000ms.

Requires the exact known running clone, verified app/dispatcher/media/manager
vtables, state 2, and zero current/previous/start/accumulated time. Verifies the
application has processed updates with zero delta for another second before the
write. Writes exactly four data bytes once; never patches code, calls remote
functions, changes authentication state, or persists a file in the game/bottle.
The process must be restarted to fully reset this diagnostic time advance.

No generic address/value arguments are accepted. The current build's ownership
chain is rediscovered from its verified application global before the test.
"""
from pathlib import Path
import datetime,json,struct,subprocess,sys,tempfile
from read_subscription import read,q,d,root
pid=int(sys.argv[1],0)
assert pid==1280, 'Prepared only for the current verified diagnostic run.'
app=q(read(0x14aa28ac0,8));assert q(read(app,8))==0x146c1ffa0
dispatcher=q(read(app+0x28,8));assert q(read(dispatcher,8))==0x146d446f0
media=q(read(dispatcher+0x60,8));assert q(read(media,8))==0x146d8af90
manager=q(read(media+0x40,8));assert q(read(manager,8))==0x146d595c0
sys.path.insert(0,str(root/'work/display-size-shim'));import build_pe

def rpm(addr,offset,size):
 return f''' movq %r12,%rcx
 movabsq ${addr},%rdx
 leaq {offset}(%rsp),%r8
 movl ${size},%r9d
 movq $0,0x20(%rsp)
 callq *read_iat(%rip)
 testl %eax,%eax
 je fail
'''

def guard():
 s=''
 for addr,expected in [(0x14aa28ac0,app),(app,0x146c1ffa0),(app+0x28,dispatcher),(dispatcher,0x146d446f0),(dispatcher+0x60,media),(media,0x146d8af90),(media+0x40,manager),(manager,0x146d595c0)]:
  s+=rpm(addr,0x30,8)+f' movabsq ${expected},%rax\n cmpq %rax,0x30(%rsp)\n jne fail\n'
 s+=rpm(manager+0x68,0x70,64)
 for off in [0x70,0x78,0x7c,0xa8]:s+=f' cmpl $0,{off}(%rsp)\n jne fail\n'
 s+=' cmpl $2,0x80(%rsp)\n jne fail\n cmpl $2,0x84(%rsp)\n jne fail\n'
 return s

asm=f'''.text
base:
.globl entrypoint, imports, imports_end, relocations, relocations_end
entrypoint:
 subq $0x188,%rsp
 movl $0x38,%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je fail
 movq %rax,%r12
 movl $-11,%ecx
 callq *stdout_iat(%rip)
 movq %rax,%r13
'''
asm+=guard()+rpm(app+0x94,0x50,12)
asm+=''' cmpl $0,0x50(%rsp)
 jne fail
 movq 0x54(%rsp),%r14
 movl $1000,%ecx
 callq *sleep_iat(%rip)
'''
asm+=guard()+rpm(app+0x94,0x50,12)
asm+=''' cmpl $0,0x50(%rsp)
 jne fail
 movq 0x54(%rsp),%rax
 subq %r14,%rax
 cmpq $10,%rax
 jb fail
 cmpq $300,%rax
 ja fail
 movl $1000,0x40(%rsp)
 movq $0,0x38(%rsp)
 movq %r12,%rcx
'''
asm+=f''' movabsq ${manager+0xa0},%rdx
 leaq 0x40(%rsp),%r8
 movl $4,%r9d
 leaq 0x38(%rsp),%rax
 movq %rax,0x20(%rsp)
 callq *wpm_iat(%rip)
 testl %eax,%eax
 je fail_write
 cmpq $4,0x38(%rsp)
 jne fail_write
 xorl %r15d,%r15d
observe:
 callq *tick_iat(%rip)
 movq %rax,0x48(%rsp)
'''
asm+=rpm(app+0x90,0x50,16)+rpm(manager+0x68,0x60,64)
asm+=''' movq %r13,%rcx
 leaq 0x48(%rsp),%rdx
 movl $88,%r8d
 leaq 0x30(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 movl $100,%ecx
 callq *sleep_iat(%rip)
 incl %r15d
 cmpl $30,%r15d
 jb observe
 xorl %ecx,%ecx
 jmp finish
fail:
 movl $21,%ecx
 jmp finish
fail_write:
 movl $22,%ecx
finish:
 callq *exit_iat(%rip)
 int3
.p2align 3
imports:
'''
imports=[('open','OpenProcess'),('read','ReadProcessMemory'),('wpm','WriteProcessMemory'),('sleep','Sleep'),('tick','GetTickCount64'),('stdout','GetStdHandle'),('write','WriteFile'),('exit','ExitProcess')]
for a,b in imports:asm+=f' .long {a}_ilt-base+0x1000,0,0,module_name-base+0x1000,{a}_iat-base+0x1000\n'
asm+=' .long 0,0,0,0,0\nimports_end:\nmodule_name:\n .asciz "kernel32.dll"\n'
for a,b in imports:asm+=f'.p2align 3\n{a}_ilt:\n .quad {a}_name-base+0x1000,0\n{a}_iat:\n .quad {a}_name-base+0x1000,0\n{a}_name:\n .short 0\n .asciz "{b}"\n'
asm+='.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
out=dict(pid=pid,time=datetime.datetime.now().astimezone().isoformat(),app=hex(app),manager=hex(manager),write_address=hex(manager+0xa0),old_value_ms=0,test_value_ms=1000,scope='one four-byte data write in the running diagnostic clone only')
with tempfile.TemporaryDirectory(prefix='ui-clock-step-',dir=d) as td:
 t=Path(td);build_pe.root=t;(t/'probe.s').write_text(asm);build_pe.build('probe.s','probe.exe')
 probe='Z:'+str(t/'probe.exe').replace('/','\\')
 r=subprocess.run([str(root/'work/gptk4-full-runtime/bin/wine'),'--bottle',str(root/'work/bottles/FH6-GPTK4'),'--debugmsg','-all','--cx-app',probe],capture_output=True,timeout=20)
out['probe_exit']=r.returncode;out['bytes_observed']=len(r.stdout)
rows=[]
if len(r.stdout)%88==0:
 for off in range(0,len(r.stdout),88):
  b=r.stdout[off:off+88];rows.append(dict(tick_ms=q(b),app_delta=struct.unpack_from('<f',b,12)[0],app_updates=q(b,16),manager_current=struct.unpack_from('<i',b,24)[0],manager_previous=struct.unpack_from('<i',b,32)[0],manager_accumulator=struct.unpack_from('<i',b,80)[0]))
out['samples']=rows
(d/f'ui-clock-step-test-{pid}.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k!='samples'},indent=2));print(json.dumps({'first':rows[:2],'last':rows[-2:]},indent=2))
assert r.returncode==0 and len(rows)==30,(r.returncode,len(rows))
