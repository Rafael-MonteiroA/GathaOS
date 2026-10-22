#include "internal.h"
static uint32_t transaction=0x47415448,server,offered,next_send,expiry,renewal;
static unsigned phase,attempts;
static void restart(void) { phase=0;attempts=0;next_send=0;server=offered=0;++transaction;net_lost(); }
static void send_message(unsigned type) {
    uint8_t p[300]={0};p[0]=1;p[1]=1;p[2]=6;write32(p+4,transaction);write16(p+10,0x8000);
    if(phase==3)write32(p+12,net.address);
    memcpy(p+28,net.mac,6);write32(p+236,0x63825363);unsigned i=240;
    p[i++]=53;p[i++]=1;p[i++]=(uint8_t)type;
    p[i++]=61;p[i++]=7;p[i++]=1;memcpy(p+i,net.mac,6);i+=6;
    if(phase==1){p[i++]=50;p[i++]=4;write32(p+i,offered);i+=4;p[i++]=54;p[i++]=4;write32(p+i,server);i+=4;}
    p[i++]=55;p[i++]=4;p[i++]=1;p[i++]=3;p[i++]=6;p[i++]=51;p[i]=255;
    (void)net_udp_send(0xffffffffu,68,67,p,sizeof(p));
}
void dhcp_poll(void) {
    if(!net.state)return;
    uint32_t now=pit_ticks();
    if(net.state==2 && due(now,expiry)){restart();return;}
    if(phase==2){if(!due(now,renewal))return;phase=3;next_send=now;attempts=0;++transaction;}
    if(!due(now,next_send))return;
    if(phase==1 && attempts>=4){restart();return;}
    send_message(phase==0?1:3);
    unsigned seconds=1u<<(attempts<5?attempts:5);next_send=now+seconds*100;if(attempts<6)++attempts;
}
void dhcp_receive(const uint8_t *p,unsigned n) {
    if(n<241 || p[0]!=2 || p[1]!=1 || p[2]!=6 || read32(p+4)!=transaction ||
       memcmp(p+28,net.mac,6) || read32(p+236)!=0x63825363)return;
    uint32_t mask=0,gateway=0,dns=0,lease=0,id=0;unsigned type=0,ended=0;
    for(unsigned i=240;i<n;) {
        unsigned option=p[i++];if(option==255){ended=1;break;}if(!option)continue;
        if(i>=n)return;
        unsigned size=p[i++];if(size>n-i)return;
        if(option==53 && size==1)type=p[i];
        if(option==1 && size==4)mask=read32(p+i);
        if(option==3 && size>=4 && !(size%4))gateway=read32(p+i);
        if(option==6 && size>=4 && !(size%4))dns=read32(p+i);
        if(option==51 && size==4)lease=read32(p+i);
        if(option==54 && size==4)id=read32(p+i);
        i+=size;
    }
    if(!ended || !id)return;
    if(type==2 && phase==0) {
        offered=read32(p+16);if(!offered || (offered>>24)>=224)return;
        server=id;phase=1;attempts=0;next_send=pit_ticks();klog("NET: DHCP offer\r\n");return;
    }
    if(phase!=1 && phase!=3)return;
    if(id!=server && phase!=3)return;
    if(type==6){restart();return;}
    if(type!=5)return;
    uint32_t address=read32(p+16),inverse=~mask;
    if(!address || (phase==1 && address!=offered) || (phase==3 && address!=net.address) ||
       (address>>24)>=224 || !mask || (inverse&(inverse+1)) || !lease || !dns ||
       (gateway && (gateway&mask)!=(address&mask)))return;
    /* Bound tick arithmetic to less than half its range, including infinite leases. */
    if(lease>86400)lease=86400;
    net.address=address;net.mask=mask;net.gateway=gateway;net.dns=dns;server=id;
    uint32_t now=pit_ticks();expiry=now+lease*100;renewal=now+lease*50;phase=2;net.state=2;
    klog("NET: DHCP bound address=");klog_hex(address);klog(" dns=");klog_hex(dns);klog("\r\n");
}
