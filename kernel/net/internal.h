#ifndef GATHA_NET_INTERNAL_H
#define GATHA_NET_INTERNAL_H
#include "network.h"
#include "../libgatha/network.h"
#include "../libgatha/memory.h"
#include "drivers/serial.h"
#include "arch/x86/pit.h"
extern struct g_network net;
static inline uint16_t read16(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
static inline uint32_t read32(const uint8_t *p){return ((uint32_t)read16(p)<<16)|read16(p+2);}
static inline void write16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static inline void write32(uint8_t *p,uint32_t v){write16(p,v>>16);write16(p+2,v);}
static inline int due(uint32_t now,uint32_t deadline){return (int32_t)(now-deadline)>=0;}
uint16_t net_checksum(const uint8_t *p,unsigned length,uint32_t sum);
uint32_t net_pseudo(uint32_t source,uint32_t destination,unsigned protocol,unsigned length);
int net_ip_send(uint32_t destination,unsigned protocol,const void *data,unsigned size);
int net_udp_send(uint32_t destination,unsigned source_port,unsigned port,const void *data,unsigned size);
void net_lost(void);
void dhcp_poll(void);
void dhcp_receive(const uint8_t *data,unsigned size);
void dns_poll(void);
void dns_receive(uint32_t source,unsigned port,const uint8_t *data,unsigned size);
int dns_start(unsigned pid,const char *name);
int dns_result(unsigned pid,int handle,uint32_t *address);
int dns_close(unsigned pid,int handle);
void dns_release(unsigned pid);
void tcp_poll(void);
void tcp_receive(uint32_t source,const uint8_t *data,unsigned size);
int tcp_connect(unsigned pid,const struct g_endpoint *endpoint);
int tcp_status(unsigned pid,int handle);
int tcp_send(unsigned pid,int handle,const void *data,unsigned size);
int tcp_read(unsigned pid,int handle,void *data,unsigned size);
int tcp_close(unsigned pid,int handle);
void tcp_release(unsigned pid);
#endif
