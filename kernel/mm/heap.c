#include "memory.h"
#include "arch/x86/io.h"
#include "drivers/serial.h"
#include "panic.h"
#define BLOCK_MAGIC UINT32_C(0x47484541)
struct block {
    uint32_t size, magic;
    struct block *previous, *next;
    uint32_t free, padding[3];
};
_Static_assert(sizeof(struct block) == 32, "Heap header alignment");
static struct block *first, *last;
static uint32_t mapped;
static uintptr_t address(const void *pointer) { return (uintptr_t)pointer; }
static void release_page(uint32_t virtual_address) {
    uint32_t frame = paging_heap_unmap(virtual_address);
    if (!frame || !pmm_free(frame)) panic("HEAP PAGE RELEASE FAILED", 0);
}
static int grow(uint32_t needed) {
    uint32_t extra = (needed + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (extra < HEAP_MINIMUM) extra = HEAP_MINIMUM;
    if (extra > HEAP_LIMIT - mapped) extra = HEAP_LIMIT - mapped;
    if (extra < needed) return 0;
    uint32_t added = 0;
    while (added < extra) {
        uint32_t frame = pmm_alloc();
        if (!frame) break;
        if (!paging_heap_map(HEAP_BASE + mapped + added, frame)) {
            (void)pmm_free(frame);
            panic("HEAP MAPPING COLLISION", 0);
        }
        added += PAGE_SIZE;
    }
    if (added != extra) {
        while (added) { added -= PAGE_SIZE; release_page(HEAP_BASE + mapped + added); }
        return 0;
    }
    if (last && last->free) {
        last->size += extra;
    } else {
        struct block *block = (void *)(uintptr_t)(HEAP_BASE + mapped);
        block->size = extra - sizeof(*block);
        block->magic = BLOCK_MAGIC;
        block->previous = last;
        block->next = 0;
        block->free = 1;
        if (last) last->next = block;
        else first = block;
        last = block;
    }
    mapped += extra;
    return 1;
}
void heap_init(void) {
    if (first || !grow(HEAP_MINIMUM)) panic("HEAP INITIALIZATION FAILED", 0);
    klog("HEAP: base=0xE0000000 limit_mib=16 alignment=16 initial_bytes=65536\r\n");
}
void *kmalloc(size_t size) {
    if (!size || size > HEAP_LIMIT - sizeof(struct block)) return 0;
    uint32_t wanted = ((uint32_t)size + 15) & ~15u;
    uint32_t flags = irq_save();
    struct block *block = first;
    for (; block; block = block->next)
        if (block->free && block->size >= wanted) break;
    if (!block) {
        uint32_t needed = last && last->free ? wanted - last->size : wanted + sizeof(struct block);
        if (!grow(needed)) { irq_restore(flags); return 0; }
        block = last;
    }
    if (block->size >= wanted + sizeof(*block) + 16) {
        struct block *tail = (void *)(address(block) + sizeof(*block) + wanted);
        tail->magic = BLOCK_MAGIC;
        tail->size = block->size - wanted - sizeof(*block);
        tail->free = 1;
        tail->previous = block;
        tail->next = block->next;
        if (tail->next) tail->next->previous = tail;
        else last = tail;
        block->next = tail;
        block->size = wanted;
    }
    block->free = 0;
    irq_restore(flags);
    return (void *)(address(block) + sizeof(*block));
}
static void merge_next(struct block *block) {
    struct block *next = block->next;
    block->size += sizeof(*next) + next->size;
    block->next = next->next;
    if (block->next) block->next->previous = block;
    else last = block;
    next->magic = 0;
}
static void trim(void) {
    if (!last || !last->free) return;
    uint32_t keep = (address(last) + sizeof(*last) + 16 - HEAP_BASE + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (keep < HEAP_MINIMUM) keep = HEAP_MINIMUM;
    if (keep >= mapped) return;
    last->size -= mapped - keep;
    while (mapped > keep) { mapped -= PAGE_SIZE; release_page(HEAP_BASE + mapped); }
}
void kfree(void *pointer) {
    if (!pointer) return;
    uint32_t flags = irq_save();
    struct block *block = first;
    for (; block; block = block->next)
        if (address(block) + sizeof(*block) == address(pointer)) break;
    if (!block || block->magic != BLOCK_MAGIC || block->free)
        panic("INVALID HEAP FREE", 0);
    block->free = 1;
    if (block->next && block->next->free) merge_next(block);
    if (block->previous && block->previous->free) {
        block = block->previous;
        merge_next(block);
    }
    trim();
    irq_restore(flags);
}
int heap_validate(void) {
    uint32_t flags = irq_save();
    uintptr_t cursor = HEAP_BASE, end = HEAP_BASE + mapped;
    struct block *previous = 0, *block = first;
    int valid = first && mapped >= HEAP_MINIMUM && mapped <= HEAP_LIMIT && !(mapped % PAGE_SIZE);
    while (valid && block) {
        if (address(block) != cursor || cursor > end - sizeof(*block)) { valid = 0; break; }
        valid = block->magic == BLOCK_MAGIC && block->previous == previous && block->free <= 1 &&
                block->size >= 16 && !(block->size % 16) && block->size <= end - cursor - sizeof(*block);
        if (!valid) break;
        if (previous && previous->free && block->free) { valid = 0; break; }
        cursor += sizeof(*block) + block->size;
        previous = block;
        block = block->next;
    }
    valid = valid && cursor == end && previous == last;
    irq_restore(flags);
    return valid;
}
uint32_t heap_mapped_bytes(void) { return mapped; }
