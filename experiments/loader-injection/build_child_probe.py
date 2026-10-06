"""ARCHIVED LAB SNAPSHOT: see README.md in this folder before adapting."""
raise SystemExit("Archived experiment: contains historical process/layout assumptions; inspect and port deliberately, do not run as a launch fix.")

"""Build inert parent/child Windows probes; no game or account access."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parent
sys.path.insert(0, str(root.parent / 'display-size-shim'))
import build_pe
build_pe.root = root

funcs = [('pid', 'GetCurrentProcessId'), ('sleep', 'Sleep'),
         ('stdout', 'GetStdHandle'), ('write', 'WriteFile'),
         ('exit', 'ExitProcess'), ('create', 'CreateProcessW')]

def imports():
    s = '.p2align 3\nimports:\n'
    for alias, name in funcs:
        s += f' .long {alias}_ilt-base+0x1000,0,0,kernel32_name-base+0x1000,{alias}_iat-base+0x1000\n'
    s += ' .long 0,0,0,0,0\nimports_end:\nkernel32_name:\n .asciz "kernel32.dll"\n'
    for alias, name in funcs:
        s += f'.p2align 3\n{alias}_ilt:\n .quad {alias}_name-base+0x1000,0\n{alias}_iat:\n .quad {alias}_name-base+0x1000,0\n{alias}_name:\n .short 0\n .asciz "{name}"\n'
    s += '.p2align 2\nrelocations:\n .long 0x1000,12\n .short 0,0\nrelocations_end:\n'
    return s

prefix = '''.text
base:
.globl entrypoint, imports, imports_end, relocations, relocations_end
entrypoint:
 subq $0x108,%rsp
'''
tail = '''
 movl $-11,%ecx
 callq *stdout_iat(%rip)
 movq %rax,%rcx
 leaq 0xe0(%rsp),%rdx
 movl $16,%r8d
 leaq 0xf0(%rsp),%r9
 movq $0,0x20(%rsp)
 callq *write_iat(%rip)
 movl $30000,%ecx
 callq *sleep_iat(%rip)
 xorl %ecx,%ecx
 callq *exit_iat(%rip)
 int3
'''
child = prefix + '''
 movl $0x4348494c,0xe0(%rsp)
 callq *pid_iat(%rip)
 movl %eax,0xe4(%rsp)
 movq $0,0xe8(%rsp)
''' + tail + imports()
parent = prefix + '''
 leaq 0x50(%rsp),%rdi
 xorl %eax,%eax
 movl $18,%ecx
 rep stosq
 movl $104,0x50(%rsp)
 xorl %ecx,%ecx
 leaq command(%rip),%rdx
 xorl %r8d,%r8d
 xorl %r9d,%r9d
 movq $1,0x20(%rsp)
 movq $0,0x28(%rsp)
 movq $0,0x30(%rsp)
 movq $0,0x38(%rsp)
 leaq 0x50(%rsp),%rax
 movq %rax,0x40(%rsp)
 leaq 0xc0(%rsp),%rax
 movq %rax,0x48(%rsp)
 callq *create_iat(%rip)
 movl %eax,0xec(%rsp)
 movl $0x50415245,0xe0(%rsp)
 callq *pid_iat(%rip)
 movl %eax,0xe4(%rsp)
 movl 0xd0(%rsp),%eax
 movl %eax,0xe8(%rsp)
''' + tail
winpath = '"Z:' + str(root / 'pipeline_child.exe').replace('/', '\\') + '"'
parent += 'command:\n .byte ' + ','.join(str(b) for b in (winpath+'\0').encode('utf-16le')) + '\n' + imports()
for name, code in [('pipeline_child', child), ('pipeline_parent', parent)]:
    (root / (name + '.s')).write_text(code)
    build_pe.build(name + '.s', name + '.exe')
