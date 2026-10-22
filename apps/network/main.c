#include "../../libgatha/network.h"
static void require(int value){if(!value){g_puts("NETWORK ABI: FAIL\n");g_exit(1);}}
int app_main(unsigned argument) {
    (void)argument;struct g_network info;
    require(!g_network(G_NET_INFO,0,&info,sizeof(info)) && info.state==2);
    require(g_network(G_NET_INFO,0,(void *)0xc0000000,sizeof(info))==-14);
    require(g_network(G_NET_INFO,0,&info,1)==-14);
    require(g_network(999,0,0,0)==-38);
    char invalid[]="bad..test",valid[]="pending.test",unterminated[]={'x'};
    require(g_network(G_DNS_START,0,invalid,sizeof(invalid))==-22);
    require(g_network(G_DNS_START,0,unterminated,sizeof(unterminated))==-22);
    int queries[4],sockets[4];struct g_endpoint endpoint={info.gateway,9};
    for(unsigned i=0;i<4;++i){queries[i]=g_network(G_DNS_START,0,valid,sizeof(valid));require(queries[i]>0);}
    require(g_network(G_DNS_START,0,valid,sizeof(valid))==-24);
    require(!g_network(G_DNS_CLOSE,queries[0],0,0));require(g_network(G_DNS_CLOSE,queries[0],0,0)==-9);
    queries[0]=g_network(G_DNS_START,0,valid,sizeof(valid));require(queries[0]>0);
    for(unsigned i=0;i<4;++i){sockets[i]=g_network(G_TCP_CONNECT,0,&endpoint,sizeof(endpoint));require(sockets[i]>0);}
    require(g_network(G_TCP_CONNECT,0,&endpoint,sizeof(endpoint))==-24);
    require(g_network(G_TCP_SEND,sockets[0],(void *)0xc0000000,16)==-14);
    require(g_network(G_TCP_RECEIVE,sockets[0],(void *)0xc0000000,16)==-14);
    require(!g_network(G_TCP_CLOSE,sockets[0],0,0));require(g_network(G_TCP_STATUS,sockets[0],0,0)==-9);
    sockets[0]=g_network(G_TCP_CONNECT,0,&endpoint,sizeof(endpoint));require(sockets[0]>0);
    g_puts("NETWORK ABI: bounds, exhaustion and stale handles PASS; intentional fault follows\n");
    *(volatile unsigned *)0=1;return 1;
}
