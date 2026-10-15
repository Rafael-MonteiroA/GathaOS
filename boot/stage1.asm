bits 16
org 0x7c00
    jmp 0:start
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    cld
    sti
    mov [boot_drive], dl
    mov ax, 0x0800
    mov es, ax
    mov ax, 1
    mov cx, 16
    xor bx, bx
.load:
    call read_sector
    jc .error
    mov dx, es
    add dx, 0x20
    mov es, dx
    inc ax
    loop .load
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
message: db 'GathaOS: stage1 disk error', 0
%include "boot/disk.inc"
times 510-($-$$) db 0
dw 0xaa55
