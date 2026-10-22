#include "internal.h"
#include "mm/memory.h"
#include "sched/task.h"
int network_call(struct interrupt_frame *f) {
    unsigned pid=task_current()->pid,op=f->ebx,size=f->edx;int handle=(int)f->esi;
    void *data=(void *)(uintptr_t)f->ecx;
    if(!task_current()->user)return -13;
    switch(op) {
    case G_NET_INFO:
        if(size!=sizeof(net) || !user_range(f->ecx,size,1))return -14;
        memcpy(data,&net,sizeof(net));return 0;
    case G_DNS_START: {
        if(!size || size>254 || !user_range(f->ecx,size,0))return -14;
        char name[254];memcpy(name,data,size);if(name[size-1])return -22;
        return dns_start(pid,name);
    }
    case G_DNS_RESULT:
        if(size!=4 || !user_range(f->ecx,size,1))return -14;
        return dns_result(pid,handle,data);
    case G_DNS_CLOSE:return dns_close(pid,handle);
    case G_TCP_CONNECT:
        if(size!=sizeof(struct g_endpoint) || !user_range(f->ecx,size,0))return -14;
        return tcp_connect(pid,data);
    case G_TCP_STATUS:return tcp_status(pid,handle);
    case G_TCP_SEND:case G_TCP_RECEIVE:
        if(size>16384 || !user_range(f->ecx,size,op==G_TCP_RECEIVE))return -14;
        return op==G_TCP_SEND?tcp_send(pid,handle,data,size):tcp_read(pid,handle,data,size);
    case G_TCP_CLOSE:return tcp_close(pid,handle);
    default:return -38;
    }
}
void network_release(unsigned pid){dns_release(pid);tcp_release(pid);}
