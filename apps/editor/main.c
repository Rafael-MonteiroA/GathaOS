#include "../common.h"
static struct ui u;
#define DOCUMENT_CAPACITY (256u*1024u)
static char path[256]="/Documents/Untitled.txt",*data,status[80]="Ready";
static struct ui_text filename={.data=path,.capacity=256},document={.capacity=DOCUMENT_CAPACITY,.multiline=1,.focused=1};
static int dirty,dialog,action;
static void load(void) {
    char *pending=g_malloc(DOCUMENT_CAPACITY);
    int r=pending?app_load(path,pending,DOCUMENT_CAPACITY-1):-12;
    if(r>=0){pending[r]=0;g_free(data);data=pending;document.data=data;document.cursor=document.anchor=document.scroll=0;dirty=0;}
    else g_free(pending);
    app_status(status,"Open",r);g_puts(r>=0?"EDITOR: opened\n":"EDITOR: open error\n");
}
static void save(void) {
    int r=app_save(path,data,(unsigned)g_strlen(data));app_status(status,"Save",r);
    if(!r){dirty=0;g_puts("EDITOR: saved\n");}
}
int app_main(unsigned argument) {
    data=g_calloc(DOCUMENT_CAPACITY,1);if(!data)return 1;document.data=data;
    (void)argument;char initial[256];app_argument(initial);if(*initial){g_copy(path,initial,256);load();}
    if(ui_open(&u,"Text Editor",620,440)<0)return 1;
    const char *menus[]={"New","Open","Save"};int first=1;
    for(;;){
        if(!ui_poll(&u)&&!first){g_sleep(2);continue;}first=0;
        ui_begin(&u);int menu=ui_menu(&u,menus,3);
        if(u.e.type==G_KEY && u.e.pressed && (u.e.mods&2) && (u.e.ascii=='s'||u.e.ascii=='S'))menu=2;
        if(menu==2)save();
        if(menu==0||menu==1){if(dirty){dialog=1;action=menu;}else if(menu==1)load();else{data[0]=0;document.cursor=document.anchor=0;}}
        struct g_event e=u.e;if(dialog)u.e.type=0;
        ui_label(&u,12,40,"Path",UI_INK);ui_textbox(&u,56,34,u.width-68,34,&filename);
        if(ui_textbox(&u,12,80,u.width-24,u.height-118,&document))dirty=1;
        ui_label(&u,12,u.height-26,dirty?"Modified - Ctrl+S saves":status,dirty?UI_ACCENT:UI_INK);
        u.e=e;
        if(dialog && ui_dialog(&u,"Unsaved changes","OK discards current edits.\nPress Escape to keep them.")){dialog=0;dirty=0;if(action==1)load();else{data[0]=0;document.cursor=document.anchor=0;}}
        if(u.e.type==G_KEY && u.e.code==1 && u.e.pressed)dialog=0;
        ui_end(&u);
    }
}
