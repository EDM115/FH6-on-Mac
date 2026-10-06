"""Read-only Wine window metadata probe; no window text, input or state changes.

Uses the established local PE packager and Apple's assembler. Fixed-size records
cover every top-level and descendant window in the probe's Wine desktop.
"""
from pathlib import Path
import sys

from local_config import load_config
root = load_config()["state"] / "probes"
root.mkdir(parents=True,exist_ok=True)
import build_pe
build_pe.root = root

code = r'''.text
base:
.globl entrypoint, imports, imports_end, relocations, relocations_end
entrypoint:
 subq $0x28,%rsp
 leaq top_callback(%rip),%rcx
 xorl %edx,%edx
 callq *enum_iat(%rip)
 testl %eax,%eax
 setz %cl
 movzbl %cl,%ecx
 callq *exit_iat(%rip)
 int3
top_callback:
 pushq %rbx
 subq $0x20,%rsp
 movq %rcx,%rbx
 xorl %edx,%edx
 callq inspect
 movq %rbx,%rcx
 leaq child_callback(%rip),%rdx
 xorl %r8d,%r8d
 callq *children_iat(%rip)
 movl $1,%eax
 addq $0x20,%rsp
 popq %rbx
 ret
child_callback:
 subq $0x28,%rsp
 movl $1,%edx
 callq inspect
 movl $1,%eax
 addq $0x28,%rsp
 ret
inspect:
 pushq %rbx
 pushq %rdi
 subq $0x128,%rsp
 movq %rcx,%rbx
 movl %edx,0x28(%rsp)
 leaq 0x30(%rsp),%rdi
 xorl %eax,%eax
 movl $28,%ecx
 rep stosq
 movl $0x36485746,0x30(%rsp)
 movl 0x28(%rsp),%eax
 movl %eax,0x34(%rsp)
 movq %rbx,0x38(%rsp)
 movq %rbx,%rcx
 leaq 0x40(%rsp),%rdx
 callq *thread_iat(%rip)
 movl %eax,0x44(%rsp)
 movq %rbx,%rcx
 callq *parent_iat(%rip)
 movq %rax,0x48(%rsp)
 movq %rbx,%rcx
 movl $4,%edx
 callq *window_iat(%rip)
 movq %rax,0x50(%rsp)
 movq %rbx,%rcx
 movl $-16,%edx
 callq *long_iat(%rip)
 movq %rax,0x58(%rsp)
 movq %rbx,%rcx
 movl $-20,%edx
 callq *long_iat(%rip)
 movq %rax,0x60(%rsp)
 callq *foreground_iat(%rip)
 movq %rax,0x68(%rsp)
 movq %rbx,%rcx
 leaq 0x70(%rsp),%rdx
 callq *rect_iat(%rip)
 movq %rbx,%rcx
 callq *visible_iat(%rip)
 movl %eax,0x80(%rsp)
 movq %rbx,%rcx
 callq *iconic_iat(%rip)
 movl %eax,0x84(%rsp)
 callq *tick_iat(%rip)
 movq %rax,0x88(%rsp)
 movq %rbx,%rcx
 leaq 0x90(%rsp),%rdx
 movl $128,%r8d
 callq *class_iat(%rip)
 movl $-11,%ecx
 callq *stdout_iat(%rip)
 movq %rax,%rcx
 leaq 0x30(%rsp),%rdx
 movl $224,%r8d
 leaq 0x118(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 addq $0x128,%rsp
 popq %rdi
 popq %rbx
 ret
.p2align 3
imports:
'''
functions = [
 ('enum','EnumWindows','user32.dll'),
 ('children','EnumChildWindows','user32.dll'),
 ('thread','GetWindowThreadProcessId','user32.dll'),
 ('parent','GetParent','user32.dll'),
 ('window','GetWindow','user32.dll'),
 ('long','GetWindowLongPtrA','user32.dll'),
 ('foreground','GetForegroundWindow','user32.dll'),
 ('rect','GetWindowRect','user32.dll'),
 ('visible','IsWindowVisible','user32.dll'),
 ('iconic','IsIconic','user32.dll'),
 ('class','GetClassNameA','user32.dll'),
 ('tick','GetTickCount64','kernel32.dll'),
 ('stdout','GetStdHandle','kernel32.dll'),
 ('write','WriteFile','kernel32.dll'),
 ('exit','ExitProcess','kernel32.dll'),
]
for alias, name, dll in functions:
 code += f' .long {alias}_ilt-base+0x1000,0,0,{alias}_dll-base+0x1000,{alias}_iat-base+0x1000\n'
code += ' .long 0,0,0,0,0\nimports_end:\n'
for alias, name, dll in functions:
 code += f'''.p2align 3
{alias}_ilt:
 .quad {alias}_name-base+0x1000,0
{alias}_iat:
 .quad {alias}_name-base+0x1000,0
{alias}_name:
 .short 0
 .asciz "{name}"
{alias}_dll:
 .asciz "{dll}"
'''
code += '.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
(root/'window_inventory.s').write_text(code)
build_pe.build('window_inventory.s','window_inventory.exe')
