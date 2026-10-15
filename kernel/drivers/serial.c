#include "serial.h"
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
void klog(const char *text) {
    while (*text) {
        while (!(inb(0x3fd) & 0x20)) { }
        outb(0x3f8, (uint8_t)*text++);
    }
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
