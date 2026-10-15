#ifndef GATHA_API_H
#define GATHA_API_H
#include <stdint.h>
#include <stddef.h>
enum { SYS_TICKS, SYS_YIELD, SYS_SLEEP, SYS_EXIT, SYS_LOG, SYS_PID, SYS_DEMO };
static inline int32_t g_call(unsigned id, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    int32_t result;
    __asm__ volatile("int $0x80" : "=a"(result) : "a"(id), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory", "cc");
    return result;
}
static inline uint32_t g_ticks(void) { return (uint32_t)g_call(SYS_TICKS,0,0,0,0,0); }
static inline void g_yield(void) { (void)g_call(SYS_YIELD,0,0,0,0,0); }
static inline void g_sleep(uint32_t ticks) { (void)g_call(SYS_SLEEP,ticks,0,0,0,0); }
static inline void g_exit(int code) { (void)g_call(SYS_EXIT,(uint32_t)code,0,0,0,0); for (;;) {} }
static inline int g_log(const char *s, unsigned length) { return g_call(SYS_LOG,(uint32_t)(uintptr_t)s,length,0,0,0); }
size_t g_strlen(const char *s);
int g_strcmp(const char *a, const char *b);
void g_copy(char *to, const char *from, unsigned capacity);
char *g_decimal(char *to, int value);
#endif
