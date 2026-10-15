#include "syscall.h"
#include "sched/task.h"
#include "mm/memory.h"
#include "arch/x86/pit.h"
#include "drivers/serial.h"
#include "../libgatha/api.h"
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *f) {
    struct task *t = task_current();
    switch (f->eax) {
    case SYS_TICKS: f->eax = pit_ticks(); break;
    case SYS_PID: f->eax = t->pid; break;
    case SYS_YIELD: f->eax = 0; return schedule(f,1);
    case SYS_SLEEP:
        if (f->ebx > 0x7fffffffu) { f->eax = (uint32_t)-22; break; }
        if (f->ebx) { t->wake = pit_ticks() + f->ebx; t->state = TASK_SLEEPING; }
        f->eax = 0; return schedule(f,1);
    case SYS_EXIT: return task_exit(f,(int)f->ebx);
    case SYS_LOG:
        if (f->ecx > 1024 || !user_range(f->ebx,f->ecx,0)) { f->eax = (uint32_t)-14; break; }
        for (unsigned i = 0; i < f->ecx; ++i) { char s[2] = {((const char *)(uintptr_t)f->ebx)[i],0}; klog(s); }
        f->eax = f->ecx; break;
    case SYS_DEMO: f->eax = (uint32_t)tasks_demo_draw(f->ebx); break;
    default: f->eax = (uint32_t)-38; break;
    }
    return f;
}
