# GathaOS

GathaOS is an original, freestanding x86 operating system written in NASM and C.
**Stage 7 — Applications** adds Terminal, Files, Text Editor, Calculator, Settings,
System Monitor, Paint and About as separate user-mode GEX programs. Drag, stack,
minimize, maximize, restore and close windows; launch from Start. GathaFS provides persistent IDE
storage, an original disk format, a host image builder and file syscalls.
The graphics/input layer provides double buffering, an original bitmap font,
PS/2 keyboard and mouse. Its own BIOS loader boots a higher-half kernel with paging, physical
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
fresh filename, such as `ISO=dist/gathaos-stage7-apps-fix1.iso`.

## Run and use

```sh
qemu-system-i386 -cdrom dist/gathaos-stage7-apps.iso -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -serial stdio
```

This works in PowerShell with QEMU on PATH, or Ubuntu with a display. WSL graphical
runs need WSLg or another display; automated tests run headless. The companion
GathaFS IDE disk is required. `make iso` creates the runtime disk only if absent;
existing disk data is preserved. Click QEMU to capture input; Ctrl+Alt releases it.

Click a field and type with a **US keyboard layout**. Text fields support arrows,
Home/End, Backspace/Delete, Shift selection and Ctrl+A. Try the check box, list,
scrollbar, Help menu and right-click context menu. Title controls are minimize,
maximize/restore and close; taskbar buttons bring windows back. The RTC clock is
UTC. Start opens the applications; the desktop icon opens Files. Read
`docs/applications.md` for controls, shell commands and format/size limits.
The user has authorized sequential completion of the remaining stages.

VirtualBox / VMware: Other 32-bit OS, legacy BIOS, EFI disabled, 256 MiB RAM,
VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB video RAM, 3D off,
ISO attached to the boot optical drive, PS/2 input and the GathaFS disk attached
as primary-master IDE (convert the raw image to a supported disk container). These
hypervisors remain untested; QEMU standard VGA is the verified target.

## Tests

`make test` performs the normal/fault/persistence matrix and host filesystem tests:

1. Normal set-2 keyboard kernel with 256, 64 and 512 MiB RAM.
2. Native set-1 keyboard kernel with 256 MiB RAM.
3. Deliberate null-page write, read-only-page write, divide error and general
   protection with 256 MiB RAM, behind separate `DEBUG_FAULT` options.
4. A second fresh QEMU process for each normal configuration, using the written
   test disk to verify persistence. The host independently parses its data/bitmap.
5. Shutdown (clean halt) and reboot command tests, including a second guest boot
   after the actual reset. These add three boots, for fifteen in total.

Every boot checks memory stress, frame accounting and timer/IRQ behavior.
Normal boots verify scheduler, file persistence and driver self-tests, then send
real emulated PS/2 events through QMP. Application tests verify each launch and
exit, saved text pixels, decimal math, file operations/associations, shell commands,
themes, task reporting and saved bitmap bytes. Stage 6 evidence retains the
window-management and toolkit-specific checks.
Earlier stage evidence preserves the detailed input-lab regression tests. Fault boots
check register diagnostics and complete panic images. Evidence lives under
`build/test-<mode>-<RAM>-set<1|2>/` with serial, QMP registers, PNG/PPM and JSON.

```sh
make test-boot RAM=64
make test-boot PS2_SET=1 ISO=build/gathaos-set1.iso
make run DEBUG_FAULT=page ISO=build/gathaos-page.iso
```

Default keyboard set 2 and optional set 1 use separate build directories. Both
disable controller translation and query the selected device set. The suite
keeps auxiliary set-1/fault ISOs in `build/`. Normal boots exercise isolated user
fault probes; deliberate kernel panics require `DEBUG_FAULT`. Other fault choices
are `readonly`, `div0` and `gp`.

## Limits and documentation

The physical allocator manages up to 512 MiB; the heap can map up to 16 MiB.
Non-PAE paging separates private user mappings and supervisor pages. Boot tests run before the back buffer
is allocated. The raw kernel payload limit is 448 KiB and bootstrap map is 4 MiB.
Software buffering is not synchronized to vertical retrace; tearing is possible.
PS/2 standard three-byte mouse and US printable ASCII are supported. There is no
USB HID, wheel support, ABNT2 or Unicode. The desktop supports eight windows,
bounded event queues and sixteen tasks. Unresponsive programs are not forcibly killed.

Read `docs/stage-4-report.md`, `docs/gex.md`, `docs/syscalls.md`,
`docs/graphics-and-input.md`, `docs/memory.md`,
`docs/memory-map.md`, `docs/boot-protocol.md` and `docs/interrupts.md`. Font and
cursor permissions are in `docs/graphics-font-license.txt`; the panic font's
permission is in `docs/panic-font-license.txt`. Previous reports and ISOs remain.
Read `docs/stage-5-report.md` and `docs/gathafs.md` for the storage format, disk
usage and persistence evidence. See `docs/gui.md` and `docs/applications.md` for
the GUI and application ABI. Stage 8 adds release polish, power-off support,
long-run stability, leak checks and redraw-performance measurements.
