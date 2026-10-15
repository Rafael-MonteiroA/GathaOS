#ifndef GATHA_INPUT_H
#define GATHA_INPUT_H
#include <stdint.h>
enum { INPUT_KEY = 1, INPUT_MOUSE = 2 };
enum { MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4, MOD_CAPS = 8 };
struct input_event {
    unsigned type, code, modifiers, pressed, repeat, buttons;
    unsigned char ascii;
    int x, y;
};
void ps2_init(unsigned width, unsigned height);
void ps2_interrupt(void);
int input_poll(struct input_event *event, uint32_t now);
unsigned input_dropped(void);
void keyboard_reset(void);
int keyboard_byte(uint8_t byte, uint32_t now, struct input_event *event);
int keyboard_repeat(uint32_t now, struct input_event *event);
void mouse_reset(unsigned width, unsigned height);
int mouse_byte(uint8_t byte, uint32_t now, struct input_event *event);
void input_self_test(void);
#endif
