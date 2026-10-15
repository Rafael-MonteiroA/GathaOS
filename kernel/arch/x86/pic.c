#include "io.h"
#include "pic.h"
#include "drivers/serial.h"
static void command(uint16_t port, uint8_t value) {
    outb(port, value);
    io_wait();
}
void pic_init(void) {
    command(0x21, 0xff);
    command(0xa1, 0xff);
    command(0x20, 0x11);
    command(0xa0, 0x11);
    command(0x21, 0x20);
    command(0xa1, 0x28);
    command(0x21, 0x04);
    command(0xa1, 0x02);
    command(0x21, 0x01);
    command(0xa1, 0x01);
    command(0x21, 0xfe);
    command(0xa1, 0xff);
    klog("PIC: IRQ0-15 remapped to 32-47; only IRQ0 unmasked\r\n");
}
void pic_unmask(unsigned irq) {
    if (irq >= 16) return;
    uint32_t flags = irq_save();
    uint16_t port = irq < 8 ? 0x21 : 0xa1;
    outb(port, inb(port) & (uint8_t)~(1u << (irq % 8)));
    if (irq >= 8) outb(0x21, inb(0x21) & (uint8_t)~4u);
    irq_restore(flags);
}
int pic_is_spurious(unsigned irq) {
    if (irq == 7) {
        outb(0x20, 0x0b);
        return !(inb(0x20) & 0x80);
    }
    if (irq == 15) {
        outb(0xa0, 0x0b);
        if (!(inb(0xa0) & 0x80)) {
            outb(0x20, 0x20);
            return 1;
        }
    }
    return 0;
}
void pic_eoi(unsigned irq) {
    if (irq >= 8)
        outb(0xa0, 0x20);
    outb(0x20, 0x20);
}
