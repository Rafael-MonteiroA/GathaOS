#include "syscall.h"
#include "net/network.h"
#include "../libgatha/network.h"
#include "../libgatha/security.h"
#include "drivers/security.h"
#include "power.h"
#include "../gui/wm/wm.h"
#include "../gui/wm/protocol.h"
#include "sched/task.h"
#include "mm/memory.h"
#include "arch/x86/pit.h"
#include "drivers/serial.h"
#include "../libgatha/api.h"
static int path_copy(uint32_t address, char path[256]) {
    for (unsigned i = 0; i < 256; ++i) {
        if (!user_range(address+i,1,0)) return -14;
        path[i] = *(const char *)(uintptr_t)(address+i);
        if (!path[i]) return 0;
    }
    return -36;
}
static int file_call(struct interrupt_frame *f) {
    struct task *t = task_current();
    char path[256], second[256];
    if (f->eax == SYS_SYNC) return fs_sync();
    if (f->eax == SYS_CLOSE) return vfs_close(t->files,f->ebx);
    if (f->eax == SYS_READ || f->eax == SYS_WRITE || f->eax == SYS_SEEK) {
        if (f->ebx >= 8 || !t->files[f->ebx].used) return -9;
        struct vfs_fd *fd = &t->files[f->ebx];
        if (f->eax == SYS_SEEK) {
            if (f->edx > 2) return -22;
            int64_t base = f->edx == 0 ? 0 : f->edx == 1 ? fd->offset : fs_node(fd->inode)->size;
            int64_t position = base + (int32_t)f->ecx;
            if (position < 0 || position > fs_node(fd->inode)->size) return -22;
            fd->offset = (unsigned)position; return (int)position;
        }
        int write = f->eax == SYS_WRITE;
        if (!(fd->flags & (write ? G_WRITE : G_READ))) return -13;
        if (f->edx > FS_MAX_SIZE || !user_range(f->ecx,f->edx,!write)) return -14;
        int result = write ? fs_write(fd->inode,fd->offset,(const void *)(uintptr_t)f->ecx,f->edx) :
                             fs_read(fd->inode,fd->offset,(void *)(uintptr_t)f->ecx,f->edx);
        if (result > 0) fd->offset += (unsigned)result;
        return result;
    }
    int result = path_copy(f->ebx,path); if (result < 0) return result;
    switch (f->eax) {
    case SYS_SAVE:
        if (f->edx > fs_max_size()) return -27;
        if (!user_range(f->ecx,f->edx,0)) return -14;
        return fs_replace(path,(const void *)(uintptr_t)f->ecx,f->edx);
    case SYS_OPEN: return vfs_open(t->files,path,f->ecx);
    case SYS_MKDIR: result = fs_create(path,G_DIR); return result < 0 ? result : 0;
    case SYS_UNLINK: return fs_unlink(path);
    case SYS_RENAME:
        result = path_copy(f->ecx,second); return result < 0 ? result : fs_rename(path,second);
    case SYS_STAT:
        if (!user_range(f->ecx,sizeof(struct g_stat),1)) return -14;
        return fs_stat(path,(struct g_stat *)(uintptr_t)f->ecx);
    case SYS_READDIR:
        if (!user_range(f->edx,sizeof(struct g_dirent),1)) return -14;
        return fs_readdir(path,f->ecx,(struct g_dirent *)(uintptr_t)f->edx);
    default: return -38;
    }
}
struct interrupt_frame *syscall_dispatch(struct interrupt_frame *f) {
    struct task *t = task_current();
    if ((f->eax >= SYS_OPEN && f->eax <= SYS_SYNC) || f->eax == SYS_SAVE) { f->eax = (uint32_t)file_call(f); return f; }
    if(f->eax==SYS_NETWORK){f->eax=(uint32_t)network_call(f);return f;}
    if(f->eax==SYS_SECURITY){f->eax=(uint32_t)security_call(f);return f;}
    if(f->eax==SYS_POWER){f->eax=(uint32_t)system_power(f->ebx);return f;}
    if (f->eax >= SYS_WINDOW && f->eax <= SYS_BITMAP) { f->eax=(uint32_t)wm_call(f); return f; }
    switch (f->eax) {
    case SYS_SBRK: f->eax = (uint32_t)task_sbrk((int32_t)f->ebx); break;
    case SYS_TICKS: f->eax = pit_ticks(); break;
    case SYS_PID: f->eax = t->pid; break;
    case SYS_YIELD: f->eax = 0; return schedule(f,1);
    case SYS_SLEEP:
        if (f->ebx > 0x7fffffffu) { f->eax = (uint32_t)-22; break; }
        if (f->ebx) { t->wake = pit_ticks() + f->ebx; t->state = TASK_SLEEPING; }
        f->eax = 0; return schedule(f,1);
    case SYS_EXIT: return task_exit(f,(int)f->ebx);
    case SYS_LOG:
        if (f->ecx > 1024 || !user_range(f->ebx,f->ecx,0)) { f->eax = (uint32_t)-14; break; }
        for (unsigned i = 0; i < f->ecx; ++i) { char s[2] = {((const char *)(uintptr_t)f->ebx)[i],0}; klog(s); }
        f->eax = f->ecx; break;
    case SYS_DEMO: f->eax = (uint32_t)tasks_demo_draw(f->ebx); break;
    default: f->eax = (uint32_t)-38; break;
    }
    return f;
}
