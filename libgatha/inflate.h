#ifndef GATHA_INFLATE_H
#define GATHA_INFLATE_H
#include <stdint.h>
enum { G_CONTENT_GZIP=1, G_CONTENT_DEFLATE=2 };
/* Original bounded RFC 1950/1951/1952 decoder. Output is usable only on success.
   Deflate means the HTTP zlib wrapper, never an unverified raw fallback.
   poll may return a negative error (deadline/cancellation); no allocations. */
int g_inflate(const void *input,unsigned size,void *output,unsigned capacity,
              unsigned *written,unsigned encoding,int (*poll)(void *),void *context);
#endif
