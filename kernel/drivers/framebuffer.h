#ifndef GATHA_FRAMEBUFFER_H
#define GATHA_FRAMEBUFFER_H
#include "boot.h"
void framebuffer_init(const struct boot_info *boot);
uint32_t *framebuffer_buffer(void);
unsigned framebuffer_width(void);
unsigned framebuffer_height(void);
void framebuffer_present(int x, int y, int width, int height);
#endif
