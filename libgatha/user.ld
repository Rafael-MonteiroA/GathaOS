OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
    . = 0x400000;
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    . = ALIGN(4096); __user_rx_end = .;
    .data : { *(.data*) BYTE(0) }
    __user_file_end = .;
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    . = ALIGN(4096); __user_mem_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
    ASSERT(. <= 0x1400000, "User image exceeds 16 MiB")
}
