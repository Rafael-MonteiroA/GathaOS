# Stage 12 - browser (expansion step 4)

Date: 2026-10-23. Status: complete.

## Changes

The approved next update is a basic graphical browser, built from original
freestanding C and the existing Gatha toolkit. Start > Browser opens a separate
GEX process with a blue/silver toolbar, address field, page viewport and status
bar. It reads HTTP/HTTPS HTML or text, follows links, provides Back/Forward,
Reload, Home, scrolling, fragment jumps and resize reflow.

The original URL resolver handles relative paths, queries and fragments with
bounded HTTP(S) validation. The HTTP helper exposes status, MIME and Location;
Browser follows at most five redirects and blocks automatic HTTPS-to-HTTP
downgrade. Terminal retains its 200-only, 4 MiB file-download API.

The renderer supports headings, paragraphs, lists, preformatted text, link
underlines, image alt text, common entities and ASCII approximations of UTF-8
Latin text. It skips script/style/iframe bodies and never executes page code.
The last page survives request/format failures. Stop, Escape and closing work
during network waits. Existing application indices remain unchanged; Browser
is index 9 and also ships as /System/Apps/browser.gex.

No guest Linux, GRUB, hosted libc or third-party browser/crypto code was added.
The original BIOS boot, kernel, GathaFS, GEX and GUI remain in use.

## Verification

Clean `make clean && make iso` and the complete `make test` passed without
compiler warnings, followed by external HTTP, TLS and Browser tests. Evidence
records **33 actual QEMU boots**, including persistence/reset boots:

- 64/256/512 MiB and PS/2 keyboard sets 1/2; every regular smoke test now also
  launches Browser. Basic app editing, painting, filesystem and persistence
  regressions pass.
- Four expected kernel fault screens and five foundation boots verify user
  isolation, capacity/failure paths and legacy storage/executable compatibility.
- Existing PCnet/NAT, malformed/lossy Ethernet peer, offline, TLS crypto,
  certificate/integrity, entropy and clock cases continue passing.
- Browser boots exercise redirects, relative links, Back/Forward/Reload,
  fragments, keyboard scrolling, maximize/restore, text/plain, HTML, HTTP 404 and
  chunked bodies. Rendered text is checked against the actual guest font pixels.
- Bad MIME/scheme/redirect, oversize and truncated responses preserve the old
  page. Escape/Stop cancel; a Calculator process computes 2+3 while Browser is
  waiting. Closing during loading reclaims the window and process resources.
- Separate boots verify offline errors and production certificate rejection
  before sending an HTTP request to the private test server.

The URL/document sources passed RFC URI examples, targeted parser/MIME/UTF-8
cases, bounds checks and **12,000 ASan/UBSan mutation inputs**. Host sanitizer
executables use a fixed base and isolated native WSL build directories; guest
memory layouts are unchanged. Existing crypto vectors and 30,000 certificate
parser mutations passed again.

The final ISO opened **http://example.com/** and displayed its Example Domain
title. It also opened **https://www.google.com/robots.txt**, validated TLS and
rendered the actual response; the first line was verified from screen pixels.
The separate Terminal HTTPS regression saved 6,624 bytes and matched
an independent host fetch byte-for-byte (SHA-256 1aba570a1b32deb66941e0c29d4f37be71ee58aa1a624aa20754b3639538cf2c).
This demonstrates basic graphical web reading and Google HTTPS text access,
not Google's interactive search interface.

The desktop soak ran **617.73 seconds**, 27 cycles.
Every cycle now includes opening/closing Browser alongside the original editor,
calculator and window tests. It returned to 64040 free
frames and 3149824 kernel-heap bytes, with zero dropped input
events. This is an offline desktop soak; networking has separate boot tests.
The browser integration boot also returned to its baseline: 64040 free
frames and 3149824 kernel-heap bytes after all windows closed.

A second clean build produced the identical ISO; earlier ISO hashes remained
unchanged. Kernel ELF checks show no unresolved symbols. Raw kernel payload:
413,820 bytes of the 458,752-byte BIOS loader limit.
The production seed has the matching Browser executable and no private test
roots/keys. Existing VirtualBox VMs and user working disks were not modified.

## Artifacts and launch

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage12-browser.iso

Size: 1,851,392 bytes.
SHA-256: 09dad106d878f4751135be800ca78a64cb29c85fbd5bbc6f1947dee9310261e2

Seed: dist/gathaos-stage12-disk.img (33,554,432 bytes, GathaFS v2).
Writable working disk: dist/gathaos-browser-disk.img, created only if missing.
Complete changed/generated tree: stage-12-files.txt.
Evidence: dist/stage12-evidence, dist/stage12-build-verification.log and
dist/stage12-SHA256SUMS.txt. JSON results identify the exact tested ISO.

From the repository root on WSL/Ubuntu:

```sh
qemu-system-i386 -cpu max -rtc base=utc -cdrom dist/gathaos-stage12-browser.iso -drive file=dist/gathaos-browser-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -nic user,model=pcnet -serial stdio
```

VirtualBox: Other/Unknown 32-bit, BIOS/EFI off, 256 MiB RAM, VBoxVGA/64 MiB,
3D off, PS/2 input, ISO in the optical drive and a working GathaFS disk on
primary-master IDE. Convert a copy of the seed to VDI if needed. Enable
PCnet-PCI II (Am79C970A), NAT, cable connected, RDRAND and correct UTC RTC.
Use Normal Start for a visible window. Right Ctrl releases input; Right Ctrl+F
exits fullscreen. VMware needs equivalent BIOS/VGA/IDE/PCnet/CPU/clock support.
**Stage 12 was boot-tested in QEMU only**, not VirtualBox or VMware.

## Manual checklist

- [ ] Boot the new ISO with the Stage 12 working disk and PCnet NAT.
- [ ] Open Start > Browser and read the built-in Home page.
- [ ] Open http://example.com/ and click a link.
- [ ] Use Back, Forward, Reload, Ctrl+L and Enter/Go.
- [ ] Open https://www.google.com/robots.txt and scroll its text.
- [ ] Maximize/restore the browser and check wrapping.
- [ ] Cancel a slow request with Escape/Stop; confirm the prior page remains.
- [ ] Open Calculator while Browser is loading; close Browser and reopen it.
- [ ] Verify existing Editor, Paint and Files documents still work.
- [ ] Save work, then use Terminal shutdown and wait for the safe-to-close screen.

## Limits and next stage

The browser accepts only text/html and text/plain, UTF-8/US-ASCII. Each response
is capped at 512 KiB, with 65,535 display characters, 8,192 rows, 128 links/IDs,
1,024-byte URL storage and 16 session history entries. Extra display text is
omitted with a notice. It uses an ASCII bitmap font and an intentionally small
best-effort HTML subset, not the complete HTML parsing model.

No CSS, JavaScript, forms, image decoding, downloads, cookies, login, tabs,
bookmarks, persistent history, local-file browsing or Google search UI.
Existing narrow TLS/certificate and network profiles still apply: no TLS 1.3,
ECDSA-only sites, broad trust store, revocation checks or security audit.
See browser.md and https.md for exact limits. These tests do not establish
arbitrary-hardware compatibility or unlimited stability.

The next roadmap step is broader modern-web compatibility, which requires
separate approval and substantial additional work. Stage 12 stops here for
user review; no modern-web stage has been started.
