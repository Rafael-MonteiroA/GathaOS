# Gatha Executable, GEX versions 1 and 2

GEX is an original fixed-address flat executable format, not an ELF runtime
loader. Build tooling links a temporary ELF and packs its binary segments.
All eight header words are unsigned little-endian 32-bit values:

| Offset | Value |
|---:|---|
| 0 | Magic 0x31584547 (`GEX1`) |
| 4 | Version 1 (legacy) or 2 (new builds) |
| 8 | Load base 0x00400000 |
| 12 | Entry offset, below RX end |
| 16 | RX bytes, nonzero multiple of 4096 |
| 20 | File payload bytes, excluding the 32-byte header |
| 24 | Memory bytes, page-aligned; v1 at most 3 MiB, v2 at most 16 MiB |
| 28 | FNV-1a 32-bit payload checksum |

The payload follows immediately. The RX portion contains code/read-only data;
subsequent pages are writable data and zero-filled BSS. Memory size >= file size
>= RX size. Exact file length, magic, version, base, bounds and checksum must
match before allocating a task. ELF relocations are resolved by the build linker.
There is no dynamic linker, relocation processing, compression or signatures.
Non-PAE paging has no NX, so writable data is also executable on this target.

Each process has private pages at the same virtual addresses, with a 64 KiB user
stack at 0x007f0000..0x007fffff for v1, or 0x03ff0000..0x03ffffff for v2,
with unmapped space below the stack. Both versions can use the new bounded
heap at 0x04000000..0x0bffffff; see foundation.md for allocation semantics. The entry gets
an application argument in EAX. `libgatha/start.asm` invokes `app_main(argument)`
and turns its return value into `exit`. Applications use original freestanding
memory/string routines and inline syscall wrappers. Release applications are
embedded in the kernel and also ship as GEX files on the seed GathaFS disk.
Files launches those disk executables through SYS_LAUNCH index 99.

The BIOS raw-kernel staging area was expanded to 0x10000..0x7ffff (448 KiB).
The existing sector loop advances ES by 0x20 per 512-byte sector; it already
crosses 64 KiB segment boundaries. It remains below the kernel stack at 0x90000.
The packed boot image still uses 1.44 MiB El Torito emulation and a custom loader.
