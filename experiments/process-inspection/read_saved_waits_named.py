"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read Wine saved syscall records, filtering to code and exact wait-owner addresses.

No debugger, suspension, GetThreadContext, register writes, or game writes.
Saved records are not guaranteed current contexts, and stack address candidates
are not an unwind. Recheck frame RIP/RSP and validate TEB identity/stack bounds.
Raw stack bytes never leave the temporary probe process.
Wine 11 layout sources: ntdll/unix/{unix_private.h,signal_x86_64.c}.
"""
from pathlib import Path
import contextlib, datetime, io, json, struct, subprocess, sys, tempfile
d=Path(__file__).resolve().parent; root=d.parents[1]
pid=int(sys.argv[1],0); assert pid>0
label=sys.argv[2]; assert label.replace('-','').isalnum()
wait_targets={0x4cb2554b8:'frame_wait',0x4cb2553d0:'frame_object',0x2207d0000:'main_owner'}
sys.path.insert(0,str(root/'work/display-size-shim')); import build_pe

def rpm(addr,dst,n,fail='emit'):
 return f'''{addr}
 movq %r14,%rcx
 leaq {dst}(%rsp),%r8
 movl ${n},%r9d
 movq $0,0x20(%rsp)
 callq *read_iat(%rip)
 testl %eax,%eax
 je {fail}
'''
asm=f'''.text
base:
.globl entrypoint,imports,imports_end,relocations,relocations_end
entrypoint:
 subq $0x1488,%rsp
 movl $0x10,%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je fail
 movq %rax,%r14
 movl $-11,%ecx
 callq *stdout_iat(%rip)
 movq %rax,%rsi
 movl $4,%ecx
 xorl %edx,%edx
 callq *snapshot_iat(%rip)
 cmpq $-1,%rax
 je fail
 movq %rax,%r12
 xorl %ebp,%ebp
 movl $28,0x60(%rsp)
 movq %r12,%rcx
 leaq 0x60(%rsp),%rdx
 callq *first_iat(%rip)
 jmp check
next:
 movl $28,0x60(%rsp)
 movq %r12,%rcx
 leaq 0x60(%rsp),%rdx
 callq *next_iat(%rip)
check:
 testl %eax,%eax
 je done
 cmpl ${pid},0x6c(%rsp)
 jne next
 incl %ebp
 cmpl $512,%ebp
 ja fail
 xorps %xmm0,%xmm0
 movups %xmm0,0xc0(%rsp)
 movups %xmm0,0xd0(%rsp)
 movups %xmm0,0xe0(%rsp)
 movups %xmm0,0xf0(%rsp)
 movl 0x68(%rsp),%r8d
 movl %r8d,0xc0(%rsp)
 movl $0x40,%ecx
 xorl %edx,%edx
 callq *openthread_iat(%rip)
 testq %rax,%rax
 je next
 movq %rax,%r13
 movq %rax,%rcx
 xorl %edx,%edx
 leaq 0x80(%rsp),%r8
 movl $48,%r9d
 movq $0,0x20(%rsp)
 callq *query_iat(%rip)
 movl %eax,0xc4(%rsp)
 testl %eax,%eax
 jne emit
 movq 0x88(%rsp),%rdx
 movq %rdx,0xc8(%rsp)
'''
asm+=rpm('',0x110,0x50)
asm+=f''' cmpq ${pid},0x150(%rsp)
 jne emit
 movl 0xc0(%rsp),%eax
 cmpq %rax,0x158(%rsp)
 jne emit
 movq 0x118(%rsp),%rax
 movq %rax,0xe8(%rsp)
 movq 0x120(%rsp),%rax
 movq %rax,0xf0(%rsp)
'''
asm+=rpm(' movq 0xc8(%rsp),%rdx\n addq $0x378,%rdx',0xd0,8)
asm+=''' movq 0xd0(%rsp),%rdx
 cmpq $0x10000,%rdx
 jbe emit
'''
asm+=rpm('',0x200,0xb8)
asm+=''' movq 0x270(%rsp),%rax
 movq %rax,0xd8(%rsp)
 movq 0x288(%rsp),%rdx
 movq %rdx,0xe0(%rsp)
 cmpq 0xf0(%rsp),%rdx
 jb emit
 movq 0xe8(%rsp),%r9
 cmpq %r9,%rdx
 jae emit
 subq %rdx,%r9
 cmpq $4096,%r9
 jbe sizeok
 movl $4096,%r9d
sizeok:
 andl $-8,%r9d
 movl %r9d,0x30(%rsp)
 testl %r9d,%r9d
 je emit
 movq %r14,%rcx
 leaq 0x400(%rsp),%r8
 movq $0,0x20(%rsp)
 callq *read_iat(%rip)
 testl %eax,%eax
 je emit
'''
asm+=rpm(' movq 0xd0(%rsp),%rdx\n addq $0x70,%rdx',0x300,0x38)
asm+=''' movq 0x300(%rsp),%rax
 cmpq 0xd8(%rsp),%rax
 jne emit
 movq 0x318(%rsp),%rax
 cmpq 0xe0(%rsp),%rax
 jne emit
 movl $1,0xf8(%rsp)
 xorl %edi,%edi
 xorl %ebx,%ebx
 movabsq $0x140001000,%r10
 movabsq $0x146384000,%r11
filter:
 movq 0x400(%rsp,%rdi),%rax
 movabsq $0x4cb2554b8,%rdx
 cmpq %rdx,%rax
 je keep
 movabsq $0x4cb2553d0,%rdx
 cmpq %rdx,%rax
 je keep
 movabsq $0x2207d0000,%rdx
 cmpq %rdx,%rax
 je keep
 cmpq %r10,%rax
 jb skip
 cmpq %r11,%rax
 jae skip
keep:
 movq %rdi,%rdx
 shlq $48,%rdx
 orq %rdx,%rax
 movq %rax,0x400(%rsp,%rbx,8)
 incl %ebx
skip:
 addl $8,%edi
 cmpl 0x30(%rsp),%edi
 jb filter
 movl %ebx,0xfc(%rsp)
emit:
 movq %r13,%rcx
 leaq 0x108(%rsp),%rdx
 movq $0,0x108(%rsp)
 callq *description_iat(%rip)
 movq 0x108(%rsp),%rdx
 xorl %r8d,%r8d
 testq %rdx,%rdx
 je name_ready
name_length:
 cmpw $0,(%rdx,%r8)
 je name_ready
 addl $2,%r8d
 cmpl $512,%r8d
 jb name_length
name_ready:
 movl %r8d,0x104(%rsp)
 movq %rsi,%rcx
 leaq 0x104(%rsp),%rdx
 movl $4,%r8d
 leaq 0x38(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 movl 0x104(%rsp),%r8d
 testl %r8d,%r8d
 je name_done
 movq 0x108(%rsp),%rdx
 movq %rsi,%rcx
 leaq 0x38(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
name_done:
 movq 0x108(%rsp),%rcx
 testq %rcx,%rcx
 je no_free
 callq *localfree_iat(%rip)
no_free:
 movq %rsi,%rcx
 leaq 0xc0(%rsp),%rdx
 movl $64,%r8d
 leaq 0x38(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 movl 0xfc(%rsp),%r8d
 testl %r8d,%r8d
 je close
 shll $3,%r8d
 movq %rsi,%rcx
 leaq 0x400(%rsp),%rdx
 leaq 0x38(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
close:
 movq %r13,%rcx
 callq *close_iat(%rip)
 jmp next
done:
 xorl %ecx,%ecx
 jmp finish
fail:
 movl $23,%ecx
finish:
 callq *exit_iat(%rip)
 int3
.p2align 3
imports:
'''
imports=[('description','GetThreadDescription','kernel32.dll'),('localfree','LocalFree','kernel32.dll'),('open','OpenProcess','kernel32.dll'),('read','ReadProcessMemory','kernel32.dll'),
 ('snapshot','CreateToolhelp32Snapshot','kernel32.dll'),('first','Thread32First','kernel32.dll'),
 ('next','Thread32Next','kernel32.dll'),('openthread','OpenThread','kernel32.dll'),
 ('query','NtQueryInformationThread','ntdll.dll'),('close','CloseHandle','kernel32.dll'),
 ('stdout','GetStdHandle','kernel32.dll'),('write','WriteFile','kernel32.dll'),('exit','ExitProcess','kernel32.dll')]
for a,b,m in imports:asm+=f' .long {a}_ilt-base+0x1000,0,0,{a}_dll-base+0x1000,{a}_iat-base+0x1000\n'
asm+=' .long 0,0,0,0,0\nimports_end:\n'
for a,b,m in imports:asm+=f'.p2align 3\n{a}_ilt:\n .quad {a}_name-base+0x1000,0\n{a}_iat:\n .quad {a}_name-base+0x1000,0\n{a}_name:\n .short 0\n .asciz "{b}"\n{a}_dll:\n .asciz "{m}"\n'
asm+='.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
with tempfile.TemporaryDirectory(prefix='saved-waits-',dir=d) as td:
 t=Path(td);build_pe.root=t;(t/'probe.s').write_text(asm)
 with contextlib.redirect_stdout(io.StringIO()):build_pe.build('probe.s','probe.exe')
 probe='Z:'+str(t/'probe.exe').replace('/','\\')
 r=subprocess.run([str(root/'work/gptk4-full-runtime/bin/wine'),'--bottle',str(root/'work/bottles/FH6-GPTK4'),'--debugmsg','-all','--cx-app',probe],capture_output=True,timeout=30)
assert r.returncode==0,(r.returncode,len(r.stdout))
rows=[]; o=0
while o<len(r.stdout):
 n=struct.unpack_from('<I',r.stdout,o)[0];o+=4;assert n<=512
 name=r.stdout[o:o+n].decode('utf-16le',errors='replace');o+=n
 v=struct.unpack_from('<II6QII',r.stdout,o);o+=64
 assert v[-1]<=512
 a=struct.unpack_from('<'+'Q'*v[-1],r.stdout,o);o+=8*v[-1]
 rows.append(dict(tid=v[0],name=name,status=hex(v[1]),**{k:hex(n) for k,n in zip(['teb','frame','saved_rip','saved_rsp','stack_base','stack_limit'],v[2:8])},coherent=bool(v[8]),code_candidates=[{'offset':x>>48,'address':hex(x&0xffffffffffff),'kind':wait_targets.get(x&0xffffffffffff,'game_code')} for x in a]))
assert o==len(r.stdout)
out=dict(pid=pid,time=datetime.datetime.now().astimezone().isoformat(),caveat='Saved syscall records and candidate code pointers only; not a live stack unwind.',threads=rows)
(d/f'saved-waits-named-{pid}-{label}.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(threads=len(rows),coherent=sum(x['coherent'] for x in rows),with_code=sum(bool(x['code_candidates']) for x in rows),output=f'saved-waits-named-{pid}-{label}.json'),indent=2))
