/* Host-only status/clock injection into the exact ATA wait implementation. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define GATHA_X86_IO_H
static unsigned polls,ready_after,clock_reads,arms,low_byte,expired,mode;
static uint8_t speaker;
static int timeout_logged;
static uint8_t inb(uint16_t port) {
    if(port==0x1f7) {
        ++polls;
        if(mode==1)return 0;
        if(mode==2)return 255;
        if(mode==3 && polls>=ready_after)return 0x41;
        if(polls<ready_after)return 0x80;
        return mode==4?0x40:0x48;
    }
    if(port==0x61) {
        if(speaker&1){if(++clock_reads==50000)expired=1;}
        return speaker|(expired?32:0);
    }
    return 0;
}
static void outb(uint16_t port,uint8_t value) {
    if(port==0x61)speaker=value&3;
    else if(port==0x43){assert(value==0xb0);low_byte=0;}
    else if(port==0x42){assert(value==255);if(++low_byte==2){clock_reads=expired=0;++arms;}}
}
static uint16_t inw(uint16_t port){(void)port;return 0;}
static void outw(uint16_t port,uint16_t value){(void)port;(void)value;}
void klog(const char *text){if(strstr(text,"timeout"))timeout_logged=1;}
void klog_hex(uint32_t value){(void)value;}
void klog_u32(uint32_t value){(void)value;}
#include "../kernel/drivers/ata.c"
static void reset(unsigned kind,unsigned ready) {
    mode=kind;ready_after=ready;polls=clock_reads=arms=low_byte=expired=0;
    speaker=2;timeout_logged=0;
}
int main(void) {
    reset(0,1);assert(wait_ready(1)==1 && arms==0 && polls==1 && speaker==2);
    reset(0,12000000);assert(wait_ready(1)==1 && polls==12000000 && arms>1 && arms<600 && speaker==2 && !timeout_logged);
    reset(0,UINT32_MAX);assert(wait_ready(0)==0 && arms==600 && timeout_logged && speaker==2);
    reset(4,1);assert(wait_ready(0)==1 && arms==0);
    reset(4,1);assert(wait_ready(1)==0 && arms==600 && timeout_logged && speaker==2);
    reset(3,100);assert(wait_ready(0)==0 && speaker==2 && !timeout_logged);
    for(unsigned kind=1;kind<=2;++kind){reset(kind,1);assert(wait_ready(0)==0 && arms==0 && speaker==2);}
    puts("ATA NATIVE PASS: delayed readiness beyond old poll ceiling, real-clock timeout, DRQ/error/absent device and speaker restoration");
    return 0;
}
