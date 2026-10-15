#include "framebuffer.h"
#include "serial.h"
#include "mm/memory.h"
#include "panic.h"
static struct boot_info mode;
static uint32_t *back;
void framebuffer_init(const struct boot_info *boot) {
    mode = *boot;
    back = kmalloc(mode.width * mode.height * sizeof(*back));
    if (!back) panic("FRAMEBUFFER BUFFER ALLOCATION FAILED", 0);
    klog("GFX: double buffer bytes=");
    klog_u32(mode.width * mode.height * sizeof(*back));
    klog("\r\n");
}
uint32_t *framebuffer_buffer(void) { return back; }
unsigned framebuffer_width(void) { return mode.width; }
unsigned framebuffer_height(void) { return mode.height; }
void framebuffer_present(int x, int y, int width, int height) {
    /* The back buffer always uses RGB; the front may use another VBE layout. */
    for (int row = 0; row < height; ++row) {
        int py = y + row;
        if (py < 0 || py >= (int)mode.height) continue;
        volatile uint32_t *front = (volatile uint32_t *)(uintptr_t)
            (mode.framebuffer + (unsigned)py * mode.pitch);
        for (int col = 0; col < width; ++col) {
            int px = x + col;
            if (px < 0 || px >= (int)mode.width) continue;
            uint32_t rgb = back[(unsigned)py * mode.width + (unsigned)px];
            front[px] = ((rgb >> 16) << mode.red_position) |
                (((rgb >> 8) & 255) << mode.green_position) |
                ((rgb & 255) << mode.blue_position);
        }
    }
}
