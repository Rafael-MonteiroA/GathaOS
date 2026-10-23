#ifndef GATHA_TLS_H
#define GATHA_TLS_H
#include "../network.h"
#include "x509.h"
struct tls;
/* A TLS object owns secret memory, but the caller owns the TCP handle. */
int g_tls_open(struct tls **out,int handle,const char *host,unsigned deadline,g_net_wait wait);
int g_tls_open_roots(struct tls **out,int handle,const char *host,unsigned deadline,g_net_wait wait,const struct der *roots,unsigned count);
int g_tls_send(struct tls *t,const void *data,unsigned n);
int g_tls_receive(struct tls *t,void *data,unsigned n);
void g_tls_free(struct tls *t);
#endif
