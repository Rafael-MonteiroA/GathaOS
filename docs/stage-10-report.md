# Stage 10 - Network expansion (new plan, step 2)

Date: 2026-10-21. Status: complete.

## Changes

The user authorized continuing the post-release roadmap after Stage 9. This
stage adds an original PCnet PCI Ethernet driver, ARP/IPv4/UDP, DHCP, DNS and
a bounded outbound TCP transport. Terminal now provides `net`, `nslookup` and
`wget`. HTTP GET supports length-delimited, chunked and connection-close bodies;
only complete successful responses are saved through staged GathaFS replacement.
Cancellation and failures preserve the destination. No existing guest OS/stack,
hosted libc or third-party guest implementation was imported. The own bootloader,
filesystem, ring-3 applications and original desktop remain in use.

Network calls are nonblocking, PID-owned and pointer-checked. The compositor polls
bounded work; user helpers yield and process UI events. Closing/faulting an app
releases DNS handles, sockets and user memory. There are four DNS slots and four
TCP connections, fixed DMA rings and 16 KiB receive rings. No-NIC boots remain
usable. See network.md for detailed protocol and resource limits.

## Verification

The final clean build and full make test suite passed, followed by the live
internet test: **25 actual QEMU boots** including fresh processes and guest reset.
The regression matrix covers 64/256/512 MiB, keyboard sets 1/2, all eight apps,
four persistence boots, four deliberate kernel faults, five foundation boots,
four network boots, clean halt/reset and a minimum ten-minute desktop soak.

The NAT network boot verifies complete binary responses with Content-Length,
chunking, connection-close and an empty body, plus a 1,048,589-byte file. A host
parser compares every saved byte with the original payload. Truncation, conflicting
or duplicate framing, malformed chunks, HTTP errors, oversize responses, encoding
rejection, refused connections and Escape cancellation preserve the destination.
Two ring-3 diagnostic faults verify invalid pointers, resource exhaustion, stale
handles and release of every socket/query plus application memory. A no-NIC boot
verifies prompt offline errors and a working desktop.

The controlled Ethernet peer sends incorrect DHCP transaction IDs, renews a
20-second lease, stops renewing to force expiry and then permits reacquisition.
DNS tests cover CNAME, incorrect IDs, cyclic compression, NXDOMAIN, truncation and
timeouts. TCP tests drop the first SYN/request, inject out-of-order, duplicate
and overlapping data, a bad checksum and an invalid reset. The completed file
still matches exactly. A connection timeout also preserves it.

The separate live boot resolves example.com using external DNS and downloads
http://example.com/ into /Documents/Internet.html. The complete saved response
matches a separate host fetch. This verifies actual external HTTP connectivity,
not HTML rendering or HTTPS. Its exact file hash is in test-network-live/result.json.

The soak lasted 610.62 seconds over 27 cycles.
Each cycle returned to 64072 free frames and
3149824 mapped kernel-heap bytes, with zero dropped input events.
It exercises editing, saving/reopening, arithmetic and window operations. This
desktop soak uses no NIC; the separate network boots cover the network behavior.

A second clean build reproduced the identical ISO. The build has no compiler
warnings or unresolved kernel symbols. Previous ISOs passed checksum checks and
were preserved. The raw kernel is 282,740 bytes, below the 448 KiB cap.
Existing VirtualBox VMs and user working disks were not altered.

Evidence: dist/stage10-evidence, dist/stage10-build-verification.log and
dist/stage10-SHA256SUMS.txt. All normal-mode result hashes identify this release.

## Artifacts

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage10-network.iso
Size: 1,851,392 bytes. SHA-256: 786fb59335dff8b3409cc679d5f30430e9160e62244772c3a3ced27e6ad3f39e
Seed: dist/gathaos-stage10-disk.img (33,554,432 bytes, immutable v2 seed).
Working disk: dist/gathaos-network-disk.img (created only if absent).
Changed/generated file tree: stage-10-files.txt.

## Manual checklist

- [ ] Boot the Stage 10 ISO with a copy of its GathaFS seed on primary-master IDE.
- [ ] Enable PCnet-PCI II in NAT mode, with cable connected; run `net` in Terminal.
- [ ] Confirm a DHCP address, gateway and DNS server appear.
- [ ] Run `nslookup example.com` and confirm an IPv4 address.
- [ ] Run `wget http://example.com/ /Documents/Internet.html`.
- [ ] Use `cat /Documents/Internet.html` or open the file in Editor to inspect HTML.
- [ ] Cancel a slow download with Escape and confirm the prior destination remains.
- [ ] Open Calculator or move windows during a network wait; close Terminal to cancel.
- [ ] Shut down, restart with the same working disk and inspect the saved file.
- [ ] Disable the NIC and confirm the desktop boots and `net` reports offline.

QEMU (from the repository root on WSL/Ubuntu):

```sh
qemu-system-i386 -cdrom dist/gathaos-stage10-network.iso -drive file=dist/gathaos-network-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: legacy BIOS (EFI off), Other/Unknown 32-bit, 256 MiB RAM, VBoxVGA,
64 MiB video RAM, 3D off, PS/2 input, IDE disk plus optical ISO. Select PCnet-PCI II
(Am79C970A), NAT and cable connected. Use Normal Start for a visible console.
VMware requires equivalent BIOS/VGA/IDE/PCnet configuration. Stage 10 verification
is QEMU-only; no new VirtualBox/VMware boot claim is made. Existing VMs and user
working disks are not replaced by the release workflow.

## Known limits and next stage

The intended environment is a single virtual PCnet adapter behind NAT. No Wi-Fi,
IPv6, other NIC drivers, TCP server sockets, full TCP close/congestion machinery,
secure random identifiers, TLS, redirects, compressed HTTP, HTML rendering,
JavaScript or Google browsing. DNS is bounded UDP/A/CNAME without DNS-over-TCP.
Downloads are at most 4 MiB and require complete-response memory plus staged disk
space. Legacy v1 volumes impose a 192 KiB file limit. Long disk saves can briefly
pause the desktop. GathaFS still has no crash journal or power-loss atomicity.

The next roadmap step is HTTPS/TLS, which also requires dependable time,
cryptographic primitives, entropy and certificate validation. It is not started
by this stage. Stop for user review after delivering the tested release.
