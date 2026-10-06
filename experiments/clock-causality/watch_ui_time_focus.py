"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Bounded read-only timing/focus watch for the already verified running FH6.

Reads 16 application bytes and 64 TimeManager bytes, plus their vtables and the
application global. Records foreground PID, OS clocks, and UI timing; no text,
credentials, function injection, debugger attachment, or game-memory writes.
Use only after sample_ui_time.py has verified the current pointer chain.
"""
from pathlib import Path
import datetime, json, struct, subprocess, sys, tempfile

d = Path(__file__).resolve().parent
root = d.parents[1]
pid = int(sys.argv[1], 0)
label = sys.argv[2]
assert label.replace('-', '').isalnum()
seconds = int(sys.argv[3]) if len(sys.argv) > 3 else 90
assert 5 <= seconds <= 180
source = json.loads((d/f'ui-time-comparison-title-control-{pid}.json').read_text())
app = int(source['chain']['app'], 16)
manager = int(source['chain']['manager'], 16)
sys.path.insert(0, str(root/'work/display-size-shim'))
import build_pe

def rpm(address, offset, size):
    return f''' movq %r12,%rcx
 movabsq ${address},%rdx
 leaq {offset}(%rsp),%r8
 movl ${size},%r9d
 movq $0,0x20(%rsp)
 callq *read_iat(%rip)
 testl %eax,%eax
 je fail_read
'''

asm = f'''.text
base:
.globl entrypoint, imports, imports_end, relocations, relocations_end
entrypoint:
 subq $0x128,%rsp
 movl $0x10,%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je fail_open
 movq %rax,%r12
 movl $-11,%ecx
 callq *stdout_iat(%rip)
 movq %rax,%r13
 leaq 0x38(%rsp),%rcx
 callq *frequency_iat(%rip)
 testl %eax,%eax
 je fail_read
 xorl %r14d,%r14d
loop:
 callq *tick_iat(%rip)
 movq %rax,0x40(%rsp)
 leaq 0x48(%rsp),%rcx
 callq *counter_iat(%rip)
 callq *foreground_iat(%rip)
 movq %rax,0x50(%rsp)
 movq %rax,%rcx
 movq $0,0x58(%rsp)
 leaq 0x58(%rsp),%rdx
 callq *thread_iat(%rip)
 movl %eax,0x5c(%rsp)
'''
asm += rpm(0x14aa28ac0, 0x28, 8)
asm += f''' movabsq ${app},%rax
 cmpq %rax,0x28(%rsp)
 jne fail_read
'''
asm += rpm(app, 0x28, 8)
asm += ''' movabsq $0x146c1ffa0,%rax
 cmpq %rax,0x28(%rsp)
 jne fail_read
'''
asm += rpm(manager, 0x28, 8)
asm += ''' movabsq $0x146d595c0,%rax
 cmpq %rax,0x28(%rsp)
 jne fail_read
'''
asm += rpm(app+0x90, 0x60, 16)
asm += rpm(manager+0x68, 0x70, 64)
asm += f''' movq 0x38(%rsp),%rax
 movq %rax,0xb0(%rsp)
 movq %r13,%rcx
 leaq 0x40(%rsp),%rdx
 movl $120,%r8d
 leaq 0x30(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 movl $250,%ecx
 callq *sleep_iat(%rip)
 incl %r14d
 cmpl ${seconds*4},%r14d
 jb loop
 xorl %ecx,%ecx
 jmp finish
fail_open:
 movl $20,%ecx
 jmp finish
fail_read:
 movl $21,%ecx
finish:
 callq *exit_iat(%rip)
 int3
.p2align 3
imports:
'''
imports=[('open','OpenProcess','kernel32.dll'),('read','ReadProcessMemory','kernel32.dll'),
         ('tick','GetTickCount64','kernel32.dll'),('frequency','QueryPerformanceFrequency','kernel32.dll'),
         ('counter','QueryPerformanceCounter','kernel32.dll'),('sleep','Sleep','kernel32.dll'),
         ('foreground','GetForegroundWindow','user32.dll'),('thread','GetWindowThreadProcessId','user32.dll'),
         ('stdout','GetStdHandle','kernel32.dll'),('write','WriteFile','kernel32.dll'),('exit','ExitProcess','kernel32.dll')]
for a,b,module in imports:
    asm += f' .long {a}_ilt-base+0x1000,0,0,{a}_dll-base+0x1000,{a}_iat-base+0x1000\n'
asm += ' .long 0,0,0,0,0\nimports_end:\n'
for a,b,module in imports:
    asm += f'.p2align 3\n{a}_ilt:\n .quad {a}_name-base+0x1000,0\n{a}_iat:\n .quad {a}_name-base+0x1000,0\n{a}_name:\n .short 0\n .asciz "{b}"\n{a}_dll:\n .asciz "{module}"\n'
asm += '.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
start = datetime.datetime.now().astimezone().isoformat()
with tempfile.TemporaryDirectory(prefix='time-focus-', dir=d) as td:
    t=Path(td);build_pe.root=t
    (t/'watch.s').write_text(asm);build_pe.build('watch.s','watch.exe')
    probe='Z:'+str(t/'watch.exe').replace('/', '\\')
    print('Read-only timing/focus capture active.', flush=True)
    r=subprocess.run([str(root/'work/gptk4-full-runtime/bin/wine'),'--bottle',str(root/'work/bottles/FH6-GPTK4'),
                      '--debugmsg','-all','--cx-app',probe],capture_output=True,timeout=seconds+30)
assert r.returncode==0 and len(r.stdout)==seconds*4*120, (r.returncode,len(r.stdout))
rows=[]
for offset in range(0,len(r.stdout),120):
    b=r.stdout[offset:offset+120]
    tick,qpc,hwnd=struct.unpack_from('<3Q',b)
    fg_pid,fg_tid=struct.unpack_from('<II',b,24)
    rows.append(dict(tick_ms=tick,qpc=qpc,foreground_hwnd=hex(hwnd),foreground_pid=fg_pid,
                     foreground_tid=fg_tid,app_delta=struct.unpack_from('<f',b,36)[0],
                     app_updates=struct.unpack_from('<Q',b,40)[0],
                     manager_current=struct.unpack_from('<i',b,48)[0],
                     manager_previous=struct.unpack_from('<i',b,56)[0],
                     manager_accumulator=struct.unpack_from('<i',b,104)[0],
                     qpc_frequency=struct.unpack_from('<Q',b,112)[0]))
out=dict(pid=pid,label=label,start=start,app=hex(app),manager=hex(manager),samples=rows)
(d/f'ui-time-focus-{label}-{pid}.json').write_text(json.dumps(out,indent=2)+'\n')
for state in [True,False]:
    subset=[x for x in rows if (x['foreground_pid']==pid)==state]
    print(json.dumps(dict(game_foreground=state,count=len(subset),
                         deltas=sorted({x['app_delta'] for x in subset})[:20],
                         manager_times=sorted({x['manager_current'] for x in subset})[:20])))
