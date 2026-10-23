# GathaOS - HTTPS expansion

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
xorriso and QEMU; verification also uses OpenSSL and python3-cryptography.
Build on WSL's native filesystem to avoid Windows/WSL clock
skew. All guest code is freestanding. The host operating system is only tooling.
`make` defaults to `make iso`; `make clean` removes build/ and preserves dist/.

Outputs: **dist/gathaos-stage11-https.iso** and the 32 MiB
**dist/gathaos-stage11-disk.img** GathaFS v2 seed. Earlier artifacts remain.
This is step 3 of the post-release plan; see docs/expansion-plan.md.
Existing ISO releases are immutable; changed bytes require a new ISO filename.
The writable dist/gathaos-https-disk.img is created only if missing. Never replace a
working disk with a seed: that would remove the user's documents.

## Run

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage11-https.iso -drive file=dist/gathaos-https-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

QEMU must be on PATH. On WSL, interactive graphics require WSLg or a display;
tests are headless. A GathaFS IDE primary-master disk is required. For a fresh
installation, copy the Stage 11 seed to a new working disk and use its path in
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
For networking, enable Adapter 1, NAT, cable connected, and select PCnet-PCI II
(Am79C970A). VMware needs equivalent legacy BIOS/VGA/IDE/PCnet configuration.
HTTPS additionally needs RDRAND exposed to the guest and a correct UTC RTC.
Automated Stage 11 verification uses QEMU; Stage 11 has not been boot-tested in
VirtualBox or VMware. Earlier Stage 8 was booted in VirtualBox with a visible GUI.

## Network commands

```text
net
nslookup example.com
wget http://example.com/ /Documents/Internet.html
cat /Documents/Internet.html
wget https://www.google.com/robots.txt /Documents/Google.txt
cat /Documents/Google.txt
```

Wait for DHCP to report connected. Escape cancels a download. HTTP/HTTPS downloads
up to 4 MiB preserve an existing destination on failure. TLS 1.2 supports X25519,
AES-128-GCM and selected RSA certificate chains, with domain/date/signature checks.
There is no graphical browser yet. HTTPS access to a file on Google does not mean
the OS can render its search interface. See [network.md](docs/network.md) and
[https.md](docs/https.md) for the supported profile and limitations, including the
small trust store and absence of revocation checks or a security audit.

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

See docs/stage-11-report.md for this release's measured results and limitations.
Earlier stage reports remain available.

`make test` runs actual QEMU TCG boots: 64/256/512 MiB, native keyboard sets 1/2,
fresh-process persistence boots, four deliberate kernel faults, clean halt,
actual reset/reboot, and a separate minimum 600-second desktop soak. It sends
PS/2 input via QMP, checks rendered text pixels and saved file/bitmap bytes,
validates the filesystem independently, and checks frame/heap reclamation and
bounded idle redraw. Diagnostic animation/worker tasks exit after six seconds.
Each normal boot still runs the memory, IRQ, input, filesystem and isolation tests.

Evidence is under build/test-*; Stage 11 release copies are in dist/stage11-evidence.
New foundation tests launch a large GEX, exercise heap OOM and fault cleanup,
verify multi-megabyte files across restarts, and test legacy disk/GEX loading.
Host tests inject disk/flush failure into the actual filesystem implementation.
Network boots test PCnet/NAT downloads, a controlled Ethernet peer with malformed
packets/loss, process fault cleanup, and offline operation. The separate live
test downloads an external HTTP page; it requires internet access. TLS tests check
original crypto against independent host implementations, certificate/DER parsing,
server interoperability, tampering/replay/truncation and entropy/clock failures.
The separate live TLS test compares Google's HTTPS robots.txt with a host fetch.
Test roots never enter Terminal's trust store.
Serial logs, screenshots and JSON results identify exactly what was tested.
The full suite takes well over ten minutes. A single configuration can run with
`make test-boot RAM=64`. Fault builds require DEBUG_FAULT=page, readonly, div0 or
gp; use an ISO path under build/ to keep release artifacts separate.

## Scope and limits

A release candidate for emulators, not a general-purpose PC replacement.
No browser, USB, audio, package management or compatibility layers. Supports
PS/2 three-byte mouse and ASCII input, no wheel, ABNT2 or Unicode. Eight windows,
sixteen tasks, 512 MiB managed RAM, 16 MiB kernel heap; no SMP or FPU/SIMD context.
The loader accepts a kernel payload up to 448 KiB. Software presentation has no
vertical synchronization. Theme settings are session-only. Editor capacity is
256 KiB; Paint uses a fixed 256x160 canvas. Save edits before closing apps.
GathaFS has 128 inodes, 4 MiB per file on v2 disks (192 KiB on v1), and no
crash journal. GEX v2 images can use 16 MiB plus a separately bounded 128 MiB
heap, subject to free RAM. GEX v1 keeps its original bounds.
New saves stage data before replacement; disk-full errors preserve old files.
An I/O error disables further writes. This is not power-loss atomicity.
Large synchronous operations may pause input; see docs/foundation.md.
Boot diagnostics reserve /persist.bin, /testdir, /user-test.txt and /userdir;
do not use those names for personal data. Ten minutes of tested operation is
not evidence of unlimited uptime or arbitrary-hardware compatibility.

## Documentation

- [Architecture](docs/architecture.md), [boot protocol](docs/boot-protocol.md)
- [Memory map](docs/memory-map.md), [memory management](docs/memory.md), [interrupts](docs/interrupts.md)
- [Syscalls](docs/syscalls.md), [GEX](docs/gex.md), [GathaFS](docs/gathafs.md)
- [Graphics/input](docs/graphics-and-input.md), [GUI](docs/gui.md), [applications](docs/applications.md)
- [Visual design](docs/visual-design.md), [Stage 8 report](docs/stage-8-report.md)
- [Foundation expansion](docs/foundation.md), [Stage 9 report](docs/stage-9-report.md)
- [Networking](docs/network.md), [Stage 10 report](docs/stage-10-report.md)
- [HTTPS/TLS](docs/https.md), [Stage 11 report](docs/stage-11-report.md)

Previous stage reports and ISOs remain available. Font artwork is original;
permissions are in docs/graphics-font-license.txt and docs/panic-font-license.txt.
