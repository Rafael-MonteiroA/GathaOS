/* Host-only fault injection against the actual freestanding filesystem code. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel/fs/fs.h"
static unsigned char *disk, *seed, *buffer;
static unsigned disk_bytes, writes;
static int fail_after=-1, flush_fail;
int ata_init(void) { return 1; }
uint32_t ata_sectors(void) { return disk_bytes/512; }
int ata_read(uint32_t s,void *p) { if(s>=ata_sectors())return 0;memcpy(p,disk+s*512,512);return 1; }
int ata_write(uint32_t s,const void *p) {
    if(s>=ata_sectors() || fail_after==0)return 0;
    if(fail_after>0)--fail_after;
    memcpy(disk+s*512,p,512);++writes;return 1;
}
int ata_flush(void) { return !flush_fail; }
uint32_t rtc_seconds(void) { return 1767225600; }
void klog(const char *s) { (void)s; }
void klog_u32(uint32_t n) { (void)n; }
int vfs_busy(unsigned n) { (void)n;return 0; }
static void reset(void) { memcpy(disk,seed,disk_bytes);fail_after=-1;flush_fail=0;assert(fs_mount()==0); }
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);disk_bytes=(unsigned)ftell(f);rewind(f);
    disk=malloc(disk_bytes);seed=malloc(disk_bytes);buffer=malloc(FS_MAX_SIZE);assert(disk&&seed&&buffer);
    assert(fread(seed,1,disk_bytes,f)==disk_bytes);fclose(f);
    for(unsigned i=0;i<FS_MAX_SIZE;++i)buffer[i]=(unsigned char)(i*37+11);
    reset();unsigned baseline=fs_free_blocks();
    int n=fs_create("/large",G_FILE);assert(n>0);
    assert(fs_write((unsigned)n,0,buffer,FS_MAX_SIZE)==FS_MAX_SIZE);
    assert(fs_free_blocks()==baseline-1025);
    assert(fs_write((unsigned)n,FS_MAX_SIZE,buffer,1)==-27);
    assert(fs_write((unsigned)n,UINT32_MAX,buffer,1)==-22);
    assert(fs_mount()==0);unsigned char check[19];
    assert(fs_read((unsigned)n,47*4096-3,check,sizeof(check))==sizeof(check));
    assert(!memcmp(check,buffer+47*4096-3,sizeof(check)));
    assert(fs_truncate((unsigned)n)==0 && fs_free_blocks()==baseline);
    assert(fs_write((unsigned)n,0,buffer,47*4096)==47*4096);
    assert(fs_write((unsigned)n,47*4096,buffer,1)==1);
    assert(fs_unlink("/large")==0 && fs_free_blocks()==baseline);
    assert(fs_replace("/saved","original",8)==0);
    for(unsigned i=0;fs_free_blocks()>0;++i) {
        char path[40];snprintf(path,sizeof(path),"/fill%u",i);n=fs_create(path,G_FILE);assert(n>0);
        unsigned count=fs_free_blocks();if(count>1025)count=1025;
        if(count>47)--count;
        assert(fs_write((unsigned)n,0,buffer,count*4096)==(int)(count*4096));
    }
    assert(fs_replace("/saved",buffer,4096)==-28);
    assert(fs_read((unsigned)fs_lookup("/saved"),0,check,8)==8 && !memcmp(check,"original",8));
    f=fopen("build/native-full-disk.img","wb");assert(f);assert(fwrite(disk,1,disk_bytes,f)==disk_bytes);fclose(f);
    reset();assert(fs_replace("/saved","original",8)==0);
    unsigned free_before=fs_free_blocks();fail_after=8;
    assert(fs_replace("/saved",buffer,48*4096)==-5);
    assert(fs_free_blocks()==free_before);
    assert(fs_read((unsigned)fs_lookup("/saved"),0,check,8)==8 && !memcmp(check,"original",8));
    unsigned before=writes;assert(fs_replace("/saved","new",3)==-30 && fs_sync()==-30);
    assert(writes==before);
    reset();flush_fail=1;assert(fs_create("/flush",G_FILE)==-5);before=writes;
    assert(fs_create("/later",G_FILE)==-30 && writes==before);
    reset();n=fs_create("/large",G_FILE);assert(fs_write((unsigned)n,0,buffer,48*4096)==48*4096);
    unsigned pointer=fs_node((unsigned)n)->blocks[47];
    memcpy(disk+pointer*4096,&pointer,4);assert(fs_mount()==-74);
    puts("FS NATIVE PASS: 4 MiB, remount, indirect boundary, truncate/unlink, full-disk safe save, I/O/flush fail-closed, corruption rejection");
    free(buffer);free(seed);free(disk);return 0;
}
