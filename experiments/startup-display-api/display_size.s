# GetIntegratedDisplaySize(double *out): expose unavailable information as HRESULT.
# Windows x64 ABI: pointer in RCX; 32-bit HRESULT in EAX; leaf function.
.text
base:
get_display_size:
 testq %rcx, %rcx
 je invalid_pointer
 movq $0, (%rcx)
 movl $0x80004001, %eax # E_NOTIMPL: do not invent screen size
 retq
invalid_pointer:
 movl $0x80004003, %eax # E_POINTER
 retq
.p2align 2
.globl exports, exports_end, relocations, relocations_end
exports:
 .long 0,0
 .short 0,0
 .long dll_name-base+0x1000
 .long 1,1,1
 .long functions-base+0x1000,names-base+0x1000,ordinals-base+0x1000
functions:
 .long get_display_size-base+0x1000
names:
 .long function_name-base+0x1000
ordinals:
 .short 0
dll_name:
 .asciz "api-ms-win-core-sysinfo-l1-2-3.dll"
function_name:
 .asciz "GetIntegratedDisplaySize"
exports_end:
.p2align 2
relocations:
 .long 0x1000,12
 .short 0,0
relocations_end:
