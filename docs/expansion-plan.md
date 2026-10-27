# Post-release expansion plan

The user asked what remains before internet access and Google. The proposed
sequence was: (1) larger applications/files and better failure handling,
(2) virtual Ethernet/IP/DHCP/DNS, (3) HTTPS, (4) a basic browser, (5) broader
modern web compatibility. On 2026-10-16 the user asked to start step 1, and on
2026-10-17 asked to continue it.

That authorization covered step 1. Its repository release number is **Stage 9:
foundation**, preserving the original completed Stages 0-8 and their artifacts.
AGENTS.md's clean-build, real-boot, evidence, report and commit workflow still
applies. Network code and the next expansion step are not part of this stage.

## Foundation scope

- GEX v2 images up to 16 MiB, compatible GEX v1 loading and a separate user heap.
- A bounded 128 MiB user heap, allocation wrappers and complete page reclamation.
- GathaFS v2 files up to 4 MiB, v1 disk compatibility, strict indirect validation.
- Save replacements staged before releasing old data; no truncation on ENOSPC.
- Read-only failure mode after disk I/O/flush errors; no power-loss atomicity claim.
- A 256 KiB editor buffer and larger terminal copies using dynamic allocation.
- Tests for capacity, corruption, allocation failure, process faults, persistence
  and the existing full desktop regression/soak suite.

On 2026-10-19 the user asked to continue after the completed foundation release.
This authorizes step 2, **Stage 10: network**: an original PCnet PCI driver,
ARP/IPv4/UDP, DHCP, DNS and a bounded client TCP implementation, with Terminal
network status, name lookup and HTTP downloads. This explicit expansion replaces
the original no-network scope for this stage. HTTPS, a browser and importing
third-party guest code are not authorized by this step. The stage workflow stays
in effect, including real boot verification and immutable earlier releases.

Future stages need separate approval. A modern browser is a substantial project;
no browser-engine import or relaxation of the original guest-code rules is
authorized by this foundation step.

On 2026-10-21 the user approved **Stage 11: HTTPS/TLS** after the completed
network release. This covers original cryptography, a bounded TLS 1.2 client,
UTC/entropy prerequisites, certificate verification, HTTPS downloads and real
positive/negative boot tests. It does not authorize third-party guest code or
starting the browser stage. The mandatory release workflow still applies.

On 2026-10-22 the user approved **Stage 12: basic graphical browser**, continued
on 2026-10-23. This covers an original bounded HTML/text reader, clickable links,
URL resolution, HTTP redirects, history, scrolling and cancellation, using the
existing HTTP/TLS implementation. No imported browser code is allowed. Modern
JavaScript/CSS web applications and broader TLS compatibility remain a later
stage. The clean build, real boot, immutable artifacts and release workflow apply.

On 2026-10-23 the user approved the next recommended update, **Stage 13: forms**.
This adds bounded GET forms (text/search, hidden fields and submit buttons),
keyboard focus/editing, required/maxlength checks, URL-encoded submission and
real server/guest tests. Unsupported form features must not silently submit
partial data. A clearly labelled offline demo makes the new controls discoverable.
POST, passwords, uploads, cookies, CSS, JavaScript and full Google Search remain
future work. The original-code and per-stage release workflow still apply.

On 2026-10-24 the user asked to continue after Stage 13. The next bounded HTTP
compatibility update is **Stage 14: compression**: original gzip and zlib-wrapped
deflate decoding, response negotiation, checksums, resource/cancellation limits,
independent host vectors and real Browser/Terminal boot tests. This implements
one part of the planned HTTP interoperability work. It does not introduce TLS
algorithms, CSS, images, JavaScript or full Google Search. No imported decoder
code is allowed. The clean-build, boot, immutable-artifact and stage workflow apply.

On 2026-10-24 the user approved **Stage 15: larger kernel loading** after the
remaining 8,068 bytes under the 448 KiB loader limit were explained. This stage
replaces floppy emulation with an original no-emulation CD/EDD loader, checks
kernel integrity and RAM bounds, expands initial paging, and verifies larger
kernels in real QEMU boots. It preserves the kernel handoff, GathaFS, GEX,
applications and earlier artifacts. No later TLS/rendering/JavaScript stage is
included. The complete release workflow remains in effect.

On 2026-10-24 the user confirmed Stage 15 worked and authorized the next update,
**Stage 16: RSA-PSS HTTPS compatibility**. The bounded profile adds original
RSA-PSS signature verification, SHA-256/384/512 parameter validation, RSAE and
PSS-only public keys, and six TLS 1.2 signature schemes. Independent host tests,
forced OpenSSL interoperability, malformed input, and real guest regression are
required. The existing trust anchors, TLS version/cipher/key-exchange profile
and browser renderer remain in scope as regressions. This does not start
ECDSA, TLS 1.3, CSS, images or JavaScript. Original-code and stage workflow rules apply.
