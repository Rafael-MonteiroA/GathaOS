#include "input.h"
#include "serial.h"
#include "panic.h"
void input_self_test(void) {
    struct input_event e;
    mouse_reset(1024, 768);
    if (mouse_byte(0, 1, &e) || mouse_byte(0x18, 1, &e) || mouse_byte(0, 1, &e) ||
        !mouse_byte(0, 1, &e) || e.x != 256 || e.y != 384)
        panic("MOUSE SIGN OR SYNC FAILED", 0);
    (void)mouse_byte(0xc9, 2, &e); (void)mouse_byte(100, 2, &e);
    if (!mouse_byte(100, 2, &e) || e.x != 256 || e.y != 384 || e.buttons != 1)
        panic("MOUSE OVERFLOW FAILED", 0);
    (void)mouse_byte(8, 3, &e);
    if (mouse_byte(0, 20, &e)) panic("MOUSE TIMEOUT FAILED", 0);
    (void)mouse_byte(8, 21, &e); (void)mouse_byte(1, 21, &e);
    if (!mouse_byte(1, 21, &e) || e.x != 257 || e.y != 383)
        panic("MOUSE RESYNC FAILED", 0);
    keyboard_reset();
    uint8_t a = PS2_SET == 2 ? 0x1c : 0x1e;
    if (!keyboard_byte(a, 0, &e) || e.ascii != 'a' ||
        keyboard_byte(a, 1, &e) || keyboard_repeat(49, &e) ||
        !keyboard_repeat(50, &e) || !e.repeat || e.ascii != 'a')
        panic("KEYBOARD REPEAT FAILED", 0);
#if PS2_SET == 2
    (void)keyboard_byte(0xf0, 51, &e);
#else
    a |= 128;
#endif
    if (!keyboard_byte(a, 51, &e) || e.pressed || keyboard_repeat(100, &e))
        panic("KEYBOARD RELEASE FAILED", 0);
    keyboard_reset();
    klog("INPUTTEST: packet sign overflow resync key repeat release PASS\r\n");
}
