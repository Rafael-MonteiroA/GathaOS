# GathaOS 1.0 RC1

An original x86 operating system with its own BIOS bootloader, kernel, GathaFS
filesystem, GEX executables, window manager and applications. Written in NASM
and freestanding C. No Linux/BSD kernel, GRUB, hosted libc or external guest GUI.

The desktop uses a quiet blue wallpaper, compact original bitmap typography,
light panels and subtle beveled controls inspired by early-2000s desktops.
Start opens Terminal, Files, Editor, Calculator, Settings, System Monitor,
Paint, About and the widget showcase. Windows drag, stack, minimize, maximize,
restore and close. Documents and drawings persist on the companion IDE disk.

## Build

Run on Ubuntu/WSL2 from the repository root:

```sh
bash scripts/setup-deps.sh
make clean && make iso
make test
```

Tools: Clang targeting i686-elf, LLD, LLVM objcopy, NASM, make, Python 3,
xorriso and QEMU. Build on WSL's native filesystem to avoid Windows/WSL clock
skew. All guest code is freestanding. The host operating system is only tooling.
`make` defaults to `make iso`; `make clean` removes build/ and preserves dist/.

Outputs: dist/gathaos-stage8-release.iso and the byte-identical
**dist/gathaos-1.0-rc1.iso**, plus the 32 MiB dist/gathaos-stage8-disk.img seed.
Existing ISO releases are immutable; changed bytes require a new ISO filename.
The writable dist/gathaos-disk.img is created only if missing. Never replace a
working disk with a seed: that would remove the user's documents.

## Run

```sh
qemu-system-i386 -cdrom dist/gathaos-1.0-rc1.iso -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -serial stdio
```

QEMU must be on PATH. On WSL, interactive graphics require WSLg or a display;
tests are headless. A GathaFS IDE primary-master disk is required. For a fresh
installation, copy the Stage 8 seed to a new working disk and use its path in
`-drive`. Keep existing working disks. Embedded apps work with older compatible
GathaFS disks; /System/Apps on an older disk still contains its older executables.
Ctrl+Alt releases QEMU's captured pointer/keyboard.

VirtualBox: Other/Unknown (32-bit), BIOS (EFI off), 256 MiB RAM, VBoxVGA,
64 MiB video RAM, 3D off, PS/2 input, ISO in the optical drive and GathaFS disk
on primary-master IDE. Convert a **copy** of the raw seed with
`VBoxManage convertfromraw fresh.img fresh.vdi --format VDI` if needed.
Use Start > Normal Start to open a visible console. A running headless session
can be shown with the manager's Show button. Right Ctrl releases input;
Right Ctrl+F leaves fullscreen. Shut down before changing the attached ISO.
VMware needs equivalent legacy BIOS/VGA/IDE configuration. Automated verification
uses QEMU; the user reported Stage 7 running well in VirtualBox. This RC has been booted in VirtualBox 7.2.8, with the visible GUI console
verified on Windows. VMware remains untested.

## Everyday controls

- Start opens applications; the desktop folder icon opens Files.
- Title buttons minimize, maximize/restore and close. Taskbar buttons restore.
- Text fields support arrows, Home/End, Backspace/Delete, Shift selection and Ctrl+A.
- Editor: Ctrl+S saves. Files opens text, GBM2 drawings and GEX executables.
- Settings changes wallpaper and window accent for the current session.
- Keyboard layout is US ASCII. Clock and date are UTC.
- To exit, save documents, open Terminal and type `shutdown`. Wait for the
  safe-to-close screen, then close/power off the VM. This is a flushed, halted
  fallback; it does not switch off the virtual hardware through ACPI.
- `reboot` flushes storage and resets through the keyboard controller. If reset
  is unsupported, a safe-to-close screen remains. Unsaved app edits are not saved
  by shutdown or reboot.

## Verification

The Stage 8 full suite is being rerun after adding synchronization to the host
drag test. See docs/stage-8-report.md for the final release verification status.

`make test` runs actual QEMU TCG boots: 64/256/512 MiB, native keyboard sets 1/2,
fresh-process persistence boots, four deliberate kernel faults, clean halt,
actual reset/reboot, and a separate minimum 600-second desktop soak. It sends
PS/2 input via QMP, checks rendered text pixels and saved file/bitmap bytes,
validates the filesystem independently, and checks frame/heap reclamation and
bounded idle redraw. Diagnostic animation/worker tasks exit after six seconds.
Each normal boot still runs the memory, IRQ, input, filesystem and isolation tests.

Evidence is under build/test-*; release copies are in dist/stage8-evidence.
Serial logs, screenshots and JSON results identify exactly what was tested.
The full suite takes well over ten minutes. A single configuration can run with
`make test-boot RAM=64`. Fault builds require DEBUG_FAULT=page, readonly, div0 or
gp; use an ISO path under build/ to keep release artifacts separate.

## Scope and limits

A release candidate for emulators, not a general-purpose PC replacement.
No networking, USB, audio, package management or compatibility layers. Supports
PS/2 three-byte mouse and ASCII input, no wheel, ABNT2 or Unicode. Eight windows,
sixteen tasks, 512 MiB managed RAM, 16 MiB kernel heap; no SMP or FPU/SIMD context.
The loader accepts a kernel payload up to 448 KiB. Software presentation has no
vertical synchronization. Theme settings are session-only. Editor capacity is
16 KiB; Paint uses a fixed 256x160 canvas. Save edits before closing apps.
GathaFS is limited to 128 inodes and 192 KiB per file, with no crash journal.
Boot diagnostics reserve /persist.bin, /testdir, /user-test.txt and /userdir;
do not use those names for personal data. Ten minutes of tested operation is
not evidence of unlimited uptime or arbitrary-hardware compatibility.

## Documentation

- [Architecture](docs/architecture.md), [boot protocol](docs/boot-protocol.md)
- [Memory map](docs/memory-map.md), [memory management](docs/memory.md), [interrupts](docs/interrupts.md)
- [Syscalls](docs/syscalls.md), [GEX](docs/gex.md), [GathaFS](docs/gathafs.md)
- [Graphics/input](docs/graphics-and-input.md), [GUI](docs/gui.md), [applications](docs/applications.md)
- [Visual design](docs/visual-design.md), [Stage 8 report](docs/stage-8-report.md)

Previous stage reports and ISOs remain available. Font artwork is original;
permissions are in docs/graphics-font-license.txt and docs/panic-font-license.txt.
