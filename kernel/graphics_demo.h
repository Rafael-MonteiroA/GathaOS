#ifndef GATHA_GRAPHICS_DEMO_H
#define GATHA_GRAPHICS_DEMO_H
#include "boot.h"
void graphics_demo_init(const struct boot_info *boot);
void graphics_demo_run(void) __attribute__((noreturn));
#endif
