#ifndef GATHA_PCNET_H
#define GATHA_PCNET_H
#include <stdint.h>
int pcnet_init(uint8_t mac[6]);
int pcnet_send(const void *frame, unsigned length);
int pcnet_receive(void *frame);
#endif
