#include "memory.h"
#include "drivers/serial.h"
#include "panic.h"
#define PRESENT 1u
#define WRITE 2u
#define UNCACHED 0x18u
static uint32_t directory[1024] __attribute__((aligned(4096)));
static uint32_t direct_tables[128][1024] __attribute__((aligned(4096)));
static uint32_t heap_tables[4][1024] __attribute__((aligned(4096)));
static uint32_t framebuffer_tables[4][1024] __attribute__((aligned(4096)));
extern char __text_start[], __text_end[], __rodata_start[], __rodata_end[];
static uint32_t physical(const void *pointer) {
    return (uint32_t)(uintptr_t)pointer - KERNEL_BASE;
}
static uint32_t *entry(uint32_t address) {
    uint32_t pde = directory[address >> 22];
    if (!(pde & PRESENT)) return 0;
    uint32_t *table = physical_pointer(pde & 0xfffff000);
    return &table[(address >> 12) & 1023];
}
void paging_init(struct boot_info *boot) {
    for (unsigned i = 0; i < 128; ++i) {
        directory[768 + i] = physical(direct_tables[i]) | PRESENT | WRITE;
        for (unsigned j = 0; j < 1024; ++j) {
            uint32_t address = (i * 1024 + j) * PAGE_SIZE;
            uint32_t virtual_address = KERNEL_BASE + address;
            int readonly = (virtual_address >= (uintptr_t)__text_start && virtual_address < (uintptr_t)__text_end) ||
                           (virtual_address >= (uintptr_t)__rodata_start && virtual_address < (uintptr_t)__rodata_end);
            direct_tables[i][j] = address ? address | PRESENT | (readonly ? 0 : WRITE) : 0;
        }
    }
    for (unsigned i = 0; i < 4; ++i) {
        directory[(HEAP_BASE >> 22) + i] = physical(heap_tables[i]) | PRESENT | WRITE;
        directory[(FRAMEBUFFER_BASE >> 22) + i] = physical(framebuffer_tables[i]) | PRESENT | WRITE;
    }
    uint32_t offset = boot->framebuffer & (PAGE_SIZE - 1);
    uint32_t first = boot->framebuffer & ~(PAGE_SIZE - 1);
    uint32_t bytes = offset + boot->pitch * boot->height;
    if (bytes > 0x01000000) panic("FRAMEBUFFER MAPPING TOO LARGE", 0);
    for (uint32_t page = 0; page < (bytes + PAGE_SIZE - 1) / PAGE_SIZE; ++page)
        framebuffer_tables[page / 1024][page % 1024] = (first + page * PAGE_SIZE) | PRESENT | WRITE | UNCACHED;
    uint32_t cr3 = physical(directory);
    /* No low PDE is installed. Code, stack, GDT and IDT already have high addresses. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    boot->framebuffer = FRAMEBUFFER_BASE + offset;
    klog("PAGING: higher_half=0xC0000000 identity=off null=unmapped wp=on\r\n");
    klog("PAGING: cr3="); klog_hex(cr3);
    klog(" framebuffer="); klog_hex(boot->framebuffer); klog("\r\n");
}
int paging_heap_map(uint32_t address, uint32_t frame) {
    if (address < HEAP_BASE || address >= HEAP_BASE + HEAP_LIMIT || address % PAGE_SIZE ||
        !frame || frame >= PHYSICAL_LIMIT || frame % PAGE_SIZE) return 0;
    uint32_t *pte = entry(address);
    if (!pte || (*pte & PRESENT)) return 0;
    *pte = frame | PRESENT | WRITE;
    __asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
    return 1;
}
uint32_t paging_heap_unmap(uint32_t address) {
    if (address < HEAP_BASE || address >= HEAP_BASE + HEAP_LIMIT || address % PAGE_SIZE) return 0;
    uint32_t *pte = entry(address);
    if (!pte || !(*pte & PRESENT)) return 0;
    uint32_t frame = *pte & 0xfffff000;
    *pte = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
    return frame;
}
uint32_t paging_flags(uint32_t address) {
    uint32_t *pte = entry(address);
    return pte ? *pte & 0xfff : 0;
}
uint32_t paging_physical(uint32_t address) {
    uint32_t *pte = entry(address);
    return pte && (*pte & PRESENT) ? (*pte & 0xfffff000) + (address & 0xfff) : UINT32_MAX;
}
