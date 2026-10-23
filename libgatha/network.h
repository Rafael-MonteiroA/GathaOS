#ifndef GATHA_NETWORK_API_H
#define GATHA_NETWORK_API_H
#include "api.h"
#define SYS_NETWORK 31
enum { G_NET_INFO, G_DNS_START, G_DNS_RESULT, G_TCP_CONNECT, G_TCP_STATUS,
       G_TCP_SEND, G_TCP_RECEIVE, G_TCP_CLOSE, G_DNS_CLOSE };
/* IPv4 values are host integers in dotted-octet order: 10.0.2.15 = 0x0a00020f. */
struct g_network { uint32_t state, address, mask, gateway, dns, received, sent, dropped; uint8_t mac[6], reserved[2]; };
struct g_endpoint { uint32_t address, port; };
static inline int g_network(unsigned op,int handle,void *data,unsigned size) {
    return g_call(SYS_NETWORK,op,(uint32_t)(uintptr_t)data,size,(uint32_t)handle,0);
}
int g_ipv4(const char *text,uint32_t *address);
void g_ip_text(uint32_t address,char text[16]);
/* The callback runs while waiting and may cancel by returning nonzero. */
typedef int (*g_net_wait)(void);
int g_resolve(const char *name,uint32_t *address,g_net_wait wait);
int g_http_get(const char *url,void **data,unsigned *size,g_net_wait wait);
struct g_http_response {
    void *data;
    unsigned size,status;
    char location[1024],type[128];
};
/* One request, no automatic redirects. Caller owns data on success. */
int g_http_fetch(const char *url,struct g_http_response *response,unsigned limit,g_net_wait wait);
#endif
