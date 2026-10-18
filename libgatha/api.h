#ifndef GATHA_API_H
#define GATHA_API_H
#include <stdint.h>
#include <stddef.h>
enum { SYS_TICKS, SYS_YIELD, SYS_SLEEP, SYS_EXIT, SYS_LOG, SYS_PID, SYS_DEMO,
       SYS_OPEN, SYS_READ, SYS_WRITE, SYS_CLOSE, SYS_SEEK, SYS_READDIR, SYS_MKDIR,
       SYS_UNLINK, SYS_STAT, SYS_RENAME, SYS_SYNC };
enum { SYS_SBRK = 29, SYS_SAVE = 30 };
static inline int32_t g_sbrk(int32_t increment);
enum { G_READ=1, G_WRITE=2, G_CREATE=4, G_TRUNC=8, G_FILE=1, G_DIR=2 };
struct g_stat { uint32_t type,size,created,modified; };
struct g_dirent { char name[40]; struct g_stat stat; };
static inline int32_t g_call(unsigned id, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    int32_t result;
    __asm__ volatile("int $0x80" : "=a"(result) : "a"(id), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e) : "memory", "cc");
    return result;
}
static inline int32_t g_sbrk(int32_t increment) { return g_call(SYS_SBRK,(uint32_t)increment,0,0,0,0); }
void *g_malloc(size_t size);
void *g_calloc(size_t count, size_t size);
void *g_realloc(void *pointer, size_t size);
void g_free(void *pointer);
static inline uint32_t g_ticks(void) { return (uint32_t)g_call(SYS_TICKS,0,0,0,0,0); }
static inline void g_yield(void) { (void)g_call(SYS_YIELD,0,0,0,0,0); }
static inline void g_sleep(uint32_t ticks) { (void)g_call(SYS_SLEEP,ticks,0,0,0,0); }
static inline void g_exit(int code) { (void)g_call(SYS_EXIT,(uint32_t)code,0,0,0,0); for (;;) {} }
static inline int g_log(const char *s, unsigned length) { return g_call(SYS_LOG,(uint32_t)(uintptr_t)s,length,0,0,0); }
static inline int g_open(const char *p, unsigned flags) { return g_call(SYS_OPEN,(uint32_t)(uintptr_t)p,flags,0,0,0); }
static inline int g_read(int fd, void *p, unsigned n) { return g_call(SYS_READ,(unsigned)fd,(uint32_t)(uintptr_t)p,n,0,0); }
static inline int g_write(int fd, const void *p, unsigned n) { return g_call(SYS_WRITE,(unsigned)fd,(uint32_t)(uintptr_t)p,n,0,0); }
static inline int g_close(int fd) { return g_call(SYS_CLOSE,(unsigned)fd,0,0,0,0); }
static inline int g_seek(int fd, int offset, unsigned whence) { return g_call(SYS_SEEK,(unsigned)fd,(unsigned)offset,whence,0,0); }
static inline int g_stat(const char *p, struct g_stat *s) { return g_call(SYS_STAT,(uint32_t)(uintptr_t)p,(uint32_t)(uintptr_t)s,0,0,0); }
static inline int g_readdir(const char *p, unsigned i, struct g_dirent *e) { return g_call(SYS_READDIR,(uint32_t)(uintptr_t)p,i,(uint32_t)(uintptr_t)e,0,0); }
static inline int g_mkdir(const char *p) { return g_call(SYS_MKDIR,(uint32_t)(uintptr_t)p,0,0,0,0); }
static inline int g_unlink(const char *p) { return g_call(SYS_UNLINK,(uint32_t)(uintptr_t)p,0,0,0,0); }
static inline int g_rename(const char *a, const char *b) { return g_call(SYS_RENAME,(uint32_t)(uintptr_t)a,(uint32_t)(uintptr_t)b,0,0,0); }
static inline int g_sync(void) { return g_call(SYS_SYNC,0,0,0,0,0); }
static inline int g_save(const char *path, const void *data, unsigned size) {
    return g_call(SYS_SAVE,(uint32_t)(uintptr_t)path,(uint32_t)(uintptr_t)data,size,0,0);
}
size_t g_strlen(const char *s);
static inline int g_puts(const char *s) { return g_log(s,(unsigned)g_strlen(s)); }
int g_strcmp(const char *a, const char *b);
void g_copy(char *to, const char *from, unsigned capacity);
char *g_decimal(char *to, int value);
#endif
