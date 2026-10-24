; BIOS EDD reads use 2048-byte logical sectors on the no-emulation CD.
; EAX=LBA, CX=count (1..16), ES:BX=buffer within one 64 KiB boundary.
; Preserve caller registers, return CF set after three failed attempts.
read_sectors:
    pushad
    push ds
    push es
    mov [disk_packet+8], eax
    mov [disk_packet+4], bx
    mov [disk_packet+6], es
    mov [disk_count], cx
    mov byte [disk_retries], 3
.retry:
    mov ax, [disk_count]
    mov [disk_packet+2], ax
    mov si, disk_packet
    mov dl, [boot_drive]
    mov ah, 0x42
    push ds
    push es
    int 0x13
    pop es
    pop ds
    jnc .done
    xor ax, ax
    mov dl, [boot_drive]
    push ds
    push es
    int 0x13
    pop es
    pop ds
    dec byte [disk_retries]
    jnz .retry
    stc
.done:
    pop es
    pop ds
    popad
    ret
align 4
disk_packet:
    db 16, 0
    dw 0, 0, 0
    dq 0
disk_count: dw 0
disk_retries: db 0
