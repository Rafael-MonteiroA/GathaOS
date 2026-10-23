# Stage 11 - HTTPS (expansion step 3)

Date: 2026-10-22. Status: complete.

## Changes

The user approved HTTPS after Stage 10. Terminal now downloads HTTPS with an
original TLS 1.2 client, X25519 ephemeral key exchange, AES-128-GCM, SHA-256 PRF
and RSA certificate/signature verification. Domain SAN, validity dates, trusted
chain signatures, CA/key/EKU constraints and Finished authentication are checked
before accepting data. There is no certificate bypass or downgrade to HTTP.
The immutable public trust set contains ISRG Root X1 and GTS Root R1 only.

Original crypto lives in libgatha/crypto and TLS/DER validation in libgatha/tls.
Kernel syscall 32 supplies bounded RDRAND-derived bytes and validated UTC RTC
time. Missing entropy or an invalid clock stops HTTPS before ClientHello.
The own BIOS bootloader, kernel, filesystem, desktop and applications remain.
No third-party guest implementation or hosted library was imported. Public
certificates are trust data; OpenSSL/Python cryptography are host test tools.

## Verification

A clean `make clean && make iso` and the complete `make test` passed, followed
by independent live HTTP and HTTPS tests: **29 actual QEMU boots**, including
fresh processes and reset. The matrix covers 64/256/512 MiB RAM, PS/2 sets 1/2,
all basic apps, persistence, four intentional kernel faults, five foundation
boots, prior network malformed-packet/loss tests, power halt/reset and a desktop
soak. Existing VMs and user disks were not changed.

SHA-256/384/512, HMAC/PRF, X25519, AES/GCM and RSA 2048/3072/4096 were compared
with independent host implementations and negative vectors. X.509 tests cover
valid chains, SAN/IP/wildcards, wrong names, invalid dates, CA/path/EKU/critical
constraints, signatures and unsupported policies. ASan/UBSan passed 30,000 DER
mutations and every truncated prefix of a generated certificate.

The real-guest TLS suite passes 20 protocol/certificate/integrity cases, including
32,781-byte payloads, fragmented handshakes, expired/future/wrong-name/untrusted
certificates, absent SAN, wrong EKU, unsupported critical extensions, must-staple,
corrupt Finished/application tags, replay, truncation, abrupt EOF, oversized
record headers, unsupported TLS/cipher, cancellation and repeated success.
The diagnostic explicitly chooses its ephemeral private test anchor; Terminal
rejects that anchor. The production seed contains no test roots or private keys.
Escape and closing Terminal cancel negotiation, preserve an existing destination,
and reclaim process resources. Separate boots prove missing RDRAND and an invalid
RTC fail before a ClientHello. Post-close frame/heap counts return to baseline.

The final ISO downloaded **https://www.google.com/robots.txt** into
/Documents/Google.txt, validating its RSA chain, hostname, time, handshake and
record authentication. The 6,624-byte saved file matches every byte from
an independent host HTTPS fetch. File SHA-256: 1aba570a1b32deb66941e0c29d4f37be71ee58aa1a624aa20754b3639538cf2c.
This proves HTTPS file access to Google; it does not provide graphical browsing.
The separate HTTP live regression also passed against example.com.

The desktop soak lasted 618.24 seconds over 28
cycles. Each cycle returned to 64061 free frames and
3149824 mapped kernel-heap bytes, with zero input-event drops.
This soak uses no NIC; the separate network/TLS boots validate connectivity.
These runs do not establish unlimited uptime or a security audit.

A second clean build reproduced the identical ISO. No compiler warnings or
unresolved kernel symbols remain. Previous ISO hashes were verified unchanged.
The kernel payload is 327,800 bytes, below the 448 KiB bootloader cap.
Evidence: dist/stage11-evidence, dist/stage11-build-verification.log and
dist/stage11-SHA256SUMS.txt. JSON results identify the tested ISO hashes.

## Artifacts

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage11-https.iso
Size: 1,851,392 bytes. SHA-256: 528b23fb1283e8a9a8db57aa7dc7f90ee6bf40b2e7bdf5d7c7b1fc249129e42a
Seed: dist/gathaos-stage11-disk.img (33,554,432 bytes, immutable GathaFS v2).
Working disk: dist/gathaos-https-disk.img (created only if absent).
Complete changed/generated tree: stage-11-files.txt.

## Manual checklist

- [ ] Boot the Stage 11 ISO with its working GathaFS disk, 256 MiB and PCnet NAT.
- [ ] Expose RDRAND and set the VM RTC to the correct UTC date/time.
- [ ] Run `net` and wait for DHCP to report connected.
- [ ] Run `wget https://www.google.com/robots.txt /Documents/Google.txt`.
- [ ] Run `cat /Documents/Google.txt`, or open the file in Editor.
- [ ] Confirm `wget http://example.com/ /Documents/Internet.html` still works.
- [ ] Cancel a slow request with Escape; check that the old destination remains.
- [ ] Close Terminal during a request and confirm other apps continue working.
- [ ] Shut down, restart with the same working disk and reopen the downloaded file.
- [ ] Confirm a missing entropy source or incorrect date produces a clear error.

QEMU, from the repository root on WSL/Ubuntu:

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage11-https.iso -drive file=dist/gathaos-https-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: legacy BIOS (EFI off), Other/Unknown 32-bit, 256 MiB, VBoxVGA/64 MiB,
3D off, PS/2 input, primary-master IDE GathaFS disk and the ISO in the optical
drive. Select PCnet-PCI II, NAT and cable connected; use Normal Start for a visible
console. HTTPS requires guest RDRAND and a correct UTC RTC. VMware needs equivalent
BIOS/VGA/IDE/PCnet/CPU/clock support. **Stage 11 was tested in QEMU only**; no
VirtualBox or VMware boot is claimed. Keep existing VMs and user working disks.

## Limits and next stage

The supported TLS/certificate profile is intentionally narrow: no TLS 1.3,
ECDSA/EdDSA/RSA-PSS, alternative curves/ciphers, client authentication, session
resumption, renegotiation or broad browser trust store. No CRL/OCSP revocation
checking, Certificate Transparency, automatic root updates or independent security
audit; a revoked but time-valid certificate may be accepted. RTC and entropy
depend on trusted hardware/hypervisor behavior. See https.md for exact bounds.

HTTP still requires status 200, uncompressed content, no redirects and at most
4 MiB. No HTML/CSS/JavaScript renderer exists. Client close uses TCP abort; a
connection-close response requires authenticated TLS close_notify. The prior
limited TCP/DNS stack and GathaFS save/power-loss limitations remain.

The next approved-roadmap step is a basic graphical browser. It has not started.
Stop here for user review and approval before that stage.
