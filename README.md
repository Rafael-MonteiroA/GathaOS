# GathaOS

GathaOS is an original, freestanding x86 operating system written in NASM and C.
**Stage 1 — Kernel Core** is implemented. Its custom BIOS loader enters 32-bit
protected mode and starts a kernel with its own GDT/IDT, exception diagnostics,
PIC handling, a 100 Hz PIT and COM1 logging. It contains no Linux kernel, GRUB,
hosted libc or existing OS/userland. No applications or interactive controls
exist yet. Stage 2 requires explicit user approval.

## Build on Ubuntu / WSL2

```sh
bash scripts/setup-deps.sh
make clean && make iso
make test
make run
```

Run from the repository root in Ubuntu/WSL2. The toolchain is Clang with
`--target=i686-elf`, LLD, LLVM objcopy, NASM, Python 3 and xorriso. Host libraries
are never linked into the kernel. `make` is equivalent to `make iso`.
Shell scripts are invoked with `bash`, including on Windows mounts.

Prefer WSL's native filesystem for builds: Windows/WSL clock skew on `/mnt/c`
can produce Make timestamp warnings. Release verification uses a temporary
native checkout, then copies the tested artifacts back and verifies their hashes.

## Run the normal kernel

```sh
qemu-system-i386 -cdrom dist/gathaos-stage1-core.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

This command also works in PowerShell with QEMU on PATH. WSL graphical runs
need a display such as WSLg. `make test` is headless and needs no graphical desktop.

The normal screen retains the Stage 0 navy background, light border, six color
bars and checkerboard. COM1 reports boot configuration, the IRQ preservation
self-check, `PIT: ticks=100`, `200`, `300`, and so on at roughly one-second
intervals. `STAGE1: PASS` appears after 300 ticks; the counter continues afterward.
Close the QEMU window to stop the guest.

For VirtualBox / VMware use a 32-bit Other OS VM, legacy BIOS (EFI disabled),
256 MiB RAM, VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB of
video RAM, and 3D acceleration disabled. Boot the mounted ISO from the optical
drive. No hard disk is required. These hypervisors have not been boot-tested.

## Deliberate exception demo

```sh
make iso DEBUG_FAULT=div0
make run DEBUG_FAULT=div0
```

This produces `dist/gathaos-stage1-core-div0.iso`. After 200 timer ticks an
assembly `div ecx` instruction with ECX zero raises a real divide exception.
The red panic screen shows the cause, vector, error, instruction pointer,
general registers, segment registers, flags and control registers. The CPU
halts with interrupts disabled; restarting the VM repeats the test. The font is
original and its permission grant is in `docs/panic-font-license.txt`.

`DEBUG_FAULT=none` is the default; deliberate fault instructions are excluded
from that binary. `DEBUG_FAULT=gp` is an additional test build that triggers a
general-protection exception with CPU-pushed error code 0x18. Separate build
directories prevent debug/normal object files from being mixed.

## Automated verification

`make test` boots three actual ISOs using QEMU TCG: the normal kernel, divide
error and general-protection error. It verifies:

- GDT/IDT/PIC/PIT initialization and IRQ preservation of seven general registers
  plus CF, IF and DF, checked across a hardware timer interrupt.
- Normal tick counts through 500 and their approximate wall-clock cadence.
- Exception vector, error code, fault EIP against the linked ELF symbol, saved
  register sentinels, segment selectors, stack bounds and flags.
- Every captured framebuffer pixel, including the panic text and register values.
- Panic leaves the virtual CPU halted with IRQs disabled and no continuing output.

Evidence is written to `build/test-none/`, `build/test-div0/` and `build/test-gp/`.
The GP ISO used by the full suite is temporary under `build/`; the normal and
divide-demo releases are in `dist/`. You can run only a fault test with
`make test DEBUG_FAULT=div0` or `make test DEBUG_FAULT=gp`.

`make clean` removes only `build/`. It preserves all stage ISOs in `dist/`,
including Stage 0. Identical rebuilds verify an existing image; different bytes
are rejected. For an approved bug-fix release choose new names, for example:

```sh
make clean && make iso ISO=dist/gathaos-stage1-core-fix1.iso
make test ISO=dist/gathaos-stage1-core-fix1.iso PANIC_ISO=dist/gathaos-stage1-core-div0-fix1.iso
```

## Scope and documentation

See `docs/stage-1-report.md`, `docs/interrupts.md`, `docs/boot-protocol.md`,
`docs/memory-map.md` and `docs/architecture.md`. Historical Stage 0 evidence
and its report remain available. The fixed boot image still limits the kernel
payload to 64 KiB and supports VBE 1024x768x32 with an unverified 800x600 fallback.

There is no paging, allocator, task switching, userspace, filesystem, input,
general graphics toolkit or application. The panic text drawer serves diagnostic
output only. `libgatha/`, `gui/`, `apps/` and unused kernel directories remain
empty placeholders for their assigned stages.
