#include "internal.h"
struct query { unsigned owner;int handle,result;uint32_t address,next,deadline;unsigned attempts;uint16_t id,port;char name[254]; };
static struct query queries[4];
static unsigned generation;
static char lower(char c){return c>='A' && c<='Z'?(char)(c+32):c;}
static struct query *get(unsigned owner,int handle) {
    for(unsigned i=0;i<4;++i)if(queries[i].owner==owner && queries[i].handle==handle && owner)return &queries[i];
    return 0;
}
int dns_start(unsigned owner,const char *name) {
    if(net.state!=2)return -100;
    unsigned length=(unsigned)g_strlen(name),label=0;
    if(!length || length>253)return -22;
    for(unsigned i=0;i<=length;++i) {
        char c=lower(name[i]);
        if(!c || c=='.'){if(!label || label>63)return -22;label=0;}
        else {if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-'))return -22;++label;}
    }
    struct query *q=0;for(unsigned i=0;i<4;++i)if(!queries[i].owner){q=&queries[i];break;}
    if(!q)return -24;
    memset(q,0,sizeof(*q));q->owner=owner;q->handle=(int)((++generation&0x0fffffffu)+1);
    q->id=(uint16_t)(generation*251+pit_ticks());q->port=(uint16_t)(53000+generation%1000);
    for(unsigned i=0;i<=length;++i)q->name[i]=lower(name[i]);
    q->result=-11;q->next=pit_ticks();q->deadline=q->next+1000;return q->handle;
}
int dns_result(unsigned owner,int handle,uint32_t *address) {
    struct query *q=get(owner,handle);if(!q)return -9;
    if(!q->result)*address=q->address;return q->result;
}
int dns_close(unsigned owner,int handle) {struct query *q=get(owner,handle);if(!q)return -9;memset(q,0,sizeof(*q));return 0;}
void dns_release(unsigned owner) {for(unsigned i=0;i<4;++i)if(queries[i].owner==owner)memset(&queries[i],0,sizeof(queries[i]));}
void dns_poll(void) {
    uint32_t now=pit_ticks();
    for(unsigned i=0;i<4;++i) {
        struct query *q=&queries[i];if(!q->owner || q->result!=-11)continue;
        if(net.state!=2){q->result=-100;continue;}
        if(due(now,q->deadline)){q->result=-110;continue;}
        if(!due(now,q->next))continue;
        uint8_t p[272]={0};write16(p,q->id);p[2]=1;p[5]=1;unsigned n=12,start=0;
        for(unsigned j=0;;++j)if(!q->name[j] || q->name[j]=='.') {
            p[n++]=(uint8_t)(j-start);memcpy(p+n,q->name+start,j-start);n+=j-start;start=j+1;if(!q->name[j])break;
        }
        p[n++]=0;write16(p+n,1);write16(p+n+2,1);n+=4;
        int result=net_udp_send(net.dns,q->port,53,p,n);
        q->next=now+(result==-11?10:100u<<(q->attempts<2?q->attempts:2));
        if(!result)++q->attempts;
    }
}
/* Decode bounded compression chains. Pointers must refer to prior message bytes. */
static int name_read(const uint8_t *p,unsigned n,unsigned *offset,char out[254]) {
    unsigned at=*offset,used=0,end=0;
    for(unsigned step=0;step<128;++step) {
        if(at>=n)return 0;
        unsigned label=p[at++];
        if(!label){out[used]=0;*offset=end?end:at;return 1;}
        if((label&0xc0)==0xc0) {
            if(at>=n)return 0;
            unsigned target=((label&63)<<8)|p[at++];if(target>=at-2)return 0;
            if(!end)end=at;at=target;continue;
        }
        if(label>63 || label>n-at || used+label+(used?1:0)>253)return 0;
        if(used)out[used++]='.';
        for(unsigned i=0;i<label;++i){uint8_t c=p[at++];if(!c || c=='.')return 0;out[used++]=lower((char)c);}
    }
    return 0;
}
void dns_receive(uint32_t source,unsigned port,const uint8_t *p,unsigned n) {
    if(source!=net.dns || n<12 || n>512)return;
    struct query *q=0;for(unsigned i=0;i<4;++i)
        if(queries[i].owner && queries[i].result==-11 && queries[i].id==read16(p) && queries[i].port==port){q=&queries[i];break;}
    if(!q || !(p[2]&0x80) || (p[2]&0x78) || read16(p+4)!=1)return;
    unsigned at=12;char owner[254],wanted[254];
    if(!name_read(p,n,&at,owner) || g_strcmp(owner,q->name) || at+4>n || read16(p+at)!=1 || read16(p+at+2)!=1)return;
    at+=4;if(p[2]&2){q->result=-90;return;}
    if(p[3]&15){q->result=(p[3]&15)==3?-2:-113;return;}
    unsigned answers=read16(p+6);if(answers>32)return;g_copy(wanted,q->name,sizeof(wanted));
    for(unsigned i=0;i<answers;++i) {
        if(!name_read(p,n,&at,owner) || at+10>n)return;
        unsigned type=read16(p+at),class=read16(p+at+2),length=read16(p+at+8);at+=10;
        if(length>n-at)return;
        if(class==1 && !g_strcmp(owner,wanted)) {
            if(type==1 && length==4){q->address=read32(p+at);q->result=0;klog("NET: DNS resolved\r\n");return;}
            if(type==5){unsigned next=at;if(!name_read(p,n,&next,wanted) || next!=at+length)return;}
        }
        at+=length;
    }
    q->result=-2;
}
