#include "api.h"
size_t g_strlen(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
int g_strcmp(const char *a, const char *b) { while (*a && *a == *b) { ++a; ++b; } return (unsigned char)*a - (unsigned char)*b; }
void g_copy(char *to, const char *from, unsigned capacity) {
    if (!capacity) return;
    unsigned i = 0; while (i + 1 < capacity && from[i]) { to[i] = from[i]; ++i; } to[i] = 0;
}
char *g_decimal(char *to, int value) {
    char digits[10]; unsigned n = 0, number = value < 0 ? 0u - (unsigned)value : (unsigned)value;
    if (value < 0) *to++ = '-';
    do { digits[n++] = (char)('0' + number % 10); number /= 10; } while (number);
    while (n) *to++ = digits[--n]; *to = 0; return to;
}
