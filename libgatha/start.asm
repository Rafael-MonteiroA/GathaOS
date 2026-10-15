bits 32
section .text.start
global _start
extern app_main
_start:
    sub esp, 12
    push eax
    call app_main
    mov ebx, eax
    mov eax, 3
    int 0x80
    ud2
section .note.GNU-stack noalloc noexec nowrite progbits
