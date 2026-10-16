#include "../../gui/toolkit/ui.h"
static struct ui u;
static char single[128]="Type here",multi[1024]="A window of your own.\nTry Shift + arrows to select text.";
static struct ui_text field={.data=single,.capacity=128},document={.data=multi,.capacity=1024,.multiline=1};
int app_main(unsigned argument) {
    (void)argument;
    if (ui_open(&u,"Widgets",500,390)<0) return 1;
    if (g_call(SYS_DRAW,(unsigned)u.window,0xc0000000,1,0,0)!=-14 ||
        g_call(SYS_EVENT,0xffffffff,(uint32_t)(uintptr_t)&u.e,0,0,0)!=-9) return 2;
    g_puts("GUITEST: invalid pointer and window rejected PASS\n");
    const char *menus[]={"File","Help"},*items[]={"Ocean","Meadow","Dusk","Sunrise","Cloud","Sand","Stone","Sky"};
    int checked=1,selected=0,offset=0,dialog=0,context=0,cx=0,cy=0,first=1;
    for (;;) {
        int event=ui_poll(&u);
        if (!event && !first) { g_sleep(2); continue; } first=0;
        if (u.e.type==G_MOUSE && u.e.pressed==2) { context=1; cx=u.e.x; cy=u.e.y; if(cx>u.width-180)cx=u.width-180; if(cy>u.height-70)cy=u.height-70; }
        ui_begin(&u);
        if (ui_menu(&u,menus,2)==1) dialog=1;
        ui_label(&u,18,44,"Make yourself at home.",UI_INK);
        if (!dialog && !context) {
            if (ui_button(&u,18,74,130,"New window")) { ui_launch(APP_WIDGETS); g_puts("WIDGET: launch\n"); }
            if (ui_button(&u,164,74,130,"Show dialog")) dialog=1;
        } else { ui_rect(&u,18,74,276,28,0xc8d6e3); }
        struct g_event saved=u.e;
        if (dialog || context) u.e.type=0;
        if (ui_checkbox(&u,18,118,"Enable preview",&checked)) g_puts("WIDGET: checkbox\n");
        if (ui_textbox(&u,18,154,278,34,&field)) g_puts("WIDGET: single changed\n");
        if (ui_textbox(&u,18,202,278,150,&document)) g_puts("WIDGET: multi changed\n");
        if (ui_list(&u,318,74,140,items,8,&selected,offset,7)>=0) g_puts("WIDGET: list\n");
        if (ui_scrollbar(&u,462,74,168,&offset,8,7)) g_puts("WIDGET: scroll\n");
        ui_label(&u,318,270,checked?"Preview on":"Preview off",UI_ACCENT);
        ui_label(&u,18,u.height-24,"Right-click for menu. Help opens a dialog.",0x61768a);
        u.e=saved;
        if (context) { const char *actions[]={"New window","About"}; int action=ui_context(&u,cx,cy,actions,2); if(action>=0){ if(!action)ui_launch(APP_WIDGETS);else dialog=1; context=0; } }
        if (dialog && ui_dialog(&u,"GathaOS toolkit","Original widgets.\nRunning in user mode.")) { dialog=0; g_puts("WIDGET: dialog closed\n"); }
        ui_end(&u);
    }
}
