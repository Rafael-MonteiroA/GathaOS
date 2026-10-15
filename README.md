# GathaOS

GathaOS is an original, freestanding x86 operating system written in NASM and C.
**Stage 3 — Graphics and Input Drivers** provides a graphical input test screen,
double buffering, drawing primitives, an original bitmap font, PS/2 keyboard
and mouse. Its own BIOS loader boots a higher-half kernel with paging, physical
page allocation, a kernel heap, exceptions, a 100 Hz timer and COM1 logging.
No Linux kernel, GRUB, hosted libc or external guest runtime is used.

## Build on Ubuntu / WSL2

```sh
bash scripts/setup-deps.sh
make clean && make iso
make test
make run
```

Run from the repository root. The tools are Clang (`--target=i686-elf`), LLD,
LLVM objcopy, NASM, Python 3 and xorriso. `make` means `make iso`. The original
`libgatha` implementations run freestanding. Prefer WSL's native filesystem:
Windows/WSL clock skew on `/mnt/c` can produce Make timestamp warnings. Release
builds run in a native temporary checkout, with verified copies returned here.

`make clean` removes `build/` and preserves previous ISOs. Existing release files
are immutable: an identical rebuild verifies them; different bytes require a
fresh filename, such as `ISO=dist/gathaos-stage3-drivers-fix1.iso`.

## Run and use

```sh
qemu-system-i386 -cdrom dist/gathaos-stage3-drivers.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

This works in PowerShell with QEMU on PATH, or Ubuntu with a display. WSL graphical
runs need WSLg or another display; automated tests run headless. No hard disk is
required. Click inside QEMU to capture input; Ctrl+Alt releases the capture.

Type into the large text area using a **US keyboard layout**. Enter, Backspace,
Tab, Shift, Caps Lock, Ctrl+L (clear), wrapping and scrolling work. Hold a key to
repeat. Move the mouse and press its left, middle and right buttons; the status
indicators respond. This is a kernel driver demonstration; there is no desktop,
window manager, file saving or user application yet. Stage 4 requires approval.

VirtualBox / VMware: Other 32-bit OS, legacy BIOS, EFI disabled, 256 MiB RAM,
VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB video RAM, 3D off,
ISO attached to the boot optical drive, PS/2 keyboard/mouse and no disk. These
hypervisors remain untested; QEMU standard VGA is the verified target.

## Tests

`make test` performs eight actual ISO boots:

1. Normal set-2 keyboard kernel with 256, 64 and 512 MiB RAM.
2. Native set-1 keyboard kernel with 256 MiB RAM.
3. Deliberate null-page write, read-only-page write, divide error and general
   protection with 256 MiB RAM, behind separate `DEBUG_FAULT` options.

Every boot checks the existing memory stress suite, frame accounting, higher-half
CPU state and timer/IRQ behavior. Normal boots additionally send real emulated
keyboard and mouse events through QMP, verify typed text against display pixels,
test repeat/release, scrolling, modifiers, three buttons, motion and edge clamps,
and compare the full display after cursor movement to detect trails. Fault boots
check register diagnostics and complete panic images. Evidence lives under
`build/test-<mode>-<RAM>-set<1|2>/` with serial, QMP registers, PNG/PPM and JSON.

```sh
make test-boot RAM=64
make test-boot PS2_SET=1 ISO=build/gathaos-set1.iso
make run DEBUG_FAULT=page ISO=build/gathaos-page.iso
```

Default keyboard set 2 and optional set 1 use separate build directories. Both
disable controller translation and query the selected device set. The suite
keeps auxiliary set-1/fault ISOs in `build/`. Normal kernels contain no deliberate
fault instructions. Other fault choices are `readonly`, `div0` and `gp`.

## Limits and documentation

The physical allocator manages up to 512 MiB; the heap can map up to 16 MiB.
Non-PAE paging is supervisor-only. Boot memory tests run before the back buffer
is allocated. The raw kernel payload limit is 64 KiB and bootstrap map is 4 MiB.
Software buffering is not synchronized to vertical retrace; tearing is possible.
PS/2 standard three-byte mouse and US printable ASCII are supported. There is no
USB HID, wheel support, ABNT2, Unicode, text navigation or persistent editing.

Read `docs/stage-3-report.md`, `docs/graphics-and-input.md`, `docs/memory.md`,
`docs/memory-map.md`, `docs/boot-protocol.md` and `docs/interrupts.md`. Font and
cursor permissions are in `docs/graphics-font-license.txt`; the panic font's
permission is in `docs/panic-font-license.txt`. Previous reports and ISOs remain.
Stage 4 (tasks, scheduler, syscalls, user mode and `.gex`) has not started.
