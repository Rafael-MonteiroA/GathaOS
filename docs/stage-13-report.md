# Stage 13 - forms

Date: 2026-10-24. Status: complete.

## Changes

The approved update adds original GET forms to the graphical Browser: editable
text/search fields, hidden values and submit buttons. Tab/Shift+Tab, Ctrl+A,
selection and Enter work with the existing toolkit. Required/maxlength checks,
readonly/disabled controls, ownership and selected-button values are handled
before URL-encoded submission. Repeated names keep document order, action queries
are replaced, and an identical submitted URL still causes a new request.

Unsupported forms, oversized data and HTTPS-to-HTTP actions fail before sending.
Existing HTTP/TLS, cancellation, navigation and prior-page preservation are used.
The Forms demo button opens about:forms: a clearly labelled offline example
showing the encoded query without contacting any server or search engine.

The window manager now delivers clicks to the covering window instead of
activating an obscured Files desktop icon. This fixes Back on a maximized Browser.
No guest Linux, GRUB, hosted libc or imported browser/crypto code was added.
See forms.md for the exact subset and browser.md for inherited limits.

## Verification

Clean `make clean && make iso` and the complete `make test` passed, followed by
live HTTP/TLS/Browser regressions. There are **34 actual QEMU boots** in the
release evidence, including memory/keyboard variants, deliberate fault cases,
filesystem persistence, network failures, certificate rejection and reboot.
Compilation has no warnings; kernel ELF files have no unresolved symbols.

The new forms integration boot sends actual PS/2 events. An independent HTTP
server records the exact request bytes and verifies query replacement, spaces,
reserved characters, hidden duplicate names and the selected submit button.
It checks required fields without requests, typing/click/Enter, same-URL
resubmission, Back/Reload, readonly/disabled/maxlength, Tab/Shift+Tab/Ctrl+A,
separate form ownership, and disabled default buttons. Unsupported methods,
controls, characters, overrides, schemes and oversized data send no request.
Network/format failures and cancellation retain the edited field. Result text
is verified against actual framebuffer pixels. Maximized navigation does not
launch Files. The local demo sends nothing. Server requests are saved as
dist/stage13-evidence/test-network-forms/http-requests.json.

The same guest parser/serializer passed host ASan/UBSan checks, including
**6,000 structured form mutations** and the existing 12,000 URL/document
mutations. Crypto vectors and 30,000 certificate-parser mutations also passed.

The offline desktop soak ran **601.49 seconds**, with
**24 cycles**. Every cycle edits and submits the offline form, along
with the original app/window tests. Each returned to 64037
free frames and 3149824 kernel-heap bytes, with zero dropped
events. The forms integration boot also reclaimed all process/window memory
to its baseline: 64037 free frames and 3149824 kernel-heap bytes.

Live regressions opened Example Domain and Google's HTTPS robots.txt in Browser.
The Terminal download matched an independent host fetch: 6,624 bytes,
SHA-256 1aba570a1b32deb66941e0c29d4f37be71ee58aa1a624aa20754b3639538cf2c. These verify continued external connectivity, not Google
Search forms or an interactive search session. Form submission tests use the
controlled host server; the built-in demo is explicitly offline.

A second clean build produced the identical ISO. Previous release ISO hashes
were preserved. The kernel payload is 426,108 bytes of the 458,752-byte
BIOS loader limit. The seed contains the matching Browser GEX and no private
test roots/keys. Existing VirtualBox VMs and working disks were not modified.

## Artifacts and launch

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage13-forms.iso

Size: 1,851,392 bytes.
SHA-256: ef5e2c5156684c2c3e688e8d6b73ca67c42f2fe2943f6d441de001b3e0ad3080

Seed: dist/gathaos-stage13-disk.img (33,554,432 bytes, GathaFS v2).
Working disk: dist/gathaos-forms-disk.img, created only if missing.
Changed/generated file tree: docs/stage-13-files.txt.
Evidence: dist/stage13-evidence, dist/stage13-build-verification.log and
dist/stage13-SHA256SUMS.txt. Result JSON files identify the tested ISO.

From the repository root in WSL:

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage13-forms.iso -drive file=dist/gathaos-forms-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: Other/Unknown 32-bit, BIOS/EFI off, 256 MiB RAM, VBoxVGA/64 MiB,
3D off, PS/2, new ISO in the optical drive, a copy of the working GathaFS disk
on primary-master IDE, PCnet-PCI II (Am79C970A) NAT and cable connected.
Convert a copy of the raw seed to VDI if necessary; never replace a user disk.
HTTPS needs exposed RDRAND and a correct UTC RTC. Use Normal Start for a visible
window. Right Ctrl releases input; Right Ctrl+F exits fullscreen.
VMware needs equivalent legacy BIOS/VGA/IDE/PCnet/CPU/clock support.
Stage 13 was boot-tested in QEMU only, not VirtualBox or VMware.

## Manual checklist

- [ ] Boot the new ISO with its working disk; open Start > Browser.
- [ ] Click Forms demo, type `gatha os` and press Enter or Preview.
- [ ] Confirm Local form preview shows `q=gatha+os` and says no network request.
- [ ] Use Back, edit again, and try Tab/Shift+Tab and Ctrl+A.
- [ ] Submit an empty demo field and confirm focus/required feedback.
- [ ] Maximize Browser, repeat the demo, then click Back; Files must not open.
- [ ] Open http://example.com/ and https://www.google.com/robots.txt.
- [ ] Check existing Editor/Files/Paint work and save documents before shutdown.

## Limits and next step

Forms are GET only, printable ASCII names/values and the supported input subset.
The limits are 16 forms, 64 controls, 127-byte names and 255-byte values, with the
existing 1,024-byte URL and 767-byte request-path/query limits after encoding.
Unsupported data is rejected rather than transliterated or partially sent.
Back/Forward refetch forms; values are not cached. The HTML parser remains a
bounded subset, not a complete standards implementation.

There is no POST, passwords, uploads, select/textarea, CSS, JavaScript, images,
cookies, login or Google Search interface. TLS/certificate support is unchanged.
Future work should broaden HTTP/HTTPS interoperability and page rendering,
then add JavaScript and web APIs in separately scoped, tested updates.
This stage stops for user review; no next stage has been started.
