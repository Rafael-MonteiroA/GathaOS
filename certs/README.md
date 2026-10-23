# Reviewed public trust anchors

These are public certificates, not third-party program code. No private key is
present. Downloaded from the CA operators over host-validated HTTPS on 2026-10-21:

| File | Official source | SHA-256 fingerprint |
|---|---|---|
| isrg-root-x1.der | https://letsencrypt.org/certs/isrgrootx1.der | 96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6 |
| gts-root-r1.der | https://pki.goog/repo/certs/gtsr1.der | d947432abde7b7fa90fc2e6b59101b1280e0e1c7e4e40fa3c6887fff57a7f4cf |

`tools/embed_roots.py` verifies SHA256SUMS and embeds these exact bytes in the
user programs. Builds are offline. Root changes require a reviewed source change
and a new release. No download server, filesystem file, test fixture or peer can
add a root to Terminal's trust store. This is a deliberately small trust set,
not a complete browser root program. Root revocations/removals require a release.

Operator repositories: [Let's Encrypt](https://letsencrypt.org/certificates/)
and [Google Trust Services](https://pki.goog/repository/).
