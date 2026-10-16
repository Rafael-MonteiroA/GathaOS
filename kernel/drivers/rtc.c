#include "rtc.h"
#include "arch/x86/io.h"
static unsigned read_reg(unsigned r) { outb(0x70,(uint8_t)(r | 0x80)); io_wait(); return inb(0x71); }
static unsigned number(unsigned n, unsigned binary) { return binary ? n : (n >> 4)*10 + (n & 15); }
uint32_t rtc_seconds(void) {
    uint32_t flags = irq_save();
    unsigned raw[7], next[7], regs[7] = {0,2,4,7,8,9,11};
    int stable = 0;
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        unsigned wait = 100000;
        while ((read_reg(10) & 128) && --wait) {}
        if (!wait) break;
        for (unsigned i = 0; i < 7; ++i) raw[i] = read_reg(regs[i]);
        for (unsigned i = 0; i < 7; ++i) next[i] = read_reg(regs[i]);
        stable = !(read_reg(10) & 128);
        for (unsigned i = 0; i < 7; ++i) if (raw[i] != next[i]) stable = 0;
        if (stable) break;
    }
    outb(0x70,0); irq_restore(flags);
    if (!stable) return 0;
    unsigned binary = raw[6] & 4, hour = number(raw[2] & 127,binary);
    if (!(raw[6] & 2)) hour = hour % 12 + ((raw[2] & 128) ? 12 : 0);
    unsigned sec = number(raw[0],binary), min = number(raw[1],binary);
    unsigned day = number(raw[3],binary), month = number(raw[4],binary), year = 2000 + number(raw[5],binary);
    if (sec > 59 || min > 59 || hour > 23 || day < 1 || day > 31 || month < 1 || month > 12 || year > 2099) return 0;
    static const unsigned days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t total = 0;
    for (unsigned y = 1970; y < year; ++y) total += 365 + (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    for (unsigned m = 1; m < month; ++m) total += days[m-1] + (m == 2 && year % 4 == 0);
    return ((total + day - 1)*24 + hour)*3600 + min*60 + sec;
}
