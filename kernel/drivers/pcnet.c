#include "pcnet.h"
#include "arch/x86/io.h"
#include "mm/memory.h"
#include "drivers/serial.h"
#include "../libgatha/memory.h"

/* AMD 19436, software style 2. DMA storage stays in the pinned kernel image. */
struct descriptor { uint32_t address; uint16_t length, status; uint32_t count, reserved; };
struct initialization {
    uint16_t mode; uint8_t receive_length, transmit_length, mac[6];
    uint16_t reserved; uint32_t filter[2], receive, transmit;
};
_Static_assert(sizeof(struct descriptor)==16 && sizeof(struct initialization)==28,"PCnet layout");
static volatile struct descriptor rx[32] __attribute__((aligned(16)));
static volatile struct descriptor tx[8] __attribute__((aligned(16)));
static struct initialization init __attribute__((aligned(16)));
static uint8_t receive_buffers[32][2048] __attribute__((aligned(16)));
static uint8_t transmit_buffers[8][1536] __attribute__((aligned(16)));
static uint16_t base;
static unsigned receive_index, transmit_index;
static uint32_t physical(const volatile void *p) { return (uint32_t)(uintptr_t)p-KERNEL_BASE; }
static uint32_t pci_read(uint32_t address, unsigned reg) { outl(0xcf8,address|reg); return inl(0xcfc); }
static uint16_t csr(unsigned reg) { outw(base+0x12,(uint16_t)reg); return inw(base+0x10); }
static void set_csr(unsigned reg,uint16_t value) { outw(base+0x12,(uint16_t)reg);outw(base+0x10,value); }
static void set_bcr(unsigned reg,uint16_t value) { outw(base+0x12,(uint16_t)reg);outw(base+0x16,value); }
int pcnet_init(uint8_t mac[6]) {
    uint32_t device=0;
    for(unsigned bus=0;bus<256 && !device;++bus)
        for(unsigned slot=0;slot<32 && !device;++slot) {
            uint32_t address=0x80000000u|(bus<<16)|(slot<<11);
            if(pci_read(address,0)==0xffffffffu)continue;
            unsigned functions=(pci_read(address,12)&0x800000u)?8:1;
            for(unsigned f=0;f<functions;++f)
                if(pci_read(address|(f<<8),0)==0x20001022u){device=address|(f<<8);break;}
        }
    if(!device)return -19;
    uint32_t bar=pci_read(device,16);
    if(!(bar&1) || (bar&~3u)>0xffe0 || !(bar&~3u))return -19;
    base=(uint16_t)(bar&~3u);
    uint16_t command=(uint16_t)pci_read(device,4);
    outl(0xcf8,device|4);outw(0xcfc,command|5); /* I/O and bus mastering; preserve status. */
    (void)inw(base+0x14);set_csr(0,4);set_bcr(20,2);set_bcr(2,2);
    for(unsigned i=0;i<6;++i)init.mac[i]=mac[i]=inb(base+i);
    init.receive_length=5<<4;init.transmit_length=3<<4;
    init.receive=physical(rx);init.transmit=physical(tx);
    for(unsigned i=0;i<32;++i){rx[i].address=physical(receive_buffers[i]);rx[i].length=(uint16_t)-2048;rx[i].status=0x8000;}
    for(unsigned i=0;i<8;++i)tx[i].address=physical(transmit_buffers[i]);
    uint32_t p=physical(&init);set_csr(1,(uint16_t)p);set_csr(2,(uint16_t)(p>>16));
    __asm__ volatile("" ::: "memory");set_csr(0,1);
    unsigned limit=100000;while(limit && !(csr(0)&0x100))--limit;
    if(!limit){set_csr(0,4);base=0;return -5;}
    set_csr(0,0x102); /* Polling, without enabling a shared PCI IRQ. */
    klog("NET: PCnet PCI DMA ready\r\n");return 0;
}
int pcnet_send(const void *frame,unsigned length) {
    if(!base)return -19;
    if(length<14 || length>1514)return -22;
    volatile struct descriptor *d=&tx[transmit_index];
    if(d->status&0x8000)return -11;
    memcpy(transmit_buffers[transmit_index],frame,length);
    if(length<60){memset(transmit_buffers[transmit_index]+length,0,60-length);length=60;}
    d->count=0;d->length=(uint16_t)-(int)length;
    __asm__ volatile("" ::: "memory");d->status=0x8300;
    transmit_index=(transmit_index+1)%8;set_csr(0,8);return 0;
}
int pcnet_receive(void *frame) {
    if(!base)return 0;
    uint16_t status=csr(0);
    if(status&0x800){set_csr(0,4);base=0;return -5;}
    set_csr(0,status&0x7f00);
    volatile struct descriptor *d=&rx[receive_index];
    if(d->status&0x8000)return 0;
    __asm__ volatile("" ::: "memory");
    unsigned length=d->count&0xfff;
    int valid=(d->status&0x4300)==0x300 && length>=18 && length<=1518;
    if(valid)memcpy(frame,receive_buffers[receive_index],length-4);
    d->count=0;d->length=(uint16_t)-2048;
    __asm__ volatile("" ::: "memory");d->status=0x8000;
    receive_index=(receive_index+1)%32;
    return valid?(int)length-4:-1;
}
