# Stage 11 HTTPS

Stage 12 reuses this TLS profile in the graphical Browser; see browser.md.
The description below records the Stage 11 downloader and cryptographic limits.

Terminal accepts `wget https://host/path destination` using original freestanding
C cryptography and TLS, over the Stage 10 TCP driver. HTTP remains supported.
There is no certificate bypass option, implicit HTTP fallback, or automatic
redirect. No browser, HTML renderer or JavaScript engine is included.

```text
wget https://www.google.com/robots.txt /Documents/Google.txt
cat /Documents/Google.txt
```

## Supported profile

- TLS 1.2, TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xc02f), X25519, SNI and optional
  HTTP/1.1 ALPN. Extended Master Secret and the initial secure-renegotiation
  indication are required. Renegotiation, resumption, tickets, client certificates,
  TLS 1.0/1.1/1.3, other ciphers and other curves are rejected.
- SHA-256 transcript, HMAC-SHA256 PRF, 128-bit GCM tags, fresh ephemeral keys and
  per-direction 64-bit record sequence counters. Authentication covers record
  type, version, length, sequence and ciphertext. No plaintext is exposed before
  its tag verifies. Finished is verified against the handshake transcript before
  HTTP is sent. Unauthenticated close, replay, malformed lengths and unexpected
  handshake/record types fail the connection.
- RSA PKCS#1 v1.5 signatures with SHA-256/384/512; 2048/3072/4096-bit public keys
  with exponent 65537. No RSA key transport, SHA-1, ECDSA, EdDSA or RSA-PSS.
  X25519 uses fixed-loop radix-2^15 field operations and rejects an all-zero shared
  secret. AES computes its substitution algebraically instead of secret-indexed
  lookup tables. RSA operations involve only public values. Fixed-loop source
  code and passing vectors are not an independent side-channel/security audit.
- Own bounded DER/X.509 v3 parsing, matching inner/outer signature algorithms,
  exact issuer/subject DER linkage, chain signatures, validity dates, SAN DNS/IP
  matching, basic constraints, CA path length, key usage and server EKU. Common
  Name fallback is disabled. Wildcards must occupy the complete first label and
  match exactly one label; suffixes require at least two labels. No Unicode/IDNA
  conversion is performed (ASCII A-label hostnames are accepted).
- Terminal embeds only ISRG Root X1 and GTS Root R1 from `certs/`. Unknown roots,
  expired/not-yet-valid certificates, unsupported critical extensions, name/policy
  constraints and OCSP must-staple fail closed. Exact DER name matching and the
  narrow algorithms deliberately reject some otherwise valid chains.

## Entropy and time

Syscall 32 (`SYS_SECURITY`) accepts EBX operation, ECX writable user buffer and
EDX length. Kernel callers are denied; all user pages are checked.

| Operation | Buffer | Result |
|---|---|---|
| 0, G_RANDOM | 1..256 bytes | 0, or -61 if unavailable/health failure |
| 1, G_UTC | exactly four bytes | uint32 Unix UTC seconds, or -62 for invalid clock |

CPUID must advertise RDRAND. Each output block hashes 64 fresh hardware bytes
with SHA-256 to produce up to 32 bytes. Each RDRAND word has at most ten attempts;
zero, all-one or repeated consecutive words latch a failure for the boot. There
is no timestamp/MAC/keyboard fallback and no deterministic guest PRNG. TLS
requests fresh material for each ClientHello and ephemeral secret, then wipes
temporary entropy, session keys and the entire heap context on normal cleanup.
The hardware/hypervisor entropy implementation remains part of the trust base;
these checks do not prove a defective or malicious source is random. VM snapshots
and compromised hosts are outside the threat model.

RTC reads use stable snapshots, the validity bit, BCD checks, calendar bounds and
the emulator's CMOS century register 0x32. The clock must represent UTC, be at
least 2020, and fall within each certificate's validity. There is no authenticated
network time, rollback detection or persistent secure clock. Correct hypervisor
time is an explicit prerequisite. QEMU invocation uses `-cpu max -rtc base=utc`.
Without advertised RDRAND, HTTP and the desktop still work, but HTTPS stops.

## Resource and security limits

Each TLS context uses about 100 KiB of private user heap, with bounded 16 KiB
plaintext records, 64 KiB handshake messages, six peer certificates of at most
16 KiB each and eight caller-supplied trust anchors at most. Production has two
anchors. Record/handshake fragmentation and coalescing are supported. The whole
post-DNS transfer has the existing 120-second deadline; waiting polls UI input
and Escape cancels. Other tasks are preemptible during crypto. The software AES
implementation favors simple fixed operations over throughput. TLS failure
never replaces an existing destination; the full authenticated HTTP result must
pass framing and size checks before the existing staged save is called.

The HTTP parser still supports only status 200 and uncompressed bodies up to
4 MiB. A connection-close body requires authenticated TLS close_notify. A known
Content-Length/chunked body may finish once its complete authenticated bytes
arrive. Client cleanup currently uses the TCP stack's RST abort rather than a
bidirectional TLS close_notify/TCP FIN exchange. No early data is used.

There is **no CRL/OCSP revocation checking, Certificate Transparency enforcement,
root auto-update or security audit**. A revoked but time-valid certificate can
therefore be accepted unless it requires stapling. Do not describe this as a
production browser's complete certificate policy. The prior limited TCP/DNS stack
also remains unchanged. More TLS algorithms, certificate policy and browser
support remain separate future work.

## Verification design

`make test` compares original primitives with the host's independent cryptography
implementation and runs certificate positives/negatives plus ASan/UBSan DER
mutation/truncation tests. Host OpenSSL/Python are test tools only.

QEMU boots use the actual ISO and private copies of GathaFS. Terminal first
rejects a private test CA, proving that a disk fixture cannot alter its trust
store. A separate `/System/Tests/tlscheck.gex` diagnostic explicitly passes that
ephemeral anchor to its own TLS client to test successful local connections and
faults deterministically. It checks entropy/clock syscall bounds and per-process
reclamation. Private test keys stay in the host build directory; the production
seed contains neither test anchors nor keys. This diagnostic cannot reconfigure
Terminal's roots.

Cases include multi-record payloads, fragmented handshakes, wrong/expired/future/
untrusted certificates, SAN/EKU/critical-extension/must-staple failures, corrupted
Finished and application tags, replay, truncation, abrupt EOF, oversized record
headers, unsupported cipher/version, cancellation, repeated connections and
destination preservation. Separate boots verify missing entropy and an invalid
UTC clock fail before ClientHello. `tools/tls_test.py ISO DISK live` downloads
Google's HTTPS robots.txt and compares every saved byte with an independent host
fetch. It requires internet and is separate from deterministic `make test`.

## Primary specifications

Original implementations were written from protocol/algorithm descriptions;
no third-party TLS, crypto, libc or network source is linked into the guest.

- [TLS 1.2, RFC 5246](https://www.rfc-editor.org/rfc/rfc5246.html)
- [ECC TLS, RFC 8422](https://www.rfc-editor.org/rfc/rfc8422.html)
- [GCM suites, RFC 5288](https://www.rfc-editor.org/rfc/rfc5288.html) and [RFC 5289](https://www.rfc-editor.org/rfc/rfc5289.html)
- [Extended Master Secret, RFC 7627](https://www.rfc-editor.org/rfc/rfc7627.html)
- [X25519, RFC 7748](https://www.rfc-editor.org/rfc/rfc7748.html)
- [RSA, RFC 8017](https://www.rfc-editor.org/rfc/rfc8017.html)
- [X.509, RFC 5280](https://www.rfc-editor.org/rfc/rfc5280.html) and [service identity, RFC 9525](https://www.rfc-editor.org/rfc/rfc9525.html)
- [AES, FIPS 197](https://csrc.nist.gov/pubs/fips/197/final), [GCM, SP 800-38D](https://csrc.nist.gov/pubs/sp/800/38/d/final), [SHA-2, FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final)
- [Intel architecture manuals, RDRAND](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
