# Stage 16 - RSA-PSS HTTPS

Date: 2026-10-27. Status: complete. Verification run: 2026-10-24.

## Changes

Browser and Terminal can authenticate TLS 1.2 servers using RSA-PSS. The
original verifier supports 2048/3072/4096-bit RSA, SHA-256/384/512, MGF1,
strict encoded-message checks and exact declared salt lengths. The modular
public-key operation is shared with the previous PKCS#1 verifier.

X.509 accepts PSS-signed chains and RSAE/PSS-only SubjectPublicKeyInfo.
Explicit key restrictions constrain hash/MGF and minimum salt length. TLS
advertises and validates rsa_pss_rsae_sha256/384/512 and rsa_pss_pss_sha256/384/512,
binds each scheme to the correct key type and requires digest-length salt.
PKCS#1 SHA-256/384/512 remains supported. SHA-1 and mismatched MGF hashes fail.

Existing domain/date/chain/usage/critical-extension checks remain active.
The compiled ISRG Root X1 and GTS Root R1 anchors are unchanged; there is no
certificate bypass or trust-store update from the filesystem. Private test
anchors are explicitly passed only to the diagnostic program.

About shows Stage 16 / HTTPS RSA-PSS; HTTP User-Agent is GathaOS/16.
TLS remains version 1.2 with X25519 and AES-128-GCM. No third-party guest
crypto/TLS code, Linux or hosted libc was introduced. See rsa-pss.md.

## Verification

The final clean `make clean && make iso`, complete `make test`, and external
HTTP/HTTPS/Browser checks passed with no compiler warnings. Normal and
fault-test kernel ELFs have no unresolved symbols. Evidence contains
**46 actual QEMU boot attempts**: 38 full-kernel entries,
two standalone loader boundary payloads, and six expected invalid-image/RAM
rejections. No successful boot is inferred from a compilation alone.

RSA-PSS-specific checks:

- **675 independent host vectors**, including **135 positives**, cover every
  supported RSA size/hash, zero/default/digest/maximal salt, wrong salt,
  digest/signature corruption, signature range/truncation, MGF mismatch and
  malformed encoded messages. The vectors plus **2,000 mutations** passed
  ASan/UBSan.
- Positive certificate fixtures were independently accepted by OpenSSL.
  The guest X.509 code passed restricted/unrestricted PSS-key checks,
  mixed PKCS1/PSS chains, all supported signature hashes, salt bounds,
  malformed/duplicate/reordered parameters, hostname/date failures and
  mismatch between certificate signature identifiers.
- PSS certificate parser sanitizers ran **30,000 mutations plus all
  truncations for each of two fixtures**, alongside the original RSA parser test.
- **18 controlled TLS cases inside QEMU** force all six PSS schemes, a
  restricted key, an old PKCS#1 handshake and success after failures.
  A host record proxy independently records negotiated signature IDs and
  injects corruption, key-type confusion, changed hash, invalid length and
  an unadvertised algorithm. Wrong-name/expired certificates remain rejected.
  All cases match their expected outcomes; windows, free frames and kernel-heap
  usage return to the pre-test baseline.

The existing regression passed: 64/256/512 MiB and keyboard variants, memory
and user isolation, apps, persistence, legacy disks, extended loader bounds,
four deliberate kernel faults, networking, TLS record/Finished integrity,
cancellation, entropy/clock rejection, Browser/forms/compression and power.
The inflater retained its 20,452-vector sanitizer pass.

Desktop soak ran **605.59 seconds**, **24 cycles**,
returning to 64026 free frames and
3149824 kernel-heap bytes each cycle, with no dropped events.

The live Terminal download from Google's HTTPS robots.txt negotiated
**RSA-PSS RSAE**, then authenticated TLS and downloaded successfully.
Saved bytes matched an independent host fetch: **6,624 bytes**,
SHA-256 1aba570a1b32deb66941e0c29d4f37be71ee58aa1a624aa20754b3639538cf2c. Browser also opened Example Domain and Google's
HTTPS robots.txt. These text downloads do not establish Google Search support.

A second clean build reproduced the ISO byte for byte. All prior ISO hashes
and existing disk images were preserved. The production payload is
471,164 bytes. Seed Browser/Terminal GEX bytes match the clean build;
private test roots/keys are absent. Existing VirtualBox VMs were not changed.

## Artifacts and launch

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage16-rsa-pss.iso

Size: **860,160 bytes**.
SHA-256: f47d14945a45e1c7faf4c70dd1cbea31f846ca5c78bf28b07319a56f25da2c6f

Seed: dist/gathaos-stage16-disk.img (33,554,432 bytes).
Working disk: dist/gathaos-rsa-pss-disk.img, created only if missing.
Tree: docs/stage-16-files.txt.
Evidence: dist/stage16-evidence, dist/stage16-build-verification.log and
dist/stage16-SHA256SUMS.txt.

From the repository root in WSL:

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage16-rsa-pss.iso -drive file=dist/gathaos-rsa-pss-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: Other/Unknown 32-bit, BIOS (EFI off), 256 MiB RAM, VBoxVGA/64 MiB,
3D off, PS/2, new ISO as optical media and GathaFS primary-master IDE disk.
Use PCnet-PCI II (Am79C970A), NAT and cable connected. HTTPS requires exposed
RDRAND and a correct UTC RTC. Convert a copy of the raw seed to VDI if needed;
never overwrite a personal working disk with the seed.
Start > Normal Start opens the visible VM console. Right Ctrl releases input;
Right Ctrl+F leaves fullscreen. VMware requires equivalent BIOS/VGA/IDE/PCnet
support. Stage 16 was tested in QEMU; Stage 15 had a VirtualBox boot/display
check. VMware has not been tested.

## Manual checklist

- [ ] Stop the old VM before changing its optical ISO.
- [ ] Boot Stage 16 and confirm About shows Stage 16 / HTTPS RSA-PSS.
- [ ] Open Browser and visit https://www.google.com/robots.txt.
- [ ] In Terminal, use `wget https://www.google.com/robots.txt /Documents/Google16.txt`.
- [ ] Read the file with `cat /Documents/Google16.txt`; serial should show the verified signature scheme.
- [ ] Try http://example.com/ and the offline Forms demo.
- [ ] Open your other apps, save a document, reboot and verify persistence.
- [ ] Save edits and type `shutdown` before closing the VM.

## Limits and next step

There is no TLS 1.3, ECDSA/EdDSA, mixed MGF hash, revocation/CT checking, root
auto-update or independent security audit. The two-root trust set and bounded
certificate parser remain deliberately limited. Original network, storage
and UI limitations still apply. RSA-PSS verification uses public values only.

The next suggested compatibility stage is ECDSA signatures/certificates with
appropriate curve negotiation and independent host/guest tests. Modern page
rendering needs separate HTML/CSS/images and then DOM/JavaScript work.
Google Search, cookies, login and multimedia support are not added here.
This stage stops for user review; no next stage has started.
