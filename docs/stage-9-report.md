# Stage 9 - Foundation expansion (new plan, step 1)

Date: 2026-10-17. Status: complete.

## Scope and changes

This is the first step of the post-release plan authorized by the user, numbered
9 in the repository to preserve the original Stages 0-8. See expansion-plan.md.
No networking or browser implementation is included.

GEX v2 supports 16 MiB images, dynamic per-process page tables, a separate
128 MiB bounded heap and malloc/calloc/realloc/free in libgatha. GEX v1 keeps its
old layout. Allocation failure rolls back new pages/tables; exit/fault reclaims
the full address space. Actual usable capacity depends on available RAM.

GathaFS v2 supports 4 MiB files through direct and indirect references. Mount
validates pointer ownership and bitmap consistency. V1 disks remain writable
at their old 192 KiB cap and are never converted. Saves stage data before
replacing the destination, preserving the old file on capacity errors. Disk
I/O/flush failures disable subsequent writes. This is not a crash journal.

Editor uses a 256 KiB dynamic buffer and a separate buffer for opening files.
Terminal cp uses dynamic memory up to the filesystem limit. Save errors now
describe memory, size, disk-full, busy and read-only conditions in plain text.

## Verification

The final clean build and full make test suite passed: 21 actual QEMU boots.
The regression matrix covers 64/256/512 MiB RAM, native keyboard sets 1/2,
all eight applications, four fresh-process persistence boots, four deliberate
kernel faults, a flushed halt, actual reset/reboot, and the desktop soak.

Five additional foundation boots cover 64/256/512 MiB, a fresh-process restart,
and a v1 disk with a GEX v1 layout fixture. Each runs the 5 MiB-BSS diagnostic
twice. Tests verify allocator content/alignment/overflow, fragmentation,
128 MiB heap capacity where RAM permits, physical-OOM rollback on 64 MiB,
cross-PDE buffers, direct/indirect file boundaries, limits, busy saves, and
full memory/descriptor cleanup after intentional user page faults. The desktop
then opens Calculator successfully. Host parsing verifies every byte of 4 MiB
files, their terminal copies, and 220 KiB editor documents saved through the UI.
The restart case verifies existing large-file bytes inside a fresh guest.

Host tests exercise the actual kernel filesystem code with an emulated ATA
backend: exact 4 MiB capacity, remount, boundary extension, truncate/unlink,
disk-full save preservation, sector-write and flush failures, blocked later
writes, and corruption rejection. The independent host format checker validates
the resulting full disk. Format tests also cover v1/v2 and malformed indirect
references. These injected I/O tests are host tests, not physical hardware tests.

The soak lasted 603.05 seconds over 28 cycles.
Every cycle returned to 64117 free physical frames and
3149824 mapped kernel-heap bytes with zero event drops.
It repeatedly saved/reopened text, used Calculator, moved/resized/minimized/
restored/closed windows and verified bounded idle redraw.

A second clean build reproduced the identical ISO. No compiler warnings or
unresolved kernel symbols remained. Prior-stage ISOs were checksum-verified and
preserved. The raw kernel is 254,064 bytes, below the 448 KiB loader cap.
Stage 9 has been booted in QEMU; no Stage 9 VirtualBox/VMware verification is
claimed. Existing user VMs and disks were not changed.

Evidence: dist/stage9-evidence, dist/stage9-build-verification.log and
dist/stage9-SHA256SUMS.txt. Candidate tests preceded final validation; only
the final release evidence above establishes the shipped ISO's results.

## Artifacts and run command

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage9-foundation.iso
Size: 1,851,392 bytes. SHA-256: 56ca3a2f03b949b478ecd522874713a1d4a443021d842691e1058bbf171831c0
Seed: dist/gathaos-stage9-disk.img (33,554,432 bytes, immutable v2 seed).
Working copy: dist/gathaos-foundation-disk.img (created only if absent).
Changed/generated tree: stage-9-files.txt.

From the repository root:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage9-foundation.iso -drive file=dist/gathaos-foundation-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -serial stdio
```

VirtualBox/VMware: legacy BIOS, 256 MiB RAM, VGA-compatible adapter, 3D off,
PS/2 input and an IDE primary-master disk plus optical ISO. For VirtualBox use
VBoxVGA with 64 MiB video RAM; convert a copy of the new raw seed to a new VDI.
Keep v1 and v2 working disks separate; do not replace an existing user's disk.

## Manual checklist

- [ ] Boot the Stage 9 ISO with a copy of its v2 seed, using primary-master IDE.
- [ ] Open, edit, save and reopen a text document larger than 16 KiB.
- [ ] Use Terminal cp on a file larger than 192 KiB; verify both copies.
- [ ] Open the normal applications and exercise window controls.
- [ ] On a disposable full disk, save over an existing file and verify that a
      capacity error retains the previous on-disk version.
- [ ] Cleanly shut down, boot again and check documents.

The /System/Tests/foundation.gex probe is destructive only within its reserved
/FoundationTest directory and deliberately faults. Run it only on a disposable
disk. Its completion is reported on serial; it has no interactive window.

## Known limits

No network/browser yet. Large operations run synchronously with IRQs disabled
and can pause input. User heap memory is committed eagerly; there is no swap or
demand paging. The kernel heap remains 16 MiB, task count 16, windows 8, stack
64 KiB, managed physical RAM 512 MiB. The user allocator keeps interior free
blocks until reused; it returns the complete arena once empty.

Filesystem metadata still lacks power-loss atomicity and an offline repair
tool. Read-only mode contains later writes after I/O failure but does not undo
already written sectors. Staged replacement needs free capacity for old and new
data plus one inode. Interrupted saves can leave .gatha-save-N files. Readdir
order can change when a file is replaced. V2 disks cannot boot older kernels;
keep separate working disks. Existing v1 disks retain the old file-size limit.

Editor still uses ASCII, closes without an unsaved-change prompt, and has no
clipboard/undo. cat retains its old 192 KiB buffer. Shutdown is a flushed halt,
not ACPI power-off. VMware is untested; Stage 9 validation targets QEMU.

## Next step

The next proposed step is a virtual Ethernet driver and a minimal network stack
with IP configuration, DNS and a real download. It requires separate approval.
