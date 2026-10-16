#include "power.h"
#include "arch/x86/io.h"
#include "fs/fs.h"
#include "drivers/serial.h"
#include "drivers/framebuffer.h"
#include "../gui/gfx/gfx.h"
int system_power(unsigned reboot) {
    if(reboot>1)return -22;int result=fs_sync();if(result<0)return result;
    klog("POWER: filesystem flushed\r\n");
    __asm__ volatile("cli":::"memory");
    if(reboot){klog("POWER: reboot requested\r\n");for(unsigned i=0;i<1000000;++i)if(!(inb(0x64)&2)){outb(0x64,0xfe);break;}}
    else klog("POWER: shutdown requested\r\n");
    gfx_target(0,0,0);gfx_rect(0,0,(int)framebuffer_width(),(int)framebuffer_height(),0x102234);
    gfx_text(60,100,reboot?"Restart requested.":"GathaOS is shut down.",0xe2e8f0,2);
    gfx_text(60,160,"You may close the virtual machine.",0x8cbabf,1);
    framebuffer_present(0,0,(int)framebuffer_width(),(int)framebuffer_height());
    klog("POWER: halted\r\n");for(;;)__asm__ volatile("hlt");
}
