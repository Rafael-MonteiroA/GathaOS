#include "interrupts.h"
#include "drivers/serial.h"
struct table_pointer { uint16_t limit; uint32_t base; } __attribute__((packed));
static const uint64_t gdt[] __attribute__((aligned(8))) = {
    0, UINT64_C(0x00cf9a000000ffff), UINT64_C(0x00cf92000000ffff)
};
extern void gdt_load(const struct table_pointer *pointer);
void gdt_init(void) {
    const struct table_pointer pointer = {sizeof(gdt) - 1, (uint32_t)(uintptr_t)gdt};
    gdt_load(&pointer);
    klog("GDT: kernel-owned CS=0x08 DS=0x10\r\n");
}
