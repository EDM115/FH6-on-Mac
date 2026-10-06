"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Continuation of the bounded UI clock test after completed optimization.

Default: three seconds of read-only guard checking. --advance: at most five
minutes of paced four-byte writes to the same accumulator as the one-shot test.
No code patches, remote calls, authentication fields, or persistent game edits.
Every iteration checks the full object chain, types, state, zero app delta,
and the exact last-written accumulator. Stops on a mismatch, loss of updates,
nonzero app delta, read/write failure, timeout, or a local STOP_UI_CLOCK file.
Writes pause when the UI counter has not advanced; no wall-time backlog is added.
Never rolls time backwards. Restarting FH6 resets the intervention.
"""
from pathlib import Path
import datetime
import json
import struct
import subprocess
import sys
import time

from read_subscription import read, q, d, root

pid = int(sys.argv[1], 0)
assert pid == 1280, 'Prepared only for this verified diagnostic run.'
advance = sys.argv[2:] == ['--advance']
assert not sys.argv[2:] or advance
duration = 300000 if advance else 3000
app = q(read(0x14aa28ac0, 8)); assert q(read(app, 8)) == 0x146c1ffa0
dispatcher = q(read(app + 0x28, 8)); assert q(read(dispatcher, 8)) == 0x146d446f0
media = q(read(dispatcher + 0x60, 8)); assert q(read(media, 8)) == 0x146d8af90
manager = q(read(media + 0x40, 8)); assert q(read(manager, 8)) == 0x146d595c0
prior = json.loads((d/'ui-clock-paced-20261004-204814-advance/result.json').read_text())
assert prior['exit_code'] == 26 and prior['pid'] == pid and prior['manager'] == hex(manager)
assert prior['samples'][-1]['last_written'] == 226956
initial = struct.unpack('<i', read(manager + 0xa0, 4))[0]
assert initial == 226956, 'Only the observed completed first paced-test state is accepted.'

def rpm(addr, offset, size):
    return f''' movabsq ${addr},%rdx
 leaq {offset}(%rsp),%r8
 movl ${size},%r9d
 callq checked_read
 testl %eax,%eax
 je failed_read
'''

guards = ''
for addr, expected in [(0x14aa28ac0, app), (app, 0x146c1ffa0),
        (app + 0x28, dispatcher), (dispatcher, 0x146d446f0),
        (dispatcher + 0x60, media), (media, 0x146d8af90),
        (media + 0x40, manager), (manager, 0x146d595c0)]:
    guards += rpm(addr, 0x38, 8)
    guards += f' movabsq ${expected},%rax\n cmpq %rax,0x38(%rsp)\n jne failed_identity\n'

asm = f'''.text
base:
.globl entrypoint, imports, imports_end, relocations, relocations_end
entrypoint:
 subq $0x208,%rsp
 movl $-11,%ecx
 callq *stdout_iat(%rip)
 movq %rax,%r13
 movl ${0x38 if advance else 0x10},%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je failed_open
 movq %rax,%r12
 movl ${initial},%ebx
 xorl %ebp,%ebp
 movq $0,0x48(%rsp)
 movl $0,0xb8(%rsp)
 callq *tick_iat(%rip)
 movq %rax,%r14
 movq %rax,%r15
 movq %rax,%rdi
 xorl %esi,%esi
loop:
 leaq stop_filename(%rip),%rcx
 callq *attributes_iat(%rip)
 cmpl $-1,%eax
 jne stop_requested
{guards}
{rpm(app + 0x90, 0x50, 16)}
{rpm(manager + 0x68, 0x60, 64)}
 movl 0x54(%rsp),%eax
 andl $0x7fffffff,%eax
 testl %eax,%eax
 jne real_delta
 cmpl $2,0x70(%rsp)
 jne failed_state
 cmpl $2,0x74(%rsp)
 jne failed_state
 cmpl $0,0x6c(%rsp)
 jne failed_state
 cmpl %ebx,0x98(%rsp)
 jne changed_clock
 cmpl ${initial},0x60(%rsp)
 jl changed_clock
 cmpl %ebx,0x60(%rsp)
 jg changed_clock
 callq *tick_iat(%rip)
 movq %rax,0xa8(%rsp)
 subq %r14,%rax
 cmpq ${duration},%rax
 jae success
 movl $0,0xb0(%rsp)
 cmpq %rsi,0x58(%rsp)
 je unchanged_counter
 movq 0x58(%rsp),%rsi
 movq 0xa8(%rsp),%rdi
 movl $1,0xb0(%rsp)
unchanged_counter:
 movq 0xa8(%rsp),%rax
 subq %rdi,%rax
 cmpq $30000,%rax
 jae no_updates
 cmpl $0,0xb0(%rsp)
 je maybe_log
 movq 0xa8(%rsp),%rax
 subq %r15,%rax
 movq 0xa8(%rsp),%r15
 cmpq $50,%rax
 jbe bounded_elapsed
 movl $50,%eax
bounded_elapsed:
 testl %eax,%eax
 je maybe_log
 addl %ebx,%eax
 movl %eax,0xa0(%rsp)
'''
if advance:
    asm += f''' movq %r12,%rcx
 movabsq ${manager + 0xa0},%rdx
 leaq 0xa0(%rsp),%r8
 movl $4,%r9d
 movq $0,0x30(%rsp)
 leaq 0x30(%rsp),%rax
 movq %rax,0x20(%rsp)
 callq *wpm_iat(%rip)
 testl %eax,%eax
 je failed_write
 cmpq $4,0x30(%rsp)
 jne failed_write
 movl 0xa0(%rsp),%ebx
 incl %ebp
'''
asm += '''maybe_log:
 movq 0x48(%rsp),%rax
 xorl %edx,%edx
 movl $60,%ecx
 divq %rcx
 testl %edx,%edx
 jne next
 callq log_row
next:
 incq 0x48(%rsp)
 movl $16,%ecx
 callq *sleep_iat(%rip)
 jmp loop
failed_open:
 movl $20,%ecx
 jmp exit_process
failed_read:
 movl $21,0xb8(%rsp)
 jmp done
failed_identity:
 movl $22,0xb8(%rsp)
 jmp done
real_delta:
 movl $23,0xb8(%rsp)
 jmp done
failed_state:
 movl $24,0xb8(%rsp)
 jmp done
changed_clock:
 movl $25,0xb8(%rsp)
 jmp done
no_updates:
 movl $26,0xb8(%rsp)
 jmp done
failed_write:
 movl $27,0xb8(%rsp)
 jmp done
stop_requested:
 movl $28,0xb8(%rsp)
 jmp done
success:
 movl $0,0xb8(%rsp)
done:
 callq log_row
 movl 0xb8(%rsp),%ecx
exit_process:
 callq *exit_iat(%rip)
 int3
checked_read:
 subq $0x38,%rsp
 movl %r9d,0x30(%rsp)
 movq $0,0x28(%rsp)
 leaq 0x28(%rsp),%rax
 movq %rax,0x20(%rsp)
 movq %r12,%rcx
 callq *read_iat(%rip)
 testl %eax,%eax
 je read_return
 movl 0x30(%rsp),%eax
 cmpq %rax,0x28(%rsp)
 sete %al
 movzbl %al,%eax
read_return:
 addq $0x38,%rsp
 ret
log_row:
 subq $0x78,%rsp
 callq *tick_iat(%rip)
 movq %rax,0x30(%rsp)
 subq %r14,%rax
 movq %rax,0x38(%rsp)
 movq %rsi,0x40(%rsp)
 movl %ebp,0x48(%rsp)
 movl 0xe0(%rsp),%eax
 movl %eax,0x4c(%rsp)
 movl 0x118(%rsp),%eax
 movl %eax,0x50(%rsp)
 movl %ebx,0x54(%rsp)
 movl 0xd4(%rsp),%eax
 movl %eax,0x58(%rsp)
 movl 0x138(%rsp),%eax
 movl %eax,0x5c(%rsp)
 movq %r13,%rcx
 leaq 0x30(%rsp),%rdx
 movl $48,%r8d
 leaq 0x28(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 addq $0x78,%rsp
 ret
.p2align 3
imports:
'''
imports = [('open','OpenProcess'),('read','ReadProcessMemory'),
           ('sleep','Sleep'),('tick','GetTickCount64'),('stdout','GetStdHandle'),
           ('write','WriteFile'),('exit','ExitProcess'),('attributes','GetFileAttributesA')]
if advance:
    imports.append(('wpm','WriteProcessMemory'))
for a, n in imports:
    asm += f' .long {a}_ilt-base+0x1000,0,0,module_name-base+0x1000,{a}_iat-base+0x1000\n'
asm += ' .long 0,0,0,0,0\nimports_end:\nmodule_name:\n .asciz "kernel32.dll"\n'
for a, n in imports:
    asm += f'.p2align 3\n{a}_ilt:\n .quad {a}_name-base+0x1000,0\n{a}_iat:\n .quad {a}_name-base+0x1000,0\n{a}_name:\n .short 0\n .asciz "{n}"\n'
stop_filename = ('Z:'+str(d/'STOP_UI_CLOCK').replace('/', '\\')).encode('ascii') + b'\0'
asm += 'stop_filename:\n .byte '+','.join(str(b) for b in stop_filename)+'\n'
asm += '.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'

sys.path.insert(0, str(root/'work/display-size-shim'))
import build_pe
stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
outdir = d/f'ui-clock-paced-resume-{stamp}-{"advance" if advance else "readonly"}'
outdir.mkdir()
build_pe.root = outdir
(outdir/'probe.s').write_text(asm)
build_pe.build('probe.s', 'probe.exe')
manifest = dict(pid=pid, advance=advance, duration_ms=duration,
    initial_ms=initial, app=hex(app), manager=hex(manager),
    write_address=hex(manager+0xa0) if advance else None,
    time=datetime.datetime.now().astimezone().isoformat(),
    pause_writes_without_update=True, no_update_stop_ms=30000,
    prior_result='ui-clock-paced-20261004-204814-advance/result.json')
(outdir/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
assert not (d/'STOP_UI_CLOCK').exists(), 'Remove the explicit stop marker before a new test.'
probe = 'Z:'+str(outdir/'probe.exe').replace('/', '\\')
with (outdir/'numeric-samples.bin').open('wb') as log, (outdir/'stderr.log').open('wb') as err:
    process = subprocess.Popen([str(root/'work/gptk4-full-runtime/bin/wine'),
        '--bottle',str(root/'work/bottles/FH6-GPTK4'),'--debugmsg','-all',
        '--cx-app',probe],stdout=log,stderr=err)
    print(json.dumps({'state':'running','advance':advance,'directory':str(outdir)}),flush=True)
    start = time.monotonic()
    forced_stop = None
    while process.poll() is None:
        if (d/'STOP_UI_CLOCK').exists() or time.monotonic()-start > duration/1000+15:
            forced_stop = 'stop-marker' if (d/'STOP_UI_CLOCK').exists() else 'watchdog'
            process.terminate()
            break
        time.sleep(0.25)
    code = process.wait(timeout=5)
data = (outdir/'numeric-samples.bin').read_bytes()
assert len(data)%48 == 0
rows = []
for off in range(0,len(data),48):
    wall, elapsed, updates, writes, current, accum, last_written, delta, status = struct.unpack_from('<QQQIiiifi',data,off)
    rows.append(dict(tick_ms=wall, elapsed_ms=elapsed, app_updates=updates,
        writes=writes, manager_current=current, manager_accumulator=accum,
        last_written=last_written, app_delta=delta, status=status))
result = dict(manifest, exit_code=code, forced_stop=forced_stop, samples=rows)
(outdir/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'exit_code':code,'forced_stop':forced_stop,
    'samples':len(rows),'first':rows[:2],'last':rows[-2:]},indent=2),flush=True)
