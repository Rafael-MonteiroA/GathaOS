#include "memory.h"
#include "arch/x86/io.h"
#include "drivers/serial.h"
#include "panic.h"
#define FRAME_COUNT (PHYSICAL_LIMIT / PAGE_SIZE)
static uint32_t usable[FRAME_COUNT / 32];
static uint32_t busy[FRAME_COUNT / 32];
static uint32_t free_frames, total_frames, next_frame;
extern char __kernel_phys_end[];

static int bit(const uint32_t *map, uint32_t frame) {
    return (map[frame / 32] >> (frame % 32)) & 1;
}
static void set(uint32_t *map, uint32_t frame, int value) {
    uint32_t mask = UINT32_C(1) << (frame % 32);
    if (value) map[frame / 32] |= mask;
    else map[frame / 32] &= ~mask;
}
static void mark_range(uint64_t base, uint64_t length, int available) {
    if (!length || base >= PHYSICAL_LIMIT) return;
    uint64_t end = base + length;
    if (end < base || end > PHYSICAL_LIMIT) end = PHYSICAL_LIMIT;
    /* Usable pages must fit wholly; reservations cover even partial pages. */
    uint32_t first = ((uint32_t)base + (available ? PAGE_SIZE - 1 : 0)) / PAGE_SIZE;
    uint32_t last = ((uint32_t)end + (available ? 0 : PAGE_SIZE - 1)) / PAGE_SIZE;
    for (uint32_t frame = first; frame < last; ++frame) {
        set(usable, frame, available);
        set(busy, frame, !available);
    }
}
static void log_u64(uint64_t value) {
    klog_hex((uint32_t)(value >> 32)); klog(":"); klog_hex((uint32_t)value);
}
static void clear_maps(void) {
    for (unsigned i = 0; i < FRAME_COUNT / 32; ++i) {
        usable[i] = 0;
        busy[i] = UINT32_MAX;
    }
}
static void range_self_test(void) {
    clear_maps();
    mark_range(0x1003, 0x2ffd, 1);
    if (bit(usable, 1) || !bit(usable, 2) || !bit(usable, 3) || bit(usable, 4))
        panic("E820 USABLE ROUNDING FAILED", 0);
    mark_range(0x2fff, 2, 0);
    if (bit(usable, 2) || bit(usable, 3)) panic("E820 RESERVATION ROUNDING FAILED", 0);
    mark_range(PHYSICAL_LIMIT - PAGE_SIZE, UINT64_MAX, 1);
    mark_range(UINT64_C(0x100000000), PAGE_SIZE, 1);
    if (!bit(usable, FRAME_COUNT - 1) || bit(usable, 0)) panic("E820 RANGE CLIPPING FAILED", 0);
    mark_range(PHYSICAL_LIMIT - 1, 1, 0);
    if (bit(usable, FRAME_COUNT - 1)) panic("E820 LIMIT RESERVATION FAILED", 0);
    klog("MMTEST: E820 rounding reservations overflow clipping PASS\r\n");
}
void pmm_init(const struct boot_info *boot) {
    const struct e820_entry *map = physical_pointer(boot->e820_address);
    range_self_test();
    clear_maps();
    klog("MEMMAP: E820 base_hi:lo length_hi:lo type attributes\r\n");
    for (unsigned i = 0; i < boot->e820_count; ++i) {
        klog("MEMMAP: "); log_u64(map[i].base); klog(" "); log_u64(map[i].length);
        klog(" type="); klog_u32(map[i].type);
        klog(" attributes="); klog_hex(map[i].attributes); klog("\r\n");
        if (map[i].type == 1 && (map[i].attributes & 1))
            mark_range(map[i].base, map[i].length, 1);
    }
    /* Reserved/disabled entries win overlaps regardless of firmware order. */
    for (unsigned i = 0; i < boot->e820_count; ++i)
        if (map[i].type != 1 || !(map[i].attributes & 1))
            mark_range(map[i].base, map[i].length, 0);
    mark_range(0, (uint32_t)(uintptr_t)__kernel_phys_end, 0);
    mark_range(boot->framebuffer, boot->pitch * boot->height, 0);
    free_frames = 0;
    for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame)
        if (bit(usable, frame)) ++free_frames;
    total_frames = free_frames;
    next_frame = 0;
    if (!free_frames) panic("NO USABLE PHYSICAL MEMORY", 0);
    klog("PMM: managed_limit_mib=512 usable_frames="); klog_u32(total_frames);
    klog(" free_frames="); klog_u32(free_frames);
    klog(" reserved_end="); klog_hex((uint32_t)(uintptr_t)__kernel_phys_end); klog("\r\n");
}
uint32_t pmm_alloc(void) {
    uint32_t flags = irq_save();
    if (!free_frames) { irq_restore(flags); return 0; }
    for (uint32_t count = 0; count < FRAME_COUNT; ++count) {
        uint32_t frame = next_frame++;
        if (next_frame == FRAME_COUNT) next_frame = 0;
        if (bit(usable, frame) && !bit(busy, frame)) {
            set(busy, frame, 1); --free_frames;
            irq_restore(flags);
            return frame * PAGE_SIZE;
        }
    }
    panic("PMM BITMAP INCONSISTENT", 0);
}
int pmm_free(uint32_t physical) {
    if (physical % PAGE_SIZE || physical >= PHYSICAL_LIMIT) return 0;
    uint32_t frame = physical / PAGE_SIZE;
    uint32_t flags = irq_save();
    if (!bit(usable, frame) || !bit(busy, frame)) { irq_restore(flags); return 0; }
    set(busy, frame, 0); ++free_frames;
    if (frame < next_frame) next_frame = frame;
    irq_restore(flags);
    return 1;
}
uint32_t pmm_free_frames(void) { return free_frames; }
uint32_t pmm_total_frames(void) { return total_frames; }
