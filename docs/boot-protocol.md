# Boot protocol version 1

The kernel is loaded at physical 0x100000 with paging disabled, flat 32-bit code
selector 0x08 and data selector 0x10. Interrupts are disabled; the direction flag
is clear. ESP starts at 0x90000. EBX is 0x4000, the address of this packed-by-design
array of 32-bit little-endian fields (`kernel/boot.h`):

| Offset | Field | Meaning |
|---|---|---|
| 0 | magic | 0x47415448 |
| 4 | version | 1 |
| 8 | e820_count | Number of raw firmware records, 1 through 64 |
| 12 | e820_address | 0x5000 |
| 16 | framebuffer | Physical VBE linear framebuffer address |
| 20 | width | Pixels per row |
| 24 | height | Rows |
| 28 | pitch | Bytes per row, including firmware padding |
| 32 | bpp | 32 |
| 36, 40 | red_size, red_position | Red field size and bit position |
| 44, 48 | green_size, green_position | Green field size and bit position |
| 52, 56 | blue_size, blue_position | Blue field size and bit position |

Each E820 record is 24 bytes: 64-bit base, 64-bit length, 32-bit type and 32-bit
extended attributes. For a 20-byte firmware response, attributes default to 1.
Entries are retained as firmware data; a later physical memory manager must honor
their types, attributes and reservations. No memory allocator exists at Stage 0.

VBE 2 uses the standard pitch and masks; VBE 3 uses the dedicated linear pitch
and masks. The kernel accepts non-overlapping byte-aligned 8-bit RGB fields and
honors their positions. It accepts 1024x768x32 or the fallback 800x600x32.

Kernel parameters are not a public userspace ABI. No `.gex` loader or syscall
entry point exists yet.
