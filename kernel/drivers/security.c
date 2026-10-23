#include "security.h"
#include "rtc.h"
#include "serial.h"
#include "mm/memory.h"
#include "sched/task.h"
#include "../libgatha/security.h"
#include "../libgatha/crypto/crypto.h"
#include "../libgatha/memory.h"
static int available=-1,failed;
static uint32_t previous,sequence;
static int word(uint32_t *out) {
    for(unsigned attempt=0;attempt<10;++attempt){uint32_t value;unsigned char ok;__asm__ volatile("rdrand %0; setc %1":"=r"(value),"=qm"(ok)::"cc");if(ok){if(!value || value==UINT32_MAX || (sequence && value==previous))return -61;previous=value;++sequence;*out=value;return 0;}}
    return -61;
}
static int random_bytes(uint8_t *out,unsigned size) {
    if(available<0){uint32_t a=1,b,c,d;__asm__ volatile("cpuid":"+a"(a),"=b"(b),"=c"(c),"=d"(d));available=(c>>30)&1;klog(available?"SECURITY: RDRAND available; RTC assumed UTC\n":"SECURITY: no RDRAND; HTTPS disabled\n");}
    if(!available || failed)return -61;
    unsigned offset=0;uint32_t input[16];uint8_t digest[32];int result=0;
    while(offset<size){for(unsigned i=0;i<16;++i)if((result=word(input+i)))goto done;sha256_sum(input,sizeof(input),digest);unsigned take=size-offset;if(take>32)take=32;memcpy(out+offset,digest,take);offset+=take;}
done:
    crypto_wipe(input,sizeof(input));crypto_wipe(digest,32);
    if(result){memset(out,0,size);failed=1;klog("SECURITY: entropy health failure; HTTPS disabled\n");}
    return result;
}
int security_call(struct interrupt_frame *f) {
    if(!task_current()->user)return -13;
    if(f->ebx==G_RANDOM){if(!f->edx || f->edx>256 || !user_range(f->ecx,f->edx,1))return -14;return random_bytes((void *)(uintptr_t)f->ecx,f->edx);}
    if(f->ebx==G_UTC){if(f->edx!=4 || !user_range(f->ecx,4,1))return -14;uint32_t now=rtc_seconds();if(now<1577836800u)return -62;*(uint32_t *)(uintptr_t)f->ecx=now;return 0;}
    return -38;
}
