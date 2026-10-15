#include "task.h"
#include "arch/x86/pit.h"
#include "arch/x86/io.h"
#include "drivers/serial.h"
#include "drivers/framebuffer.h"
#include "panic.h"
#include "../gui/gfx/gfx.h"
#include "../libgatha/api.h"
extern const uint8_t demo_gex[];
extern const unsigned demo_gex_size;
static volatile uint32_t frames[2], dirty, worker_ticks;
static void worker(void) {
    klog("THREAD: kernel worker running\r\n");
    for (;;) { ++worker_ticks; g_sleep(5); }
}
void tasks_demo_init(void) {
    scheduler_init();
    if (task_spawn(demo_gex,demo_gex_size,1,"animation-a") < 0 ||
        task_spawn(demo_gex,demo_gex_size,2,"animation-b") < 0 || task_kernel(worker,"worker") < 0)
        panic("TASK START FAILED",0);
    for (unsigned i = 10; i <= 13; ++i)
        if (task_spawn(demo_gex,demo_gex_size,i,"isolation-test") < 0) panic("TEST TASK START FAILED",0);
    if (task_spawn(demo_gex,16,0,"invalid") >= 0) panic("INVALID GEX ACCEPTED",0);
    klog("GEXTEST: truncated image rejected PASS\r\nSTAGE4: READY\r\n");
}
int tasks_demo_draw(unsigned frame) {
    struct task *t = task_current();
    if (t->argument < 1 || t->argument > 2) return -1;
    frames[t->argument - 1] = frame; ++t->draws; dirty = 1;
    return 0;
}
int tasks_demo_dirty(void) { return (int)dirty; }
void tasks_demo_paint(void) {
    int card = ((int)framebuffer_width() - 96) / 3;
    for (unsigned i = 0; i < 2; ++i) {
        int x = 48 + (int)i * (card + 16);
        gfx_rect(x,176,card-32,80,0x18283d);
        gfx_text(x,176,i ? "USER B / CPU BOUND" : "USER A / SLEEP",0x94a3b8,1);
        gfx_rect(x,211,card-32,44,0x101e30);
        unsigned offset = frames[i] % (unsigned)(card-60);
        gfx_round_rect(x+(int)offset,218,28,28,6,i ? 0xa78bfa : 0x5eead4);
    }
    dirty = 0;
}
void tasks_demo_report(void) {
    static unsigned passed;
    task_reap();
    if (!passed && pit_ticks() >= 400 && frames[0] > 10 && frames[1] > 10 &&
        scheduler_preemptions() > 10 && worker_ticks > 10) {
        klog("SCHEDTEST: user animations sleep yield kernel-thread preemption PASS\r\nSTAGE4: PASS\r\n");
        passed = 1;
    }
    klog("SCHED: frames_a="); klog_u32(frames[0]); klog(" frames_b="); klog_u32(frames[1]);
    klog(" preemptions="); klog_u32(scheduler_preemptions()); klog(" worker="); klog_u32(worker_ticks); klog("\r\n");
}
