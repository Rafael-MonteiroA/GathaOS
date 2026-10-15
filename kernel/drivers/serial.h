#ifndef GATHA_SERIAL_H
#define GATHA_SERIAL_H
#include <stdint.h>
void serial_init(void);
void klog(const char *text);
void klog_u32(uint32_t value);
void klog_hex(uint32_t value);
#endif
