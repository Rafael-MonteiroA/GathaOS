#include "../common.h"
static struct ui u;
int app_main(unsigned argument) {
    (void)argument;if(ui_open(&u,"About GathaOS",480,300)<0)return 1;int first=1;
    for(;;){if(!ui_poll(&u)&&!first){g_sleep(5);continue;}first=0;ui_begin(&u);
        ui_rect(&u,24,24,52,52,UI_ACCENT);ui_label(&u,40,42,"G",0xffffff);ui_label(&u,96,30,"GathaOS",UI_INK);ui_label(&u,96,56,"An original operating system",UI_ACCENT);
        ui_label(&u,24,108,"Custom BIOS loader, kernel, GathaFS,\nGEX programs and window toolkit.\n\nx86 / 32-bit / NASM + freestanding C\nOriginal bitmap font. No external guest runtime.",UI_INK);
        if(ui_button(&u,340,250,112,"Close")){ui_close(&u);return 0;}ui_end(&u);
    }
}
