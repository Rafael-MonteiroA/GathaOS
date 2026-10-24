#include "../common.h"
static struct ui u;
int app_main(unsigned argument) {
    (void)argument;
    if(ui_open(&u,"About GathaOS",480,300)<0)return 1;
    int first=1;
    for(;;){
        if(!ui_poll(&u)&&!first){g_sleep(5);continue;}first=0;ui_begin(&u);
        ui_rect(&u,0,0,u.width,94,0xe4edf6);
        ui_rect(&u,24,22,52,52,0x426d9c);
        ui_rect(&u,27,25,46,22,0x608bb9);
        ui_label(&u,46,40,"G",0xffffff);
        ui_label(&u,96,28,"GathaOS",UI_INK);
        ui_label(&u,96,52,"A small world of your own.",0x526f8c);
        ui_label(&u,28,116,"Welcome to your desktop.",UI_INK);
        ui_label(&u,28,150,"Write a note. Draw something. Make it yours.\nYour apps are waiting in the Start menu.",0x526477);
        ui_line(&u,28,216,u.width-28,216,0xd2dae3);
        ui_label(&u,28,234,"Stage 13 / Web forms",UI_INK);
        ui_label(&u,28,258,"Made from scratch, with care.",0x526477);
        if(ui_button(&u,u.width-132,250,104,"Close")){ui_close(&u);return 0;}
        ui_end(&u);
    }
}
