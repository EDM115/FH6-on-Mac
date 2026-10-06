"""Guarded per-session use of FH6 build 440.853's existing timing selector.

Reuses the verified A/B/A helper while rediscovering all heap objects from
fixed, build-checked globals in the specified Wine process. Default is read-only;
--cpu selects the CPU frame history with one data-byte write and retains it for
this process. --original reverses that write. No code or game files are patched.
Each sample rechecks the pointer chain, types, and constant config block.
"""
import argparse, contextlib, datetime, io, json, struct, subprocess, sys
from pathlib import Path
from read_process_session import ReadSession
from local_config import load_config


parser = argparse.ArgumentParser()
parser.add_argument('pid', type=int, help='Wine process ID; build and objects are independently checked')
opts = parser.add_mutually_exclusive_group()
opts.add_argument('--cpu', action='store_true')
opts.add_argument('--original', action='store_true')
opts.add_argument('--test', action='store_true')
args = parser.parse_args()
cfg = load_config()
d = cfg["state"] / "timing"
d.mkdir(parents=True, exist_ok=True)
pid = args.pid
mode = 'cpu' if args.cpu else 'original' if args.original else 'test' if args.test else 'readonly'
test = mode != 'readonly'
def ptr(r, address):
    result = struct.unpack('<Q', r.read(address, 8))[0]
    assert 0x10000 <= result < 0x800000000000, ('invalid object pointer', hex(address))
    return result
with ReadSession(pid) as r:
    assert r.read(0x142a57f41,8) == bytes.fromhex('0fb6412088442428'), 'Build mismatch'
    owner, control, app = [ptr(r,a) for a in (0x14a8af088,0x14a8af0a0,0x14aa28ac0)]
    dispatcher = ptr(r,app+0x28)
    media = ptr(r,dispatcher+0x60)
    manager = ptr(r,media+0x40)
    starting_selector = r.read(control+32,1)[0]
assert starting_selector in (0,1)
initial = starting_selector if not test else (0 if mode == 'original' else 1)
temporary = 1 if mode == 'original' else 0
control_bytes = bytes.fromhex('8fc2ef410000000000002041010100000ad7233e0a000000040000000000e03f01010000')
guards = [(0x14a8af088,struct.pack('<Q',owner)), (0x14a8af0a0, struct.pack('<Q',control)), (owner, struct.pack('<Q',0x146559db0)),
          (0x142a57f41, bytes.fromhex('0fb6412088442428')), # selector load and sixth argument
          (0x14aa28ac0,struct.pack('<Q',app)), (app,struct.pack('<Q',0x146c1ffa0)),
          (app+0x28,struct.pack('<Q',dispatcher)), (dispatcher,struct.pack('<Q',0x146d446f0)),
          (dispatcher+0x60,struct.pack('<Q',media)), (media,struct.pack('<Q',0x146d8af90)),
          (media+0x40,struct.pack('<Q',manager)), (manager,struct.pack('<Q',0x146d595c0))]
guards += [(control+i,control_bytes[i:i+8]) for i in range(0,32,8)]
with ReadSession(pid) as r:
    for addr, value in guards:
        assert r.read(addr,len(value)) == value, ('identity mismatch',hex(addr))
    assert r.read(control+32,4) == bytes([initial])+control_bytes[33:]

def read(addr, off, size, fail='failed'):
    return f''' movq %r12,%rcx
 movabsq ${addr},%rdx
 leaq {off}(%rsp),%r8
 movl ${size},%r9d
 movq $0,0x30(%rsp)
 leaq 0x30(%rsp),%rax
 movq %rax,0x20(%rsp)
 callq *rpm_iat(%rip)
 testl %eax,%eax
 je {fail}
 cmpq ${size},0x30(%rsp)
 jne {fail}
'''

def write(value, fail):
    return f''' movb ${value},0xb0(%rsp)
 movq %r12,%rcx
 movabsq ${control+32},%rdx
 leaq 0xb0(%rsp),%r8
 movl $1,%r9d
 movq $0,0x30(%rsp)
 leaq 0x30(%rsp),%rax
 movq %rax,0x20(%rsp)
 callq *wpm_iat(%rip)
 testl %eax,%eax
 je {fail}
 cmpq $1,0x30(%rsp)
 jne {fail}
'''

a=f'''.text
base:
.globl entrypoint,imports,imports_end,relocations,relocations_end
entrypoint:
 subq $0x188,%rsp
 movl ${0x38 if test else 0x10},%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je failed_open
 movq %rax,%r12
 movl $-11,%ecx
 callq *std_iat(%rip)
 movq %rax,%r13
 xorl %r15d,%r15d
 xorl %ebp,%ebp
 xorl %ebx,%ebx
 callq *tick_iat(%rip)
 movq %rax,%r14
loop:
'''
for addr,value in guards:
    assert len(value)==8
    a+=read(addr,0x90,8)+f' movabsq ${int.from_bytes(value,"little")},%rax\n cmpq %rax,0x90(%rsp)\n jne failed\n'
a+=read(control+32,0x90,4)
a+=f''' movl ${0x100+initial},%eax
 cmpl $1,%r15d
 jne expect_original
 movl ${0x100+temporary},%eax
expect_original:
 cmpl %eax,0x90(%rsp)
 jne failed
 callq *tick_iat(%rip)
 movq %rax,0x40(%rsp)
 movq $0,0x60(%rsp)
 movq $0,0x78(%rsp)
'''
a+=read(owner+0x29c,0x48,16)+read(app+0x94,0x58,12)+read(manager+0xa0,0x68,4)
a+=''' movl %r15d,0x6c(%rsp)
 movl 0x90(%rsp),%eax
 andl $255,%eax
 movl %eax,0x70(%rsp)
 movl %ebx,0x74(%rsp)
 movl %ebp,0x78(%rsp)
 movq %r13,%rcx
 leaq 0x40(%rsp),%rdx
 movl $64,%r8d
 leaq 0x30(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *out_iat(%rip)
 callq *tick_iat(%rip)
 subq %r14,%rax
 cmpl $1,%r15d
 je test_deadline
 cmpq $3000,%rax
 jb next
'''
if test:
    a+=''' cmpl $0,%r15d
 jne done
 movl $1,%ebp
'''+write(temporary,'failed')+''' movl $1,%r15d
 jmp new_phase
test_deadline:
'''+f' cmpq ${30000 if mode=="test" else 3000},%rax\n'+''' 
 jb next
'''
    if mode in ('cpu','original'):
        a+=' xorl %ebp,%ebp\n jmp done\n'
    else:
        a+=write(1,'rollback_failed')+''' xorl %ebp,%ebp
 movl $2,%r15d
'''
    a+='''new_phase:
 callq *tick_iat(%rip)
 movq %rax,%r14
 jmp loop
'''
else:
    a+=' jmp done\ntest_deadline:\n jmp failed\n'
a+='''next:
 movl $100,%ecx
 callq *sleep_iat(%rip)
 jmp loop
failed:
 movl $21,%ebx
done:
 testl %ebp,%ebp
 je exit_code
'''
if test:
    a+=read(0x14a8af0a0,0x90,8,'rollback_failed')+f''' movabsq ${control},%rax
 cmpq %rax,0x90(%rsp)
 jne rollback_failed
'''+read(control+32,0x90,4,'rollback_failed')+''' cmpl $0x100,0x90(%rsp)
 jne rollback_failed
'''+write(1,'rollback_failed')+' xorl %ebp,%ebp\n'
a+='''exit_code:
 movl %ebx,%ecx
 jmp finish
rollback_failed:
 movl $24,%ecx
 jmp finish
failed_open:
 movl $20,%ecx
finish:
 callq *exit_iat(%rip)
 int3
.p2align 3
imports:
'''
imports=[('open','OpenProcess'),('std','GetStdHandle'),('rpm','ReadProcessMemory'),('out','WriteFile'),('tick','GetTickCount64'),('sleep','Sleep'),('exit','ExitProcess')]
if test:imports.append(('wpm','WriteProcessMemory'))
for k,n in imports:a+=f' .long {k}_ilt-base+0x1000,0,0,module-base+0x1000,{k}_iat-base+0x1000\n'
a+=' .long 0,0,0,0,0\nimports_end:\nmodule:\n .asciz "kernel32.dll"\n'
for k,n in imports:a+=f'.p2align 3\n{k}_ilt:\n .quad {k}_name-base+0x1000,0\n{k}_iat:\n .quad {k}_name-base+0x1000,0\n{k}_name:\n .short 0\n .asciz "{n}"\n'
a+='.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
outdir=d/f'frame-timing-session-{pid}-{stamp}-{mode}'
outdir.mkdir()
manifest=dict(time=datetime.datetime.now().astimezone().isoformat(),pid=pid,owner=hex(owner),control=hex(control),test=test,mode=mode,selector_address=hex(control+32),original=initial,temporary=temporary,seconds=30 if mode=='test' else 3)
(outdir/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
import build_pe
build_pe.root=outdir;(outdir/'probe.s').write_text(a)
with contextlib.redirect_stdout(io.StringIO()):build_pe.build('probe.s','probe.exe')
probe='Z:'+str(outdir/'probe.exe').replace('/','\\')
print(json.dumps({'state':'running','directory':str(outdir),'test':test}),flush=True)
r=subprocess.run([str(cfg['probe_runtime']/'bin/wine'),'--bottle',str(cfg['bottle']),'--debugmsg','-all','--cx-app',probe],capture_output=True,timeout=55)
(outdir/'numeric-samples.bin').write_bytes(r.stdout)
(outdir/'stderr.log').write_bytes(r.stderr)
assert len(r.stdout)%64==0
rows=[]
for off in range(0,len(r.stdout),64):
    b=r.stdout[off:off+64]
    rows.append(dict(tick_ms=struct.unpack_from('<Q',b)[0],frame_tick=struct.unpack_from('<I',b,8)[0],frame_delta=struct.unpack_from('<f',b,16)[0],cpu_elapsed=struct.unpack_from('<f',b,20)[0],ui_delta=struct.unpack_from('<f',b,24)[0],ui_updates=struct.unpack_from('<Q',b,28)[0],ui_accumulator=struct.unpack_from('<i',b,40)[0],phase=struct.unpack_from('<I',b,44)[0],selector=struct.unpack_from('<I',b,48)[0]))
with ReadSession(pid) as reader:
    assert reader.read(0x14a8af0a0,8)==struct.pack('<Q',control)
    final=reader.read(control+32,1)[0]
result=dict(manifest,exit_code=r.returncode,final_selector=final,samples=rows)
(outdir/'result.json').write_text(json.dumps(result,indent=2)+'\n')
summary={}
for phase in (0,1,2):
    subset=[x for x in rows if x['phase']==phase]
    if subset:summary[phase]=dict(n=len(subset),first=subset[0],last=subset[-1],min_delta=min(x['frame_delta'] for x in subset),max_delta=max(x['frame_delta'] for x in subset))
print(json.dumps(dict(exit_code=r.returncode,final_selector=final,phases=summary),indent=2),flush=True)
assert r.returncode==0 and final==(initial if mode=='readonly' else 0 if mode=='cpu' else 1)
