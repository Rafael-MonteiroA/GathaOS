bits 16
org 0x7c00
    jmp 0:start
    times 8-($-$$) db 0
; xorriso fills the boot info table at offsets 8..63 in the ISO copy.
    times 64-($-$$) db 0
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7c00
    cld
    sti
    mov [boot_drive], dl
    mov bx, 0x55aa
    mov ah, 0x41
    int 0x13
    jc .error
    cmp bx, 0xaa55
    jne .error
    test cx, 1
    jz .error
    mov eax, [0x7c00+12]
    cmp eax, 17
    jb .error
    inc eax
    jz .error
    mov cx, 4
    mov bx, 0x8000
    call read_sectors
    jc .error
    mov dl, [boot_drive]
    jmp 0:0x8000
.error:
    mov si, message
.print:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x0e
    xor bx, bx
    int 0x10
    jmp .print
.halt:
    cli
    hlt
    jmp .halt
boot_drive: db 0
message: db 'GathaOS: CD EDD boot error', 0
%include "boot/disk.inc"
times 510-($-$$) db 0
dw 0xaa55
