#include "api.h"
#include "memory.h"
#include "layout.h"

struct block {
    size_t size;
    struct block *next, *previous;
    unsigned available;
} __attribute__((aligned(16)));
static struct block *head, *tail;

static void split(struct block *b, size_t size) {
    if (b->size < size + sizeof(*b) + 16) return;
    struct block *rest = (void *)((unsigned char *)(b + 1) + size);
    *rest = (struct block){b->size - size - sizeof(*b), b->next, b, 1};
    if (rest->next) rest->next->previous = rest; else tail = rest;
    b->next = rest; b->size = size;
}
void *g_malloc(size_t size) {
    if (!size || size > USER_LIMIT - USER_HEAP_BASE - 4096) return 0;
    size = (size + 15) & ~15u;
    for (struct block *b = head; b; b = b->next) if (b->available && b->size >= size) {
        split(b, size); b->available = 0; return b + 1;
    }
    unsigned bytes = (size + sizeof(struct block) + 4095) & ~4095u;
    int32_t address = g_sbrk((int32_t)bytes);
    if (address < 0) return 0;
    struct block *b = (void *)(uintptr_t)address;
    *b = (struct block){bytes - sizeof(*b), 0, tail, 0};
    if (tail) tail->next = b; else head = b;
    tail = b; split(b, size); return b + 1;
}
static void merge_next(struct block *b) {
    struct block *next = b->next;
    if (!next || !next->available) return;
    b->size += sizeof(*b) + next->size; b->next = next->next;
    if (b->next) b->next->previous = b; else tail = b;
}
void g_free(void *pointer) {
    if (!pointer) return;
    struct block *b = (struct block *)pointer - 1;
    b->available = 1; merge_next(b);
    if (b->previous && b->previous->available) { b = b->previous; merge_next(b); }
    /* Return the whole arena when empty; keep interior holes for reuse. */
    if (b == head && !b->next) {
        int32_t bytes = (int32_t)(sizeof(*b) + b->size);
        if (g_sbrk(-bytes) >= 0) head = tail = 0;
    }
}
void *g_calloc(size_t count, size_t size) {
    if (count && size > SIZE_MAX / count) return 0;
    size_t bytes = count * size;
    void *p = g_malloc(bytes); if (p) memset(p, 0, bytes); return p;
}
void *g_realloc(void *pointer, size_t size) {
    if (!pointer) return g_malloc(size);
    if (!size) { g_free(pointer); return 0; }
    struct block *b = (struct block *)pointer - 1;
    if (size <= b->size) return pointer;
    void *p = g_malloc(size);
    if (p) { memcpy(p, pointer, b->size); g_free(pointer); }
    return p;
}
