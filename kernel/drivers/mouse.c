#include "input.h"
static uint8_t packet[3];
static unsigned used, max_x, max_y;
static int x, y;
static uint32_t last_byte;
void mouse_reset(unsigned width, unsigned height) {
    max_x = width - 1; max_y = height - 1;
    x = (int)width / 2; y = (int)height / 2;
    used = 0; last_byte = 0;
}
int mouse_byte(uint8_t byte, uint32_t now, struct input_event *event) {
    if (now - last_byte > 10) used = 0;
    last_byte = now;
    if (!used && !(byte & 8)) return 0;
    packet[used++] = byte;
    if (used != 3) return 0;
    used = 0;
    /* PS/2 uses a separate sign bit: 0x00 with sign set represents -256. */
    if (!(packet[0] & 0xc0)) {
        x += (int)packet[1] - ((packet[0] & 16) ? 256 : 0);
        y -= (int)packet[2] - ((packet[0] & 32) ? 256 : 0);
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > (int)max_x) x = (int)max_x;
        if (y > (int)max_y) y = (int)max_y;
    }
    *event = (struct input_event){.type=INPUT_MOUSE, .x=x, .y=y, .buttons=packet[0] & 7};
    return 1;
}
