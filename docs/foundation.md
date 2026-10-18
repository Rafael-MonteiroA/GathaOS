# Foundation expansion (Stage 9)

## Process memory

GEX v2 keeps the GEX1 magic and eight-word header but uses version 2. Images
load at 0x00400000, with at most 16 MiB of code/data/BSS, and a 64 KiB stack
ending at 0x04000000. The space below the stack remains unmapped. GEX v1
retains its old 3 MiB image cap and stack ending at 0x00800000.

Every user process has a separate initially empty heap at 0x04000000 through
0x0bffffff (128 MiB maximum). Page tables are allocated on demand, shared
kernel mappings stay supervisor-only, and every user pointer check walks all
covered PDEs/PTEs. The cap is virtual capacity, not a promise that RAM is free.
There is no demand paging or swap. Pages are committed and zeroed on growth.

SYS_SBRK (29) accepts a signed byte increment in EBX and returns the previous
break, or -12 on bounds/OOM. Zero queries the break. Shrinking releases wholly
unused pages and empty page tables. Failed growth rolls back every new frame
and table; existing mappings and the break stay unchanged. Task exit/fault
reclaims all user PDEs, not only the original single 4 MiB mapping.

libgatha supplies g_malloc/g_calloc/g_realloc/g_free. Allocations are 16-byte
aligned, adjacent free blocks coalesce, realloc failure retains the original,
and calloc rejects multiplication overflow. Empty arenas return all their pages.
Nonempty arenas retain free blocks for reuse until the process exits or all
allocations are freed. Zero-size allocation returns NULL; free(NULL) is valid.
Other frees require a live pointer returned by this allocator. Do not mix direct
sbrk manipulation with live allocator blocks. There is no multithreading API.

## Larger GathaFS volumes

New seeds use version 2, with the same 32 MiB volume and 128 x 256-byte inodes.
The last of the old 48 direct pointers is instead an indirect-block pointer.
The first 47 pointers remain direct. The indirect block holds 1024 little-endian
u32 pointers; the file cap is 1024 data blocks (4 MiB), so the final 47 entries
must be zero even at the cap. A file exceeding 47 blocks requires one extra
metadata block. Mount accounts for its ownership and verifies every unused
pointer, block bound, duplicate and bitmap bit. The kernel caches pointer blocks
in a fixed 512 KiB BSS array, avoiding allocation failure during metadata access.

Version 1 remains readable/writable with its original 48-direct / 192 KiB limit.
There is no automatic upgrade, and older kernels cannot mount version 2.
The host builder/checker/reader supports both versions; --v1 builds a v1 image.
The new writable disk is gathaos-foundation-disk.img. Existing user disks and
all earlier stage seeds remain untouched.

## Save and failure behavior

SYS_SAVE (30) takes path, buffer and byte count in EBX/ECX/EDX. It stages a
replacement under a temporary inode/name, writes and flushes the new bytes,
then releases the old blocks and publishes the destination name. Editor,
Paint and terminal copies use this path. It requires enough free space for
both versions and one free inode. Oversize, busy, inode-full and disk-full
errors preserve the old destination. Open destinations return -16. A successful
replacement preserves creation time but can change the inode/readdir order.

Disk read/write/flush failure makes the mounted filesystem read-only. Further
mutations and sync return -30, preventing later operations from publishing
partially updated metadata. A failed in-place low-level write can have changed
existing bytes; read-only mode is damage containment, not rollback. Failed
staged saves can leave a .gatha-save-N file. Do not edit those files while saving.
There is no journal, atomic multi-sector commit, or offline repair tool. Power
loss can still corrupt metadata, and an I/O error during the final publish does
not guarantee recovery of the old name. Keep backups and use clean shutdown.

## Applications and diagnostics

Editor allocates 256 KiB, allowing up to 262143 text bytes plus NUL. Open uses
a second temporary buffer; unsuccessful reads do not replace the current text.
Terminal cp allocates the source size; cat still has its 192 KiB display limit.
The diagnostic /System/Tests/foundation.gex is launched explicitly through
Files. It owns /FoundationTest, exercises a 5 MiB BSS and dynamic allocations,
writes large files, then deliberately faults while holding memory and a file
descriptor so the host can verify cleanup. Run it only on a disposable disk.

## Remaining limits

Large syscalls still run with interrupts disabled and ATA uses synchronous PIO.
Bulk I/O/zeroing may pause the desktop and delay timer/input handling; latency
work is needed before networking. Kernel heap stays 16 MiB, task count 16,
windows 8 and user stack 64 KiB. There is no NX, FPU/SIMD context, SMP, threads,
networking, Unicode, clipboard, journaling or modern browser in this stage.
