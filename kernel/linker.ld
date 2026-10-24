OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .boot.text : { KEEP(*(.boot.text)) }
    .boot.bss (NOLOAD) : ALIGN(4096) {
        __boot_bss_start = .; *(.boot.bss) __boot_bss_end = .;
    }
    . += 0xc0000000;
    .text : AT(ADDR(.text) - 0xc0000000) {
        __text_start = .; KEEP(*(.text.entry)) *(.text*)
        . = ALIGN(4096); __text_end = .;
    }
    .rodata : AT(ADDR(.rodata) - 0xc0000000) {
        __rodata_start = .; *(.rodata*)
        . = ALIGN(4096); __rodata_end = .;
    }
    .data : AT(ADDR(.data) - 0xc0000000) { *(.data*) }
    .bss (NOLOAD) : AT(ADDR(.bss) - 0xc0000000) {
        . = ALIGN(4096); __bss_start = .; *(.bss*) *(COMMON)
        . = ALIGN(4096); __bss_end = .;
    }
    __kernel_phys_end = . - 0xc0000000;
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
    ASSERT(__kernel_phys_end <= 0x1000000, "Kernel exceeds 16 MiB bootstrap mapping")
}
