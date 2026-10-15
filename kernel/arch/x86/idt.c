#include "interrupts.h"
#include "pic.h"
#include "pit.h"
#include "drivers/serial.h"
#include "panic.h"
struct idt_gate {
    uint16_t offset_low, selector;
    uint8_t zero, attributes;
    uint16_t offset_high;
} __attribute__((packed));
struct idt_pointer { uint16_t limit; uint32_t base; } __attribute__((packed));
_Static_assert(sizeof(struct idt_gate) == 8, "IDT gate size");
extern void (*isr_table[256])(void);
static struct idt_gate idt[256] __attribute__((aligned(16)));
static const char *const exceptions[32] = {
    "DIVIDE ERROR", "DEBUG", "NMI", "BREAKPOINT", "OVERFLOW", "BOUND RANGE",
    "INVALID OPCODE", "DEVICE NOT AVAILABLE", "DOUBLE FAULT", "COPROCESSOR OVERRUN",
    "INVALID TSS", "SEGMENT NOT PRESENT", "STACK FAULT", "GENERAL PROTECTION",
    "PAGE FAULT", "RESERVED EXCEPTION", "X87 FLOATING POINT", "ALIGNMENT CHECK",
    "MACHINE CHECK", "SIMD FLOATING POINT", "VIRTUALIZATION", "CONTROL PROTECTION",
    "RESERVED EXCEPTION", "RESERVED EXCEPTION", "RESERVED EXCEPTION",
    "RESERVED EXCEPTION", "RESERVED EXCEPTION", "RESERVED EXCEPTION",
    "HYPERVISOR INJECTION", "VMM COMMUNICATION", "SECURITY", "RESERVED EXCEPTION"
};
void idt_init(void) {
    for (unsigned i = 0; i < 256; ++i) {
        uint32_t address = (uint32_t)(uintptr_t)isr_table[i];
        idt[i].offset_low = (uint16_t)address;
        idt[i].selector = 0x08;
        idt[i].zero = 0;
        idt[i].attributes = 0x8e;
        idt[i].offset_high = (uint16_t)(address >> 16);
    }
    const struct idt_pointer pointer = {sizeof(idt) - 1, (uint32_t)(uintptr_t)idt};
    __asm__ volatile("lidt %0" : : "m"(pointer) : "memory");
    klog("IDT: 256 ring-0 interrupt gates installed\r\n");
}
void interrupt_dispatch(struct interrupt_frame *frame) {
    if (frame->vector < 32)
        panic(exceptions[frame->vector], frame);
    if (frame->vector >= 32 && frame->vector < 48) {
        unsigned irq = frame->vector - 32;
        if (pic_is_spurious(irq))
            return;
        if (irq == 0)
            pit_interrupt();
        else
            panic("UNEXPECTED IRQ", frame);
        pic_eoi(irq);
        return;
    }
    panic("UNEXPECTED INTERRUPT", frame);
}
