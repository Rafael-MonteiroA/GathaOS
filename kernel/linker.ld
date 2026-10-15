OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .text : { KEEP(*(.text.entry)) *(.text*) }
    .rodata : ALIGN(16) { *(.rodata*) }
    .data : ALIGN(16) { *(.data*) }
    .bss : ALIGN(16) { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; }
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
    ASSERT(__bss_end < 0x200000, "Kernel exceeds reserved boot memory")
}
