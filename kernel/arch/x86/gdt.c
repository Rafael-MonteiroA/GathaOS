#include "interrupts.h"
#include "drivers/serial.h"
struct table_pointer { uint16_t limit; uint32_t base; } __attribute__((packed));
static uint32_t tss[26];
static uint64_t gdt[] __attribute__((aligned(8))) = {
    0, UINT64_C(0x00cf9a000000ffff), UINT64_C(0x00cf92000000ffff),
    UINT64_C(0x00cffa000000ffff), UINT64_C(0x00cff2000000ffff), 0
};
extern void gdt_load(const struct table_pointer *pointer);
void gdt_init(void) {
    uint32_t base = (uint32_t)(uintptr_t)tss;
    gdt[5] = UINT64_C(103) | ((uint64_t)(base & 0xffffff) << 16) |
             (UINT64_C(0x89) << 40) | ((uint64_t)(base >> 24) << 56);
    tss[2] = 0x10;
    tss[25] = sizeof(tss) << 16; /* I/O bitmap beyond limit denies ring-3 port I/O. */
    const struct table_pointer pointer = {sizeof(gdt) - 1, (uint32_t)(uintptr_t)gdt};
    gdt_load(&pointer);
    __asm__ volatile("ltr %w0" : : "r"((uint16_t)0x28));
    klog("GDT: kernel-owned CS=0x08 DS=0x10\r\n");
}
void tss_stack(uint32_t top) { tss[1] = top; }
