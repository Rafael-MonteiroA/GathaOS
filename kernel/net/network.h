#ifndef GATHA_NETWORK_H
#define GATHA_NETWORK_H
#include "arch/x86/interrupts.h"
void network_init(void);
void network_poll(void);
void network_release(unsigned pid);
int network_call(struct interrupt_frame *frame);
#endif
