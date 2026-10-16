#include "memory.h"
void *memmove(void *destination, const void *source, size_t size) {
    unsigned char *to=destination; const unsigned char *from=source;
    if ((size_t)to<(size_t)from) { for (size_t i=0;i<size;++i) to[i]=from[i]; }
    else { while(size) { --size; to[size]=from[size]; } }
    return destination;
}
void *memcpy(void *restrict destination, const void *restrict source, size_t size) {
    unsigned char *to = destination;
    const unsigned char *from = source;
    for (size_t i = 0; i < size; ++i) to[i] = from[i];
    return destination;
}
void *memset(void *destination, int value, size_t size) {
    unsigned char *to = destination;
    for (size_t i = 0; i < size; ++i) to[i] = (unsigned char)value;
    return destination;
}
