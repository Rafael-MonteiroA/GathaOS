#include "fs.h"
static unsigned references[FS_INODES];
int vfs_busy(unsigned inode) { return inode < FS_INODES && references[inode]; }
int vfs_open(struct vfs_fd *fds, const char *path, unsigned flags) {
    if (!(flags & (G_READ|G_WRITE)) || flags & ~15u || ((flags & G_TRUNC) && !(flags & G_WRITE))) return -22;
    unsigned fd; for (fd = 0; fd < 8 && fds[fd].used; ++fd) {}
    if (fd == 8) return -24;
    int inode = fs_lookup(path);
    if (inode == -2 && (flags & G_CREATE)) inode = fs_create(path,G_FILE);
    if (inode < 0) return inode;
    if (fs_node((unsigned)inode)->type != G_FILE) return -21;
    if ((flags & G_TRUNC)) { int r = fs_truncate((unsigned)inode); if (r < 0) return r; }
    fds[fd] = (struct vfs_fd){1,(unsigned)inode,0,flags}; ++references[inode]; return (int)fd;
}
int vfs_close(struct vfs_fd *fds, unsigned fd) {
    if (fd >= 8 || !fds[fd].used) return -9;
    --references[fds[fd].inode]; fds[fd].used = 0; return 0;
}
void vfs_close_all(struct vfs_fd *fds) { for (unsigned i = 0; i < 8; ++i) if (fds[i].used) vfs_close(fds,i); }
