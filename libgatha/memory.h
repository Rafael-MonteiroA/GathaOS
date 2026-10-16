#ifndef LIBGATHA_MEMORY_H
#define LIBGATHA_MEMORY_H
#include <stddef.h>
void *memcpy(void *restrict destination, const void *restrict source, size_t size);
void *memset(void *destination, int value, size_t size);
void *memmove(void *destination, const void *source, size_t size);
#endif
