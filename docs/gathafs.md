# GathaFS v1 disk format and VFS

GathaFS is an original fixed-capacity filesystem for the GathaOS IDE disk.
The volume starts at sector zero, without a partition table. All integers are
little-endian. Sectors are 512 bytes; filesystem blocks are 4096 bytes. The
builder emits a deterministic 32 MiB / 8192-block image from `rootfs/`.

## Superblock (block 0)

The first twelve 32-bit words are:

| Offset | Field/value |
|---:|---|
| 0 | Magic 0x31534647 (`GFS1`) |
| 4 | Version 1 |
| 8 | Block size 4096 |
| 12 | Total blocks (16..32768, within ATA capacity) |
| 16 | Inode count 128 |
| 20 | Bitmap block 1 |
| 24 | Inode-table first block 2 |
| 28 | First data block 10 |
| 32 | Root inode 0 |
| 36 | Image creation timestamp, Unix seconds |
| 40 | Features 0 |
| 44 | FNV-1a checksum of bytes 0..43 |

Remaining bytes are zero. Block 1 contains the allocation bitmap: bit `b%8` of
byte `b/8` describes block b; one means reserved/allocated. Blocks 0..9 are always
reserved. Unused bitmap bits above volume capacity must be zero.

## Inodes and directories

Blocks 2..9 hold 128 fixed 256-byte inodes. Inode zero is the root directory.

| Offset | Field |
|---:|---|
| 0 | Used flag (zero means free) |
| 4 | Type: file=1, directory=2 |
| 8 | Parent inode |
| 12 | Byte size (directories use zero) |
| 16 | Created Unix timestamp |
| 20 | Modified Unix timestamp |
| 24 | NUL-terminated name, 40 bytes including terminator |
| 64 | 48 direct data-block numbers |

Directory membership is represented by the parent/name pair in each inode,
scanned in inode order by readdir. There are no separate directory data blocks.
Names are case-sensitive; sibling duplicates and slash-containing names are
invalid. The host builder accepts ASCII names of at most 39 bytes and rejects
symlinks. Absolute paths are limited to 255 bytes; lookup supports repeated `/`,
`.` and `..`. Rename rejects ancestor cycles and an existing destination.

Files have at most 48 blocks / 192 KiB. There are no indirect blocks, sparse
files, hard links, permissions, symbolic links or journaling. Reads stop at EOF;
seeks stay between zero and current size; writes may extend from EOF. New blocks
are zero-filled. Allocation checks free capacity before changing metadata.
Truncate and unlink release blocks; non-empty directories and open files cannot
be unlinked. Each task owns eight descriptors; task cleanup closes them.

Mount verifies header checksum/capacity, inode types/sizes, names, parent chains,
duplicate names, block bounds/ownership and exact bitmap consistency. A malformed
volume is rejected. Updates write data/bitmap/inodes and issue ATA FLUSH CACHE.
Clean VM restarts are supported; sudden power loss during metadata updates is
not atomic and can leave a volume requiring offline repair. There is no fsck
repair tool yet; the host checker diagnoses corruption but does not modify it.

RTC timestamps use CMOS date/time interpreted as UTC in years 2000..2099. The
builder uses a fixed 2026-01-01 timestamp for reproducibility; runtime changes use
the emulated RTC. A failed RTC sample returns zero. Access time is not recorded.

## Tooling and disk usage

```sh
python3 tools/mkgathafs.py rootfs dist/gathaos-stage5-disk.img
python3 tools/mkgathafs.py check dist/gathaos-stage5-disk.img
python3 tools/mkgathafs.py read dist/gathaos-stage5-disk.img /Documents/Welcome.txt
python3 tools/test_gathafs.py
```

The builder refuses to replace a different existing image. `make iso` supplies
both an immutable stage seed and `dist/gathaos-disk.img`, created only if absent.
The latter is the writable runtime disk used by `make run`; existing user data
is preserved across builds. Tests clone the seed into their evidence directories
and never modify the runtime disk. Attach the disk as primary-master IDE:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage5-storage.iso -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -serial stdio -no-reboot
```

The PIO driver polls with bounded waits, checks status/error bits, uses LBA28,
single-sector transfers and explicit cache flush. Only primary-master ATA is
supported; no AHCI/NVMe/DMA/ATAPI filesystem. The ISO remains a BIOS boot medium;
the companion disk carries persistent data. File syscalls are in `syscalls.md`.

Stage 5's diagnostic boot creates `/persist.bin` (9000 deterministic test bytes),
then verifies it on later boots. Its transient `/testdir` and user syscall probe
`/userdir` are removed after testing. These diagnostic names should not be reused
for manual files in this stage.
