#include "ui.h"
#include "../../libgatha/memory.h"
int ui_open(struct ui *u,const char *title,int width,int height) {
    memset(u,0,sizeof(*u)); struct g_window w={.width=width,.height=height};
    g_copy(w.title,title,sizeof(w.title));
    u->width=width; u->height=height;
    u->window=g_call(SYS_WINDOW,(uint32_t)(uintptr_t)&w,0,0,0,0);
    return u->window;
}
int ui_poll(struct ui *u) {
    int result=g_call(SYS_EVENT,(unsigned)u->window,(uint32_t)(uintptr_t)&u->e,0,0,0);
    if (result<=0) { u->e.type=0; return result; }
    if (u->e.type==G_RESIZE) { u->width=u->e.width; u->height=u->e.height; }
    if (u->e.type==G_CLOSE) { ui_close(u); g_exit(0); }
    return 1;
}
static void flush(struct ui *u,unsigned end) {
    g_call(SYS_DRAW,(unsigned)u->window,(uint32_t)(uintptr_t)u->commands,(unsigned)u->count,end,0);u->count=0;
}
static struct g_draw *command(struct ui *u,int type,int x,int y,int w,int h,unsigned color) {
    if (u->count==256) flush(u,0);
    struct g_draw *d=&u->commands[u->count++];
    *d=(struct g_draw){.type=type,.x=x,.y=y,.w=w,.h=h,.color=color}; return d;
}
void ui_rect(struct ui *u,int x,int y,int w,int h,unsigned color) { command(u,DRAW_RECT,x,y,w,h,color); }
void ui_line(struct ui *u,int x,int y,int x2,int y2,unsigned color) { command(u,DRAW_LINE,x,y,x2,y2,color); }
void ui_label(struct ui *u,int x,int y,const char *text,unsigned color) {
    int origin=x;
    while (*text) {
        char line[64]; unsigned n=0;
        while (*text && *text!='\n' && n<63) line[n++]=*text++;
        line[n]=0; g_copy(command(u,DRAW_TEXT,x,y,0,0,color)->text,line,64);
        x+=(int)n*8;
        if (*text=='\n') { ++text; x=origin; y+=20; }
    }
}
void ui_begin(struct ui *u) { u->count=0;g_call(SYS_DRAW,(unsigned)u->window,0,0,1,0);ui_rect(u,0,0,u->width,u->height,UI_BG); }
void ui_end(struct ui *u) {
    flush(u,2);
}
void ui_bitmap(struct ui *u,const struct g_bitmap *bitmap) { flush(u,0);g_call(SYS_BITMAP,(unsigned)u->window,(uint32_t)(uintptr_t)bitmap,0,0,0); }
int ui_hit(struct ui *u,int x,int y,int w,int h) {
    return u->e.type==G_MOUSE && u->e.pressed==1 && u->e.x>=x && u->e.x<x+w && u->e.y>=y && u->e.y<y+h;
}
int ui_button(struct ui *u,int x,int y,int w,const char *label) {
    int hit=ui_hit(u,x,y,w,28);
    ui_rect(u,x,y,w,28,hit?0x125c63:UI_ACCENT);
    ui_label(u,x+8,y+6,label,0xffffff); return hit;
}
int ui_checkbox(struct ui *u,int x,int y,const char *label,int *value) {
    int hit=ui_hit(u,x,y,24+(int)g_strlen(label)*8,24);
    if (hit) *value=!*value;
    ui_rect(u,x,y,20,20,0xc8d6e3); ui_rect(u,x+2,y+2,16,16,0xffffff);
    if (*value) { ui_line(u,x+4,y+10,x+8,y+14,UI_ACCENT); ui_line(u,x+8,y+14,x+16,y+5,UI_ACCENT); }
    ui_label(u,x+28,y+2,label,UI_INK); return hit;
}
static void erase_selection(struct ui_text *t) {
    unsigned lo=t->cursor<t->anchor?t->cursor:t->anchor, hi=t->cursor>t->anchor?t->cursor:t->anchor;
    if (lo!=hi) { unsigned n=(unsigned)g_strlen(t->data); memmove(t->data+lo,t->data+hi,n-hi+1); t->cursor=t->anchor=lo; }
}
int ui_textbox(struct ui *u,int x,int y,int w,int h,struct ui_text *t) {
    unsigned n=(unsigned)g_strlen(t->data); int changed=0;
    if (t->cursor>n) t->cursor=n;
    if (t->anchor>n) t->anchor=n;
    unsigned cols=(unsigned)(w-16)/8, rows=(unsigned)(h-12)/20;
    if (!cols || !rows) return 0;
    if (u->e.type==G_MOUSE && u->e.pressed==1) {
        t->focused=ui_hit(u,x,y,w,h);
        if (t->focused) {
            unsigned target_row=(unsigned)(u->e.y-y-6<0?0:u->e.y-y-6)/20+t->scroll;
            unsigned target_col=(unsigned)(u->e.x-x-8<0?0:u->e.x-x-8)/8;
            unsigned r=0,c=0,p=0;
            while (p<n && (r<target_row || (r==target_row && c<target_col && t->data[p]!='\n'))) {
                if (t->data[p++]=='\n' || ++c==cols) { ++r; c=0; }
            }
            t->cursor=t->anchor=p;
        }
    }
    if (t->focused && u->e.type==G_KEY && u->e.pressed) {
        unsigned code=u->e.code; int shift=u->e.mods&1;
        if ((u->e.mods&2) && (u->e.ascii=='a' || u->e.ascii=='A')) { t->anchor=0; t->cursor=n; }
        else if (code==203 || code==205 || code==199 || code==207 || code==200 || code==208) {
            if (code==203 && t->cursor) --t->cursor;
            if (code==205 && t->cursor<n) ++t->cursor;
            if (code==199) { while (t->cursor && t->data[t->cursor-1]!='\n') --t->cursor; }
            if (code==207) { while (t->cursor<n && t->data[t->cursor]!='\n') ++t->cursor; }
            if (code==200 || code==208) {
                unsigned start=t->cursor; while (start && t->data[start-1]!='\n') --start;
                unsigned col=t->cursor-start;
                if (code==200 && start) {
                    unsigned end=start-1; start=end; while (start && t->data[start-1]!='\n') --start;
                    t->cursor=start+col<end?start+col:end;
                } else if (code==208) {
                    while (start<n && t->data[start]!='\n') ++start;
                    if (start<n) { ++start; unsigned end=start; while (end<n && t->data[end]!='\n') ++end; t->cursor=start+col<end?start+col:end; }
                }
            }
            if (!shift) t->anchor=t->cursor;
        } else if (!(u->e.mods&6)) {
            unsigned c=u->e.ascii;
            if (c==8 || code==211) {
                if (t->cursor==t->anchor) {
                    if (c==8 && t->cursor) --t->anchor;
                    else if (code==211 && t->cursor<n) ++t->anchor;
                }
                erase_selection(t); changed=1;
            } else if ((c>=32 && c<=126) || (c==10 && t->multiline)) {
                erase_selection(t); n=(unsigned)g_strlen(t->data);
                if (n+1<t->capacity) { memmove(t->data+t->cursor+1,t->data+t->cursor,n-t->cursor+1); t->data[t->cursor++]=(char)c; t->anchor=t->cursor; changed=1; }
            }
        }
    }
    n=(unsigned)g_strlen(t->data);
    unsigned cr=0,cc=0;
    for (unsigned p=0;p<t->cursor;++p) if (t->data[p]=='\n' || ++cc==cols) { ++cr; cc=0; }
    if (cr<t->scroll) t->scroll=cr;
    if (cr>=t->scroll+rows) t->scroll=cr-rows+1;
    ui_rect(u,x,y,w,h,t->focused?UI_ACCENT:0xc8d6e3); ui_rect(u,x+1,y+1,w-2,h-2,0xffffff);
    unsigned r=0,c=0,lo=t->cursor<t->anchor?t->cursor:t->anchor,hi=t->cursor>t->anchor?t->cursor:t->anchor;
    for (unsigned p=0;p<n;++p) {
        if (r>=t->scroll && r<t->scroll+rows && t->data[p]!='\n') {
            int px=x+8+(int)c*8,py=y+6+(int)(r-t->scroll)*20;
            if (p>=lo && p<hi) ui_rect(u,px,py,8,18,0xbde8e5);
            char ch[2]={t->data[p],0}; ui_label(u,px,py,ch,UI_INK);
        }
        if (t->data[p]=='\n' || ++c==cols) { ++r; c=0; }
    }
    if (t->focused) ui_rect(u,x+8+(int)cc*8,y+6+(int)(cr-t->scroll)*20,1,16,UI_ACCENT);
    return changed;
}
int ui_list(struct ui *u,int x,int y,int w,const char *const *items,int count,int *selected,int offset,int visible) {
    int hit=-1; ui_rect(u,x,y,w,visible*24,0xffffff);
    for (int row=0;row<visible && row+offset<count;++row) {
        int i=row+offset; if (ui_hit(u,x,y+row*24,w,24)) { *selected=i; hit=i; }
        if (*selected==i) ui_rect(u,x,y+row*24,w,24,0xcbe9e8);
        ui_label(u,x+8,y+row*24+4,items[i],UI_INK);
    }
    return hit;
}
int ui_scrollbar(struct ui *u,int x,int y,int h,int *offset,int count,int visible) {
    int old=*offset,max=count>visible?count-visible:0;
    if (ui_hit(u,x,y,20,h)) *offset=(u->e.y-y-12)*max/(h>24?h-24:1);
    if (*offset>max) *offset=max;
    if (*offset<0) *offset=0;
    ui_rect(u,x,y,20,h,0xdce5ed); ui_rect(u,x+3,y+(max?*offset*(h-24)/max:0),14,24,0x829cb1);
    return old!=*offset;
}
int ui_menu(struct ui *u,const char *const *items,int count) {
    ui_rect(u,0,0,u->width,30,0xdce5ed); int hit=-1;
    for (int i=0;i<count;++i) { ui_label(u,12+i*100,7,items[i],UI_INK); if (ui_hit(u,i*100,0,100,30)) hit=i; }
    return hit;
}
int ui_context(struct ui *u,int x,int y,const char *const *items,int count) {
    ui_rect(u,x,y,170,count*28+8,0x20344d); int hit=-1;
    for (int i=0;i<count;++i) { ui_label(u,x+10,y+8+i*28,items[i],0xffffff); if (ui_hit(u,x,y+4+i*28,170,28)) hit=i; }
    return hit;
}
int ui_dialog(struct ui *u,const char *title,const char *message) {
    int x=(u->width-380)/2,y=(u->height-170)/2;
    ui_rect(u,x+5,y+5,380,170,0x829cb1); ui_rect(u,x,y,380,170,0xffffff);
    ui_rect(u,x,y,380,34,UI_ACCENT); ui_label(u,x+12,y+9,title,0xffffff);
    ui_label(u,x+16,y+52,message,UI_INK); return ui_button(u,x+270,y+126,90,"OK");
}
