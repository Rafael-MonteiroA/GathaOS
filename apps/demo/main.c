#include "../../libgatha/api.h"
int app_main(unsigned argument) {
    if (argument == 10) { __asm__ volatile("cli"); return 1; }
    if (argument == 11) { (void)*(volatile unsigned *)0xc0103000; return 1; }
    if (argument == 12) { *(volatile unsigned *)0x400000 = 0; return 1; }
    if (argument == 13) return 0;
    if (argument == 14) {
        int fd = g_open("/user-test.txt",G_CREATE|G_TRUNC|G_WRITE|G_READ);
        static const char data[] = "GathaFS from ring 3";
        char readback[sizeof(data)]; struct g_stat stat; struct g_dirent ent;
        int ok = fd >= 0 && g_write(fd,data,sizeof(data)) == sizeof(data) &&
            g_seek(fd,0,0) == 0 && g_read(fd,readback,sizeof(readback)) == sizeof(data) &&
            !g_strcmp(data,readback) && g_stat("/user-test.txt",&stat) == 0 && stat.size == sizeof(data) &&
            g_read(fd,(void *)0xc0103000,1) == -14 && g_close(fd) == 0 && g_close(fd) == -9 &&
            g_mkdir("/userdir") == 0 && g_rename("/user-test.txt","/userdir/file") == 0 &&
            g_readdir("/userdir",0,&ent) == 1 && !g_strcmp(ent.name,"file") &&
            g_unlink("/userdir/file") == 0 && g_unlink("/userdir") == 0 && g_sync() == 0;
        const char *message = ok ? "USERFS: all file syscalls PASS\r\n" : "USERFS: FAIL\r\n";
        g_log(message,(unsigned)g_strlen(message)); return ok ? 0 : 1;
    }
    if (g_log((const char *)0xc0103000,16) != -14 || g_call(999,0,0,0,0,0) != -38) return 2;
    const char message[] = "USERTEST: invalid pointer and syscall rejected PASS\r\n";
    g_log(message,sizeof(message)-1);
    unsigned frame = 0;
    while (g_ticks()<600) {
        (void)g_call(SYS_DEMO,frame++,0,0,0,0);
        if (argument == 1) g_sleep(3);
        else {
            /* Deliberately CPU-bound: progress of the other tasks requires preemption. */
            for (volatile unsigned spin = 0; spin < 2000000; ++spin) {}
            g_yield();
        }
    }
    return 0;
}
