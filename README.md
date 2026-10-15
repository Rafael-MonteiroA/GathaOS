# GathaOS

GathaOS is an original, freestanding x86 operating system written in NASM and C.
**Stage 2 — Memory Management** is implemented. The custom BIOS loader starts a
higher-half kernel with a physical page allocator, paging, a growing kernel heap,
exception diagnostics, a 100 Hz timer and COM1 logging. There is no Linux kernel,
GRUB, hosted libc or external guest runtime. Stage 3 requires user approval.

## Build on Ubuntu / WSL2

```sh
bash scripts/setup-deps.sh
make clean && make iso
make test
make run
```

Run from the repository root. The cross toolchain uses Clang with
`--target=i686-elf`, LLD, LLVM objcopy, NASM, Python 3 and xorriso. `make` means
`make iso`. The kernel uses original `libgatha` memcpy/memset implementations;
host libraries are never linked into it. Scripts are invoked with `bash`.

Prefer WSL's native filesystem: Windows/WSL clock skew on `/mnt/c` can cause
Make timestamp warnings. Release builds are verified in a temporary native
checkout, then copied back and compared by hash. `make clean` removes `build/`
and preserves every previous stage ISO in `dist/`. A changed ISO must use a new
filename; an identical rebuild verifies the existing image without replacing it.

## Run

```sh
qemu-system-i386 -cdrom dist/gathaos-stage2-memory.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

This works in PowerShell with QEMU on PATH. WSL graphical runs need a display
such as WSLg; automated tests are headless. No hard disk is required.

The normal screen still shows the navy background, color bars and checkerboard.
Memory work is visible in the serial console: E820 regions, usable frame counts,
paging configuration, allocation test results, `MMTEST: ALL PASS`, continued
PIT ticks and `STAGE2: PASS`. There is no desktop, mouse or keyboard input yet.

VirtualBox / VMware manual settings: 32-bit Other OS, legacy BIOS, EFI disabled,
256 MiB RAM, VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB video
RAM, 3D disabled, and the ISO in the boot optical drive. Those hypervisors remain
unverified; QEMU standard VGA is the tested target.

## Memory behavior

- Physical allocator: 4 KiB pages and two bitmaps, based on E820. Firmware,
  low memory, kernel/bootstrap structures and framebuffer storage are reserved.
  At most 512 MiB of physical address space is managed; higher RAM is ignored.
- Paging: 32-bit, 4 KiB supervisor pages; kernel code and stack execute above
  0xC0000000. Low identity mappings and both null aliases are removed. Kernel
  text/rodata are read-only with CR0.WP enabled. The framebuffer is mapped apart.
- Heap: `kmalloc`/`kfree`, 16-byte alignment, first-fit splitting/coalescing,
  64 KiB initially mapped, up to 16 MiB of virtual capacity. Growth maps physical
  pages on demand; freeing trailing space returns pages down to the initial size.
  Allocation failure returns NULL and rolls back partial growth.
- Self-tests run on every boot before the PIT starts. See `docs/memory.md` for
  contracts, limits and test details. These are single-CPU, kernel-only allocators.

## Tests and deliberate faults

`make test` performs seven actual ISO boots:

1. Normal kernel with 256, 64 and 512 MiB RAM.
2. Unmapped null write, read-only page write, divide error and general protection
   with 256 MiB RAM, each behind its own `DEBUG_FAULT` compilation option.

Every boot checks memory stress and accounting, timer/IRQ behavior, actual screen
pixels and hardware paging state. Fault cases also check CPU error codes, CR2,
EIP against linked symbols, register values and the final halted state. Evidence
is saved under `build/test-<mode>-<RAM>/`.

```sh
make test-boot RAM=64
make iso DEBUG_FAULT=page
make run DEBUG_FAULT=page
```

The page demo ISO is `dist/gathaos-stage2-memory-page.iso`. It deliberately writes
to address zero after the memory tests and 200 ticks, then shows a page-fault
panic with vector 14, error 2 and CR2 zero. Restart the VM to repeat the demo.
Other modes are `none` (default), `readonly`, `div0` and `gp`. Fault instructions
are absent from the normal kernel, and each variant has separate build objects.
The full test suite keeps readonly/div0/gp images in `build/`.

For a future approved bug fix, choose fresh release names:

```sh
make clean && make iso ISO=dist/gathaos-stage2-memory-fix1.iso
make test ISO=dist/gathaos-stage2-memory-fix1.iso PAGE_ISO=dist/gathaos-stage2-memory-page-fix1.iso
```

## Documentation and remaining scope

Read `docs/stage-2-report.md`, `docs/memory.md`, `docs/memory-map.md`,
`docs/boot-protocol.md` and `docs/interrupts.md`. Historical Stage 0/1 reports and
ISOs remain available. `docs/panic-font-license.txt` records the original font's
permission grant. The current 64 KiB loaded kernel payload limit remains.

No scheduler, userspace, filesystem, general graphics toolkit or application is
implemented. The existing panic text drawer is diagnostic only. Stage 3 will
add graphics primitives, double buffering, keyboard and mouse support after
explicit approval; those features have not been started.
