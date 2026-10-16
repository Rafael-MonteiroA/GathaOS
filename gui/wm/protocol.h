#ifndef GATHA_WINDOW_PROTOCOL_H
#define GATHA_WINDOW_PROTOCOL_H
#include <stdint.h>
enum { SYS_WINDOW=18, SYS_DRAW, SYS_EVENT, SYS_WINDOW_CLOSE, SYS_LAUNCH, SYS_INFO, SYS_THEME, SYS_POWER };
enum { G_MOUSE=1, G_KEY, G_RESIZE, G_CLOSE, G_FOCUS };
enum { DRAW_RECT=1, DRAW_TEXT, DRAW_LINE };
struct g_window { int width,height; char title[48]; };
struct g_event { unsigned type,code,ascii,mods,buttons,pressed; int x,y,width,height; };
struct g_draw { int type,x,y,w,h; uint32_t color; char text[64]; };
struct g_info { unsigned width,height,seconds,ticks,free_frames,total_frames,heap_bytes,windows,frames,pixels,event_drops; };
struct g_process { unsigned pid,state,ticks; char name[24]; };
#endif
