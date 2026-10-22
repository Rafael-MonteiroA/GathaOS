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
