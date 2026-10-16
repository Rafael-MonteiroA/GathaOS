#ifndef GATHA_WM_H
#define GATHA_WM_H
#include "boot.h"
#include "arch/x86/interrupts.h"
void wm_init(const struct boot_info *boot);
void wm_run(void);
int wm_call(struct interrupt_frame *frame);
void wm_release(unsigned pid);
int desktop_launch(unsigned app);
int desktop_open(unsigned app,const char *path);
#endif
