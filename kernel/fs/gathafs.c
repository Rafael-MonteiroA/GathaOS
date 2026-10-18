#include "fs.h"
#include "drivers/ata.h"
#include "drivers/rtc.h"
#include "drivers/serial.h"
#include "../libgatha/memory.h"
static struct fs_inode nodes[FS_INODES];
static uint8_t bitmap[FS_BLOCK], scratch[FS_BLOCK], seen[FS_BLOCK];
/* Cache pointer blocks separately: sector scratch must never alias metadata. */
static uint32_t indirect[FS_INODES][FS_INDIRECT];
static unsigned block_count, mounted, version, read_only;
unsigned fs_max_size(void) { return version == 1 ? FS_DIRECT * FS_BLOCK : FS_MAX_SIZE; }
static unsigned direct_count(void) { return version == 1 ? FS_DIRECT : FS_V2_DIRECT; }
static uint32_t *reference(unsigned inode, unsigned index) {
    return index < direct_count() ? &nodes[inode].blocks[index] : &indirect[inode][index - FS_V2_DIRECT];
}
static int block_io(unsigned block, void *data, int write) {
    if (block >= block_count) return -5;
    for (unsigned i = 0; i < 8; ++i)
        if (!(write ? ata_write(block*8+i,(uint8_t *)data+i*512) : ata_read(block*8+i,(uint8_t *)data+i*512))) {
            if (mounted) { read_only = 1; klog("GATHAFS: I/O failure; further writes disabled\r\n"); }
            return -5;
        }
    return 0;
}
static unsigned bit(const uint8_t *map, unsigned b) { return (map[b/8] >> (b%8)) & 1; }
static void mark(uint8_t *map, unsigned b, int value) {
    if (value) map[b/8] |= (uint8_t)(1u << (b%8)); else map[b/8] &= (uint8_t)~(1u << (b%8));
}
int fs_sync(void) {
    if (!mounted) return -5;
    if (read_only) return -30;
    if (version == 2) for (unsigned i = 0; i < FS_INODES; ++i)
        if (nodes[i].used && nodes[i].blocks[FS_V2_DIRECT] &&
            block_io(nodes[i].blocks[FS_V2_DIRECT],indirect[i],1)) return -5;
    if (block_io(1,bitmap,1)) return -5;
    for (unsigned i = 0; i < 8; ++i) if (block_io(2+i,(uint8_t *)nodes+i*FS_BLOCK,1)) return -5;
    if (!ata_flush()) { read_only = 1; klog("GATHAFS: flush failure; further writes disabled\r\n"); return -5; }
    return 0;
}
int fs_mount(void) {
    mounted = read_only = 0;
    memset(indirect,0,sizeof(indirect));
    if (!ata_init()) return -5;
    block_count = 1;
    if (block_io(0,scratch,0)) return -5;
    uint32_t h[12]; memcpy(h,scratch,sizeof(h));
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 44; ++i) hash = (hash ^ scratch[i])*16777619u;
    if (h[0] != 0x31534647 || (h[1] != 1 && h[1] != 2) || h[2] != FS_BLOCK || h[3] < 16 || h[3] > 32768 ||
        h[3] > ata_sectors()/8 || h[4] != FS_INODES || h[5] != 1 || h[6] != 2 || h[7] != 10 || h[8] || h[10] || h[11] != hash) return -74;
    block_count = h[3]; version = h[1];
    if (block_io(1,bitmap,0)) return -5;
    for (unsigned i = 0; i < 8; ++i) if (block_io(2+i,(uint8_t *)nodes+i*FS_BLOCK,0)) return -5;
    memset(seen,0,sizeof(seen));
    for (unsigned i = 0; i < 10; ++i) mark(seen,i,1);
    if (!nodes[0].used || nodes[0].type != G_DIR || nodes[0].parent || nodes[0].name[0] != '/' || nodes[0].name[1]) return -74;
    for (unsigned i = 0; i < FS_INODES; ++i) if (nodes[i].used) {
        struct fs_inode *n = &nodes[i];
        if ((n->type != G_FILE && n->type != G_DIR) || n->size > fs_max_size() ||
            n->parent >= FS_INODES || !nodes[n->parent].used || nodes[n->parent].type != G_DIR) return -74;
        unsigned length = 0;
        while (length < 40 && n->name[length]) { if (i && n->name[length] == '/') return -74; ++length; }
        if (!length || length == 40 || (i && (!g_strcmp(n->name,".") || !g_strcmp(n->name,"..")))) return -74;
        unsigned ancestor = i, depth = 0;
        while (ancestor && depth++ < FS_INODES) {
            if (ancestor >= FS_INODES || !nodes[ancestor].used) return -74;
            ancestor = nodes[ancestor].parent;
        }
        if (ancestor) return -74;
        for (unsigned j = 0; j < i; ++j) if (nodes[j].used && nodes[j].parent == n->parent && !g_strcmp(nodes[j].name,n->name)) return -74;
        unsigned count = (n->size + FS_BLOCK-1)/FS_BLOCK;
        if (n->type == G_DIR && n->size) return -74;
        if (version == 2) {
            unsigned b = n->blocks[FS_V2_DIRECT];
            if (count <= FS_V2_DIRECT) { if (b) return -74; }
            else {
                if (b < 10 || b >= block_count || bit(seen,b)) return -74;
                mark(seen,b,1);
                if (block_io(b,indirect[i],0)) return -5;
            }
        }
        unsigned slots = version == 1 ? FS_DIRECT : FS_V2_DIRECT + FS_INDIRECT;
        for (unsigned j = 0; j < slots; ++j) {
            unsigned b = *reference(i,j);
            if (j >= count) { if (b) return -74; continue; }
            if (b < 10 || b >= block_count || bit(seen,b)) return -74;
            mark(seen,b,1);
        }
    }
    for (unsigned i = 0; i < sizeof(bitmap); ++i) if (seen[i] != bitmap[i]) return -74;
    mounted = 1;
    klog("GATHAFS: mounted version="); klog_u32(version);
    klog(" block_bytes=4096 inodes=128 blocks="); klog_u32(block_count); klog("\r\n");
    return 0;
}
static int child(unsigned parent, const char *name) {
    for (unsigned i = 1; i < FS_INODES; ++i)
        if (nodes[i].used && nodes[i].parent == parent && !g_strcmp(nodes[i].name,name)) return (int)i;
    return -2;
}
static int walk(const char *path, char *leaf) {
    if (!mounted) return -5;
    if (*path != '/') return -22;
    unsigned length = 0; while (length < 256 && path[length]) ++length;
    if (length == 256) return -36;
    unsigned pos = 1, node = 0;
    while (pos < length) {
        while (path[pos] == '/') ++pos;
        if (pos == length) break;
        char component[40]; unsigned n = 0;
        while (path[pos] && path[pos] != '/') { if (n == 39) return -36; component[n++] = path[pos++]; }
        component[n] = 0;
        if (nodes[node].type != G_DIR) return -20;
        unsigned next = pos; while (path[next] == '/') ++next;
        if (leaf && !path[next]) {
            if (!g_strcmp(component,".") || !g_strcmp(component,"..")) return -22;
            g_copy(leaf,component,40); return (int)node;
        }
        if (!g_strcmp(component,".")) continue;
        if (!g_strcmp(component,"..")) { node = nodes[node].parent; continue; }
        int found = child(node,component); if (found < 0) return found; node = (unsigned)found;
    }
    return leaf ? -22 : (int)node;
}
int fs_lookup(const char *path) { return walk(path,0); }
const struct fs_inode *fs_node(unsigned inode) { return inode < FS_INODES && nodes[inode].used ? &nodes[inode] : 0; }
int fs_create(const char *path, unsigned type) {
    if (!mounted) return -5;
    if (read_only) return -30;
    if (type != G_FILE && type != G_DIR) return -22;
    char name[40]; int parent = walk(path,name); if (parent < 0) return parent;
    if (child((unsigned)parent,name) >= 0) return -17;
    for (unsigned i = 1; i < FS_INODES; ++i) if (!nodes[i].used) {
        struct fs_inode *n = &nodes[i]; memset(n,0,sizeof(*n)); n->used = 1;
        n->type = type; n->parent = (unsigned)parent; n->created = n->modified = rtc_seconds(); g_copy(n->name,name,40);
        nodes[parent].modified = n->modified;
        int result = fs_sync(); return result < 0 ? result : (int)i;
    }
    return -28;
}
static void release_blocks(unsigned inode) {
    struct fs_inode *n = &nodes[inode];
    unsigned count = (n->size + FS_BLOCK - 1) / FS_BLOCK;
    for (unsigned i = 0; i < count; ++i) {
        uint32_t *p = reference(inode,i); mark(bitmap,*p,0); *p = 0;
    }
    if (version == 2 && n->blocks[FS_V2_DIRECT]) {
        mark(bitmap,n->blocks[FS_V2_DIRECT],0); n->blocks[FS_V2_DIRECT] = 0;
        memset(indirect[inode],0,FS_BLOCK);
    }
    n->size = 0;
}
int fs_truncate(unsigned inode) {
    if (!mounted) return -5;
    if (read_only) return -30;
    if (!inode || inode >= FS_INODES || !nodes[inode].used) return -2;
    struct fs_inode *n = &nodes[inode]; if (n->type != G_FILE) return -21;
    release_blocks(inode); n->modified = rtc_seconds(); return fs_sync();
}
int fs_unlink(const char *path) {
    if (!mounted) return -5;
    if (read_only) return -30;
    int inode = fs_lookup(path); if (inode < 0) return inode; if (!inode) return -13;
    if (vfs_busy((unsigned)inode)) return -16;
    for (unsigned i = 1; i < FS_INODES; ++i) if (nodes[i].used && nodes[i].parent == (unsigned)inode) return -39;
    if (nodes[inode].type == G_FILE) { int r = fs_truncate((unsigned)inode); if (r < 0) return r; }
    nodes[nodes[inode].parent].modified = rtc_seconds();
    memset(&nodes[inode],0,sizeof(nodes[inode])); return fs_sync();
}
int fs_rename(const char *old, const char *new_path) {
    if (!mounted) return -5;
    if (read_only) return -30;
    int inode = fs_lookup(old); if (inode <= 0) return inode ? inode : -13;
    char name[40]; int parent = walk(new_path,name); if (parent < 0) return parent;
    if (child((unsigned)parent,name) >= 0) return -17;
    for (unsigned p = (unsigned)parent; p; p = nodes[p].parent) if (p == (unsigned)inode) return -22;
    nodes[inode].parent = (unsigned)parent; g_copy(nodes[inode].name,name,40);
    nodes[inode].modified = rtc_seconds(); return fs_sync();
}
static void info(unsigned inode, struct g_stat *out) {
    const struct fs_inode *n = &nodes[inode]; *out = (struct g_stat){n->type,n->size,n->created,n->modified};
}
int fs_stat(const char *path, struct g_stat *out) { int i = fs_lookup(path); if (i < 0) return i; info((unsigned)i,out); return 0; }
int fs_readdir(const char *path, unsigned index, struct g_dirent *out) {
    int parent = fs_lookup(path); if (parent < 0) return parent; if (nodes[parent].type != G_DIR) return -20;
    for (unsigned i = 1; i < FS_INODES; ++i) if (nodes[i].used && nodes[i].parent == (unsigned)parent) {
        if (index--) continue;
        memset(out,0,sizeof(*out)); g_copy(out->name,nodes[i].name,40); info(i,&out->stat); return 1;
    }
    return 0;
}
int fs_read(unsigned inode, unsigned offset, void *data, unsigned size) {
    if (!fs_node(inode)) return -2;
    struct fs_inode *n = &nodes[inode]; if (n->type != G_FILE) return -21;
    if (offset >= n->size) return 0;
    if (size > n->size-offset) size = n->size-offset;
    unsigned done = 0;
    while (done < size) {
        unsigned position = offset+done, chunk = FS_BLOCK-position%FS_BLOCK;
        if (chunk > size-done) chunk = size-done;
        if (block_io(*reference(inode,position/FS_BLOCK),scratch,0)) return -5;
        memcpy((uint8_t *)data+done,scratch+position%FS_BLOCK,chunk); done += chunk;
    }
    return (int)done;
}
unsigned fs_free_blocks(void) {
    unsigned count = 0; for (unsigned i = 10; i < block_count; ++i) if (!bit(bitmap,i)) ++count; return count;
}
int fs_write(unsigned inode, unsigned offset, const void *data, unsigned size) {
    if (!mounted) return -5;
    if (read_only) return -30;
    if (!fs_node(inode)) return -2;
    struct fs_inode *n = &nodes[inode]; if (n->type != G_FILE) return -21;
    if (offset > n->size) return -22;
    if (offset > fs_max_size() || size > fs_max_size()-offset) return -27;
    if (!size) return 0;
    unsigned end = offset+size, old_count = (n->size+FS_BLOCK-1)/FS_BLOCK;
    unsigned count = (end+FS_BLOCK-1)/FS_BLOCK;
    if (count < old_count) count = old_count;
    unsigned pointer_needed = version == 2 && count > FS_V2_DIRECT && !n->blocks[FS_V2_DIRECT];
    if (fs_free_blocks() < count-old_count+pointer_needed) return -28;
    if (pointer_needed) {
        unsigned b = 10; while (bit(bitmap,b)) ++b;
        n->blocks[FS_V2_DIRECT] = b; mark(bitmap,b,1); memset(indirect[inode],0,FS_BLOCK);
    }
    for (unsigned j = old_count; j < count; ++j) {
        unsigned b = 10; while (bit(bitmap,b)) ++b;
        *reference(inode,j) = b; mark(bitmap,b,1); memset(scratch,0,FS_BLOCK);
        if (block_io(b,scratch,1)) goto failed;
    }
    unsigned done = 0;
    while (done < size) {
        unsigned pos = offset+done, chunk = FS_BLOCK-pos%FS_BLOCK;
        if (chunk > size-done) chunk = size-done;
        if (block_io(*reference(inode,pos/FS_BLOCK),scratch,0)) goto failed;
        memcpy(scratch+pos%FS_BLOCK,(const uint8_t *)data+done,chunk);
        if (block_io(*reference(inode,pos/FS_BLOCK),scratch,1)) goto failed;
        done += chunk;
    }
    if (end > n->size) n->size = end;
    n->modified = rtc_seconds(); int result = fs_sync(); return result < 0 ? result : (int)done;
failed:
    /* Unpublished allocations can be reclaimed. Existing bytes may be partially
       written, so keep the volume read-only until it is checked offline. */
    for (unsigned j = old_count; j < count; ++j) {
        uint32_t *p = reference(inode,j); if (*p) { mark(bitmap,*p,0); *p = 0; }
    }
    if (pointer_needed) { mark(bitmap,n->blocks[FS_V2_DIRECT],0); n->blocks[FS_V2_DIRECT] = 0; }
    return -5;
}
int fs_replace(const char *path, const void *data, unsigned size) {
    if (!mounted) return -5;
    if (read_only) return -30;
    if (size > fs_max_size()) return -27;
    char name[40]; int parent = walk(path,name); if (parent < 0) return parent;
    int old = child((unsigned)parent,name);
    if (old >= 0 && nodes[old].type != G_FILE) return -21;
    if (old >= 0 && vfs_busy((unsigned)old)) return -16;
    unsigned count = (size + FS_BLOCK - 1) / FS_BLOCK;
    if (fs_free_blocks() < count + (version == 2 && count > FS_V2_DIRECT)) return -28;
    unsigned slot;
    char temporary[40];
    for (slot = 1; slot < FS_INODES; ++slot) {
        g_copy(temporary,".gatha-save-",40); g_decimal(temporary+12,(int)slot);
        if (!nodes[slot].used && child((unsigned)parent,temporary) < 0) break;
    }
    if (slot == FS_INODES) return -28;
    struct fs_inode *n = &nodes[slot]; memset(n,0,sizeof(*n));
    n->used = 1; n->type = G_FILE; n->parent = (unsigned)parent;
    n->created = n->modified = rtc_seconds(); g_copy(n->name,temporary,40);
    int result = fs_write(slot,0,data,size);
    if (result < 0) {
        if (!read_only) { release_blocks(slot); memset(n,0,sizeof(*n)); }
        return result;
    }
    /* The old name remains intact until all replacement data has been flushed.
       This protects capacity failures, but is not a power-loss journal. */
    if (old >= 0) { n->created = nodes[old].created; release_blocks((unsigned)old); memset(&nodes[old],0,sizeof(nodes[old])); }
    g_copy(n->name,name,40); nodes[parent].modified = n->modified;
    return fs_sync();
}
