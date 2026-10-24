# Stage 15 - large boot

Date: 2026-10-24. Status: complete.

## Changes

The original two-stage BIOS loader now uses El Torito no-emulation CD boot and
EDD reads. It loads in 32 KiB chunks through a low bounce buffer, copying to
physical 1 MiB in protected mode and returning to real mode for firmware reads.
The raw payload limit grows from 448 KiB to **15 MiB**. Kernel plus BSS must fit
below physical **16 MiB**, checked by the linker, packer and loader. Initial
paging uses four tables for 16 MiB; final page protections and PMM reservations
remain active. The kernel/boot-info ABI, GEX, GathaFS and user data formats do
not change.

Before loading, E820 must cover every destination page without a conflicting
reserved entry. A20 is rechecked after reads. Disk failures receive three
attempts, then halt. CRC32 of bytes at their final address must match before
the kernel starts. Metadata, checksum, unavailable RAM and read failures are
reported on serial. Stage 1 reports unsupported EDD/read failures with BIOS text.
CRC32 is corruption detection, not signed or authenticated boot.

About identifies Stage 15 / Extended boot. Browser/Terminal identify version 15
in their HTTP User-Agent. No new browser rendering or TLS algorithms are added.
See boot-loader.md for the layout, transition, bounds and specification references.
No Linux, GRUB or imported guest runtime/code was introduced.

## Verification

Clean `make clean && make iso`, the full `make test`, and external HTTP,
HTTPS and Browser checks passed with no compiler warnings. The normal/fault
kernel ELFs have no unresolved symbols.

Evidence contains **45 actual QEMU boot attempts**:
**37 entered the full GathaOS kernel**, two entered standalone
boundary-test payloads, and six halted on deliberately invalid input.
An additional boot of the identical ISO passed in VirtualBox 7.2.8r173730,
using a newly created 256 MiB BIOS/VBoxVGA VM and a separate test disk.
Serial confirms memory, IRQ and desktop self-tests; the screenshot shows the
1024x768 desktop and Stage 15 About window. This is boot/display coverage,
not the complete QEMU regression. The temporary VM was powered off and
unregistered afterward; the user's existing VMs were not changed.

All normal memory/input/app/persistence cases ran at the existing 64/256/512 MiB
and keyboard-set coverage. Fault screens, user isolation, file I/O,
network/HTTPS negative cases, forms, compression, shutdown and reset passed.

New loader cases:

- A real **6,754,428-byte kernel** contains 6 MiB of independently
  generated patterned data plus 5 MiB of BSS. Guest code checks every data word,
  BSS zeroing, writes and reads before entering the ordinary desktop. Reserved
  physical end: **0x00dc3000**; this crosses the former
  4 MiB bootstrap ceiling.
- A 1,048,577-byte payload exercises a one-byte final 32 KiB chunk; the exact
  **15,728,640-byte maximum** also boots and reaches its own code.
- A large kernel with only 8 MiB RAM stops before any kernel entry.
- Corruption at the first byte, a 32 KiB boundary and the final byte is rejected.
  A truncated CD read and altered image-length metadata are also rejected.
- Moving the boot image from CD LBA 34 to
  164 still reaches the desktop. Catalog units, boot
  info table fields/checksum and exact payload bytes are checked independently.
- Host checks reject empty/oversized payloads, undersized BSS bounds and invalid
  alignment. The maximum accepted size is checked both on host and in QEMU.

The existing crypto/X509, filesystem, HTML/form/URL sanitizer tests passed.
The original inflater passed **20,452 host vectors/mutations** with sanitizers;
real compressed downloads match the independent host bytes.

Desktop soak: **613.06 seconds**, **24 cycles**.
Every cycle returned to 64028 free frames and
3149824 kernel-heap bytes, with no dropped events.
External Browser checks opened Example Domain and Google's HTTPS robots.txt.
Terminal's Google download matched the independent host fetch:
6,624 bytes, SHA-256 1aba570a1b32deb66941e0c29d4f37be71ee58aa1a624aa20754b3639538cf2c.
This does not establish support for the Google Search page.

A second clean build reproduced the ISO byte for byte. Previous ISO hashes
and existing disk images were preserved. The production payload is
**462,972 bytes**, already above the previous 458,752-byte ceiling.
The seed's Browser/Terminal binaries match the clean build; test roots/keys
are absent. Existing VirtualBox VMs were not changed.

## Artifacts and launch

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage15-large-boot.iso

Size: **851,968 bytes**.
SHA-256: e487e71decfbcdf58544c140be28d70a1b509d0886ad3fa360968258ff2cab41

Seed: dist/gathaos-stage15-disk.img (33,554,432 bytes, GathaFS v2).
Working disk: dist/gathaos-large-boot-disk.img, created only if missing.
Tree: docs/stage-15-files.txt.
Evidence: dist/stage15-evidence, dist/stage15-build-verification.log and
dist/stage15-SHA256SUMS.txt.

From the repository root in WSL:

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage15-large-boot.iso -drive file=dist/gathaos-large-boot-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: Other/Unknown 32-bit, BIOS (EFI off), 256 MiB RAM, VBoxVGA/64 MiB,
3D off, PS/2, new ISO as optical media, GathaFS disk on primary-master IDE,
PCnet-PCI II (Am79C970A) NAT with cable connected. Convert a copy of the raw
seed to VDI if needed; never overwrite a disk with personal documents.
HTTPS needs RDRAND exposed and a correct UTC RTC. Start > Normal Start opens a
visible console; Right Ctrl releases input and Right Ctrl+F exits fullscreen.
VMware needs equivalent legacy BIOS/VGA/IDE/PCnet/CPU/clock support.
Stage 15 was boot-tested in QEMU and VirtualBox; VMware was not tested.

## Manual checklist

- [ ] Shut down the old VM before changing its optical ISO.
- [ ] Boot the Stage 15 ISO and confirm About shows Stage 15 / Extended boot.
- [ ] Open Terminal, Files, Editor and Browser; move and close their windows.
- [ ] Save a document, use Terminal `reboot`, and reopen it.
- [ ] Open http://example.com/ and https://www.google.com/robots.txt in Browser.
- [ ] Try Forms demo, submit a query, then return and edit it.
- [ ] Save documents and type `shutdown`; close the VM after the safe-to-close screen.

## Limits and next step

BIOS CD boot only: no UEFI, floppy, USB/raw-disk installer or optical driver in
the kernel. EDD, E820 and VBE are required. The loader rejects memory holes in
the required range; it does not relocate around them. The 15 MiB file limit
shares the 16 MiB physical ceiling with kernel BSS. Physical hardware and
firmware outside QEMU/VirtualBox have not been tested. The loader itself is not
authenticated. Existing browser, networking, storage and GUI limits remain.

The next recommended stage is broader HTTPS/TLS interoperability, choosing and
testing an additional certificate/signature profile with independent host
vectors and positive/negative guest boots. HTML/CSS/images and then DOM/
JavaScript remain later work toward modern sites. No full Google Search,
cookies, login, images, CSS or JavaScript is added here.
This stage stops for user review; the next stage has not been started.
