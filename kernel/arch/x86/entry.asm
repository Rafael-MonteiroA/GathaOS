bits 32
%define KERNEL_BASE 0xc0000000
section .boot.text
global _start
extern kernel_main
extern __bss_start
extern __bss_end
extern __boot_bss_start
extern __boot_bss_end
_start:
    cli
    cld
    mov edi, __boot_bss_start
    mov ecx, __boot_bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    mov edi, bootstrap_tables
    mov eax, 3
    mov ecx, 4*1024
.map:
    stosd
    add eax, 4096
    loop .map
    mov eax, bootstrap_tables
    or eax, 3
    xor ecx, ecx
.directories:
    mov [bootstrap_directory+ecx*4], eax
    mov [bootstrap_directory+(768*4)+ecx*4], eax
    add eax, 4096
    inc ecx
    cmp ecx, 4
    jb .directories
    mov eax, bootstrap_directory
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80010000
    mov cr0, eax
    mov eax, higher_entry
    jmp eax

section .text.entry
higher_entry:
    mov esp, KERNEL_BASE+0x90000
    add ebx, KERNEL_BASE
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    ; Align the stack to 16 bytes immediately before the C call.
    sub esp, 12
    push ebx
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt
section .boot.bss nobits align=4096
bootstrap_directory: resd 1024
bootstrap_tables: resd 4*1024
section .note.GNU-stack noalloc noexec nowrite progbits
