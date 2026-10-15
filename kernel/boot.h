#ifndef GATHA_BOOT_H
#define GATHA_BOOT_H
#include <stdint.h>
struct boot_info {
    uint32_t magic, version, e820_count, e820_address;
    uint32_t framebuffer, width, height, pitch, bpp;
    uint32_t red_size, red_position, green_size, green_position;
    uint32_t blue_size, blue_position;
};
_Static_assert(sizeof(struct boot_info) == 60, "Boot protocol layout");
#endif
