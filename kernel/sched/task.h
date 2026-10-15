#ifndef GATHA_TASK_H
#define GATHA_TASK_H
#include "arch/x86/interrupts.h"
#define TASK_MAX 16
enum { TASK_EMPTY, TASK_READY, TASK_SLEEPING, TASK_DEAD };
struct task {
    struct interrupt_frame *frame;
    uint32_t directory, stack_top, wake, pid, switches, cpu_ticks, argument;
    unsigned state, user, draws;
    void *stack;
    void (*function)(void);
    char name[24];
};
void scheduler_init(void);
int task_spawn(const void *image, unsigned size, unsigned argument, const char *name);
int task_kernel(void (*function)(void), const char *name);
struct task *task_current(void);
const struct task *task_table(void);
struct interrupt_frame *schedule(struct interrupt_frame *frame, int force);
struct interrupt_frame *task_exit(struct interrupt_frame *frame, int code);
void task_reap(void);
uint32_t scheduler_preemptions(void);
void tasks_demo_init(void);
int tasks_demo_draw(unsigned frame);
#endif
