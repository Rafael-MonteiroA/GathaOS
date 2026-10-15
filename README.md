# GathaOS

GathaOS is an original, freestanding x86 operating system written in NASM and C.
Only **Stage 0 — Toolchain and Boot** is implemented. The current ISO boots its
own two-stage loader, enters 32-bit protected mode, logs to COM1 and draws a
static test pattern. It contains no Linux kernel, GRUB, hosted libc or userland.
There are no applications or interactive controls yet.

## Build on Ubuntu / WSL2

```sh
bash scripts/setup-deps.sh
make clean && make iso
make test
make run
```

Use these commands from the repository root in Ubuntu/WSL2. The build uses
Clang's `--target=i686-elf`, LLD, LLVM objcopy, NASM, Python 3 and xorriso.
The host's libraries are never linked into the kernel. `make` is equivalent to
`make iso`. Shell scripts are invoked with `bash`, including on Windows mounts.
For verification, prefer a checkout in WSL's native filesystem: clock differences
between Windows-mounted files and WSL can cause Make timestamp warnings. The
delivered ISO was clean-built and tested in the native filesystem, then copied
back with its SHA-256 verified.

The image is `dist/gathaos-stage0-boot.iso`. `make clean` removes only `build/`;
it preserves all released ISOs. An identical rebuild verifies the existing ISO;
a different build refuses to overwrite it. For an approved Stage 0 bug fix use:

```sh
make clean && make iso ISO=dist/gathaos-stage0-boot-fix1.iso
make test ISO=dist/gathaos-stage0-boot-fix1.iso
```

## Run

```sh
qemu-system-i386 -cdrom dist/gathaos-stage0-boot.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

On Windows, run that command in PowerShell from the repository root with QEMU
on PATH. On WSL2, `make run` needs a graphical display such as WSLg. `make test`
is headless and does not need one. Close the QEMU window to stop the guest.

For VirtualBox or VMware: create a 32-bit Other OS VM with legacy BIOS (EFI
disabled), 256 MiB RAM, a VGA-compatible adapter (VBoxVGA in VirtualBox), at
least 8 MiB video RAM, and 3D acceleration disabled. Attach the ISO to the
optical drive and boot from it. No hard disk is needed. These hypervisors have
not been tested; QEMU `-vga std` is the verified target.

## Expected screen and test

COM1 prints `GathaOS booting...`, E820 count, VBE dimensions and `STAGE0: PASS`.
The screen has a navy background, light border, six colored bars and a gray / white
checkerboard. The mode is 1024x768x32, with an 800x600x32 fallback when the BIOS
does not advertise the preferred mode. The fallback is implemented but not yet
verified on a BIOS lacking 1024x768.

The automated test boots the **ISO**, waits for serial success, requests a real
QEMU screen dump over QMP and compares every pixel against the expected pattern.
It exits unsuccessfully on a timeout, boot error, missing marker or pixel mismatch.
Logs, a PNG and JSON evidence are written under `build/test/`.

## Layout and scope

- `boot/`: custom 512-byte first stage and 8 KiB second stage.
- `kernel/`: freestanding entry point, boot protocol and minimal serial output.
- `scripts/`, `tools/`: host setup, image packaging, run and smoke test tools.
- `docs/`: architecture, boot protocol, memory map and stage report.
- `libgatha/`, `gui/`, `apps/` and unused kernel directories: empty placeholders
  for later stages, as required by `AGENTS.md`.
- `dist/`: immutable ISO releases and their test evidence.

No IDT, IRQ handlers, timer, allocator, paging, input, filesystem, userspace or GUI
toolkit is implemented. Interrupts stay disabled after entering protected mode.
The required flat GDT and tiny COM1 logger support boot only. Stage 1 must wait
for the user's test and explicit approval.
