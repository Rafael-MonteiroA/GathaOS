#ifndef GATHA_PIT_H
#define GATHA_PIT_H
#include <stdint.h>
void pit_init(void);
void pit_interrupt(void);
uint32_t pit_ticks(void);
#endif
