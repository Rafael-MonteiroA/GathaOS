#include "../../libgatha/api.h"
int app_main(unsigned argument) {
    if (argument == 10) { __asm__ volatile("cli"); return 1; }
    if (argument == 11) { (void)*(volatile unsigned *)0xc0103000; return 1; }
    if (argument == 12) { *(volatile unsigned *)0x400000 = 0; return 1; }
    if (argument == 13) return 0;
    if (g_log((const char *)0xc0103000,16) != -14 || g_call(999,0,0,0,0,0) != -38) return 2;
    const char message[] = "USERTEST: invalid pointer and syscall rejected PASS\r\n";
    g_log(message,sizeof(message)-1);
    unsigned frame = 0;
    for (;;) {
        (void)g_call(SYS_DEMO,frame++,0,0,0,0);
        if (argument == 1) g_sleep(3);
        else {
            /* Deliberately CPU-bound: progress of the other tasks requires preemption. */
            for (volatile unsigned spin = 0; spin < 2000000; ++spin) {}
            g_yield();
        }
    }
}
