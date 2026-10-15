#include "boot.h"
#include "drivers/serial.h"
#include "arch/x86/interrupts.h"
#include "arch/x86/io.h"
#include "arch/x86/pic.h"
#include "arch/x86/pit.h"
#include "panic.h"
static uint32_t pixel_color(const struct boot_info *b, uint32_t rgb) {
    return (((rgb >> 16) & 255) << b->red_position) |
           (((rgb >> 8) & 255) << b->green_position) |
           ((rgb & 255) << b->blue_position);
}
static void draw_pattern(const struct boot_info *b) {
    static const uint32_t bars[6] = {
        0xef4444, 0xf59e0b, 0x22c55e, 0x06b6d4, 0x3b82f6, 0xa855f7
    };
    for (uint32_t y = 0; y < b->height; ++y) {
        volatile uint32_t *row = (volatile uint32_t *)(uintptr_t)
            (b->framebuffer + y * b->pitch);
        for (uint32_t x = 0; x < b->width; ++x) {
            uint32_t rgb = 0x142238;
            if (x < 8 || y < 8 || x >= b->width - 8 || y >= b->height - 8)
                rgb = 0xe2e8f0;
            else if (y >= b->height / 4 && y < b->height / 2 &&
                     x >= b->width / 8 && x < b->width * 7 / 8)
                rgb = bars[(x - b->width / 8) * 6 / (b->width * 3 / 4)];
            else if (y >= b->height * 5 / 8 && y < b->height * 7 / 8 &&
                     x >= b->width / 8 && x < b->width * 7 / 8)
                rgb = ((x / 32 + y / 32) & 1) ? 0xffffff : 0x334155;
            row[x] = pixel_color(b, rgb);
        }
    }
}
void kernel_main(const struct boot_info *b) {
    serial_init();
    klog("GathaOS booting...\r\n");
    if (b->magic != 0x47415448 || b->version != 1 || !b->e820_count ||
        b->e820_count > 64 || b->e820_address != 0x5000 ||
        b->bpp != 32 || !b->framebuffer ||
        !((b->width == 1024 && b->height == 768) ||
          (b->width == 800 && b->height == 600)) ||
        b->pitch < b->width * 4 || b->pitch > 16384 ||
        b->framebuffer > UINT32_MAX - b->height * b->pitch ||
        b->red_size != 8 || b->green_size != 8 || b->blue_size != 8 ||
        b->red_position > 24 || b->green_position > 24 || b->blue_position > 24 ||
        b->red_position % 8 || b->green_position % 8 || b->blue_position % 8 ||
        b->red_position == b->green_position ||
        b->red_position == b->blue_position || b->green_position == b->blue_position) {
        panic("INVALID BOOT PROTOCOL", 0);
    }
    klog("BOOT: protected-mode A20=on\r\nE820: entries=");
    klog_u32(b->e820_count);
    klog("\r\nVBE: ");
    klog_u32(b->width);
    klog("x");
    klog_u32(b->height);
    klog("x32 pitch=");
    klog_u32(b->pitch);
    klog("\r\n");
    panic_init(b);
    draw_pattern(b);
    gdt_init();
    idt_init();
    pic_init();
    pit_init();
    if (!interrupt_probe() || pit_ticks() == 0)
        panic("IRQ REGISTER RESTORE FAILED", 0);
    klog("IRQ: register-and-flags preservation PASS\r\n");
    klog("STAGE1: TIMER RUNNING\r\n");
    uint32_t next_report = 100;
    for (;;) {
        /* Test the counter with IRQs disabled, then atomically sleep with STI/HLT. */
        __asm__ volatile("cli" : : : "memory");
        uint32_t ticks = pit_ticks();
        if (ticks - next_report < UINT32_C(0x80000000)) {
            next_report += 100;
            __asm__ volatile("sti" : : : "memory");
            klog("PIT: ticks=");
            klog_u32(ticks);
            klog("\r\n");
#if DEBUG_FAULT == 1
            if (ticks >= 200) {
                klog("DEBUG: deliberate divide-by-zero\r\n");
                debug_trigger_divide();
            }
#elif DEBUG_FAULT == 2
            if (ticks >= 200) {
                klog("DEBUG: deliberate invalid data selector\r\n");
                debug_trigger_gp();
            }
#endif
            if (ticks == 300)
                klog("STAGE1: PASS\r\n");
        } else {
            __asm__ volatile("sti; hlt" : : : "memory");
        }
    }
}
