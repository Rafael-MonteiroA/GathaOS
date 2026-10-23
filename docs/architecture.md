# Architecture - GathaOS foundation expansion

The host toolchain produces an original NASM/C guest with no imported OS runtime.

## Boot path

1. BIOS selects the ISO's El Torito 1.44 MiB floppy-emulation entry. The original
   512-byte stage loads the 8 KiB second stage with retrying INT 13h CHS reads.
2. Stage two loads the raw kernel from sector 17 into physical 0x10000..0x7ffff
   (448 KiB limit), verifies A20, collects up to 64 E820 records, selects VBE
   1024x768x32 (800x600 fallback), and builds the versioned boot information.
3. Protected mode copies the payload to 1 MiB. The bootstrap maps the first
   4 MiB low/high, enables paging and CR0.WP, jumps high, clears BSS and calls C.
4. GDT/IDT, PMM and final paging replace the bootstrap environment. Low identity
   mappings are removed, text is read-only, kernel mappings supervisor-only.
5. Heap and memory self-tests run. The framebuffer back buffer is allocated;
   the startup splash is presented. GathaFS mounts the ATA primary-master disk
   and verifies storage. Graphics/input self-tests prepare the compositor.
6. PIC/PIT and PS/2 initialize. The scheduler launches isolation probes and two
   diagnostic user animations plus a kernel worker, which exit after six seconds.
   About opens on the desktop. Serial retains boot, fault and health telemetry.

The boot floppy is raw fixed-layout code, not GathaFS. User data lives on the
separate IDE disk. There is no ISO9660 reader. BIOS services end at the protected
mode jump; subsequent I/O uses original port/MMIO drivers.

## Execution and memory

A single CPU runs a two-tick, 100 Hz preemptive round-robin scheduler, up to
sixteen tasks. Ring-3 GEX programs have private executable/data/stack pages and
separate page directories. Kernel mappings are shared and protected. A TSS
selects each task's 16 KiB kernel stack. User faults terminate the task; kernel
faults produce a diagnostic panic screen. Dead tasks are reaped on another stack.

The PMM manages usable E820 pages below 512 MiB. The heap grows from 64 KiB to
at most 16 MiB, splitting/coalescing allocations and returning trailing pages.
User heaps are separate and bounded to 128 MiB per process; GEX v2 images can
use 16 MiB. See foundation.md for compatibility and failure behavior.
Interrupt-gate syscalls validate all user pointers and execute with IRQs disabled.
This is a small uniprocessor design, without SMP or floating-point context.

## Desktop and applications

The kernel compositor owns screen and window buffers, damage tracking, stacking,
focus, title controls and bounded event queues. It redraws the union of damaged
rectangles and presents only that region. Pointer movement restores its old
pixels through ordinary composition. Draw transactions prevent partial toolkit
frames from being presented. The compositor periodically reports allocations,
window count, frames, presented pixels and dropped events for host verification.

User-mode toolkit code builds bounded drawing-command batches. Each application
is a separate GEX process. Built-in launch images are embedded in the kernel;
matching copies ship in /System/Apps on a fresh seed. Files can load GEX from
GathaFS, pass document paths, or select Editor/Paint by extension. Closing a
window/process releases its pixel buffers, kernel stack, address space and FDs.

ATA uses polling PIO; GathaFS stores a bitmap, fixed inode table and direct block
references, plus one indirect block per large file on v2 volumes. Operations validate bounds and metadata. Sync flushes the disk cache,
but there is no journal or power-loss atomicity. The shutdown syscall syncs then
halts with a visible safe-to-close screen. Reboot uses the 8042 reset command.
This fallback is intentional and does not claim ACPI hardware power-off.

## Network expansion

An original PCI/PCnet driver and bounded ARP/IPv4/UDP/DHCP/DNS/TCP client stack
are polled from the compositor loop. Network syscalls validate user buffers and
return immediately; user-mode helpers yield while waiting. Terminal implements
status/name lookup and HTTP downloads, saving only complete responses. Exit or
fault releases process-owned sockets and DNS queries. Networking is optional;
absence of a supported adapter leaves the desktop operational. See network.md
for execution, ABI, resource bounds, protocol restrictions and verification.

Stage 11 adds TLS 1.2 and certificate verification entirely in user mode, using
the same TCP API. Original crypto code is shared by Terminal and its diagnostic;
only SHA-256 entropy conditioning runs in the kernel. Syscall 32 exposes bounded
RDRAND output and validated UTC RTC readings. Public roots are compiled into the
application; a disk file cannot change Terminal's trust store. See https.md for
the exact algorithms, bounds and security-policy limitations.

Stage 12 adds a separate Browser GEX, using the shared user-mode HTTP/TLS code.
URL resolution, bounded HTML scanning and visible-row text rendering live in
user space. No new kernel syscall is needed. The Start menu adds index 9 while
preserving existing application indices. See browser.md for navigation policy,
resource caps and unsupported web features.

## Verification boundary

QEMU TCG standard VGA/IDE/PS2 is the automated target. Tests use private copies of
the seed, serial markers, QMP input/registers/screenshots and independent disk
parsing. A separate 600-second minimum soak checks normal application activity,
window operations, post-close physical frames/heap and idle redraw bounds.
A passing soak establishes that run's behavior, not indefinite stability.
