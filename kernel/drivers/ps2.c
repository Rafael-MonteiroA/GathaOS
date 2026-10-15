#include "input.h"
#include "arch/x86/io.h"
#include "arch/x86/pic.h"
#include "arch/x86/pit.h"
#include "serial.h"
#include "panic.h"
struct raw_byte { uint8_t byte, mouse; uint32_t tick; };
static struct raw_byte queue[512];
static volatile unsigned head, tail, dropped;
static unsigned screen_width, screen_height, observed_dropped;
static void writable(void) {
    for (unsigned i = 0; i < 100000; ++i) {
        if (!(inb(0x64) & 2)) return;
        io_wait();
    }
    panic("PS2 CONTROLLER WRITE TIMEOUT", 0);
}
static void command(uint8_t value) { writable(); outb(0x64, value); }
static void data(uint8_t value) { writable(); outb(0x60, value); }
static uint8_t response(int mouse) {
    for (unsigned i = 0; i < 100000; ++i) {
        uint8_t status = inb(0x64);
        if (status & 1) {
            uint8_t value = inb(0x60);
            if ((status & 0xc0) || !!(status & 32) != mouse)
                panic("PS2 INVALID COMMAND RESPONSE", 0);
            return value;
        }
        io_wait();
    }
    panic("PS2 DEVICE RESPONSE TIMEOUT", 0);
}
static void device(int mouse, uint8_t value) {
    for (unsigned retry = 0; retry < 3; ++retry) {
        if (mouse) command(0xd4);
        data(value);
        uint8_t reply = response(mouse);
        if (reply == 0xfa) return;
        if (reply != 0xfe) break;
    }
    panic("PS2 DEVICE COMMAND FAILED", 0);
}
void ps2_init(unsigned width, unsigned height) {
    uint32_t flags = irq_save();
    screen_width = width; screen_height = height;
    keyboard_reset(); mouse_reset(width, height);
    command(0xad); command(0xa7);
    for (unsigned i = 0; i < 256 && (inb(0x64) & 1); ++i) (void)inb(0x60);
    command(0x20);
    uint8_t config = response(0) & (uint8_t)~0x43; /* No translation or IRQs during setup. */
    command(0x60); data(config);
    command(0xab);
    if (response(0)) panic("PS2 KEYBOARD PORT TEST FAILED", 0);
    command(0xa9);
    if (response(0)) panic("PS2 MOUSE PORT TEST FAILED", 0);
    command(0xae); command(0xa8);
    device(0, 0xf5);
    device(0, 0xf0); device(0, PS2_SET);
    device(0, 0xf0); device(0, 0);
    if (response(0) != PS2_SET) panic("PS2 SCANCODE SET NOT ACCEPTED", 0);
    device(1, 0xf5); device(1, 0xf6);
    device(1, 0xf2);
    if (response(1) != 0) panic("PS2 THREE BYTE MOUSE REQUIRED", 0);
    device(1, 0xf3); device(1, 100);
    device(1, 0xe6);
    device(1, 0xf4); device(0, 0xf4);
    command(0x60); data((config & (uint8_t)~0x30) | 3);
    pic_unmask(1); pic_unmask(12);
    klog("PS2: keyboard set="); klog_u32(PS2_SET);
    klog(" translation=off repeat_delay_ms=500 repeat_period_ms=40\r\n");
    klog("PS2: mouse id=0 packet_bytes=3 sample_hz=100 IRQ1/12 enabled\r\n");
    irq_restore(flags);
}
void ps2_interrupt(void) {
    for (unsigned n = 0; n < 32; ++n) {
        uint8_t status = inb(0x64);
        if (!(status & 1)) break;
        uint8_t byte = inb(0x60);
        unsigned next = (head + 1) % 512;
        if ((status & 0xc0) || next == tail) { ++dropped; continue; }
        queue[head] = (struct raw_byte){byte, !!(status & 32), pit_ticks()};
        head = next;
    }
}
int input_poll(struct input_event *event, uint32_t now) {
    for (;;) {
        uint32_t flags = irq_save();
        if (observed_dropped != dropped) {
            observed_dropped = dropped;
            tail = head;
            keyboard_reset(); mouse_reset(screen_width, screen_height);
            irq_restore(flags);
            klog("PS2: dropped bytes; decoder state reset\r\n");
            *event = (struct input_event){.type=INPUT_MOUSE, .x=(int)screen_width/2, .y=(int)screen_height/2};
            return 1;
        }
        if (head == tail) { irq_restore(flags); break; }
        struct raw_byte raw = queue[tail];
        tail = (tail + 1) % 512;
        irq_restore(flags);
        if (raw.mouse ? mouse_byte(raw.byte, raw.tick, event) : keyboard_byte(raw.byte, raw.tick, event))
            return 1;
    }
    return keyboard_repeat(now, event);
}
unsigned input_dropped(void) { return dropped; }
