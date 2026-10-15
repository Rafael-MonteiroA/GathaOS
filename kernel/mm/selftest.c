#include "memory.h"
#include "drivers/serial.h"
#include "panic.h"
extern char __text_start[], __rodata_start[];
static void check(int condition, const char *reason) {
    if (!condition) panic(reason, 0);
}
static void fill(void *pointer, uint32_t length, uint8_t seed) {
    uint8_t *bytes = pointer;
    for (uint32_t i = 0; i < length; ++i) bytes[i] = (uint8_t)(seed + i * 17);
}
static void verify(const void *pointer, uint32_t length, uint8_t seed) {
    const uint8_t *bytes = pointer;
    for (uint32_t i = 0; i < length; ++i)
        check(bytes[i] == (uint8_t)(seed + i * 17), "MEMORY CONTENT CORRUPTED");
}
static void paging_test(void) {
    check(!paging_flags(0) && !paging_flags(0x100000) && !paging_flags(KERNEL_BASE), "LOW MAPPING STILL PRESENT");
    check((paging_flags((uintptr_t)__text_start) & 7) == 1 &&
          (paging_flags((uintptr_t)__rodata_start) & 7) == 1, "KERNEL WRITE PROTECTION FAILED");
    check(!paging_flags(HEAP_BASE + HEAP_LIMIT), "HEAP GUARD MAPPED");
    uint32_t page = pmm_alloc();
    check(page != 0, "PAGE TEST ALLOCATION FAILED");
    uint32_t alias = HEAP_BASE + HEAP_LIMIT - PAGE_SIZE;
    check(paging_heap_map(alias, page), "PAGE TEST MAPPING FAILED");
    volatile uint32_t *direct = physical_pointer(page);
    volatile uint32_t *mapped = (void *)(uintptr_t)alias;
    *direct = 0x1234abcd;
    check(*mapped == 0x1234abcd, "PAGE ALIAS READ FAILED");
    *mapped = 0x98765432;
    check(*direct == 0x98765432 && paging_physical(alias) == page, "PAGE ALIAS WRITE FAILED");
    check(!paging_heap_map(alias, page), "DUPLICATE PAGE MAPPING ACCEPTED");
    check(paging_heap_unmap(alias) == page && !paging_flags(alias), "PAGE UNMAP FAILED");
    check(pmm_free(page), "PAGE TEST FREE FAILED");
    klog("MMTEST: paging aliases protections unmap PASS\r\n");
}
static void pmm_exhaustion(void) {
    uint32_t baseline = pmm_free_frames(), head = 0, count = 0, frame;
    while ((frame = pmm_alloc()) != 0) {
        uint32_t *words = physical_pointer(frame);
        words[0] = head;
        words[1] = frame ^ 0x13579bdf;
        words[1023] = frame ^ 0x2468ace0;
        head = frame;
        check(++count <= baseline, "PMM DUPLICATE ALLOCATION");
    }
    check(count == baseline && !pmm_free_frames(), "PMM EXHAUSTION COUNT FAILED");
    check(!kmalloc(128 * 1024) && heap_mapped_bytes() == HEAP_MINIMUM, "HEAP OOM HANDLING FAILED");
    /* One available frame forces a partial heap growth followed by rollback. */
    uint32_t one = head;
    head = *(uint32_t *)physical_pointer(one);
    check(pmm_free(one), "PMM FIRST FREE FAILED");
    check(!kmalloc(128 * 1024) && pmm_free_frames() == 1 && heap_mapped_bytes() == HEAP_MINIMUM,
          "HEAP PARTIAL GROWTH LEAKED");
    uint32_t restored = 1;
    while (head) {
        uint32_t *words = physical_pointer(head);
        uint32_t next = words[0];
        check(words[1] == (head ^ 0x13579bdf) && words[1023] == (head ^ 0x2468ace0), "PMM PAGE CONTENT FAILED");
        check(pmm_free(head), "PMM FREE FAILED");
        head = next;
        check(++restored <= baseline, "PMM FREE LIST CYCLE");
    }
    check(restored == baseline && pmm_free_frames() == baseline && heap_validate(), "PMM RESTORE FAILED");
    check(!pmm_free(0) && !pmm_free(0x100000) && !pmm_free(1) && !pmm_free(PHYSICAL_LIMIT),
          "PMM INVALID FREE ACCEPTED");
    frame = pmm_alloc();
    check(frame && pmm_free(frame) && !pmm_free(frame), "PMM DOUBLE FREE ACCEPTED");
    klog("MMTEST: pmm exhaustion_frames="); klog_u32(count);
    klog(" restored_frames="); klog_u32(pmm_free_frames()); klog(" PASS\r\n");
    klog("MMTEST: heap OOM partial-growth rollback PASS\r\n");
}
static void heap_stress(void) {
    void *slots[128] = {0};
    uint32_t sizes[128] = {0};
    uint32_t random = 0x47415448, baseline = pmm_free_frames();
    check(!kmalloc(0) && !kmalloc(SIZE_MAX), "HEAP SIZE BOUNDS FAILED");
    kfree(0);
    void *a = kmalloc(128), *b = kmalloc(256), *c = kmalloc(128);
    check(a && b && c, "HEAP SPLIT FAILED");
    kfree(b); kfree(a);
    void *merged = kmalloc(384);
    check(merged == a, "HEAP COALESCING FAILED");
    kfree(merged); kfree(c);
    for (unsigned step = 0; step < 4096; ++step) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        unsigned slot = (random >> 24) & 127;
        if (slots[slot]) {
            verify(slots[slot], sizes[slot], (uint8_t)slot);
            kfree(slots[slot]); slots[slot] = 0;
        } else {
            sizes[slot] = (random & 8191) + 1 + (step % 37 == 0 ? 65536 : 0);
            slots[slot] = kmalloc(sizes[slot]);
            check(slots[slot] && !((uintptr_t)slots[slot] & 15), "HEAP ALLOCATION ALIGNMENT FAILED");
            fill(slots[slot], sizes[slot], (uint8_t)slot);
        }
        if (!(step % 64)) {
            check(heap_validate(), "HEAP METADATA CORRUPTED");
            for (unsigned i = 0; i < 128; ++i)
                if (slots[i]) verify(slots[i], sizes[i], (uint8_t)i);
        }
    }
    for (unsigned i = 0; i < 128; ++i) {
        if (slots[i]) verify(slots[i], sizes[i], (uint8_t)i);
        kfree(slots[i]);
    }
    check(heap_validate() && heap_mapped_bytes() == HEAP_MINIMUM && pmm_free_frames() == baseline, "HEAP STRESS LEAKED");
    klog("MMTEST: heap mixed_operations=4096 alignment content coalescing PASS\r\n");
}
static void heap_capacity(void) {
    void *blocks[256];
    unsigned count = 0;
    uint32_t baseline = pmm_free_frames();
    for (; count < 256; ++count) {
        blocks[count] = kmalloc(65536);
        if (!blocks[count]) break;
        fill(blocks[count], 65536, (uint8_t)count);
    }
    check(count == 255 && !kmalloc(65536) && heap_validate(), "HEAP CAPACITY LIMIT FAILED");
    for (unsigned parity = 0; parity < 2; ++parity)
        for (unsigned i = parity; i < count; i += 2) {
            verify(blocks[i], 65536, (uint8_t)i); kfree(blocks[i]);
        }
    check(heap_validate() && heap_mapped_bytes() == HEAP_MINIMUM && pmm_free_frames() == baseline, "HEAP CAPACITY LEAKED");
    klog("MMTEST: heap exhaustion_blocks=255 trim_and_reuse PASS\r\n");
}
void memory_self_test(void) {
    uint32_t baseline = pmm_free_frames();
    paging_test();
    pmm_exhaustion();
    heap_stress();
    heap_capacity();
    check(pmm_free_frames() == baseline, "MEMORY SELF TEST LEAKED");
    klog("MMTEST: final_free_frames="); klog_u32(baseline);
    klog(" heap_bytes="); klog_u32(heap_mapped_bytes()); klog(" PASS\r\n");
    klog("MMTEST: ALL PASS\r\n");
}
