#include "wm.h"
#include "sched/task.h"
extern const unsigned char widgets_gex[];
extern const unsigned widgets_gex_size;
int desktop_launch(unsigned app) {
    if (app) return -22;
    return task_spawn(widgets_gex,widgets_gex_size,0,"Widgets");
}
