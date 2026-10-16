#ifndef GATHA_FS_H
#define GATHA_FS_H
#include <stdint.h>
#include "../libgatha/api.h"
#define FS_BLOCK 4096u
#define FS_INODES 128u
#define FS_DIRECT 48u
#define FS_MAX_SIZE (FS_DIRECT * FS_BLOCK)
struct fs_inode { uint32_t used,type,parent,size,created,modified; char name[40]; uint32_t blocks[FS_DIRECT]; };
_Static_assert(sizeof(struct fs_inode) == 256,"GathaFS inode");
struct vfs_fd { unsigned used,inode,offset,flags; };
int fs_mount(void);
int fs_sync(void);
int fs_lookup(const char *path);
int fs_create(const char *path, unsigned type);
int fs_unlink(const char *path);
int fs_rename(const char *old, const char *new_path);
int fs_stat(const char *path, struct g_stat *out);
int fs_readdir(const char *path, unsigned index, struct g_dirent *out);
int fs_read(unsigned inode, unsigned offset, void *data, unsigned size);
int fs_write(unsigned inode, unsigned offset, const void *data, unsigned size);
int fs_truncate(unsigned inode);
const struct fs_inode *fs_node(unsigned inode);
unsigned fs_free_blocks(void);
void fs_self_test(void);
int vfs_open(struct vfs_fd *fds, const char *path, unsigned flags);
int vfs_close(struct vfs_fd *fds, unsigned fd);
void vfs_close_all(struct vfs_fd *fds);
int vfs_busy(unsigned inode);
#endif
