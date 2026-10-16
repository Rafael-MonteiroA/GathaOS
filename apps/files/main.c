#include "../common.h"
static struct ui u;
static char directory[256]="/",name[80]="New.txt",status[80]="Select a file, then Open";
static struct ui_text location={.data=directory,.capacity=256},filename={.data=name,.capacity=80};
static struct g_dirent entries[128];static char labels[128][44];static const char *items[128];
static int count,selected=-1,offset;
static void refresh(void) {
    count=0;struct g_dirent entry;
    while(count<128 && g_readdir(directory,(unsigned)count,&entry)>0){entries[count]=entry;g_copy(labels[count],entry.stat.type==G_DIR?"+ ":"  ",44);g_copy(labels[count]+2,entry.name,42);items[count]=labels[count];++count;}
    selected=-1;offset=0;
}
static void open_selected(void) {
    if(selected<0 || selected>=count)return;
    char path[256];app_path(path,directory,entries[selected].name);
    if(entries[selected].stat.type==G_DIR){g_copy(directory,path,256);refresh();g_puts("FILES: directory\n");return;}
    unsigned n=(unsigned)g_strlen(path),app=APP_EDITOR;
    if(n>=4 && !g_strcmp(path+n-4,".gbm"))app=APP_PAINT;
    if(n>=4 && !g_strcmp(path+n-4,".gex"))app=99;
    app_status(status,"Open",app_open(app,path));g_puts("FILES: open\n");
}
int app_main(unsigned argument) {
    (void)argument;if(ui_open(&u,"Files",600,420)<0)return 1;refresh();int first=1,confirm=0;
    const char *menus[]={"Up","Refresh","Open"};
    for(;;){
        if(!ui_poll(&u)&&!first){g_sleep(2);continue;}first=0;
        ui_begin(&u);int menu=ui_menu(&u,menus,3);
        if(menu==0){unsigned n=(unsigned)g_strlen(directory);while(n>1 && directory[n-1]!='/')--n;if(n>1)--n;directory[n]=0;refresh();}
        if(menu==1)refresh();if(menu==2)open_selected();
        ui_textbox(&u,12,38,u.width-100,34,&location);if(ui_button(&u,u.width-78,40,66,"Go"))refresh();
        ui_label(&u,12,86,"Folders and files",UI_INK);
        int rows=(u.height-210)/24;if(rows<1)rows=1;
        int hit=ui_list(&u,12,110,u.width-48,items,count,&selected,offset,rows);
        if(hit>=0){g_copy(name,entries[hit].name,80);filename.cursor=filename.anchor=(unsigned)g_strlen(name);g_puts("FILES: selected\n");}
        ui_scrollbar(&u,u.width-32,110,rows*24,&offset,count,rows);
        int y=u.height-90;ui_textbox(&u,12,y,u.width-24,32,&filename);
        char path[256];app_path(path,directory,name);
        if(ui_button(&u,12,y+40,110,"New file")){struct g_stat existing;int r=g_stat(path,&existing);r=r==-2?app_save(path,"",0):r<0?r:-17;app_status(status,"Create",r);refresh();if(!r)g_puts("FILES: created\n");}
        if(ui_button(&u,130,y+40,110,"New folder")){app_status(status,"Mkdir",g_mkdir(path));refresh();}
        if(ui_button(&u,248,y+40,110,"Rename") && selected>=0){char old[256];app_path(old,directory,entries[selected].name);app_status(status,"Rename",g_rename(old,path));refresh();g_puts("FILES: renamed\n");}
        if(ui_button(&u,366,y+40,100,"Delete") && selected>=0)confirm=1;
        ui_label(&u,12,u.height-20,status,UI_INK);
        if(confirm && ui_dialog(&u,"Delete selected item","OK removes this item.\nPress Escape to cancel.")){
            if(selected>=0){app_path(path,directory,entries[selected].name);app_status(status,"Delete",g_unlink(path));refresh();g_puts("FILES: deleted\n");}confirm=0;
        }
        if(u.e.type==G_KEY && u.e.pressed && u.e.code==1)confirm=0;
        ui_end(&u);
    }
}
