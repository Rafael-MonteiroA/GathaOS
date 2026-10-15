#ifndef GATHA_MEMORY_H
#define GATHA_MEMORY_H
#include <stddef.h>
#include <stdint.h>
#include "boot.h"
#define PAGE_SIZE UINT32_C(4096)
#define KERNEL_BASE UINT32_C(0xc0000000)
#define PHYSICAL_LIMIT UINT32_C(0x20000000)
#define HEAP_BASE UINT32_C(0xe0000000)
#define HEAP_LIMIT UINT32_C(0x01000000)
#define HEAP_MINIMUM UINT32_C(65536)
#define FRAMEBUFFER_BASE UINT32_C(0xe8000000)
static inline void *physical_pointer(uint32_t address) {
    return (void *)(uintptr_t)(KERNEL_BASE + address);
}
struct e820_entry {
    uint64_t base, length;
    uint32_t type, attributes;
} __attribute__((packed));
_Static_assert(sizeof(struct e820_entry) == 24, "E820 layout");
void pmm_init(const struct boot_info *boot);
uint32_t pmm_alloc(void);
int pmm_free(uint32_t physical);
uint32_t pmm_free_frames(void);
uint32_t pmm_total_frames(void);
void paging_init(struct boot_info *boot);
int paging_heap_map(uint32_t virtual_address, uint32_t physical);
uint32_t paging_heap_unmap(uint32_t virtual_address);
uint32_t paging_flags(uint32_t virtual_address);
uint32_t paging_physical(uint32_t virtual_address);
void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *pointer);
int heap_validate(void);
uint32_t heap_mapped_bytes(void);
void memory_self_test(void);
uint32_t paging_kernel_directory(void);
uint32_t paging_user_create(void);
int paging_user_map(uint32_t directory, uint32_t address, uint32_t frame, int writable);
void paging_user_destroy(uint32_t directory);
int user_range(uint32_t address, uint32_t size, int writable);
#endif
