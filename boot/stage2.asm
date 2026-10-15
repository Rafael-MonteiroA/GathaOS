bits 16
org 0x8000
%include "kernel-size.inc"
%define INFO 0x4000
%define MAP 0x5000
%define MODE 0x6000
%define CTRL 0x6200
%define MODES 0x7000
start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    cld
    mov [boot_drive], dl
    call serial_init
    mov si, msg_stage2
    call puts
    mov di, INFO
    mov cx, 64/2
    xor ax, ax
    rep stosw
    mov dword [INFO], 0x47415448
    mov dword [INFO+4], 1
    mov dword [INFO+12], MAP

    ; Stage 2 owns kernel loading; stage 1 only loads this 8 KiB stage.
    mov ax, 0x1000
    mov es, ax
    mov ax, 17
    mov cx, KERNEL_SECTORS
    xor bx, bx
.load:
    call read_sector
    jc disk_error
    mov dx, es
    add dx, 0x20
    mov es, dx
    inc ax
    loop .load
    xor ax, ax
    mov es, ax

    xor ebx, ebx
    mov di, MAP
.e820:
    mov dword [es:di+20], 1
    mov eax, 0xe820
    mov edx, 0x534d4150
    mov ecx, 24
    int 0x15
    jc memory_error
    cmp eax, 0x534d4150
    jne memory_error
    cmp ecx, 20
    jb memory_error
    inc dword [INFO+8]
    add di, 24
    test ebx, ebx
    jz .memory_done
    cmp dword [INFO+8], 64
    jae memory_error
    jmp .e820
.memory_done:
    call enable_a20
    test ax, ax
    jz a20_error

    mov dword [CTRL], 'VBE2'
    mov di, CTRL
    mov ax, 0x4f00
    int 0x10
    cmp ax, 0x004f
    jne video_error
    cmp dword [CTRL], 'VESA'
    jne video_error
    cmp word [CTRL+4], 0x0200
    jb video_error
    ; Copy the BIOS-owned far mode list before making more VBE calls.
    mov si, [CTRL+14]
    mov ax, [CTRL+16]
    mov fs, ax
    mov di, MODES
    mov cx, 256
.copy_modes:
    mov ax, [fs:si]
    add si, 2
    stosw
    cmp ax, 0xffff
    je .choose
    loop .copy_modes
    jmp video_error
.choose:
    mov word [target_width], 1024
    mov word [target_height], 768
    call find_mode
    jnc .set_mode
    mov word [target_width], 800
    mov word [target_height], 600
    call find_mode
    jc video_error
.set_mode:
    mov bx, [chosen_mode]
    or bx, 0x4000
    mov ax, 0x4f02
    int 0x10
    cmp ax, 0x004f
    jne video_error
    mov eax, [MODE+40]
    mov [INFO+16], eax
    movzx eax, word [MODE+18]
    mov [INFO+20], eax
    movzx eax, word [MODE+20]
    mov [INFO+24], eax
    movzx eax, word [MODE+16]
    mov [INFO+28], eax
    mov dword [INFO+32], 32
    ; VBE 3 defines separate linear pitch and color masks.
    mov si, MODE+31
    cmp word [CTRL+4], 0x0300
    jb .masks
    movzx eax, word [MODE+50]
    mov [INFO+28], eax
    mov si, MODE+54
.masks:
    mov di, INFO+36
    mov cx, 6
.mask_copy:
    movzx eax, byte [si]
    mov [di], eax
    inc si
    add di, 4
    loop .mask_copy
    cli
    lgdt [gdt_pointer]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_entry

find_mode:
    mov si, MODES
.next:
    lodsw
    cmp ax, 0xffff
    je .missing
    mov [chosen_mode], ax
    mov cx, ax
    mov di, MODE
    push si
    mov ax, 0x4f01
    int 0x10
    pop si
    cmp ax, 0x004f
    jne .next
    mov ax, [MODE]
    and ax, 0x0091
    cmp ax, 0x0091
    jne .next
    cmp byte [MODE+25], 32
    jne .next
    cmp byte [MODE+27], 6
    jne .next
    mov ax, [target_width]
    cmp [MODE+18], ax
    jne .next
    mov ax, [target_height]
    cmp [MODE+20], ax
    jne .next
    cmp dword [MODE+40], 0
    je .next
    clc
    ret
.missing:
    stc
    ret

; Verify that address bit 20 no longer aliases low memory.
check_a20:
    pushf
    cli
    push es
    push bx
    push dx
    mov ax, 0xffff
    mov es, ax
    mov bl, [0x500]
    mov dl, [es:0x510]
    mov byte [0x500], 0
    mov byte [es:0x510], 0xff
    xor ax, ax
    cmp byte [0x500], 0xff
    je .restore
    inc ax
.restore:
    mov [es:0x510], dl
    mov [0x500], bl
    pop dx
    pop bx
    pop es
    popf
    ret
enable_a20:
    call check_a20
    test ax, ax
    jnz .done
    mov ax, 0x2401
    int 0x15
    call check_a20
    test ax, ax
    jnz .done
    in al, 0x92
    and al, 0xfe
    or al, 2
    out 0x92, al
    call check_a20
.done:
    ret
serial_init:
    mov dx, 0x3fb
    mov al, 0x80
    out dx, al
    mov dx, 0x3f8
    mov al, 1
    out dx, al
    inc dx
    xor al, al
    out dx, al
    mov dx, 0x3fb
    mov al, 3
    out dx, al
    mov dx, 0x3fc
    out dx, al
    ret
puts:
    lodsb
    test al, al
    jz .done
    mov ah, al
    mov dx, 0x3fd
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    mov al, ah
    mov dx, 0x3f8
    out dx, al
    jmp puts
.done:
    ret
disk_error:
    mov si, msg_disk
    jmp fatal
memory_error:
    mov si, msg_memory
    jmp fatal
a20_error:
    mov si, msg_a20
    jmp fatal
video_error:
    mov si, msg_video
fatal:
    call puts
    cli
.halt:
    hlt
    jmp .halt
boot_drive: db 0
target_width: dw 0
target_height: dw 0
chosen_mode: dw 0
msg_stage2: db 'BOOT: stage2', 13, 10, 0
msg_disk: db 'BOOT ERROR: kernel read', 13, 10, 0
msg_memory: db 'BOOT ERROR: E820', 13, 10, 0
msg_a20: db 'BOOT ERROR: A20', 13, 10, 0
msg_video: db 'BOOT ERROR: VBE', 13, 10, 0
%include "boot/disk.inc"
align 8
gdt:
    dq 0
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff
gdt_end:
gdt_pointer:
    dw gdt_end-gdt-1
    dd gdt
bits 32
protected_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000
    cld
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, KERNEL_BYTES
    rep movsb
    mov ebx, INFO
    mov eax, 0x100000
    jmp eax
times 8192-($-$$) db 0
