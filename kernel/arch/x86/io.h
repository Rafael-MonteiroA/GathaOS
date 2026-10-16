#ifndef GATHA_X86_IO_H
#define GATHA_X86_IO_H
#include <stdint.h>
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) { uint16_t v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(port)); return v; }
static inline void outw(uint16_t port, uint16_t v) { __asm__ volatile("outw %0,%1" : : "a"(v),"Nd"(port)); }
static inline void io_wait(void) { outb(0x80, 0); }
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
static inline void irq_restore(uint32_t flags) {
    __asm__ volatile("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}
#endif
