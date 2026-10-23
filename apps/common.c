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
    return g_save(path,data,size);
}
int app_open(unsigned app,const char *path) { return g_call(SYS_LAUNCH,app,(uint32_t)(uintptr_t)path,0,0,0); }
void app_argument(char path[256]) { if(g_call(SYS_ARGUMENT,(uint32_t)(uintptr_t)path,256,0,0,0)<0)path[0]=0; }
void app_number(struct ui *u,int x,int y,const char *label,unsigned value) {
    char line[100];g_copy(line,label,70);g_decimal(line+g_strlen(line),(int)value);ui_label(u,x,y,line,UI_INK);
}
void app_status(char *to,const char *action,int result) {
    g_copy(to,action,80);unsigned n=(unsigned)g_strlen(to);
    const char *message=0;
    switch(result) {
    case -2:message=": Name or file not found";break;
    case -71:message=": Invalid or unsuccessful HTTP response";break;
    case -90:message=": Network message exceeds supported limits";break;
    case -95:message=": Unsupported protocol or encoding";break;
    case -61:message=": Secure randomness unavailable (requires RDRAND)";break;
    case -62:message=": Invalid UTC clock; check VM date and time";break;
    case -74:message=": TLS protocol or authentication failure";break;
    case -129:message=": TLS certificate rejected";break;
    case -100:message=": Network offline or waiting for DHCP";break;
    case -104:message=": Connection reset";break;
    case -110:message=": Connection timed out";break;
    case -111:message=": Connection refused";break;
    case -113:message=": Host or DNS unavailable";break;
    case -125:message=": Cancelled";break;
    case -5:message=": Disk I/O error";break;
    case -12:message=": Not enough memory";break;
    case -16:message=": File is in use";break;
    case -27:message=": File is too large";break;
    case -28:message=": Not enough free disk space";break;
    case -30:message=": Disk is read-only after an error";break;
    default:break;
    }
    if(message)g_copy(to+n,message,80-n);
    else if(result<0){g_copy(to+n," error ",80-n);g_decimal(to+g_strlen(to),result);}else g_copy(to+n," OK",80-n);
}
unsigned app_processes(struct g_process *processes,unsigned count) {
    int n=g_call(SYS_PROCESSES,(uint32_t)(uintptr_t)processes,count,0,0,0);return n>0?(unsigned)n:0;
}
