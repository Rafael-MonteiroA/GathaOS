#include "wm.h"
#include "protocol.h"
#include "sched/task.h"
#include "mm/memory.h"
#include "../../libgatha/api.h"
#define IMAGE(name) extern const unsigned char name##_gex[]; extern const unsigned name##_gex_size
IMAGE(terminal);IMAGE(files);IMAGE(editor);IMAGE(calculator);IMAGE(settings);IMAGE(sysmon);IMAGE(paint);IMAGE(about);IMAGE(widgets);
const char *desktop_names[]={"Terminal","Files","Text Editor","Calculator","Settings","System Monitor","Paint","About GathaOS","Widgets"};
int desktop_open(unsigned app,const char *path) {
    const unsigned char *images[]={terminal_gex,files_gex,editor_gex,calculator_gex,settings_gex,sysmon_gex,paint_gex,about_gex,widgets_gex};
    const unsigned sizes[]={terminal_gex_size,files_gex_size,editor_gex_size,calculator_gex_size,settings_gex_size,sysmon_gex_size,paint_gex_size,about_gex_size,widgets_gex_size};
    int pid;
    if(app==99){
        int node=fs_lookup(path);if(node<0)return node;const struct fs_inode *n=fs_node((unsigned)node);
        if(n->type!=G_FILE || n->size<32)return -22;
        void *image=kmalloc(n->size);if(!image)return -12;
        int bytes=fs_read((unsigned)node,0,image,n->size);pid=bytes<0?bytes:task_spawn(image,(unsigned)bytes,0,"GEX program");kfree(image);
    }else{if(app>APP_WIDGETS)return -22;pid=task_spawn(images[app],sizes[app],0,desktop_names[app]);}
    if(pid>0){struct task *tasks=(struct task *)(uintptr_t)task_table();for(unsigned i=1;i<TASK_MAX;++i)if(tasks[i].pid==(unsigned)pid){g_copy(tasks[i].path,path,256);break;}}
    return pid;
}
int desktop_launch(unsigned app) { return desktop_open(app,""); }
