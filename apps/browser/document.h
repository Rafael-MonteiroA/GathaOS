#ifndef BROWSER_DOCUMENT_H
#define BROWSER_DOCUMENT_H
#include <stdint.h>
#include "../../libgatha/url.h"
#define PAGE_TEXT 65536
#define PAGE_LINKS 128
#define PAGE_ROWS 8192
struct page_row { unsigned start,end; };
struct page_anchor { char name[64];unsigned offset; };
struct page {
    char text[PAGE_TEXT],title[96],links[PAGE_LINKS][G_URL_MAX];
    uint16_t link[PAGE_TEXT];
    uint8_t heading[PAGE_TEXT];
    struct page_row rows[PAGE_ROWS];
    struct page_anchor anchors[128];
    unsigned size,link_count,anchor_count,row_count;
    int truncated,columns;
};
void page_parse(struct page *page,const void *data,unsigned size,int html,const char *base);
void page_layout(struct page *page,int columns);
int page_fragment(const struct page *page,const char *url);
/* 1 = HTML, 0 = plain text, negative = unsupported MIME/charset. */
int page_type(const char *type);
#endif
