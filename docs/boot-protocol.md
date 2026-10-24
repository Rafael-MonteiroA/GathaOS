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
Entries are retained as firmware data. Stage 2's physical memory manager honors
their types, attributes and reservations; the original Stage 0 had no allocator.

VBE 2 uses the standard pitch and masks; VBE 3 uses the dedicated linear pitch
and masks. The kernel accepts non-overlapping byte-aligned 8-bit RGB fields and
honors their positions. It accepts 1024x768x32 or the fallback 800x600x32.

Kernel parameters are not a public userspace ABI. Stage 4 defines separate
`.gex` and syscall contracts in `gex.md` and `syscalls.md`.

Stages 1 and 2 preserve the bootloader handoff above. In Stage 2 the low entry
trampoline enables paging, jumps to the linked higher half and changes ESP to
0xc0090000 before calling C. EBX becomes 0xc0004000. C copies the structure into
its own static storage; the original E820 address remains physical, while the
copied framebuffer address is changed to its virtual mapping after paging setup.
No userspace ABI is implied. See `memory.md` for the transition and
`interrupts.md` for the kernel's ring-0 exception-frame contract.

Stage 15 preserves this version-1 handoff. The CD now uses no emulation with
EDD reads, a 32 KiB bounce buffer and verified copies above 1 MiB. The first
16 MiB are temporarily mapped by the kernel. See `boot-loader.md`; GEX and
GathaFS formats are unchanged.
