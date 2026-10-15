# Boot architecture through Stage 2

The build runs on a host OS; the guest uses only original NASM and C code.
Firmware services are used in real mode and never after the protected-mode jump.
There are no third-party runtime sources, host libc or dynamic libraries.
Stage 1 adds an original hand-authored font for panic diagnostics only.

1. Legacy BIOS boots the ISO through its El Torito floppy-emulation catalog.
2. The BIOS exposes `boot/gatha.img` as a 1.44 MiB floppy with 18 sectors/track,
   2 heads and 80 cylinders. DL identifies that BIOS device.
3. The first 512-byte stage reads 16 sectors starting at logical sector 1 into
   physical 0x8000. Each read uses INT 13h CHS, with three attempts and resets.
4. The second-stage loader reads the kernel from sector 17 into 0x10000. It gathers E820 entries,
   enables and verifies A20, selects a direct-color VBE linear framebuffer, and
   fills the versioned boot information structure.
5. A minimal flat GDT enables protected mode. The loader copies the raw kernel to
   0x100000 and jumps there with EBX pointing to the boot information.
6. Stage 2's low assembly entry sets up temporary identity/high mappings and
   enables paging. It jumps to high text, clears BSS, moves the stack high and
   calls C with a 16-byte-aligned call stack.
7. The kernel installs high GDT/IDT pointers, initializes the physical allocator
   from E820, installs final mappings and removes the low identity window. It
   maps the framebuffer, draws the static pattern, initializes the heap and
   runs allocation/rollback/content stress tests. See `memory.md`.
8. The kernel remaps the PIC, starts the PIT, verifies register preservation
   across an IRQ and idles between timer interrupts. Debug builds raise an
   exception after 200 ticks. See `interrupts.md`.

The floppy is embedded in the ISO, not a separately required VM disk. It contains
raw fixed-layout boot code, not a filesystem. There is no ISO9660 parser yet.
The packed kernel is limited to 64 KiB; this deliberate Stage 0 limit is checked
by the build. The 8 KiB second-stage limit is checked by NASM padding.

Graphics are written directly once to the hardware framebuffer; double buffering
and reusable graphics primitives belong to Stage 3. A20 is tried via firmware
and port 0x92, then tested using an address alias check. Missing E820, overflow
of its 64-entry buffer, missing VBE or a failed A20 check stops boot with a serial
error. First-stage loader read errors appear through BIOS text output.

The smoke test runs QEMU with software emulation (TCG), no virtual hard disk and
no network device. Serial success alone does not satisfy it: the displayed pixels
must also match, timer counts must continue, and debug builds must report the
expected exception frame. Guest success does not automatically exit the VM;
the test host captures the display and then asks QEMU to quit.
