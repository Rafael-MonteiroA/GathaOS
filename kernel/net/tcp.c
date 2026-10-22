#include "internal.h"
#define CAPACITY 16384u
enum { SYN_SENT=1,ESTABLISHED,PEER_FIN,FAILED };
struct connection {
    unsigned owner,state;int handle,error;uint32_t remote,send_next,send_unacked,receive_next;
    uint32_t retry,deadline;unsigned attempts,length,head,count,window,mss;
    uint16_t local_port,remote_port;uint8_t pending[1460],received[CAPACITY];
};
static struct connection connections[4];
static unsigned generation;
static int before(uint32_t a,uint32_t b){return (int32_t)(a-b)<0;}
static struct connection *get(unsigned owner,int handle) {
    for(unsigned i=0;i<4;++i)if(connections[i].owner==owner && connections[i].handle==handle && owner)return &connections[i];
    return 0;
}
static int packet(struct connection *c,unsigned flags,uint32_t sequence,const void *data,unsigned length) {
    uint8_t p[1480]={0};unsigned header=flags&2?24:20;
    write16(p,c->local_port);write16(p+2,c->remote_port);write32(p+4,sequence);
    if(flags&16)write32(p+8,c->receive_next);
    p[12]=(uint8_t)(header<<2);p[13]=(uint8_t)flags;write16(p+14,CAPACITY-c->count);
    if(flags&2){p[20]=2;p[21]=4;write16(p+22,1460);}
    if(length)memcpy(p+header,data,length);
    unsigned size=header+length;write16(p+16,net_checksum(p,size,net_pseudo(net.address,c->remote,6,size)));
    return net_ip_send(c->remote,6,p,size);
}
static void acknowledge(struct connection *c){(void)packet(c,16,c->send_next,0,0);}
static void fail(struct connection *c,int error){c->state=FAILED;c->error=error;}
int tcp_connect(unsigned owner,const struct g_endpoint *endpoint) {
    if(net.state!=2)return -100;
    if(!endpoint->address || (endpoint->address>>24)>=224 || !endpoint->port || endpoint->port>65535)return -22;
    struct connection *c=0;for(unsigned i=0;i<4;++i)if(!connections[i].owner){c=&connections[i];break;}
    if(!c)return -24;
    memset(c,0,sizeof(*c));c->owner=owner;c->state=SYN_SENT;c->remote=endpoint->address;c->remote_port=(uint16_t)endpoint->port;
    c->handle=(int)((++generation&0x0fffffffu)+1);c->local_port=(uint16_t)(40000+generation%12000);
    c->send_unacked=pit_ticks()*64000u+generation*1048573u;c->send_next=c->send_unacked+1;
    c->retry=pit_ticks();c->deadline=c->retry+1500;c->mss=536;return c->handle;
}
int tcp_status(unsigned owner,int handle) {
    struct connection *c=get(owner,handle);if(!c)return -9;
    return c->state==FAILED?c->error:c->state==SYN_SENT?-11:0;
}
int tcp_send(unsigned owner,int handle,const void *data,unsigned size) {
    struct connection *c=get(owner,handle);if(!c)return -9;
    if(c->state==FAILED)return c->error;
    if(c->state==PEER_FIN)return -32;
    if(c->state!=ESTABLISHED || c->length || !c->window)return -11;
    if(size>c->mss)size=c->mss;if(size>c->window)size=c->window;
    if(!size)return 0;
    memcpy(c->pending,data,size);c->length=size;c->send_unacked=c->send_next;c->send_next+=size;
    c->attempts=0;c->retry=pit_ticks();c->deadline=c->retry+3000;return (int)size;
}
int tcp_read(unsigned owner,int handle,void *data,unsigned size) {
    struct connection *c=get(owner,handle);if(!c)return -9;
    if(c->state==FAILED)return c->error;
    if(!size)return 0;
    if(!c->count)return c->state==PEER_FIN?0:-11;
    if(size>c->count)size=c->count;
    for(unsigned i=0;i<size;++i)((uint8_t *)data)[i]=c->received[(c->head+i)%CAPACITY];
    c->head=(c->head+size)%CAPACITY;c->count-=size;acknowledge(c);return (int)size;
}
int tcp_close(unsigned owner,int handle) {
    struct connection *c=get(owner,handle);if(!c)return -9;
    /* This bounded client API uses TCP abort, including cancellation and process exit. */
    if(c->state==ESTABLISHED || c->state==PEER_FIN)(void)packet(c,20,c->send_next,0,0);
    memset(c,0,sizeof(*c));return 0;
}
void tcp_release(unsigned owner) {for(unsigned i=0;i<4;++i)if(connections[i].owner==owner)(void)tcp_close(owner,connections[i].handle);}
void tcp_poll(void) {
    uint32_t now=pit_ticks();
    for(unsigned i=0;i<4;++i) {
        struct connection *c=&connections[i];if(!c->owner || c->state==FAILED || c->state==PEER_FIN)continue;
        if(net.state!=2){fail(c,-100);continue;}
        if(due(now,c->deadline)){fail(c,-110);continue;}
        if(!due(now,c->retry) || (c->state!=SYN_SENT && !c->length))continue;
        if(c->attempts>=5){fail(c,-110);continue;}
        int r=packet(c,c->state==SYN_SENT?2:24,c->send_unacked,c->pending,c->length);
        c->retry=now+(r==-11?10:100u<<c->attempts);
        if(!r)++c->attempts;else if(r!=-11)fail(c,r);
    }
}
void tcp_receive(uint32_t source,const uint8_t *p,unsigned n) {
    if(n<20 || net_checksum(p,n,net_pseudo(source,net.address,6,n))){++net.dropped;return;}
    unsigned header=(p[12]>>4)*4;if(header<20 || header>n){++net.dropped;return;}
    struct connection *c=0;for(unsigned i=0;i<4;++i)
        if(connections[i].owner && connections[i].remote==source && connections[i].remote_port==read16(p) && connections[i].local_port==read16(p+2)){c=&connections[i];break;}
    if(!c || c->state==FAILED)return;
    uint32_t seq=read32(p+4),ack=read32(p+8);unsigned flags=p[13],length=n-header;
    if(c->state==SYN_SENT) {
        if((flags&20)==20 && ack==c->send_next){fail(c,-111);return;}
        if((flags&23)!=18 || ack!=c->send_next || length)return;
        for(unsigned i=20;i<header;) {
            unsigned kind=p[i++];if(!kind)break;if(kind==1)continue;
            if(i>=header || p[i]<2 || p[i]>header-i+1)return;
            unsigned size=p[i++];if(kind==2 && size==4){unsigned m=read16(p+i);if(!m)return;c->mss=m>1460?1460:m;}i+=size-2;
        }
        c->send_unacked=ack;c->receive_next=seq+1;c->window=read16(p+14);c->state=ESTABLISHED;
        c->deadline=pit_ticks()+3000;acknowledge(c);klog("NET: TCP connected\r\n");return;
    }
    if(flags&4){if(seq==c->receive_next)fail(c,-104);else acknowledge(c);return;}
    if(flags&2){acknowledge(c);return;}
    /* Only in-order sequence space can update acknowledgments and the peer window. */
    if(seq!=c->receive_next) {
        if(before(seq,c->receive_next) && c->receive_next-seq<=length) {
            unsigned trim=c->receive_next-seq;seq+=trim;header+=trim;length-=trim;
        } else {acknowledge(c);return;}
    }
    if(!(flags&16) || before(c->send_next,ack)){acknowledge(c);return;}
    if(before(ack,c->send_unacked))return;
    c->window=read16(p+14);c->deadline=pit_ticks()+3000;
    unsigned acknowledged=ack-c->send_unacked;
    if(acknowledged && acknowledged<=c->length) {
        memmove(c->pending,c->pending+acknowledged,c->length-acknowledged);c->length-=acknowledged;
        c->send_unacked=ack;c->attempts=0;c->retry=pit_ticks()+100;
    }
    if(c->state==PEER_FIN){acknowledge(c);return;}
    if(length>CAPACITY-c->count){acknowledge(c);return;}
    for(unsigned i=0;i<length;++i)c->received[(c->head+c->count+i)%CAPACITY]=p[header+i];
    c->count+=length;c->receive_next+=length;
    if(flags&1){++c->receive_next;c->state=PEER_FIN;}
    if(length || (flags&1))acknowledge(c);
}
