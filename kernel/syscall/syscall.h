#ifndef GATHA_SYSCALL_H
#define GATHA_SYSCALL_H
#include "arch/x86/interrupts.h"
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame);
#endif
