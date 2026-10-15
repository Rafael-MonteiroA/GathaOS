#include "serial.h"
#include "arch/x86/io.h"
void serial_init(void) {
    outb(0x3f9, 0);
    outb(0x3fb, 0x80);
    outb(0x3f8, 1);
    outb(0x3f9, 0);
    outb(0x3fb, 3);
    outb(0x3fa, 0xc7);
    outb(0x3fc, 3);
}
void klog(const char *text) {
    uint32_t flags = irq_save();
    while (*text) {
        /* A missing UART must not prevent a framebuffer panic from appearing. */
        unsigned timeout = 100000;
        while (!(inb(0x3fd) & 0x20) && --timeout) { }
        if (!timeout)
            break;
        outb(0x3f8, (uint8_t)*text++);
    }
    irq_restore(flags);
}
void klog_u32(uint32_t value) {
    char digits[11];
    unsigned i = 10;
    digits[i] = '\0';
    do {
        digits[--i] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    klog(&digits[i]);
}
void klog_hex(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    char output[11] = "0x00000000";
    for (unsigned i = 0; i < 8; ++i)
        output[2 + i] = digits[(value >> (28 - i * 4)) & 15];
    klog(output);
}
