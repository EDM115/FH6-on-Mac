"""Bounded local pipe helper for fast read-only Wine process inspection.

Imports no debugger, thread-control or process-write APIs. Reads <=4096 bytes per
request and <=8 MiB total. The caller validates semantic object/stack bounds.
Payloads remain in the caller's memory; this module does not persist them.
"""
import contextlib, io, os, select, struct, subprocess, sys, tempfile
from pathlib import Path
from local_config import load_config
class ReadSession:
 def __init__(self,pid):
  assert 0<pid<0xffffffff
  cfg=load_config(); self.d=cfg["state"]/"probes"; self.d.mkdir(parents=True,exist_ok=True)
  import build_pe
  self.t=tempfile.TemporaryDirectory(prefix='read-session-',dir=self.d);t=Path(self.t.name)
  a=f'''.text
base:
.globl entrypoint,imports,imports_end,relocations,relocations_end
entrypoint:
 subq $0x1188,%rsp
 movl $0x10,%ecx
 xorl %edx,%edx
 movl ${pid},%r8d
 callq *open_iat(%rip)
 testq %rax,%rax
 je fail
 movq %rax,%r12
 movl $-10,%ecx
 callq *std_iat(%rip)
 movq %rax,%r13
 movl $-11,%ecx
 callq *std_iat(%rip)
 movq %rax,%r14
 xorl %ebp,%ebp
loop:
 xorl %ebx,%ebx
header:
 movq %r13,%rcx
 leaq 0x40(%rsp,%rbx),%rdx
 movl $16,%r8d
 subl %ebx,%r8d
 leaq 0x30(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *input_iat(%rip)
 testl %eax,%eax
 je success
 cmpl $0,0x30(%rsp)
 je success
 addl 0x30(%rsp),%ebx
 cmpl $16,%ebx
 jb header
 cmpl $0x52454144,0x4c(%rsp)
 jne fail
 movl 0x48(%rsp),%ebx
 testl %ebx,%ebx
 je fail
 cmpl $4096,%ebx
 ja fail
 addl %ebx,%ebp
 cmpl $8388608,%ebp
 ja fail
 movq 0x40(%rsp),%rdx
 cmpq $0x10000,%rdx
 jb fail
 movq %r12,%rcx
 leaq 0x100(%rsp),%r8
 movl %ebx,%r9d
 leaq 0x58(%rsp),%rax
 movq %rax,0x20(%rsp)
 movq $0,0x58(%rsp)
 callq *rpm_iat(%rip)
 movl %eax,0x50(%rsp)
 movq %r14,%rcx
 leaq 0x50(%rsp),%rdx
 movl $16,%r8d
 leaq 0x30(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *out_iat(%rip)
 cmpl $0,0x50(%rsp)
 je loop
 movq %r14,%rcx
 leaq 0x100(%rsp),%rdx
 movl 0x58(%rsp),%r8d
 leaq 0x30(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *out_iat(%rip)
 jmp loop
success:
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
  imports=[('open','OpenProcess'),('std','GetStdHandle'),('input','ReadFile'),('out','WriteFile'),('rpm','ReadProcessMemory'),('exit','ExitProcess')]
  for k,n in imports:a+=f' .long {k}_ilt-base+0x1000,0,0,module-base+0x1000,{k}_iat-base+0x1000\n'
  a+=' .long 0,0,0,0,0\nimports_end:\nmodule:\n .asciz "kernel32.dll"\n'
  for k,n in imports:a+=f'.p2align 3\n{k}_ilt:\n .quad {k}_name-base+0x1000,0\n{k}_iat:\n .quad {k}_name-base+0x1000,0\n{k}_name:\n .short 0\n .asciz "{n}"\n'
  a+='.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
  (t/'probe.s').write_text(a);build_pe.root=t
  with contextlib.redirect_stdout(io.StringIO()):build_pe.build('probe.s','probe.exe')
  probe='Z:'+str(t/'probe.exe').replace('/','\\')
  self.proc=subprocess.Popen([str(cfg['probe_runtime']/'bin/wine'),'--bottle',str(cfg['bottle']),'--debugmsg','-all','--cx-app',probe],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,bufsize=0)
  self.total=0
 def exact(self,n):
  b=b''
  while len(b)<n:
   assert select.select([self.proc.stdout],[],[],10)[0],'read helper timeout'
   x=os.read(self.proc.stdout.fileno(),n-len(b));assert x,'read helper EOF';b+=x
  return b
 def read(self,addr,size):
  assert 0x10000<=addr<0x800000000000 and 0<size<=4096
  self.total+=size;assert self.total<=8388608
  self.proc.stdin.write(struct.pack('<QII',addr,size,0x52454144))
  header=self.exact(16);ok=struct.unpack_from('<I',header)[0];n=struct.unpack_from('<Q',header,8)[0]
  assert ok and n==size,('read failed',hex(addr),size,n)
  return self.exact(n)
 def close(self):
  if getattr(self,'proc',None):
   self.proc.stdin.close()
   try:self.proc.wait(timeout=3)
   except subprocess.TimeoutExpired:self.proc.terminate();self.proc.wait(timeout=3)
   self.proc.stdout.close();self.proc=None
  if getattr(self,'t',None):self.t.cleanup();self.t=None
 def __enter__(self):return self
 def __exit__(self,*args):self.close()
