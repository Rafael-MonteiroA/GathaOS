bits 32
section .text
global gdt_load
extern interrupt_dispatch
gdt_load:
    mov eax, [esp+4]
    lgdt [eax]
    jmp 0x08:.reload
.reload:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

%assign vector 0
%rep 256
isr_%+vector:
    ; These exceptions already push an error code on the interrupted stack.
%if vector != 8 && vector != 10 && vector != 11 && vector != 12 && vector != 13 && vector != 14 && vector != 17 && vector != 21 && vector != 29 && vector != 30
    push dword 0
%endif
    push dword vector
    jmp interrupt_common
%assign vector vector+1
%endrep
interrupt_common:
    cld
    pushad
    xor eax, eax
    mov ax, ds
    push eax
    mov ax, es
    push eax
    mov ax, fs
    push eax
    mov ax, gs
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebx, esp
    ; An interrupt can arrive at any stack alignment. C still requires 16 bytes.
    and esp, -16
    sub esp, 12
    push ebx
    call interrupt_dispatch
    mov esp, ebx
    pop eax
    mov gs, ax
    pop eax
    mov fs, ax
    pop eax
    mov es, ax
    pop eax
    mov ds, ax
    popad
    add esp, 8
    iretd

global interrupt_probe
interrupt_probe:
    pushfd
    push ebx
    push esi
    push edi
    push ebp
    mov eax, 0x13579bdf
    mov ebx, 0x2468ace0
    mov ecx, 0x10203040
    mov edx, 0x50607080
    mov esi, 0x11223344
    mov edi, 0x55667788
    mov ebp, 0x99aabbcc
    std
    stc
    ; STI's interrupt shadow makes HLT and its IRQ return deterministic.
    sti
    hlt
    pushfd
    cli
    cld
    pushad
    cmp dword [esp], 0x55667788
    jne .fail
    cmp dword [esp+4], 0x11223344
    jne .fail
    cmp dword [esp+8], 0x99aabbcc
    jne .fail
    cmp dword [esp+16], 0x2468ace0
    jne .fail
    cmp dword [esp+20], 0x50607080
    jne .fail
    cmp dword [esp+24], 0x10203040
    jne .fail
    cmp dword [esp+28], 0x13579bdf
    jne .fail
    mov eax, [esp+32]
    and eax, 0x601
    cmp eax, 0x601
    jne .fail
    mov eax, 1
    jmp .done
.fail:
    xor eax, eax
.done:
    add esp, 36
    pop ebp
    pop edi
    pop esi
    pop ebx
    popfd
    ret

%if DEBUG_FAULT = 1
global debug_trigger_divide
global debug_divide_instruction
debug_trigger_divide:
    mov eax, 0x13579bdf
    mov ebx, 0x2468ace0
    xor ecx, ecx
    xor edx, edx
    mov esi, 0x11223344
    mov edi, 0x55667788
    mov ebp, 0x99aabbcc
    std
    stc
debug_divide_instruction:
    div ecx
    ud2
%elif DEBUG_FAULT = 2
global debug_trigger_gp
global debug_gp_instruction
debug_trigger_gp:
    mov eax, 0x18
debug_gp_instruction:
    mov ds, ax
    ud2
%endif

section .rodata
align 4
global isr_table
isr_table:
%assign vector 0
%rep 256
    dd isr_%+vector
%assign vector vector+1
%endrep
section .note.GNU-stack noalloc noexec nowrite progbits
