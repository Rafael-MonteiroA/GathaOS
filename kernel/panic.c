#include "panic.h"
#include "panic_font.h"
#include "drivers/serial.h"
#include "arch/x86/io.h"
static const struct boot_info *screen;
static unsigned line_number;
static int active;
void panic_init(const struct boot_info *boot) { screen = boot; }
static _Noreturn void halt(void) {
    for (;;) { __asm__ volatile("cli; hlt"); }
}
static void pixel(unsigned x, unsigned y, uint32_t rgb) {
    if (x >= screen->width || y >= screen->height)
        return;
    volatile uint32_t *address = (volatile uint32_t *)(uintptr_t)
        (screen->framebuffer + y * screen->pitch + x * 4);
    *address = (((rgb >> 16) & 255) << screen->red_position) |
               (((rgb >> 8) & 255) << screen->green_position) |
               ((rgb & 255) << screen->blue_position);
}
static void clear_screen(void) {
    if (!screen)
        return;
    for (unsigned y = 0; y < screen->height; ++y)
        for (unsigned x = 0; x < screen->width; ++x)
            pixel(x, y, y < 64 ? 0x991b1b : 0x200e16);
}
static void draw_line(const char *text) {
    if (!screen)
        return;
    unsigned x = 32;
    unsigned y = line_number ? 84 + (line_number - 1) * 32 : 24;
    for (; *text; ++text, x += 12) {
        unsigned char character = (unsigned char)*text;
        if (character >= 'a' && character <= 'z')
            character = (unsigned char)(character - 'a' + 'A');
        if (character >= 128)
            character = '?';
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned column = 0; column < 5; ++column)
                if (panic_font[character][row] & (1u << (4 - column)))
                    for (unsigned dy = 0; dy < 2; ++dy)
                        for (unsigned dx = 0; dx < 2; ++dx)
                            pixel(x + column * 2 + dx, y + row * 2 + dy, 0xf8fafc);
    }
}
static void line(const char *text) {
    klog(text);
    klog("\r\n");
    draw_line(text);
    ++line_number;
}
static unsigned append(char *buffer, unsigned index, const char *text) {
    while (*text && index < 79)
        buffer[index++] = *text++;
    buffer[index] = '\0';
    return index;
}
static unsigned field(char *buffer, unsigned index, const char *name, uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char hex[11] = "0x00000000";
    for (unsigned i = 0; i < 8; ++i)
        hex[2 + i] = digits[(value >> (28 - 4 * i)) & 15];
    index = append(buffer, index, name);
    index = append(buffer, index, "=");
    return append(buffer, index, hex);
}
static void registers(const char *a, uint32_t av, const char *b, uint32_t bv,
                      const char *c, uint32_t cv) {
    char buffer[80];
    unsigned n = field(buffer, 0, a, av);
    n = append(buffer, n, "  ");
    n = field(buffer, n, b, bv);
    if (c) {
        n = append(buffer, n, "  ");
        field(buffer, n, c, cv);
    }
    line(buffer);
}
_Noreturn void panic(const char *reason, const struct interrupt_frame *frame) {
    (void)irq_save();
    if (active) {
        klog("PANIC: NESTED FAULT\r\n");
        halt();
    }
    active = 1;
    clear_screen();
    line("GATHAOS KERNEL PANIC");
    char title[80];
    append(title, append(title, 0, "PANIC: "), reason);
    line(title);
    if (frame) {
        uint32_t ss, cr0, cr2, cr3;
        __asm__ volatile("xor %0, %0; movw %%ss, %w0" : "=&r"(ss) : : "cc");
        __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        registers("VECTOR", frame->vector, "ERROR", frame->error, 0, 0);
        registers("EIP", frame->eip, "CS", frame->cs, "EFLAGS", frame->eflags);
        registers("EAX", frame->eax, "EBX", frame->ebx, "ECX", frame->ecx);
        registers("EDX", frame->edx, "ESI", frame->esi, "EDI", frame->edi);
        registers("EBP", frame->ebp, "ESP", frame->saved_esp + 20, "SS", ss);
        registers("DS", frame->ds, "ES", frame->es, 0, 0);
        registers("FS", frame->fs, "GS", frame->gs, 0, 0);
        registers("CR0", cr0, "CR2", cr2, "CR3", cr3);
    }
    line("CPU HALTED - RESTART THE VM");
    klog("PANIC: HALTED\r\n");
    halt();
}
