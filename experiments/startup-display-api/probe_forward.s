.text
base:
.globl entrypoint, imports, imports_end, relocations, relocations_end
entrypoint:
 subq $0x38, %rsp
 movabsq $0x405ee00000000000, %rax # sentinel 123.5 inches
 movq %rax, 0x20(%rsp)
 leaq 0x20(%rsp), %rcx
 callq *display_iat(%rip)
 cmpl $0x80004001, %eax # E_NOTIMPL, ordinary failure instead of abort
 jne failed_result
 cmpq $0, 0x20(%rsp)
 jne failed_output
 xorl %ecx, %ecx
 callq *display_iat(%rip)
 cmpl $0x80004003, %eax # E_POINTER, no null dereference
 jne failed_null
 xorl %ecx, %ecx
 movl $2, %edx
 movl $3, %r8d
 callq *version_iat(%rip)
 testq %rax, %rax
 je failed_forward
 xorl %ecx, %ecx
 jmp finish
failed_forward:
 movl $4, %ecx
 jmp finish
failed_result:
 movl $1, %ecx
 jmp finish
failed_output:
 movl $2, %ecx
 jmp finish
failed_null:
 movl $3, %ecx
finish:
 callq *exit_iat(%rip)
 int3
.p2align 3
imports:
 .long display_ilt-base+0x1000,0,0,display_module-base+0x1000,display_iat-base+0x1000
 .long exit_ilt-base+0x1000,0,0,exit_module-base+0x1000,exit_iat-base+0x1000
 .long version_ilt-base+0x1000,0,0,display_module-base+0x1000,version_iat-base+0x1000
 .long 0,0,0,0,0
imports_end:
display_ilt:
 .quad display_name-base+0x1000,0
display_iat:
 .quad display_name-base+0x1000,0
exit_ilt:
 .quad exit_name-base+0x1000,0
exit_iat:
 .quad exit_name-base+0x1000,0
version_ilt:
 .quad version_name-base+0x1000,0
version_iat:
 .quad version_name-base+0x1000,0
version_name:
 .short 0
 .asciz "VerSetConditionMask"
display_module:
 .asciz "api-ms-win-core-sysinfo-l1-2-3.dll"
exit_module:
 .asciz "kernel32.dll"
.p2align 1
display_name:
 .short 0
 .asciz "GetIntegratedDisplaySize"
.p2align 1
exit_name:
 .short 0
 .asciz "ExitProcess"
.p2align 2
relocations:
 .long 0x1000,12
 .short 0,0
relocations_end:
