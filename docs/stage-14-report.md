# Stage 14 - compression

Date: 2026-10-24. Status: complete.

## Changes

Browser and Terminal now request and decode gzip and zlib-wrapped deflate web
responses automatically. The original freestanding decoder supports stored,
fixed-Huffman and dynamic-Huffman blocks, overlapping matches, bounded gzip
optional headers and concatenated members. CRC32/ISIZE, optional header CRC and
Adler-32 must verify before a page or file is replaced. No imported decoder,
hosted libc, guest Linux or external browser code was added.

HTTP framing precedes decoding. Unsupported/stacked/duplicate codings and
forbidden framing/coding trailers fail clearly. Both encoded and decoded sizes
are bounded; UI polling, Escape/Stop, window close and the existing deadline
remain active during decoder work. The original page/file survives errors.
About identifies Stage 14 and Browser Home mentions compressed-page support.
See compression.md for the precise format subset and limits.

The first full regression exposed a pre-desktop ATA flush timeout at 64 MiB.
The existing CPU-iteration deadline was replaced with a polled PIT-channel-2
deadline (about 33 seconds), usable before IRQ0 and with interrupts disabled.
An independent host I/O model verifies delayed readiness past the old ceiling,
expiration, DRQ/error/absent-device handling and speaker-control restoration.
No storage error is suppressed; real timeouts keep the existing read-only failure
policy. Synchronous disk waits can still stall the GUI. The final suite below
uses this fix and was restarted from a clean build.

## Verification

Clean `make clean && make iso`, the complete `make test`, and separate external
HTTP/TLS/Browser tests passed. Evidence contains **35 actual QEMU boots**,
including 64/256/512 MiB, keyboard variants, intentional fault recovery,
filesystem persistence, network/TLS failures, browser forms, shutdown and reboot.
Compilation has no warnings and the kernel ELF variants have no unresolved symbols.

The exact decoder linked into the guest passed **20,452 independent host vectors
and mutations** under ASan/UBSan. Host zlib/gzip supplied the independent oracle;
the tests cover compression levels/block types, windows, optional headers, member
boundaries, checksums, all truncations of small specimens, expansion limits,
dictionary/raw rejection, 20,000 mutations and callback cancellation/timeouts.
This includes cancellation at the start of a later gzip member.

The compression QEMU boot exercised actual PS/2 Browser/Terminal input. It
verified rendered text pixels, a real GET form query and compressed response,
length/chunked/close framing, empty bodies, optional headers, concatenated
members, case-insensitive x-gzip and identity. Bad checksums, truncation, trailing
garbage, unsupported/duplicate coding, excessive expansion and coding trailers
preserved the current page or file. Escape and close occurred after the decoder
started. Subsequent requests succeeded and process/window memory returned to
64031 free frames and 3149824 kernel-heap bytes.

Three independently parsed GathaFS downloads match the host's 131,073-byte binary
payload exactly (including the destination preserved across failed/cancelled
requests): SHA-256 57b58320bc0483339ebf974a2a7c23e6eee57c18f363bbf8c41b9b46c716d174.
The server's recorded headers/query are in
dist/stage14-evidence/test-network-compression/http-requests.json.

Existing URL/HTML/form sanitizers, crypto vectors, certificate mutations and
filesystem tests also passed. The desktop soak ran **617.54
seconds**, **24 cycles**, returning each cycle to
64031 free frames and 3149824 kernel-heap
bytes with zero dropped events. It exercises the offline form and existing apps.

External Browser tests opened Example Domain and Google's HTTPS robots.txt.
Terminal's authenticated Google response matched an independent host fetch:
6,624 bytes, SHA-256 1aba570a1b32deb66941e0c29d4f37be71ee58aa1a624aa20754b3639538cf2c.
The Google HTTPS download also used content decompression, confirmed by the serial log.
These tests do not establish a working Google Search interface.

A second clean build reproduced the identical ISO; prior release ISO hashes and
existing working disks were preserved. The kernel payload is 450,684
bytes, leaving **8,068 bytes** below the BIOS loader's current
458,752-byte limit. Both production Browser/Terminal GEX files match the build.
No private test roots or keys enter the seed. Existing VirtualBox VMs were not changed.

## Artifacts and launch

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage14-compression.iso

Size: 1,851,392 bytes.
SHA-256: ac97b9c4d8a6e7ce69fd8ae0c83447a5bfd7dcb50d35b291f9dc2096999263cb

Seed: dist/gathaos-stage14-disk.img (33,554,432 bytes, GathaFS v2).
Working disk: dist/gathaos-compression-disk.img, created only if missing.
Changed/generated file tree: docs/stage-14-files.txt.
Evidence: dist/stage14-evidence, dist/stage14-build-verification.log and
dist/stage14-SHA256SUMS.txt. Result JSON files identify the tested ISO.

From the repository root in WSL:

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage14-compression.iso -drive file=dist/gathaos-compression-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: Other/Unknown 32-bit, BIOS/EFI off, 256 MiB RAM, VBoxVGA/64 MiB,
3D off, PS/2, new ISO in the optical drive, a copy of the GathaFS working disk
on primary-master IDE, PCnet-PCI II (Am79C970A) NAT and cable connected.
Convert a copy of the raw seed to VDI if needed; never replace a user disk.
HTTPS needs exposed RDRAND and a correct UTC RTC. Normal Start opens a visible
window; Right Ctrl releases input and Right Ctrl+F exits fullscreen.
VMware needs equivalent legacy BIOS/VGA/IDE/PCnet/CPU/clock support.
Stage 14 was boot-tested in QEMU only, not VirtualBox or VMware.

## Manual checklist

- [ ] Boot the new ISO and confirm About shows Stage 14 / Web compression.
- [ ] Open Start > Browser and visit http://example.com/.
- [ ] Visit https://www.google.com/robots.txt; text should render when TLS is supported.
- [ ] In Terminal, download that URL to /Documents/Google14.txt and inspect it with cat.
- [ ] Use Forms demo, type a query, submit, return with Back and edit again.
- [ ] On a controlled gzip/deflate HTTP server, compare displayed text with the original.
- [ ] Try corrupt/oversized responses; the prior page/file must survive.
- [ ] Cancel a long transfer with Escape/Stop; close and reopen the app.
- [ ] Check existing apps and save personal documents before shutdown.

## Limits and next step

Browser decodes at most 512 KiB; Terminal at most 4 MiB. Encoded bodies may be
64 KiB larger. The whole input and separate output buffer are held in user memory.
There are at most 32 gzip members and 8 KiB of header prefix per member. No Brotli,
stacked coding, preset dictionary, raw-deflate fallback or streaming display.
The prior ASCII HTML/GET-form and limited TLS 1.2/RSA trust profile remain.

No CSS, images, JavaScript, cookies, login or Google Search interface is added.
Before adding larger browser features, the next update should address the BIOS
payload limit while keeping an original bootloader and real-boot regression
coverage. Then HTTP/HTTPS compatibility and page rendering can expand further.
This stage stops for user review; no next stage has been started.
