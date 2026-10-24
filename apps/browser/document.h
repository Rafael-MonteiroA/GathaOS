#ifndef BROWSER_DOCUMENT_H
#define BROWSER_DOCUMENT_H
#include <stdint.h>
#include "../../libgatha/url.h"
#define PAGE_TEXT 65536
#define PAGE_LINKS 128
#define PAGE_ROWS 8192
#define PAGE_FORMS 16
#define PAGE_CONTROLS 64
enum { FIELD_TEXT, FIELD_HIDDEN, FIELD_SUBMIT };
struct page_form { char action[G_URL_MAX]; int unsupported; };
struct page_control {
    char name[128],value[256],caption[64];
    unsigned form,type,offset,maximum;
    int disabled,readonly,required;
};
struct form_parser { int form,button; unsigned caption_size; };
struct page_row { unsigned start,end; };
struct page_anchor { char name[64];unsigned offset; };
struct page {
    char text[PAGE_TEXT],title[96],links[PAGE_LINKS][G_URL_MAX];
    uint16_t link[PAGE_TEXT];
    uint8_t heading[PAGE_TEXT];
    struct page_row rows[PAGE_ROWS];
    struct page_anchor anchors[128];
    struct page_form forms[PAGE_FORMS];
    struct page_control controls[PAGE_CONTROLS];
    unsigned form_count,control_count;
    int forms_blocked;
    unsigned size,link_count,anchor_count,row_count;
    int truncated,columns;
};
void page_parse(struct page *page,const void *data,unsigned size,int html,const char *base);
void page_layout(struct page *page,int columns);
int page_fragment(const struct page *page,const char *url);
/* 1 = HTML, 0 = plain text, negative = unsupported MIME/charset. */
int page_type(const char *type);
/* Exact form attributes reject unsupported characters instead of changing data. */
int page_attribute(const char *s,unsigned n,const char *name,char *out,unsigned capacity,int exact);
int page_form_tag(struct page *p,struct form_parser *state,const char *name,unsigned n,
                  const char *attrs,unsigned size,int closing,const char *base);
int page_submit(const struct page *p,unsigned control,const char *current,char out[G_URL_MAX],int *invalid);
#endif
