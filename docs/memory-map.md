# Memory map - GathaOS foundation expansion

## Physical layout

| Range | Use |
|---|---|
| 0x00000–0x003ff | BIOS interrupt vectors, preserved |
| 0x00400–0x004ff | BIOS data area, preserved |
| 0x04000–0x0403f | Original boot information |
| 0x05000–0x055ff | Up to 64 E820 records |
| 0x06000–0x063ff | VBE firmware buffers |
| 0x07000–0x071ff | Copied VBE mode IDs |
| Below 0x07c00 | Real-mode stack |
| 0x07c00–0x07dff | First boot stage |
| 0x08000–0x09fff | Second boot stage and bootstrap GDT |
| 0x10000–0x7ffff | Kernel raw-image loading buffer (448 KiB from Stage 4) |
| Below 0x90000 | Kernel stack's physical backing, reserved |
| 0xa0000–0xfffff | Legacy firmware/device region, preserved |
| 0x100000 | Physical ELF entry, original bootstrap assembly |
| 0x101000–0x102fff | Temporary directory/table for the first 4 MiB |
| 0x103000 onward | Higher-half kernel text, rodata, data and BSS |
| Through __kernel_phys_end | Includes all static bitmaps and final page tables |
| E820 usable pages below 0x20000000 | Physical allocator candidates after reservations |
| Firmware-selected address | Framebuffer, reserved from allocation |

The linker bounds the entire loaded kernel and BSS below physical 4 MiB, the
bootstrap mapping size. The PMM reserves every byte from zero through the
page-aligned linker symbol `__kernel_phys_end`, including the stack and bootstrap
buffers. It additionally reserves framebuffer memory if within its managed range.
The actual reserved end is printed on serial and saved in the ELF map.

## Virtual layout after paging initialization

| Range | Mapping |
|---|---|
| 0x00400000–0x013fffff | GEX v2 image, at most 16 MiB; v1 ends at 0x006fffff |
| 0x007f0000–0x007fffff | GEX v1 stack only |
| 0x03ff0000–0x03ffffff | GEX v2 stack, 64 KiB, lower guard unmapped |
| 0x04000000–0x0bffffff | Per-process heap; only committed pages mapped |
| Other low addresses below 0xc0000000 | Unmapped; no identity map remains |
| 0xc0000000–0xc0000fff | Unmapped high alias of physical page zero |
| 0xc0001000–0xdfffffff | Direct map of physical addresses 0x1000–0x1fffffff |
| 0xc0080000–0xc008ffff | Expected kernel stack region; initial ESP is 0xc0090000 |
| 0xc0103000 onward | Higher-half text/rodata/data/BSS; text and rodata read-only |
| 0xe0000000–0xe0ffffff | Up to 16 MiB kernel heap; initially only 64 KiB present |
| 0xe1000000–0xe7ffffff | Unmapped, including heap's upper guard |
| 0xe8000000–0xe8ffffff | Framebuffer window; only required pages mapped |
| Remaining addresses | Unmapped |

Kernel entries are supervisor-only, using 4 KiB non-PAE paging. User directories
add private user-accessible low pages. The direct map is an
addressing convenience and includes holes/reserved regions: a mapped address does
not mean its physical frame is allocatable or even installed RAM. Allocation
must use PMM results, never arbitrary direct-map addresses. No RAM above 512 MiB
is managed. Text/rodata have no writable low alias; CR0.WP enforces their flags.
The framebuffer has cache-disable/write-through flags and is separate from RAM.

Kernel GDT, IDT, bitmaps and page tables are statically reserved. The heap's
physical pages are tracked by the PMM and return to it when trailing mappings
are released. The bootstrap stack has no dedicated guard page or emergency
fault stack. Stage 4 adds per-process user address spaces; there is no swapping,
PAE or NX.

Stage 3 allocates a width * height * 4 RGB back buffer from the heap after memory
tests complete (3 MiB at 1024x768). The front buffer remains in the VBE window.
The input ring, text cells and cursor save area use static kernel BSS.

Stage 10 reserves aligned PCnet DMA descriptors and packet buffers in kernel BSS,
below the linker-enforced 4 MiB bootstrap boundary. The DMA address is the pinned
physical address, not a virtual pointer. TCP/DNS state and bounded receive rings
also reside in supervisor-only BSS. HTTP response buffers are per-process heap
allocations and are never exposed directly to the NIC.
