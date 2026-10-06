"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read semaphore counts via query-only duplicate handles; never wait or signal."""
from pathlib import Path
import contextlib,datetime,io,json,struct,subprocess,sys,tempfile
p=Path(__file__).resolve().parent;root=p.parents[1];pid=int(sys.argv[1]);assert pid==1280
rows=json.loads((p/'loading-event-handles.json').read_text());handles=[0x4d8,0x74e8]
assert 0<len(handles)<=16
sys.path.insert(0,str(root/'work/display-size-shim'));import build_pe
asm=f'''.text
base:
.globl entrypoint,imports,imports_end,relocations,relocations_end
entrypoint:
 subq $0xb8,%rsp
 movl $0x40,%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je fail
 movq %rax,%r12
 movl $-11,%ecx
 callq *std_iat(%rip)
 movq %rax,%r13
 leaq handles(%rip),%r14
 movl ${len(handles)},%ebp
loop:
 movq (%r14),%rax
 movq %rax,0x60(%rsp)
 movq $0,0x68(%rsp)
 movq $0,0x70(%rsp)
 movq $0,0x80(%rsp)
 movq %r12,%rcx
 movq %rax,%rdx
 movq $-1,%r8
 leaq 0x80(%rsp),%r9
 movq $1,0x20(%rsp)
 movq $0,0x28(%rsp)
 movq $0,0x30(%rsp)
 callq *dup_iat(%rip)
 testl %eax,%eax
 je fail
 movq 0x80(%rsp),%rcx
 xorl %edx,%edx
 leaq 0x70(%rsp),%r8
 movl $8,%r9d
 movq $0,0x20(%rsp)
 callq *query_iat(%rip)
 movl %eax,0x68(%rsp)
 movq 0x80(%rsp),%rcx
 callq *close_iat(%rip)
 movq %r13,%rcx
 leaq 0x60(%rsp),%rdx
 movl $24,%r8d
 leaq 0x88(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *out_iat(%rip)
 addq $8,%r14
 decl %ebp
 jne loop
 xorl %ecx,%ecx
 jmp finish
fail:
 movl $23,%ecx
finish:
 callq *exit_iat(%rip)
 int3
.p2align 3
handles:
'''+''.join(f' .quad {h}\n' for h in handles)+'.p2align 3\nimports:\n'
imports=[('open','OpenProcess','kernel32.dll'),('std','GetStdHandle','kernel32.dll'),('dup','DuplicateHandle','kernel32.dll'),('query','NtQuerySemaphore','ntdll.dll'),('close','CloseHandle','kernel32.dll'),('out','WriteFile','kernel32.dll'),('exit','ExitProcess','kernel32.dll')]
for k,n,m in imports:asm+=f' .long {k}_ilt-base+0x1000,0,0,{k}_dll-base+0x1000,{k}_iat-base+0x1000\n'
asm+=' .long 0,0,0,0,0\nimports_end:\n'
for k,n,m in imports:asm+=f'.p2align 3\n{k}_ilt:\n .quad {k}_name-base+0x1000,0\n{k}_iat:\n .quad {k}_name-base+0x1000,0\n{k}_name:\n .short 0\n .asciz "{n}"\n{k}_dll:\n .asciz "{m}"\n'
asm+='.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
with tempfile.TemporaryDirectory(prefix='semaphore-query-',dir=p) as td:
 t=Path(td);build_pe.root=t;(t/'probe.s').write_text(asm)
 with contextlib.redirect_stdout(io.StringIO()):build_pe.build('probe.s','probe.exe')
 exe='Z:'+str(t/'probe.exe').replace('/','\\')
 r=subprocess.run([str(root/'work/gptk4-full-runtime/bin/wine'),'--bottle',str(root/'work/bottles/FH6-GPTK4'),'--debugmsg','-all','--cx-app',exe],capture_output=True,timeout=15)
assert r.returncode==0 and len(r.stdout)==len(handles)*24,(r.returncode,len(r.stdout))
events=[]
for i in range(len(handles)):
 h,status,pad,typ,state=struct.unpack_from('<QIIii',r.stdout,i*24);events.append({'handle':hex(h),'status':hex(status),'current_count':typ if status==0 else None,'maximum_count':state if status==0 else None})
out={'time':datetime.datetime.now().astimezone().isoformat(),'pid':pid,'semaphores':events,'method':'query-only duplicate handles; no wait, signal, reset, or target handle closure'}
(p/'loading-semaphore-states.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
