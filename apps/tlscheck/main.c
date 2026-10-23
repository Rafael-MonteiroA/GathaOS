#include "../../libgatha/tls/tls.h"
#include "../../libgatha/security.h"
#include "../../libgatha/memory.h"
/* This diagnostic reads an explicit private-test anchor. It cannot change
 * Terminal's compiled trust store and is never launched automatically. */
static unsigned cancel_at;
static int wait_callback(void){return cancel_at && (int32_t)(g_ticks()-cancel_at)>=0;}
static void require(int condition,const char *what){if(!condition){g_puts("TLS TEST FAIL: ");g_puts(what);g_puts("\n");g_exit(1);}}
static int load(const char *path,void *data,unsigned size){int fd=g_open(path,G_READ);if(fd<0)return fd;int r=g_read(fd,data,size);g_close(fd);return r;}
static void line(unsigned case_id,int result){char text[80];g_copy(text,"TLS TEST: case ",sizeof(text));char *p=g_decimal(text+g_strlen(text),(int)case_id);g_copy(p," result=",24);p+=8;g_decimal(p,result);g_puts(text);g_puts("\n");}
int app_main(unsigned argument) {
    (void)argument;
    uint8_t root[4096],random[64];uint32_t plan[1+32*4],now;
    int n=load("/Documents/TLS-root.der",root,sizeof(root)),bytes=load("/Documents/TLS-plan.bin",plan,sizeof(plan));
    require(n>0 && bytes>=4 && plan[0]<=32 && (unsigned)bytes==4+plan[0]*16,"test fixtures missing");
    require(g_random((void *)0xc0000000,32)==-14,"random pointer validation");require(g_random(random,257)==-14,"random size validation");
    require(g_utc((uint32_t *)0xc0000000)==-14,"clock pointer validation");require(g_utc(&now)==0 && now>=1577836800,"UTC");
    require(g_random(random,64)==0 && !crypto_equal(random,random+32,32),"fresh entropy");crypto_wipe(random,sizeof(random));
    for(unsigned index=0;index<plan[0];++index){uint32_t *test=plan+1+index*4;struct g_endpoint endpoint={0x0a000202,test[0]};int handle=g_network(G_TCP_CONNECT,0,&endpoint,sizeof(endpoint));require(handle>0,"TCP connect");
        int r;while((r=g_network(G_TCP_STATUS,handle,0,0))==-11)g_sleep(1);require(!r,"TCP established");
        struct der anchor={root,(unsigned)n};struct tls *tls=0;cancel_at=(test[2]&1)?g_ticks()+30:0;
        r=g_tls_open_roots(&tls,handle,"10.0.2.2",g_ticks()+12000,wait_callback,&anchor,1);
        if(!r){const char *request="GET / HTTP/1.1\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n";r=g_tls_send(tls,request,(unsigned)g_strlen(request));
            if(r>=0){uint8_t data[997];unsigned received=0;while((r=g_tls_receive(tls,data,sizeof(data)))>0){
                for(int i=0;i<r;++i){if(received>=test[3] || data[i]!=(uint8_t)(received*37+11)){r=-71;break;}++received;}if(r<0)break;
            }if(!r && received!=test[3])r=-71;}
        }
        g_tls_free(tls);g_network(G_TCP_CLOSE,handle,0,0);cancel_at=0;
        line(index,r);require(r==(int32_t)test[1],"unexpected TLS result");
    }
    g_puts("TLS TEST: all protocol, certificate and integrity cases PASS\n");return 0;
}
