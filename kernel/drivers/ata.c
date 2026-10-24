#include "ata.h"
#include "arch/x86/io.h"
#include "serial.h"
static uint32_t sectors;
static void delay(void) { for (unsigned i = 0; i < 4; ++i) (void)inb(0x3f6); }
/* Disk waits also run before IRQ0 exists and with interrupts masked. Reserve
   PIT channel 2 for a polled 65535-tick one-shot (~55 ms), leaving channel 0
   and the scheduler alone. CPU iteration counts are not elapsed time. */
static void wait_clock(uint8_t saved) {
    outb(0x61,saved&~3u);
    outb(0x43,0xb0);outb(0x42,255);outb(0x42,255);
    outb(0x61,(saved&~3u)|1u);
}
static int wait_ready(int data) {
    uint8_t saved=0;unsigned elapsed=0;int timing=0,result=0;
    /* The iteration ceiling is only a last resort if the PIT is broken. */
    for (unsigned i = 0; i < 1000000000u; ++i) {
        uint8_t s = inb(0x1f7);
        if (!s || s == 255) goto done;
        if (!(s & 0x80)) {
            if (s & 0x21) { klog("ATA: error status="); klog_hex(s); klog(" error="); klog_hex(inb(0x1f1)); klog("\r\n"); goto done; }
            if (!data || (s & 8)) {result=1;goto done;}
        }
        if(!timing){saved=inb(0x61);timing=1;wait_clock(saved);}
        else if(inb(0x61)&32){if(++elapsed==600)break;wait_clock(saved);}
    }
    klog("ATA: polling timeout\r\n");
done:
    if(timing)outb(0x61,saved);
    return result;
}
int ata_init(void) {
    outb(0x3f6,2); /* Polling, no IDE IRQ. */
    outb(0x1f6,0xa0); delay();
    outb(0x1f2,0); outb(0x1f3,0); outb(0x1f4,0); outb(0x1f5,0); outb(0x1f7,0xec);
    delay();
    if (!wait_ready(1) || inb(0x1f4) || inb(0x1f5)) return 0;
    uint16_t id[256]; for (unsigned i = 0; i < 256; ++i) id[i] = inw(0x1f0);
    if (!(id[49] & 0x200)) return 0;
    sectors = id[60] | ((uint32_t)id[61] << 16);
    if (sectors > 0x10000000) sectors = 0x10000000;
    klog("ATA: primary master LBA28 PIO sectors="); klog_u32(sectors); klog("\r\n");
    return sectors != 0;
}
uint32_t ata_sectors(void) { return sectors; }
static int select_sector(uint32_t sector, uint8_t command) {
    if (sector >= sectors || !wait_ready(0)) return 0;
    outb(0x1f6,0xe0 | (uint8_t)(sector >> 24)); delay();
    outb(0x1f2,1); outb(0x1f3,(uint8_t)sector); outb(0x1f4,(uint8_t)(sector >> 8));
    outb(0x1f5,(uint8_t)(sector >> 16)); outb(0x1f7,command); delay();
    return wait_ready(1);
}
int ata_read(uint32_t sector, void *data) {
    if (!select_sector(sector,0x20)) return 0;
    uint8_t *p = data;
    for (unsigned i = 0; i < 256; ++i) { uint16_t v = inw(0x1f0); p[i*2] = (uint8_t)v; p[i*2+1] = (uint8_t)(v >> 8); }
    delay(); return wait_ready(0);
}
int ata_write(uint32_t sector, const void *data) {
    if (!select_sector(sector,0x30)) return 0;
    const uint8_t *p = data;
    for (unsigned i = 0; i < 256; ++i) outw(0x1f0,(uint16_t)(p[i*2] | (uint16_t)p[i*2+1] << 8));
    delay(); return wait_ready(0);
}
int ata_flush(void) { if (!wait_ready(0)) return 0; outb(0x1f7,0xe7); delay(); return wait_ready(0); }
