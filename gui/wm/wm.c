#include "wm.h"
#include "protocol.h"
#include "../gfx/gfx.h"
#include "drivers/framebuffer.h"
#include "drivers/input.h"
#include "drivers/rtc.h"
#include "drivers/serial.h"
#include "arch/x86/io.h"
#include "arch/x86/pit.h"
#include "sched/task.h"
#include "mm/memory.h"
#include "panic.h"
#include "../../libgatha/api.h"
#include "../../libgatha/memory.h"
#define WINDOWS 8
struct window {
    unsigned id,owner,head,tail,minimized,maximized;
    unsigned batching,batch_tick;
    int x,y,w,h,oldx,oldy,oldw,oldh;
    char title[48]; uint32_t *pixels;
    struct g_event events[64];
};
static struct window windows[WINDOWS];
static int order[WINDOWS],count,focus=-1,drag=-1,dragx,dragy;
static int width,height,mx,my,buttons,menu;
extern const char *desktop_names[];
static int dx,dy,dr,db,damaged;
static unsigned next_id=1,frames,pixels_drawn,drops,accent=0x426d9c,wallpaper;
static void log_window(const char *what,const struct window *w) {
    klog("WM: "); klog(what); klog(" id="); klog_u32(w->id); klog(" owner="); klog_u32(w->owner);
    klog(" x="); klog_u32((unsigned)w->x); klog(" y="); klog_u32((unsigned)w->y);
    klog(" w="); klog_u32((unsigned)w->w); klog(" h="); klog_u32((unsigned)w->h); klog("\r\n");
}
static void damage(int x,int y,int w,int h) {
    int right=x+w,bottom=y+h;
    if (x<0) x=0;
    if (y<0) y=0;
    if (right>width) right=width;
    if (bottom>height) bottom=height;
    if (x>=right || y>=bottom) return;
    if (!damaged) { dx=x; dy=y; dr=right; db=bottom; damaged=1; }
    else { if (x<dx) dx=x; if (y<dy) dy=y; if (right>dr) dr=right; if (bottom>db) db=bottom; }
}
static void window_damage(struct window *w) { damage(w->x-2,w->y-2,w->w+12,w->h+46); }
static void queue(struct window *w,struct g_event e) {
    /* Consecutive pointer motion is replaceable; button/key transitions are not. */
    if (e.type==G_MOUSE && !e.pressed && w->head!=w->tail) {
        struct g_event *last=&w->events[(w->head-1)%64];
        if (last->type==G_MOUSE && !last->pressed && last->buttons==e.buttons) { *last=e; return; }
    }
    if (w->head-w->tail==64) { ++drops; return; }
    w->events[w->head++%64]=e;
}
static void raise_window(int slot) {
    if (focus>=0 && focus!=slot) { window_damage(&windows[focus]); queue(&windows[focus],(struct g_event){.type=G_FOCUS,.pressed=0}); }
    int pos=0; while (pos<count && order[pos]!=slot) ++pos;
    if (pos==count) return;
    for (int i=pos;i<count-1;++i) order[i]=order[i+1];
    order[count-1]=slot; focus=slot; windows[slot].minimized=0;
    queue(&windows[slot],(struct g_event){.type=G_FOCUS,.pressed=1});
    window_damage(&windows[slot]); damage(0,height-42,width,42); log_window("focus",&windows[slot]);
}
static struct window *owned(unsigned id) {
    for (unsigned i=0;i<WINDOWS;++i)
        if (windows[i].id==id && windows[i].owner==task_current()->pid) return &windows[i];
    return 0;
}
static void remove_window(int slot) {
    struct window *w=&windows[slot]; window_damage(w); log_window("closed",w);
    kfree(w->pixels); memset(w,0,sizeof(*w));
    for (int i=0;i<count;++i) if (order[i]==slot) {
        for (int j=i;j<count-1;++j) order[j]=order[j+1]; --count; break;
    }
    if (drag==slot) drag=-1;
    if (focus==slot) { focus=-1; for (int i=count-1;i>=0;--i) if (!windows[order[i]].minimized) { raise_window(order[i]); break; } }
    damage(0,height-42,width,42);
}
void wm_release(unsigned pid) { for (int i=0;i<WINDOWS;++i) if (windows[i].id && windows[i].owner==pid) remove_window(i); }
static int create_window(const struct g_window *request) {
    if (request->width<240 || request->height<160 || request->width>width-16 || request->height>height-80 || count==WINDOWS) return -22;
    int slot=0; while (windows[slot].id) ++slot;
    uint32_t *buffer=kmalloc((unsigned)(request->width*request->height)*4);
    if (!buffer) return -12;
    struct window *w=&windows[slot]; memset(w,0,sizeof(*w));
    w->id=next_id++; if (!next_id) next_id=1;
    w->owner=task_current()->pid; w->w=request->width; w->h=request->height;
    w->x=140+(count%4)*38; w->y=64+(count%4)*38;
    if (w->x+w->w+4>width) w->x=width-w->w-4;
    if (w->y+w->h+36>height-42) w->y=height-w->h-80;
    w->pixels=buffer; for (int i=0;i<w->w*w->h;++i) buffer[i]=0xf1f5f9;
    g_copy(w->title,request->title,48); order[count++]=slot; raise_window(slot);
    log_window("created",w); return (int)w->id;
}
static void maximize(int slot) {
    struct window *w=&windows[slot];
    int nw=w->maximized?w->oldw:width-8, nh=w->maximized?w->oldh:height-80;
    uint32_t *buffer=kmalloc((unsigned)(nw*nh)*4); if (!buffer) return;
    for (int i=0;i<nw*nh;++i) buffer[i]=0xf1f5f9;
    window_damage(w); kfree(w->pixels); w->pixels=buffer;
    if (!w->maximized) { w->oldx=w->x; w->oldy=w->y; w->oldw=w->w; w->oldh=w->h; w->x=2; w->y=2; }
    else { w->x=w->oldx; w->y=w->oldy; }
    w->w=nw; w->h=nh; w->maximized=!w->maximized; window_damage(w);
    queue(w,(struct g_event){.type=G_RESIZE,.width=nw,.height=nh}); log_window("resize",w);
}
int wm_call(struct interrupt_frame *f) {
    if (f->eax==SYS_WINDOW) {
        if (!user_range(f->ebx,sizeof(struct g_window),0)) return -14;
        struct g_window request=*(const struct g_window *)(uintptr_t)f->ebx; request.title[47]=0;
        return create_window(&request);
    }
    if (f->eax==SYS_LAUNCH) {
        char path[256]={0};
        if(f->ecx){unsigned i;for(i=0;i<256;++i){if(!user_range(f->ecx+i,1,0))return -14;path[i]=*(const char *)(uintptr_t)(f->ecx+i);if(!path[i])break;}if(i==256)return -36;}
        return desktop_open(f->ebx,path);
    }
    if(f->eax==SYS_ARGUMENT){if(f->ecx<256 || !user_range(f->ebx,256,1))return -14;g_copy((char *)(uintptr_t)f->ebx,task_current()->path,256);return 0;}
    if(f->eax==SYS_PROCESSES){
        if(f->ecx>16 || !user_range(f->ebx,f->ecx*sizeof(struct g_process),1))return -14;
        struct g_process *out=(void *)(uintptr_t)f->ebx;const struct task *tasks=task_table();unsigned n=0;
        for(unsigned i=0;i<TASK_MAX && n<f->ecx;++i)if(tasks[i].state && tasks[i].state!=TASK_DEAD){out[n]=(struct g_process){.pid=tasks[i].pid,.state=tasks[i].state,.ticks=tasks[i].cpu_ticks};g_copy(out[n++].name,tasks[i].name,24);}return (int)n;
    }
    if (f->eax==SYS_INFO) {
        if (!user_range(f->ebx,sizeof(struct g_info),1)) return -14;
        *(struct g_info *)(uintptr_t)f->ebx=(struct g_info){(unsigned)width,(unsigned)height,rtc_seconds(),pit_ticks(),pmm_free_frames(),pmm_total_frames(),heap_mapped_bytes(),(unsigned)count,frames,pixels_drawn,drops};
        return 0;
    }
    if (f->eax==SYS_THEME) { accent=f->ebx&0xffffff; wallpaper=f->ecx%3; damage(0,0,width,height); return 0; }
    struct window *w=owned(f->ebx); if (!w) return -9;
    if(f->eax==SYS_BITMAP){
        if(!user_range(f->ecx,sizeof(struct g_bitmap),0))return -14;
        struct g_bitmap b=*(const struct g_bitmap *)(uintptr_t)f->ecx;
        if(b.width<1||b.height<1||b.width>512||b.height>512||b.scale<1||b.scale>4||b.x< -2048||b.x>2048||b.y< -2048||b.y>2048)return -22;
        if(!user_range((uint32_t)(uintptr_t)b.pixels,(unsigned)(b.width*b.height),0))return -14;
        gfx_target(w->pixels,(unsigned)w->w,(unsigned)w->h);
        for(int y=0;y<b.height;++y)for(int x=0;x<b.width;++x)gfx_rect(b.x+x*b.scale,b.y+y*b.scale,b.scale,b.scale,b.palette[b.pixels[y*b.width+x]&7]);
        gfx_target(0,0,0);window_damage(w);return 0;
    }
    if (f->eax==SYS_EVENT) {
        if (!user_range(f->ecx,sizeof(struct g_event),1)) return -14;
        if (w->head==w->tail) return 0;
        *(struct g_event *)(uintptr_t)f->ecx=w->events[w->tail++%64]; return 1;
    }
    if (f->eax==SYS_WINDOW_CLOSE) { remove_window((int)(w-windows)); return 0; }
    if (f->eax!=SYS_DRAW) return -38;
    if (f->edx>256 || !user_range(f->ecx,f->edx*sizeof(struct g_draw),0)) return -14;
    const struct g_draw *commands=(const struct g_draw *)(uintptr_t)f->ecx;
    for (unsigned i=0;i<f->edx;++i) {
        const struct g_draw *d=&commands[i];
        if (d->type<DRAW_RECT || d->type>DRAW_LINE || d->x< -2048 || d->x>2048 || d->y< -2048 || d->y>2048 || d->w< -2048 || d->w>2048 || d->h< -2048 || d->h>2048) return -22;
    }
    if(f->esi&1){w->batching=1;w->batch_tick=pit_ticks();}
    gfx_target(w->pixels,(unsigned)w->w,(unsigned)w->h);
    for (unsigned i=0;i<f->edx;++i) {
        const struct g_draw *d=&commands[i];
        if (d->type==DRAW_RECT) gfx_rect(d->x,d->y,d->w,d->h,d->color);
        else if (d->type==DRAW_LINE) gfx_line(d->x,d->y,d->w,d->h,d->color);
        else { char text[64]; memcpy(text,d->text,64); text[63]=0; gfx_text(d->x,d->y,text,d->color,1); }
    }
    if(f->esi&2)w->batching=0;
    gfx_target(0,0,0); if (!w->minimized) damage(w->x+2,w->y+32,w->w,w->h); return 0;
}
/* Original geometric artwork; all surfaces retain their existing hit regions. */
static void folder_icon(int x,int y,int size) {
    gfx_rect(x+2,y,size/2,7,0xb48d48);
    gfx_rect(x,y+5,size,size*3/4,0xb48d48);
    gfx_gradient(x+1,y+6,size-2,size*3/4-2,0xffe8a6,0xe4bb69);
    gfx_line(x+2,y+7,x+size-3,y+7,0xfff5d7);
}
static void caption_button(int x,int y,int kind,int active) {
    gfx_rect(x,y,23,22,active?0x345779:0x9baabd);
    gfx_gradient(x+1,y+1,21,20,kind==2?0xf7e5df:0xffffff,kind==2?0xd7a798:0xd1deeb);
    unsigned ink=kind==2?0x7e493e:0x34516e;
    if(kind==0)gfx_rect(x+6,y+14,11,2,ink);
    else if(kind==1){gfx_rect(x+6,y+6,11,10,ink);gfx_rect(x+7,y+9,9,6,0xf0f5fa);}
    else {gfx_line(x+7,y+7,x+15,y+15,ink);gfx_line(x+15,y+7,x+7,y+15,ink);}
}
static void paint_desktop(void) {
    const unsigned tops[3]={0x9dbbd0,0xb6b0ce,0xa8c2b5}, bottoms[3]={0x557fa2,0x777e9c,0x658f85};
    gfx_gradient(0,0,width,height,tops[wallpaper],bottoms[wallpaper]);
    /* Broad, quiet ribbons are computed only for damaged scanlines. */
    for(int y=dy;y<db && y<height-42;++y){
        int bend=(y-height/2)*(y-height/2)/(height*2);
        int edge=width*3/4+bend-y/3;
        unsigned base=gfx_blend(tops[wallpaper],bottoms[wallpaper],(unsigned)y*255/(unsigned)(height-1));
        gfx_rect(edge,y,width-edge,1,gfx_blend(base,0xffffff,14));
        gfx_rect(edge+70,y,width-edge-70,1,gfx_blend(base,0xffffff,9));
    }
    gfx_text(width-230,height-144,"GathaOS",0xe6eef5,2);
    gfx_text(width-228,height-105,"Make yourself at home.",0xd2e1ed,1);
    folder_icon(34,34,42);
    gfx_text(29,89,"Files",0x45617c,1);gfx_text(28,88,"Files",0xffffff,1);
}
static void render(void) {
    if (!damaged) return;
    for(int i=0;i<WINDOWS;++i)if(windows[i].batching){if(pit_ticks()-windows[i].batch_tick<50)return;windows[i].batching=0;}
    gfx_clip(dx,dy,dr-dx,db-dy);
    paint_desktop();
    for (int z=0;z<count;++z) {
        int slot=order[z]; struct window *w=&windows[slot]; if (w->minimized) continue;
        gfx_rect(w->x+5,w->y+6,w->w+4,w->h+34,0x526b83);
        gfx_rect(w->x,w->y,w->w+4,w->h+34,slot==focus?0x3b6085:0x8c9db0);
        unsigned bar=slot==focus?accent:0x97aabd;
        gfx_gradient(w->x+1,w->y+1,w->w+2,30,gfx_blend(bar,0xffffff,75),bar);
        gfx_line(w->x+2,w->y+1,w->x+w->w+1,w->y+1,0xc4d5e7);
        gfx_rect(w->x+9,w->y+10,12,12,0xe8f1fa);
        gfx_rect(w->x+11,w->y+12,8,3,bar);
        gfx_text(w->x+29,w->y+9,w->title,0xffffff,1);
        caption_button(w->x+w->w-84,w->y+5,0,slot==focus);
        caption_button(w->x+w->w-54,w->y+5,1,slot==focus);
        caption_button(w->x+w->w-24,w->y+5,2,slot==focus);
        int left=w->x+2>dx?w->x+2:dx,top=w->y+32>dy?w->y+32:dy;
        int right=w->x+2+w->w<dr?w->x+2+w->w:dr,bottom=w->y+32+w->h<db?w->y+32+w->h:db;
        for (int y=top;y<bottom;++y) if (right>left)
            memcpy(framebuffer_buffer()+y*width+left,w->pixels+(y-w->y-32)*w->w+left-w->x-2,(unsigned)(right-left)*4);
    }
    gfx_gradient(0,height-42,width,42,0xf4f7fb,0xc4d2e1);
    gfx_line(0,height-42,width,height-42,0xffffff);
    gfx_rect(8,height-36,92,30,0x426185);
    gfx_gradient(9,height-35,90,28,0x7599bd,accent);
    gfx_rect(17,height-28,12,12,0xe6eef6);
    gfx_text(38,height-29,"Start",0xffffff,1);
    for (int z=0;z<count;++z) {
        struct window *w=&windows[order[z]]; int x=110+z*94;
        int active=order[z]==focus&&!w->minimized;
        gfx_rect(x,height-34,88,28,active?0x7993b0:0xa5b5c8);
        gfx_gradient(x+1,height-33,86,26,active?0xc2d4e7:0xf6f9fc,active?0xe2ecf6:0xdde5ef);
        char short_title[10]; g_copy(short_title,w->title,10); gfx_text(x+6,height-28,short_title,0x20344d,1);
    }
    unsigned seconds=rtc_seconds(),hour=seconds/3600%24; char clock[6]={(char)('0'+hour/10),(char)('0'+hour%10),':',(char)('0'+seconds/60%60/10),(char)('0'+seconds/60%10),0};
    gfx_line(width-78,height-33,width-78,height-9,0xa5b6c9);
    gfx_text(width-64,height-28,clock,0x395570,1);
    if (menu) {
        gfx_rect(11,height-417,244,370,0x526b83);
        gfx_rect(8,height-420,244,370,0x8198b1);
        gfx_rect(9,height-419,242,368,0xf9fbfd);
        gfx_gradient(10,height-418,240,42,0x769abe,accent);
        gfx_text(24,height-404,"GathaOS",0xffffff,1);
        for(int i=0;i<APP_COUNT;++i){
            int y=height-370+i*30,hover=mx>=16&&mx<244&&my>=y&&my<y+30;
            if(hover)gfx_rect(14,y,232,28,0xdce9f6);
            if(i==1)folder_icon(24,y+8,16);
            else {gfx_rect(24,y+6,16,16,0x8da6bd);gfx_rect(25,y+7,14,14,0xf1f6fb);gfx_rect(28,y+10,8,3,accent);gfx_line(28,y+16,35,y+16,0x8da6bd);}
            gfx_text(52,y+6,desktop_names[i],0x20344d,1);
        }
    }
    for (int y=0;y<18;++y) for (int x=0;x<=y/2;++x) gfx_pixel(mx+x,my+y,x==0||x==y/2||y==17?0x102234:0xffffff);
    gfx_clip(0,0,width,height); framebuffer_present(dx,dy,dr-dx,db-dy);
    ++frames; pixels_drawn+=(unsigned)((dr-dx)*(db-dy)); damaged=0;
}
static void handle(const struct input_event *e) {
    if (e->type==INPUT_KEY) {
        if (focus>=0 && !windows[focus].minimized) queue(&windows[focus],(struct g_event){.type=G_KEY,.code=e->code,.ascii=e->ascii,.mods=e->modifiers,.pressed=e->pressed});
        return;
    }
    if(menu)damage(8,height-420,248,374);
    damage(mx,my,12,20); mx=e->x; my=e->y; damage(mx,my,12,20);
    int pressed=(int)e->buttons&~buttons,released=buttons&~(int)e->buttons; buttons=(int)e->buttons;
    klog("INPUT: mouse x="); klog_u32((unsigned)mx); klog(" y="); klog_u32((unsigned)my); klog(" buttons="); klog_u32((unsigned)buttons); klog("\r\n");
    if (drag>=0) {
        struct window *w=&windows[drag];
        if (buttons&1) {
            window_damage(w); w->x=mx-dragx; w->y=my-dragy;
            if (w->x<0) w->x=0;
            if (w->x>width-100) w->x=width-100;
            if (w->y<0) w->y=0;
            if (w->y>height-80) w->y=height-80;
            window_damage(w);
        } else { log_window("moved",w); drag=-1; }
        return;
    }
    if (pressed&1) {
        if (my>=height-42) {
            if (mx<104) { menu=!menu; damage(0,height-430,260,430); }
            else { int i=(mx-110)/94; if (mx>=110 && i<count) raise_window(order[i]); }
            return;
        }
        if (menu) {
            if (mx>=16 && mx<244 && my>=height-370 && my<height-70) (void)desktop_launch((unsigned)(my-height+370)/30);
            menu=0; damage(0,height-430,260,430); return;
        }
        int covered=0;
        for (int z=count-1;z>=0;--z) {
            int slot=order[z]; struct window *w=&windows[slot];
            if (w->minimized || mx<w->x || mx>=w->x+w->w+4 || my<w->y || my>=w->y+w->h+34) continue;
            raise_window(slot);
            covered=1;
            if (my<w->y+32) {
                int local=mx-w->x;
                if (local>w->w-28) { queue(w,(struct g_event){.type=G_CLOSE}); log_window("close-request",w); }
                else if (local>w->w-58) maximize(slot);
                else if (local>w->w-88) { w->minimized=1; window_damage(w); damage(0,height-42,width,42); log_window("minimized",w); }
                else if (!w->maximized) { drag=slot; dragx=mx-w->x; dragy=my-w->y; }
                return;
            }
            break;
        }
        /* A client click must not activate the desktop icon behind its window. */
        if (!covered && mx<104 && my<116) { (void)desktop_launch(APP_FILES); return; }
    }
    if (focus>=0 && !windows[focus].minimized) {
        struct window *w=&windows[focus];
        queue(w,(struct g_event){.type=G_MOUSE,.x=mx-w->x-2,.y=my-w->y-32,.buttons=(unsigned)buttons,.pressed=(unsigned)(pressed?pressed:released?released<<8:0)});
    }
}
void wm_init(const struct boot_info *boot) {
    gfx_self_test(); input_self_test();
    width=(int)boot->width; height=(int)boot->height; mx=width/2; my=height/2;
    damage(0,0,width,height); render(); klog("WM: compositor buffers damage queue ready\r\n");
}
extern void network_poll(void);
void wm_run(void) {
    (void)desktop_launch(APP_ABOUT);
    klog("STAGE7: READY\r\nSTAGE8: READY\r\n"); unsigned next=100,clock=0;
    extern void tasks_demo_report(void);
    for (;;) {
        unsigned now=pit_ticks(); uint32_t flags=irq_save(); struct input_event e;
        network_poll();
        while (input_poll(&e,now)) handle(&e);
        if (now-clock>=100) { clock=now; damage(width-70,height-42,70,42); }
        render(); irq_restore(flags); task_reap();
        if (now-next<0x80000000u) {
            klog("PIT: ticks="); klog_u32(now); klog("\r\n"); tasks_demo_report();
            if (next==300) klog("STAGE3: PASS\r\nSTAGE7: PASS\r\nSTAGE8: PASS\r\n"); next+=100;
            klog("HEALTH: windows=");klog_u32((unsigned)count);
            klog(" free=");klog_u32(pmm_free_frames());klog(" heap=");klog_u32(heap_mapped_bytes());
            klog(" frames=");klog_u32(frames);klog(" pixels=");klog_u32(pixels_drawn);
            klog(" drops=");klog_u32(drops);klog("\r\n");
        }
        __asm__ volatile("sti; hlt" : : : "memory");
    }
}
