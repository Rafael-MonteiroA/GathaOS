#ifndef GATHA_GFX_H
#define GATHA_GFX_H
#include <stdint.h>
/* Coordinates and dimensions are signed screen-space values, limited to +/-32767. */
void gfx_target(uint32_t *pixels, unsigned width, unsigned height);
void gfx_clip(int x,int y,int w,int h);
void gfx_pixel(int x, int y, uint32_t rgb);
uint32_t gfx_blend(uint32_t background, uint32_t foreground, unsigned alpha);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t rgb);
void gfx_rect(int x, int y, int w, int h, uint32_t rgb);
void gfx_round_rect(int x, int y, int w, int h, int radius, uint32_t rgb);
void gfx_gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void gfx_alpha_rect(int x, int y, int w, int h, uint32_t rgb, unsigned alpha);
void gfx_text(int x, int y, const char *text, uint32_t rgb, unsigned scale);
void gfx_self_test(void);
#endif
