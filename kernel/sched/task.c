#include "task.h"
#include "../gui/wm/wm.h"
#include "mm/memory.h"
#include "arch/x86/io.h"
#include "arch/x86/pit.h"
#include "drivers/serial.h"
#include "panic.h"
#include "../libgatha/memory.h"
#include "../libgatha/api.h"
static struct task tasks[TASK_MAX];
static unsigned current, active;
static uint32_t next_pid = 1, preemptions;
struct task *task_current(void) { return &tasks[current]; }
const struct task *task_table(void) { return tasks; }
uint32_t scheduler_preemptions(void) { return preemptions; }
void scheduler_init(void) {
    tasks[0].state = TASK_READY; tasks[0].directory = paging_kernel_directory();
    tasks[0].stack_top = 0xc0090000; g_copy(tasks[0].name,"kernel",24);
    active = 1;
    klog("SCHED: round-robin quantum_ticks=2 user_cs=0x1B user_ds=0x23 TSS=ready\r\n");
}
static int reserve(const char *name) {
    for (unsigned i = 1; i < TASK_MAX; ++i) if (!tasks[i].state) {
        struct task *t = &tasks[i]; memset(t,0,sizeof(*t));
        t->stack = kmalloc(16384);
        if (!t->stack) return -1;
        *(uint32_t *)t->stack = 0x5354414b;
        t->stack_top = (uint32_t)(uintptr_t)t->stack + 16384;
        t->pid = next_pid++; g_copy(t->name,name,24);
        return (int)i;
    }
    return -1;
}
static void initial_frame(struct task *t, uint32_t entry, unsigned user) {
    unsigned bytes = sizeof(struct interrupt_frame) + (user ? 8 : 4);
    t->frame = (void *)(uintptr_t)(t->stack_top - bytes);
    memset(t->frame,0,bytes);
    t->frame->ds = t->frame->es = t->frame->fs = t->frame->gs = user ? 0x23 : 0x10;
    t->frame->cs = user ? 0x1b : 8; t->frame->eflags = 0x202; t->frame->eip = entry;
    t->frame->eax = t->argument;
    if (user) { uint32_t *tail = (void *)(t->frame + 1); tail[0] = t->user_stack_top; tail[1] = 0x23; }
    t->user = user; t->state = TASK_READY;
}
static void kernel_entry(void) { task_current()->function(); g_exit(0); }
int task_kernel(void (*function)(void), const char *name) {
    uint32_t flags = irq_save(); int slot = reserve(name);
    if (slot >= 0) {
        tasks[slot].directory = paging_kernel_directory(); tasks[slot].function = function;
        initial_frame(&tasks[slot],(uint32_t)(uintptr_t)kernel_entry,0);
    }
    irq_restore(flags); return slot;
}
int task_spawn(const void *image, unsigned size, unsigned argument, const char *name) {
    if (size < 32) return -1;
    uint32_t h[8]; memcpy(h,image,32);
    const uint8_t *data = (const uint8_t *)image + 32;
    if (h[0] != 0x31584547 || (h[1] != 1 && h[1] != 2) || h[2] != USER_BASE ||
        !h[4] || h[4] % PAGE_SIZE || h[3] >= h[4] || h[4] > h[5] ||
        h[5] != size - 32 || h[5] > h[6] || h[6] > (h[1] == 1 ? 0x300000u : USER_IMAGE_MAX) || h[6] % PAGE_SIZE) return -1;
    uint32_t sum = 2166136261u;
    for (unsigned i = 0; i < h[5]; ++i) sum = (sum ^ data[i]) * 16777619u;
    if (sum != h[7]) return -1;
    uint32_t flags = irq_save(); int slot = reserve(name);
    if (slot < 0) { irq_restore(flags); return -1; }
    struct task *t = &tasks[slot]; t->directory = paging_user_create(); t->argument = argument;
    t->user_stack_top = h[1] == 1 ? 0x800000 : USER_STACK_TOP;
    t->heap_break = USER_HEAP_BASE;
    if (!t->directory) goto failed;
    for (unsigned page = 0; page < h[6] / PAGE_SIZE + 16; ++page) {
        uint32_t offset = page * PAGE_SIZE;
        uint32_t address = offset < h[6] ? USER_BASE + offset : t->user_stack_top - USER_STACK_BYTES + offset - h[6];
        uint32_t physical = pmm_alloc();
        if (!physical) goto failed;
        memset(physical_pointer(physical),0,PAGE_SIZE);
        if (!paging_user_map(t->directory,address,physical,offset >= h[4])) { pmm_free(physical); goto failed; }
        if (offset < h[5]) {
            unsigned bytes = h[5] - offset; if (bytes > PAGE_SIZE) bytes = PAGE_SIZE;
            memcpy(physical_pointer(physical),data + offset,bytes);
        }
    }
    initial_frame(t,h[2]+h[3],1);
    klog("TASK: loaded pid="); klog_u32(t->pid); klog(" name="); klog(t->name);
    klog(" cr3="); klog_hex(t->directory); klog("\r\n");
    irq_restore(flags); return (int)t->pid;
failed:
    paging_user_destroy(t->directory); kfree(t->stack); memset(t,0,sizeof(*t));
    irq_restore(flags); return -1;
}
int32_t task_sbrk(int32_t increment) {
    struct task *t = task_current();
    if (!t->user) return -13;
    uint32_t old = t->heap_break;
    int64_t requested = (int64_t)old + increment;
    if (requested < USER_HEAP_BASE || requested > USER_LIMIT) return -12;
    uint32_t end = (uint32_t)requested;
    uint32_t first = (old + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t last = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t address = first;
    for (; address < last; address += PAGE_SIZE) {
        uint32_t frame = pmm_alloc();
        if (!frame) break;
        memset(physical_pointer(frame), 0, PAGE_SIZE);
        if (!paging_user_map(t->directory, address, frame, 1)) { pmm_free(frame); break; }
    }
    if (address < last) {
        while (address > first) { address -= PAGE_SIZE; paging_user_unmap(t->directory, address); }
        return -12;
    }
    for (address = last; address < first; address += PAGE_SIZE) paging_user_unmap(t->directory, address);
    t->heap_break = end;
    return (int32_t)old;
}
struct interrupt_frame *schedule(struct interrupt_frame *f, int force) {
    if (!active) return f;
    struct task *old = &tasks[current]; old->frame = f;
    uint32_t now = pit_ticks();
    if (f->vector == 32) ++old->cpu_ticks;
    for (unsigned i = 1; i < TASK_MAX; ++i)
        if (tasks[i].state == TASK_SLEEPING && now - tasks[i].wake < 0x80000000u) tasks[i].state = TASK_READY;
    if (!force && now % 2) return f;
    unsigned next = current;
    for (unsigned n = 1; n <= TASK_MAX; ++n) {
        unsigned i = (current+n) % TASK_MAX;
        if (tasks[i].state == TASK_READY) { next = i; break; }
    }
    if (next == current) return f;
    if (old->stack && *(uint32_t *)old->stack != 0x5354414b) panic("TASK KERNEL STACK OVERFLOW",f);
    if (!force) ++preemptions;
    current = next; struct task *t = &tasks[next]; ++t->switches;
    tss_stack(t->stack_top);
    __asm__ volatile("mov %0, %%cr3" : : "r"(t->directory) : "memory");
    return t->frame;
}
struct interrupt_frame *task_exit(struct interrupt_frame *f, int code) {
    if (!current) panic("IDLE TASK EXIT",f);
    klog("TASK: exit pid="); klog_u32(tasks[current].pid); klog(" code="); klog_hex((uint32_t)code); klog("\r\n");
    tasks[current].state = TASK_DEAD;
    return schedule(f,1);
}
void task_reap(void) {
    uint32_t flags = irq_save();
    for (unsigned i = 1; i < TASK_MAX; ++i) if (i != current && tasks[i].state == TASK_DEAD) {
        wm_release(tasks[i].pid);
        vfs_close_all(tasks[i].files);
        paging_user_destroy(tasks[i].directory); kfree(tasks[i].stack); memset(&tasks[i],0,sizeof(tasks[i]));
    }
    irq_restore(flags);
}
