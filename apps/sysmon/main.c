#include "../common.h"
static struct ui u;
int app_main(unsigned argument) {
    (void)argument;if(ui_open(&u,"System Monitor",560,440)<0)return 1;unsigned last=0;int first=1;
    for(;;){int event=ui_poll(&u);if(!event&&!first&&g_ticks()-last<100){g_sleep(5);continue;}first=0;last=g_ticks();
        struct g_info info;struct g_process ps[16];ui_info(&info);unsigned count=app_processes(ps,16);
        ui_begin(&u);app_number(&u,16,18,"Free memory KiB: ",info.free_frames*4);app_number(&u,16,46,"Kernel heap KiB: ",info.heap_bytes/1024);
        app_number(&u,290,18,"Uptime seconds: ",info.ticks/100);app_number(&u,290,46,"Frames: ",info.frames);
        ui_label(&u,16,88,"PID     STATE     CPU TICKS     PROCESS",UI_ACCENT);
        for(unsigned i=0;i<count && i<14;++i){int y=118+(int)i*20;char num[16];g_decimal(num,(int)ps[i].pid);ui_label(&u,16,y,num,UI_INK);ui_label(&u,80,y,ps[i].state==2?"sleep":"ready",UI_INK);g_decimal(num,(int)ps[i].ticks);ui_label(&u,168,y,num,UI_INK);ui_label(&u,288,y,ps[i].name,UI_INK);}
        ui_end(&u);g_puts("SYSMON: refreshed\n");
    }
}
