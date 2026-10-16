#include "fs.h"
#include "drivers/serial.h"
#include "panic.h"
#include "../libgatha/memory.h"
static uint8_t bytes[9000], readback[9000];
void fs_self_test(void) {
    for (unsigned i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)(i*37+11);
    struct g_stat info;
    int old = fs_lookup("/persist.bin");
    if (old >= 0) {
        if (fs_stat("/persist.bin",&info) || info.size != sizeof(bytes) ||
            fs_read((unsigned)old,0,readback,sizeof(readback)) != sizeof(readback)) panic("FS PERSISTENCE READ FAILED",0);
        for (unsigned i = 0; i < sizeof(bytes); ++i) if (bytes[i] != readback[i]) panic("FS PERSISTENCE CONTENT FAILED",0);
        klog("FSTEST: persisted data verified after restart PASS\r\n");
    } else {
        int created = fs_create("/persist.bin",G_FILE);
        if (created < 0 || fs_write((unsigned)created,0,bytes,sizeof(bytes)) != sizeof(bytes)) panic("FS PERSISTENCE WRITE FAILED",0);
        klog("FSTEST: persistent data written and flushed PASS\r\n");
    }
    unsigned baseline = fs_free_blocks();
    if (fs_create("/testdir",G_DIR) < 0) panic("FS MKDIR FAILED",0);
    int file = fs_create("/testdir/temp",G_FILE);
    if (file < 0 || fs_write((unsigned)file,0,bytes,sizeof(bytes)) != sizeof(bytes) ||
        fs_read((unsigned)file,0,readback,sizeof(readback)) != sizeof(readback)) panic("FS IO FAILED",0);
    for (unsigned i = 0; i < sizeof(bytes); ++i) if (bytes[i] != readback[i]) panic("FS CONTENT FAILED",0);
    if (fs_rename("/testdir/temp","/testdir/renamed") || fs_lookup("/testdir/temp") != -2 ||
        fs_lookup("/testdir/./renamed") != file || fs_create("/testdir/renamed",G_FILE) != -17 ||
        fs_unlink("/testdir") != -39 || fs_unlink("/testdir/renamed") || fs_unlink("/testdir") ||
        fs_free_blocks() != baseline) panic("FS DIRECTORY OR FREE SPACE FAILED",0);
    klog("FSTEST: multiblock content directories rename unlink free-space PASS\r\n");
    struct g_dirent entry;
    klog("GATHAFS: root entries:");
    for (unsigned i = 0; fs_readdir("/",i,&entry) > 0; ++i) { klog(" "); klog(entry.name); }
    klog("\r\nSTAGE5: DISK PASS\r\n");
}
