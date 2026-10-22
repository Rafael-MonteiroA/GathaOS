#include "internal.h"
#include "drivers/pcnet.h"
struct g_network net;
static uint8_t incoming[1518],outgoing[1514];
static struct { uint32_t address,time;uint8_t mac[6]; } neighbors[8];
static unsigned replacement,identification;
static uint32_t arp_target,arp_time;
static const uint8_t broadcast[6]={255,255,255,255,255,255};
uint16_t net_checksum(const uint8_t *p,unsigned length,uint32_t sum) {
    while(length>1){sum+=read16(p);p+=2;length-=2;}
    if(length)sum+=(unsigned)*p<<8;
    while(sum>>16)sum=(sum&65535)+(sum>>16);
    return (uint16_t)~sum;
}
uint32_t net_pseudo(uint32_t source,uint32_t destination,unsigned protocol,unsigned length) {
    return (source>>16)+(source&65535)+(destination>>16)+(destination&65535)+protocol+length;
}
static int ethernet(const uint8_t mac[6],unsigned type,const void *data,unsigned size) {
    memcpy(outgoing,mac,6);memcpy(outgoing+6,net.mac,6);write16(outgoing+12,type);
    memmove(outgoing+14,data,size);
    int result=pcnet_send(outgoing,size+14);if(!result)++net.sent;return result;
}
static void arp(unsigned operation,uint32_t target,const uint8_t mac[6]) {
    uint8_t p[28]={0};write16(p,1);write16(p+2,0x800);p[4]=6;p[5]=4;write16(p+6,operation);
    memcpy(p+8,net.mac,6);write32(p+14,net.address);
    if(operation==2)memcpy(p+18,mac,6);
    write32(p+24,target);(void)ethernet(mac,0x806,p,28);
}
int net_ip_send(uint32_t destination,unsigned protocol,const void *data,unsigned size) {
    if(!net.state)return -100;
    if(size>1480)return -90;
    const uint8_t *mac=broadcast;
    if(destination!=0xffffffffu) {
        if(net.state!=2)return -100;
        uint32_t hop=(destination&net.mask)==(net.address&net.mask)?destination:net.gateway;
        if(!hop)return -113;
        mac=0;uint32_t now=pit_ticks();
        for(unsigned i=0;i<8;++i)if(neighbors[i].address==hop && now-neighbors[i].time<6000){mac=neighbors[i].mac;break;}
        if(!mac){if(hop!=arp_target || now-arp_time>=50){arp_target=hop;arp_time=now;arp(1,hop,broadcast);}return -11;}
    }
    uint8_t p[1500]={0};p[0]=0x45;write16(p+2,size+20);write16(p+4,++identification);
    write16(p+6,0x4000);p[8]=64;p[9]=(uint8_t)protocol;write32(p+12,net.address);write32(p+16,destination);
    write16(p+10,net_checksum(p,20,0));memcpy(p+20,data,size);return ethernet(mac,0x800,p,size+20);
}
int net_udp_send(uint32_t destination,unsigned source_port,unsigned port,const void *data,unsigned size) {
    if(size>1472)return -90;
    uint8_t p[1480];write16(p,source_port);write16(p+2,port);write16(p+4,size+8);write16(p+6,0);memcpy(p+8,data,size);
    uint16_t sum=net_checksum(p,size+8,net_pseudo(net.address,destination,17,size+8));write16(p+6,sum?sum:65535);
    return net_ip_send(destination,17,p,size+8);
}
static void receive_arp(const uint8_t *p,unsigned n,const uint8_t *ether_source) {
    if(n<28 || read16(p)!=1 || read16(p+2)!=0x800 || p[4]!=6 || p[5]!=4 ||
       (read16(p+6)!=1 && read16(p+6)!=2) || memcmp(p+8,ether_source,6) || (p[8]&1))return;
    uint32_t sender=read32(p+14),target=read32(p+24);
    if(net.state!=2 || target!=net.address || !sender || (sender&net.mask)!=(net.address&net.mask))return;
    unsigned i;for(i=0;i<8;++i)if(neighbors[i].address==sender)break;
    if(i==8)i=replacement++%8;
    neighbors[i].address=sender;neighbors[i].time=pit_ticks();memcpy(neighbors[i].mac,p+8,6);
    if(read16(p+6)==1)arp(2,sender,p+8);
}
static void receive_ip(const uint8_t *p,unsigned n) {
    if(n<20 || p[0]!=0x45 || read16(p+2)<20 || read16(p+2)>n || net_checksum(p,20,0) || (read16(p+6)&0x3fff)){++net.dropped;return;}
    uint32_t source=read32(p+12),destination=read32(p+16);
    if(destination!=net.address && destination!=0xffffffffu && !(net.state==1 && p[9]==17))return;
    n=read16(p+2)-20;unsigned protocol=p[9];p+=20;
    if(protocol==17 && n>=8) {
        unsigned size=read16(p+4);if(size<8 || size>n || (read16(p+6) && net_checksum(p,size,net_pseudo(source,destination,17,size)))){++net.dropped;return;}
        if(read16(p)==67 && read16(p+2)==68)dhcp_receive(p+8,size-8);
        else if(net.state==2 && destination==net.address && read16(p)==53)dns_receive(source,read16(p+2),p+8,size-8);
    } else if(protocol==6 && net.state==2 && destination==net.address)tcp_receive(source,p,n);
    else if(protocol==1 && net.state==2 && destination==net.address && n>=8 && n<=1480 && !net_checksum(p,n,0) && p[0]==8 && !p[1]) {
        uint8_t reply[1480];memcpy(reply,p,n);reply[0]=0;write16(reply+2,0);write16(reply+2,net_checksum(reply,n,0));(void)net_ip_send(source,1,reply,n);
    }
}
void net_lost(void) {
    net.address=net.mask=net.gateway=net.dns=0;memset(neighbors,0,sizeof(neighbors));
    /* Outstanding operations retain their handles but become errors on the next poll. */
    net.state=1;klog("NET: DHCP acquiring\r\n");
}
void network_init(void) {
    if(pcnet_init(net.mac)){klog("NET: offline (no supported PCnet adapter)\r\n");return;}
    net_lost();
}
void network_poll(void) {
    if(!net.state)return;
    for(unsigned budget=0;budget<32;++budget) {
        int n=pcnet_receive(incoming);if(!n)break;
        if(n==-5){net.state=0;klog("NET: DMA error, interface stopped\r\n");break;}
        if(n<14){++net.dropped;continue;}
        ++net.received;
        if(memcmp(incoming,net.mac,6) && memcmp(incoming,broadcast,6))continue;
        unsigned type=read16(incoming+12);
        if(type==0x806)receive_arp(incoming+14,(unsigned)n-14,incoming+6);
        else if(type==0x800)receive_ip(incoming+14,(unsigned)n-14);
    }
    dhcp_poll();dns_poll();tcp_poll();
}
