#ifndef GATHA_APP_COMMON_H
#define GATHA_APP_COMMON_H
#include "../gui/toolkit/ui.h"
#include "../libgatha/memory.h"
void app_path(char out[256],const char *directory,const char *name);
int app_load(const char *path,void *data,unsigned capacity);
int app_save(const char *path,const void *data,unsigned size);
int app_open(unsigned app,const char *path);
void app_argument(char path[256]);
void app_number(struct ui *u,int x,int y,const char *label,unsigned value);
void app_status(char *to,const char *action,int result);
unsigned app_processes(struct g_process *processes,unsigned count);
#endif
