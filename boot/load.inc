; Read through a 32 KiB bounce buffer; never depend on unreal-mode segments
; surviving BIOS calls. Page checks cover both the file and the kernel BSS.
%if KERNEL_BYTES <= 0 || KERNEL_BYTES > 15*1024*1024
    %error "Kernel file exceeds loader bounds"
%endif
%if KERNEL_MEMORY_END < 0x100000+KERNEL_BYTES || KERNEL_MEMORY_END > 0x1000000
    %error "Kernel memory exceeds bootstrap bounds"
%endif
check_kernel_memory:
    mov ebp, 0x100000
.page:
    mov si, MAP
    mov ecx, [INFO+8]
    xor edi, edi
.entry:
    cmp dword [si+4], 0
    jne .next
    mov eax, [si]
    mov edx, [si+8]
    or edx, [si+12]
    jz .next
    mov edx, [si+8]
    add edx, eax
    jc .unbounded
    cmp dword [si+12], 0
    je .end
.unbounded:
    mov edx, 0xffffffff
.end:
    cmp edx, ebp
    jbe .next
    mov ebx, ebp
    add ebx, 4096
    cmp eax, ebx
    jae .next
    cmp dword [si+16], 1
    jne memory_range_error
    test dword [si+20], 1
    jz memory_range_error
    cmp eax, ebp
    ja .next
    cmp edx, ebx
    jb .next
    mov edi, 1
.next:
    add si, 24
    loop .entry
    test edi, edi
    jz memory_range_error
    add ebp, 4096
    cmp ebp, KERNEL_MEMORY_END
    jb .page
    ret

load_kernel:
    cmp dword [0x7c00+16], (5+KERNEL_SECTORS)*2048
    jne metadata_error
    mov eax, [0x7c00+12]
    add eax, 5
    jc metadata_error
    mov [kernel_lba], eax
.read:
    mov eax, [remaining]
    cmp eax, 32768
    jbe .sized
    mov eax, 32768
.sized:
    mov [chunk_bytes], eax
    add eax, 2047
    shr eax, 11
    mov cx, ax
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    mov eax, [kernel_lba]
    call read_sectors
    jc disk_error
    movzx eax, cx
    add [kernel_lba], eax
    jc metadata_error
    ; A BIOS may have changed A20 while servicing the read.
    call enable_a20
    test ax, ax
    jz a20_error
    cli
    lgdt [gdt_pointer]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:copy_chunk

bits 32
copy_chunk:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    cld
    mov esi, 0x10000
    mov edi, [destination]
    mov ecx, [chunk_bytes]
    rep movsb
    mov esi, [destination]
    mov [destination], edi
    mov ecx, [chunk_bytes]
    sub [remaining], ecx
    mov eax, [kernel_crc]
    ; Verify the bytes in their final location, excluding sector padding.
.byte:
    movzx edx, byte [esi]
    inc esi
    xor eax, edx
    mov ebx, 8
.bit:
    shr eax, 1
    jnc .plain
    xor eax, 0xedb88320
.plain:
    dec ebx
    jnz .bit
    dec ecx
    jnz .byte
    mov [kernel_crc], eax
    jmp 0x18:copy_return16
bits 16
copy_return16:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp 0:copy_return_real
copy_return_real:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    cld
    sti
    cmp dword [remaining], 0
    jne load_kernel.read
    mov eax, [kernel_crc]
    not eax
    cmp eax, KERNEL_CRC32
    jne checksum_error
    mov si, msg_loaded
    call puts
    ret

memory_range_error:
    mov si, msg_range
    jmp fatal
metadata_error:
    mov si, msg_metadata
    jmp fatal
checksum_error:
    mov si, msg_checksum
    jmp fatal
align 4
kernel_lba: dd 0
remaining: dd KERNEL_BYTES
chunk_bytes: dd 0
destination: dd 0x100000
kernel_crc: dd 0xffffffff
%defstr KERNEL_BYTES_TEXT KERNEL_BYTES
msg_loaded: db 'BOOT: CD EDD kernel CRC32 PASS bytes=', KERNEL_BYTES_TEXT, 13, 10, 0
msg_range: db 'BOOT ERROR: kernel memory unavailable', 13, 10, 0
msg_metadata: db 'BOOT ERROR: CD image metadata', 13, 10, 0
msg_checksum: db 'BOOT ERROR: kernel checksum', 13, 10, 0
