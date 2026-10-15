#ifndef GATHA_PANIC_H
#define GATHA_PANIC_H
#include "boot.h"
#include "arch/x86/interrupts.h"
void panic_init(const struct boot_info *boot);
_Noreturn void panic(const char *reason, const struct interrupt_frame *frame);
#endif
