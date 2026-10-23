#ifndef GATHA_SECURITY_H
#define GATHA_SECURITY_H
#include "arch/x86/interrupts.h"
int security_call(struct interrupt_frame *f);
#endif
