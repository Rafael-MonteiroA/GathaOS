#ifndef GATHA_SECURITY_API_H
#define GATHA_SECURITY_API_H
#include "api.h"
#define SYS_SECURITY 32
enum { G_RANDOM, G_UTC };
static inline int g_random(void *data,unsigned size){return g_call(SYS_SECURITY,G_RANDOM,(uint32_t)(uintptr_t)data,size,0,0);}
static inline int g_utc(uint32_t *seconds){return g_call(SYS_SECURITY,G_UTC,(uint32_t)(uintptr_t)seconds,4,0,0);}
#endif
