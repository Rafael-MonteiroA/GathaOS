#include "common.h"
void app_path(char out[256],const char *directory,const char *name) {
    if (*name=='/') { g_copy(out,name,256); return; }
    g_copy(out,directory,256); unsigned n=(unsigned)g_strlen(out);
    if (n && out[n-1]!='/' && n<255) out[n++]='/';
    g_copy(out+n,name,256-n);
}
int app_load(const char *path,void *data,unsigned capacity) {
    struct g_stat info; int r=g_stat(path,&info); if(r<0)return r;
    if(info.size>capacity)return -27;
    int fd=g_open(path,G_READ);if(fd<0)return fd;
    r=g_read(fd,data,capacity);g_close(fd);return r;
}
int app_save(const char *path,const void *data,unsigned size) {
    int fd=g_open(path,G_WRITE|G_CREATE|G_TRUNC);if(fd<0)return fd;
    int r=g_write(fd,data,size);int closed=g_close(fd);
    if(r!=(int)size)return r<0?r:-5;
    return closed<0?closed:g_sync();
}
int app_open(unsigned app,const char *path) { return g_call(SYS_LAUNCH,app,(uint32_t)(uintptr_t)path,0,0,0); }
void app_argument(char path[256]) { if(g_call(SYS_ARGUMENT,(uint32_t)(uintptr_t)path,256,0,0,0)<0)path[0]=0; }
void app_number(struct ui *u,int x,int y,const char *label,unsigned value) {
    char line[100];g_copy(line,label,70);g_decimal(line+g_strlen(line),(int)value);ui_label(u,x,y,line,UI_INK);
}
void app_status(char *to,const char *action,int result) {
    g_copy(to,action,80);unsigned n=(unsigned)g_strlen(to);
    if(result<0){g_copy(to+n," error ",80-n);g_decimal(to+g_strlen(to),result);}else g_copy(to+n," OK",80-n);
}
unsigned app_processes(struct g_process *processes,unsigned count) {
    int n=g_call(SYS_PROCESSES,(uint32_t)(uintptr_t)processes,count,0,0,0);return n>0?(unsigned)n:0;
}
