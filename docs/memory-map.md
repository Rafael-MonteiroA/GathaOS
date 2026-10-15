# Stage 0 physical memory map

| Range | Use |
|---|---|
| 0x00000–0x003ff | BIOS interrupt vector table, preserved |
| 0x00400–0x004ff | BIOS data area, preserved |
| 0x00500 and 0x100500 | Temporary A20 probe; original bytes restored |
| 0x04000–0x0403f | Boot information block |
| 0x05000–0x055ff | Up to 64 E820 records of 24 bytes |
| 0x06000–0x060ff | VBE mode information |
| 0x06200–0x063ff | VBE controller information |
| 0x07000–0x071ff | Copy of VBE mode IDs, including terminator |
| Below 0x07c00 | Real-mode stack, growing down |
| 0x07c00–0x07dff | First boot stage |
| 0x08000–0x09fff | Second boot stage and initial GDT |
| 0x10000–0x1ffff | Kernel loading buffer (maximum 64 KiB) |
| Below 0x90000 | Protected-mode stack, growing down |
| 0xa0000–0xfffff | Firmware / device legacy space, preserved |
| 0x100000–0x1fffff | Reserved kernel code, data and BSS (link-time bounded) |
| Firmware-selected address | VBE linear framebuffer |

Stage 0 targets a legacy PC with 256 MiB RAM. There is no paging or heap. All
subsequent memory ownership and reservations will be implemented in Stage 2.
