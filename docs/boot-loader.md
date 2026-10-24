# Stage 15 CD boot loader

The loader remains original NASM code in two stages. No third-party guest
bootloader, kernel or runtime is embedded. The ISO uses BIOS El Torito
**no emulation**; it is not a floppy image or a bootable raw IDE/USB image.
UEFI is unsupported. A BIOS with CD EDD reads, E820 and VBE is required.

## On-disc layout

Offsets below are relative to the boot image, not to the ISO.

| Offset | Content |
|---|---|
| 0..511 | Stage 1, including 55 AA signature |
| 8..63 | Boot info table, patched only in the ISO copy by xorriso |
| 512..2047 | Zero padding in the first CD sector |
| 2048..10239 | Stage 2, exactly 8192 bytes |
| 10240 onward | Raw kernel at relative CD LBA 5, final sector zero-padded |

The boot catalog tells BIOS to load four **512-byte units** (2048 bytes) at
0x7c00. Later EDD reads address **2048-byte CD sectors**. Stage 1's code and
data fit below 0x7e00, so replacing 0x8000..0x9fff with Stage 2 is safe.
Stage 1 probes INT 13h AH=41h and uses AH=42h to read four CD sectors at the
boot-image LBA plus one. It never assumes an absolute ISO sector position.
The info-table fields at offsets 8/12/16/20 contain PVD LBA, image LBA, image
length and the mastering checksum. Host tests verify all these fields and the
catalog. Stage 2 checks the length against its compiled payload size.

## Loading and memory

The raw payload starts at physical 0x100000 and can contain **15 MiB**.
The entire kernel including BSS must end at or before physical 0x1000000.
The linker, packer and loader enforce the bounds. The practical file allowance
shrinks when BSS needs part of this same range.

Before loading, each 4 KiB destination page must fit wholly inside an enabled
E820 type-1 entry. Any overlapping reserved or disabled entry rejects the boot,
independent of firmware record order. The loader rejects maps whose split
entries cannot individually cover a whole page. It does not allocate or place
the kernel around holes. Low firmware/stack/buffer areas retain their previous
reservations; normal kernel PMM reservation extends through its full linker end.

Each EDD request reads at most 16 sectors into 0x10000..0x17fff, within one
64 KiB segment. Three attempts are allowed, restoring the disk packet's count
and resetting the BIOS drive between failures. A20 is verified after each read.
Interrupts are disabled for the short protected-mode copy and CRC calculation;
32-bit flat segments permit writes above 1 MiB. A 16-bit code/data transition
restores real-mode segment limits before the next BIOS call. No unreal-mode
segment cache is expected to survive firmware.

CRC32 is accumulated from the copied destination over exactly KERNEL_BYTES;
sector padding is excluded. Only a matching image can reach the kernel.
CRC32 detects accidental corruption; this is not authenticated/secure boot.
Stage 1 and Stage 2 themselves are not cryptographically verified.
Read, metadata, memory, A20 and checksum failures halt without entering C.
Stage-2 failures appear on COM1; Stage-1 EDD failures use BIOS text output.

After VBE setup, the unchanged boot-info-v1 ABI enters the kernel with paging
disabled. The kernel builds one directory and four page tables to map 16 MiB
both low and high, then clears BSS. Final paging still removes low aliases,
protects text/rodata and preserves the 512 MiB managed-physical-memory limit.

## Verification

`make test` includes `tools/boot_test.py`. It checks the mastered ISO,
builds a real kernel with 6 MiB of patterned read-only data and 5 MiB of BSS,
verifies every test word in the guest, and reaches the ordinary desktop.
Separate payloads exercise a one-byte final chunk and the exact 15 MiB limit.
Negative boots corrupt the first byte, a chunk boundary and the final byte,
truncate the ISO, alter image metadata, and provide insufficient RAM.
A placement variant changes the boot image's absolute LBA.
All boot cases retain serial logs and JSON evidence. The normal suite adds
64/256/512 MiB, persistence, IRQ/input, apps, network, faults, power and soak.

Earlier immutable ISOs retain their original floppy boot paths. Existing IDE
GathaFS disks do not need conversion for the new ISO.

## Specification references

- [xorrisofs manual: no-emulation, boot load size and boot info table](https://manpages.debian.org/unstable/xorriso/xorrisofs.1.en.html).
- [Intel system programming manual, switching processor modes](https://cdrdv2-public.intel.com/835754/253668-sdm-vol-3a.pdf).

These describe firmware/tool interfaces; their code is not used in the guest.
