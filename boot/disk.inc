; Read one sector from an El Torito 1.44 MiB floppy-emulation device.
; AX = logical sector, ES:BX = destination. Preserve all registers.
read_sector:
    pusha
    mov [disk_lba], ax
    mov byte [disk_retries], 3
.retry:
    mov ax, [disk_lba]
    xor dx, dx
    mov cx, 18
    div cx
    inc dl
    mov cl, dl
    xor dx, dx
    mov si, 2
    div si
    mov ch, al
    mov dh, dl
    mov dl, [boot_drive]
    mov ax, 0x0201
    push es
    push bx
    int 0x13
    pop bx
    pop es
    jnc .done
    xor ax, ax
    mov dl, [boot_drive]
    push es
    push bx
    int 0x13
    pop bx
    pop es
    dec byte [disk_retries]
    jnz .retry
    popa
    stc
    ret
.done:
    popa
    clc
    ret
disk_lba: dw 0
disk_retries: db 0
