"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Read-only Windows system/process memory counters in the existing test bottle.
No game memory is changed. Numeric output only; no account or content reads.
"""
import sys,subprocess,json,struct,datetime,contextlib,io
from pathlib import Path
p=Path(__file__).resolve().parent;root=p.parents[1]
pid=int(sys.argv[1]);assert 0<pid<0xffffffff
sys.path.insert(0,str(root/'work/display-size-shim'));import build_pe
assembly=f'''.text
base:
.globl entrypoint,imports,imports_end,relocations,relocations_end
entrypoint:
 subq $0x308,%rsp
 leaq 0x40(%rsp),%rdi
 xorl %eax,%eax
 movl $40,%ecx
 rep stosq
 movl $64,0x40(%rsp)
 leaq 0x40(%rsp),%rcx
 callq *status_iat(%rip)
 movl %eax,0x138(%rsp)
 leaq 0x80(%rsp),%rcx
 movl $104,%edx
 callq *perf_iat(%rip)
 movl %eax,0x13c(%rsp)
 movl $0x410,%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je output
 movq %rax,%r12
 movq %rax,%rcx
 leaq 0xe8(%rsp),%rdx
 movl $80,%r8d
 callq *proc_iat(%rip)
 movl %eax,0x140(%rsp)
 movq %r12,%rcx
 callq *close_iat(%rip)
output:
 movl $-11,%ecx
 callq *std_iat(%rip)
 movq %rax,%rcx
 leaq 0x40(%rsp),%rdx
 movl $260,%r8d
 leaq 0x150(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *out_iat(%rip)
 xorl %ecx,%ecx
 callq *exit_iat(%rip)
 int3
.p2align 3
imports:
'''
imports=[('status','GlobalMemoryStatusEx'),('perf','K32GetPerformanceInfo'),('proc','K32GetProcessMemoryInfo'),('open','OpenProcess'),('close','CloseHandle'),('std','GetStdHandle'),('out','WriteFile'),('exit','ExitProcess')]
for k,n in imports:assembly+=f' .long {k}_ilt-base+0x1000,0,0,module-base+0x1000,{k}_iat-base+0x1000\n'
assembly+=' .long 0,0,0,0,0\nimports_end:\nmodule:\n .asciz "kernel32.dll"\n'
for k,n in imports:assembly+=f'.p2align 3\n{k}_ilt:\n .quad {k}_name-base+0x1000,0\n{k}_iat:\n .quad {k}_name-base+0x1000,0\n{k}_name:\n .short 0\n .asciz "{n}"\n'
assembly+='.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
(p/'probe.s').write_text(assembly);build_pe.root=p
with contextlib.redirect_stdout(io.StringIO()):build_pe.build('probe.s','probe.exe')
r=subprocess.run([str(root/'work/gptk4-pipeline-runtime/bin/wine'),'--bottle',str(root/'work/bottles/FH6-GPTK4'),'--debugmsg','-all','--cx-app','Z:'+str(p/'probe.exe').replace('/','\\')],capture_output=True,timeout=20)
(p/'probe.stderr.log').write_bytes(r.stderr)
assert r.returncode==0 and len(r.stdout)==260,(r.returncode,len(r.stdout))
b=r.stdout;flags=struct.unpack_from('<III',b,248);assert all(flags),flags
length,load,*values=struct.unpack_from('<II7Q',b)
assert length==64
system=dict(zip(['total_physical','available_physical','total_commit_limit','available_commit','total_virtual_for_probe','available_virtual_for_probe','extended_virtual'],values));system['memory_load_percent']=load
perf=dict(zip(['commit_total_pages','commit_limit_pages','commit_peak_pages','physical_total_pages','physical_available_pages','system_cache_pages','kernel_total_pages','kernel_paged_pages','kernel_nonpaged_pages','page_size'],struct.unpack_from('<10Q',b,64+8)))
proc=dict(zip(['peak_working_set','working_set','peak_paged_pool','paged_pool','peak_nonpaged_pool','nonpaged_pool','pagefile_usage','peak_pagefile_usage','private_usage'],struct.unpack_from('<9Q',b,168+8)))
result=dict(time=datetime.datetime.now().astimezone().isoformat(),wine_pid=pid,system=system,performance=perf,game_process=proc)
(p/'memory-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
