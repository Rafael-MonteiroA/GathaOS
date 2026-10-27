# Stage 16 RSA-PSS HTTPS compatibility

Browser and Terminal accept TLS 1.2 peers using RSA-PSS for authenticated
ServerKeyExchange signatures and certificate chains. All guest implementation
is original freestanding C. OpenSSL and Python cryptography are host test tools;
neither is linked into GathaOS. The two compiled public trust anchors are unchanged.

## Supported signature profile

| Context | Supported |
|---|---|
| Public RSA keys | 2048, 3072 or 4096 bits, exponent 65537 |
| Hash | SHA-256, SHA-384 or SHA-512 |
| PSS mask | MGF1 with the same hash as the signed digest |
| Encoded message | emBits = modulus bits - 1, trailer 0xbc |
| Certificate signature | Explicit PSS parameters; exact declared salt size within the RSA encoding bound |
| RSAE TLS schemes | rsa_pss_rsae_sha256/384/512 (0x0804..0x0806) |
| PSS-key TLS schemes | rsa_pss_pss_sha256/384/512 (0x0809..0x080b) |
| TLS salt | Exactly the digest size: 32, 48 or 64 bytes |
| Existing signatures | RSA PKCS#1 v1.5 SHA-256/384/512 remain supported |

The modular public-key operation is shared with the existing RSA verifier.
PSS verification checks signature range/length, unused top bit, trailer,
MGF1, zero padding, delimiter, exact salt length and recomputed hash.
No guessed salt length or alternative digest is tried after failure.
The implementation performs public signature verification only; it does not
generate signatures or use an RSA private key.

## Certificate and key parameters

The signature AlgorithmIdentifier must contain RSASSA-PSS-params. Inner and
outer certificate identifiers still match exactly. Hash and MGF1 hash must be
explicit supported SHA-2 identifiers; omitted defaults imply unsupported SHA-1
and fail. Hash AlgorithmIdentifier accepts absent or NULL parameters.

Salt length defaults to 20 bytes when omitted, can be zero, and must fit the
selected modulus/hash encoding. The parser bounds its integer to 512 before
the verifier applies the tighter mathematical bound. TrailerField may be absent
or explicitly 1, as required by RFC 4055; other values are rejected.
Duplicate/out-of-order fields, trailing bytes, negative/oversized integers,
missing/unsupported MGF parameters and mismatched hashes fail.

SubjectPublicKeyInfo can use rsaEncryption or RSASSA-PSS. PSS keys with absent
parameters are unrestricted within the supported PSS profile. Present
parameters constrain the hash/MGF and impose a minimum salt length; those
constraints are enforced when verifying child certificates or TLS signatures.
A PSS-only key never verifies PKCS#1 signatures. TLS additionally binds RSAE
scheme IDs to rsaEncryption keys and PSS scheme IDs to RSASSA-PSS keys.
Unsupported or unadvertised scheme IDs fail rather than falling back.

SAN/hostname, dates, issuer linkage, signatures, CA/path length, key usage,
server EKU and critical-extension policy remain in force. The test CA passed
explicitly to tlscheck.gex cannot change Browser or Terminal's trust store.

## Verification

`make test` includes:

- `pss_test.py`: independent cryptography signatures for all supported key/hash
  sizes, variable salts, corrupted digest/signature, wrong MGF, PKCS#1 confusion
  and malformed recovered encodings. The same vectors run under ASan/UBSan.
- `pss_x509_test.py`: positive fixture chains independently verified by
  OpenSSL, PSS key restrictions, malformed AlgorithmIdentifiers, policy failures
  and sanitizer mutation/truncation runs on PSS certificates.
- `pss_tls_test.py`: a real QEMU boot with OpenSSL servers forcing all six PSS
  schemes, a constrained key, PSS-signed chains and an old PKCS#1 handshake.
  A record proxy checks selected schemes and injects signature corruption,
  algorithm/key-type confusion, changed hash, invalid length and an unoffered
  algorithm. Wrong-name/expired certificates still fail; repeated success and
  memory reclamation are checked.

The ordinary regression still tests record integrity, Finished, replay,
truncation, cancellation, missing entropy/clock, HTTP forms/compression, storage,
apps, power and stability. Separate live tests compare downloaded HTTPS bytes
with an independent host fetch. See stage-16-report.md for measured results.

## Limits and next step

This remains TLS 1.2, ECDHE_RSA with X25519 and AES-128-GCM. RSA-PSS support does
not enable TLS 1.3. ECDSA, EdDSA, SHA-1, mixed PSS/MGF hashes and other key sizes
are unsupported. The small trust store, lack of CRL/OCSP/CT/root auto-update and
absence of an independent security audit remain. Do not infer modern-browser
certificate policy or support for all HTTPS sites from these tests.

The next compatibility candidate is ECDSA certificate/signature support, with
careful curve negotiation and independent verification. Rendering modern pages
still requires later HTML/CSS/image and DOM/JavaScript work. Google Search,
cookies, login and media playback are not added by this update.

## Primary specifications

- [RFC 8017, RSA-PSS encoding and verification](https://www.rfc-editor.org/rfc/rfc8017.html#section-9.1.2).
- [RFC 4055, certificate parameters and key restrictions](https://www.rfc-editor.org/rfc/rfc4055.html#section-3).
- [RFC 8446, signature-scheme definitions and TLS 1.2 updates](https://www.rfc-editor.org/rfc/rfc8446.html#section-4.2.3).

These specify algorithms and wire formats; no implementation code was imported.
