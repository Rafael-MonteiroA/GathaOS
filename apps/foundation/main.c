/* Explicitly launched diagnostics; never run against personal files. */
#include "../../libgatha/api.h"
#include "../../libgatha/layout.h"
#include "../../gui/wm/protocol.h"
static volatile unsigned char image_data[5u*1024u*1024u];
static void require(int condition, const char *message) {
    if (!condition) { g_puts("FOUNDATION FAIL: "); g_puts(message); g_puts("\n"); g_exit(1); }
}
static unsigned frames(void) {
    struct g_info info;
    require(g_call(SYS_INFO,(uint32_t)(uintptr_t)&info,0,0,0,0)==0,"memory stats");
    return info.free_frames;
}
static void memory_test(void) {
    for (unsigned i=0;i<sizeof(image_data);i+=4096) { require(image_data[i]==0,"zero BSS"); image_data[i]=(unsigned char)(i/4096); }
    for (unsigned i=0;i<sizeof(image_data);i+=4096) require(image_data[i]==(unsigned char)(i/4096),"large BSS content");
    unsigned baseline=frames();
    unsigned char *a=g_malloc(8u*1024u*1024u),*b=g_calloc(1024,1024);
    require(a && b && !((uintptr_t)a&15) && !((uintptr_t)b&15),"allocation/alignment");
    for(unsigned i=0;i<8u*1024u*1024u;++i)a[i]=(unsigned char)(i*13+7);
    for(unsigned i=0;i<1024u*1024u;++i)require(!b[i],"calloc zero");
    unsigned char *grown=g_realloc(a,12u*1024u*1024u);require(grown!=0,"realloc grow");a=grown;
    for(unsigned i=0;i<8u*1024u*1024u;++i)require(a[i]==(unsigned char)(i*13+7),"realloc content");
    require(!g_realloc(a,UINT32_MAX) && a[0]==7,"realloc failure retains original");
    require(!g_calloc(UINT32_MAX,16),"calloc overflow");
    g_free(b);g_free(a);require(frames()==baseline,"allocator frame reclamation");
    for(unsigned cycle=0;cycle<32;++cycle) {
        void *slots[32];
        for(unsigned i=0;i<32;++i) { slots[i]=g_malloc(16+i*177);require(slots[i]!=0,"fragmentation allocate"); }
        for(unsigned i=0;i<32;i+=2)g_free(slots[i]);
        for(unsigned i=1;i<32;i+=2)g_free(slots[i]);
        require(frames()==baseline,"fragmentation reclaim");
    }
    require(g_sbrk(0)==USER_HEAP_BASE,"heap reset");
    int32_t result=g_sbrk((int32_t)(USER_LIMIT-USER_HEAP_BASE));
    if(result>=0) {
        volatile unsigned char *p=(void *)(uintptr_t)result;
        for(unsigned i=0;i<USER_LIMIT-USER_HEAP_BASE;i+=4096) { require(!p[i],"page zero");p[i]=91; }
        require(g_sbrk(1)==-12,"heap limit");
        require(g_sbrk(-(int32_t)(USER_LIMIT-USER_HEAP_BASE))==USER_LIMIT,"heap shrink");
        g_puts("FOUNDATION: 128 MiB heap capacity PASS\n");
    }else { require(result==-12,"OOM result");g_puts("FOUNDATION: physical OOM rollback PASS\n"); }
    require(frames()==baseline && g_sbrk(0)==USER_HEAP_BASE,"OOM/shrink reclamation");
    require(g_sbrk(-1)==-12 && g_sbrk(INT32_MIN)==-12 && g_sbrk(INT32_MAX)==-12,"signed bounds");
    require(g_sbrk(1)==USER_HEAP_BASE && g_sbrk(4095)==USER_HEAP_BASE+1,"unaligned breaks");
    require(g_sbrk(-4096)==USER_HEAP_BASE+4096 && frames()==baseline,"unaligned shrink");
    g_puts("FOUNDATION: large image allocator bounds reclaim PASS\n");
}
static void file_test(void) {
    int r=g_mkdir("/FoundationTest");require(r==0 || r==-17,"test directory");
    unsigned size=4u*1024u*1024u;
    unsigned char *data=g_malloc(size);require(data!=0,"file buffer");
    for(unsigned i=0;i<size;++i)data[i]=(unsigned char)(i*37+11);
    struct g_stat st;
    if(g_stat("/FoundationTest/large.bin",&st)==0) {
        int fd=g_open("/FoundationTest/large.bin",G_READ);require(fd>=0,"persistent open");
        require(g_read(fd,data,st.size)==(int)st.size,"persistent read");g_close(fd);
        for(unsigned i=0;i<st.size;++i)require(data[i]==(unsigned char)(i*37+11),"persistent content");
        g_puts("FOUNDATION: persisted large file PASS\n");
    }
    r=g_save("/FoundationTest/large.bin",data,size);
    if(r==-27) { size=192u*1024u;r=g_save("/FoundationTest/large.bin",data,size); }
    require(r==0,"save large file");
    int fd=g_open("/FoundationTest/large.bin",G_READ|G_WRITE);require(fd>=0,"open large file");
    require(g_save("/FoundationTest/large.bin","x",1)==-16,"busy replacement rejected");
    require(g_seek(fd,(int)(47u*4096u-3),0)>0,"seek boundary");
    require(g_read(fd,data,16)==16,"read across indirect boundary");
    for(unsigned i=0;i<16;++i)require(data[i]==(unsigned char)((47u*4096u-3+i)*37+11),"boundary content");
    require(g_read(fd,(void *)(USER_LIMIT-2),4)==-14,"unmapped/overflow pointer");
    require(g_seek(fd,(int)size,0)==(int)size && g_write(fd,"x",1)==-27,"file size cap");
    g_close(fd);
    g_puts(size==4u*1024u*1024u?"FOUNDATION: 4 MiB file save seek bounds PASS\n":"FOUNDATION: legacy v1 192 KiB bounds PASS\n");
    if(size>192u*1024u) {
        for(unsigned i=0;i<220u*1024u;++i)data[i]=(i%64==63)?'\n':'x';
        const char *title="Large document test";for(unsigned i=0;title[i];++i)data[i]=(unsigned char)title[i];
        require(g_save("/FoundationTest/EditorLarge.txt",data,220u*1024u)==0,"editor fixture");
    }
    g_free(data);
}
int app_main(unsigned argument) {
    (void)argument;g_puts("FOUNDATION: probe started\n");memory_test();file_test();
    /* Leave an allocation and descriptor behind: task fault cleanup owns them. */
    require(g_malloc(2u*1024u*1024u)!=0,"fault allocation");
    require(g_open("/FoundationTest/large.bin",G_READ)>=0,"fault descriptor");
    g_puts("FOUNDATION: checks PASS; intentional user page fault follows\n");
    __asm__ volatile("movl $1, 0");
    return 1;
}
