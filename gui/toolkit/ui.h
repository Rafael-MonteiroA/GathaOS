#ifndef GATHA_UI_H
#define GATHA_UI_H
#include "../../libgatha/api.h"
#include "../wm/protocol.h"
#define UI_BG 0xf3f4f6
#define UI_INK 0x20344d
#define UI_ACCENT 0x426d9c
struct ui { int window,width,height,count,previous; struct g_event e; struct g_draw commands[256]; };
struct ui_text { char *data; unsigned capacity,cursor,anchor,scroll; int focused,multiline; };
int ui_open(struct ui *u,const char *title,int width,int height);
int ui_poll(struct ui *u);
void ui_begin(struct ui *u);
void ui_end(struct ui *u);
void ui_bitmap(struct ui *u,const struct g_bitmap *bitmap);
void ui_rect(struct ui *u,int x,int y,int w,int h,unsigned color);
void ui_line(struct ui *u,int x,int y,int x2,int y2,unsigned color);
void ui_label(struct ui *u,int x,int y,const char *text,unsigned color);
int ui_hit(struct ui *u,int x,int y,int w,int h);
int ui_button(struct ui *u,int x,int y,int w,const char *label);
int ui_checkbox(struct ui *u,int x,int y,const char *label,int *value);
int ui_textbox(struct ui *u,int x,int y,int w,int h,struct ui_text *text);
int ui_list(struct ui *u,int x,int y,int w,const char *const *items,int count,int *selected,int offset,int visible);
int ui_scrollbar(struct ui *u,int x,int y,int h,int *offset,int count,int visible);
int ui_menu(struct ui *u,const char *const *items,int count);
int ui_context(struct ui *u,int x,int y,const char *const *items,int count);
int ui_dialog(struct ui *u,const char *title,const char *message);
static inline int ui_launch(unsigned app) { return g_call(SYS_LAUNCH,app,0,0,0,0); }
static inline int ui_info(struct g_info *i) { return g_call(SYS_INFO,(uint32_t)(uintptr_t)i,0,0,0,0); }
static inline void ui_close(struct ui *u) { g_call(SYS_WINDOW_CLOSE,(unsigned)u->window,0,0,0,0); }
#endif
