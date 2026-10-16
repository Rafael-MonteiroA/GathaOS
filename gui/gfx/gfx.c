#include "gfx.h"
#include "font.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "panic.h"
static uint32_t *target;
static unsigned tw,th;
static int left,top,right=32767,bottom=32767;
static unsigned surface_width(void) { return target?tw:framebuffer_width(); }
static unsigned surface_height(void) { return target?th:framebuffer_height(); }
static uint32_t *surface_buffer(void) { return target?target:framebuffer_buffer(); }
void gfx_clip(int x,int y,int w,int h) { left=x; top=y; right=x+w; bottom=y+h; }
void gfx_target(uint32_t *p,unsigned w,unsigned h) { target=p; tw=w; th=h; gfx_clip(0,0,(int)surface_width(),(int)surface_height()); }
void gfx_pixel(int x, int y, uint32_t rgb) {
    if (x>=left && y>=top && x<right && y<bottom && (unsigned)x < surface_width() && (unsigned)y < surface_height())
        surface_buffer()[(unsigned)y * surface_width() + (unsigned)x] = rgb & 0xffffff;
}
uint32_t gfx_blend(uint32_t bg, uint32_t fg, unsigned alpha) {
    if (alpha > 255) alpha = 255;
    uint32_t result = 0;
    for (unsigned shift = 0; shift <= 16; shift += 8)
        result |= ((((bg >> shift) & 255) * (255 - alpha) +
                    ((fg >> shift) & 255) * alpha + 127) / 255) << shift;
    return result;
}
void gfx_line(int x0, int y0, int x1, int y1, uint32_t rgb) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        gfx_pixel(x0, y0, rgb);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}
void gfx_rect(int x, int y, int w, int h, uint32_t rgb) {
    int edge = x + w, end = y + h;
    if (edge>right) edge=right;
    if (end>bottom) end=bottom;
    if (x<left) x=left;
    if (y<top) y=top;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (edge > (int)surface_width()) edge = (int)surface_width();
    if (end > (int)surface_height()) end = (int)surface_height();
    for (int py = y; py < end; ++py)
        for (int px = x; px < edge; ++px) surface_buffer()[(unsigned)py*surface_width()+(unsigned)px]=rgb&0xffffff;
}
void gfx_round_rect(int x, int y, int w, int h, int radius, uint32_t rgb) {
    if (w <= 0 || h <= 0) return;
    if (radius < 0) radius = 0;
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;
    for (int py = 0; py < h; ++py) {
        for (int px = 0; px < w; ++px) {
            int dx = px < radius ? radius - 1 - px : (px >= w - radius ? px - (w - radius) : 0);
            int dy = py < radius ? radius - 1 - py : (py >= h - radius ? py - (h - radius) : 0);
            if (dx * dx + dy * dy <= radius * radius) gfx_pixel(x + px, y + py, rgb);
        }
    }
}
void gfx_gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int row = 0; row < h; ++row)
        gfx_rect(x, y + row, w, 1, gfx_blend(top, bottom, h > 1 ? (unsigned)row * 255 / (unsigned)(h - 1) : 0));
}
void gfx_alpha_rect(int x, int y, int w, int h, uint32_t rgb, unsigned alpha) {
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            int px = x + col, py = y + row;
            if ((unsigned)px < surface_width() && (unsigned)py < surface_height()) {
                uint32_t bg = surface_buffer()[(unsigned)py * surface_width() + (unsigned)px];
                gfx_pixel(px, py, gfx_blend(bg, rgb, alpha));
            }
        }
}
void gfx_text(int x, int y, const char *text, uint32_t rgb, unsigned scale) {
    if (scale < 1 || scale > 4) return;
    int origin = x;
    while (*text) {
        unsigned c = (unsigned char)*text++;
        if (c == '\n') { x = origin; y += (int)(16 * scale); continue; }
        if (c < 32 || c > 126) c = '?';
        for (unsigned row = 0; row < 16; ++row)
            for (unsigned col = 0; col < 8; ++col)
                if (gatha_font[c - 32][row] & (0x80u >> col))
                    gfx_rect(x + (int)(col * scale), y + (int)(row * scale), (int)scale, (int)scale, rgb);
        x += (int)(8 * scale);
    }
}
void gfx_self_test(void) {
    uint32_t *p = framebuffer_buffer();
    gfx_rect(0, 0, (int)framebuffer_width(), (int)framebuffer_height(), 0);
    gfx_rect(-2, -2, 4, 4, 0x123456);
    gfx_pixel(-1, 0, 0xffffff);
    gfx_pixel((int)framebuffer_width(), 0, 0xffffff);
    if (p[0] != 0x123456 || p[1] != 0x123456 || p[2] ||
        p[framebuffer_width() - 1] || gfx_blend(0, 0xffffff, 128) != 0x808080 ||
        gfx_blend(0x123456, 0xabcdef, 0) != 0x123456 ||
        gfx_blend(0x123456, 0xabcdef, 255) != 0xabcdef)
        panic("GRAPHICS CLIPPING OR ALPHA FAILED", 0);
    gfx_line(10, 10, 14, 14, 0xffffff);
    for (unsigned i = 10; i <= 14; ++i)
        if (p[i * framebuffer_width() + i] != 0xffffff) panic("GRAPHICS LINE FAILED", 0);
    klog("GFXTEST: clipping alpha endpoints diagonal PASS\r\n");
}
