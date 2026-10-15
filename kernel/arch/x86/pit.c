#include "io.h"
#include "pit.h"
#include "drivers/serial.h"
static volatile uint32_t ticks;
void pit_init(void) {
    const uint16_t divisor = 11932; /* 1,193,182 Hz / 11932 = 99.9985 Hz. */
    ticks = 0;
    outb(0x43, 0x34);
    outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8));
    klog("PIT: channel=0 mode=2 divisor=11932 target_hz=100\r\n");
}
void pit_interrupt(void) { ++ticks; }
uint32_t pit_ticks(void) { return ticks; }
