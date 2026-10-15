# Stage 4 — Tasks, Scheduler, Syscalls and User Mode

Date: 2026-10-15. Status: completed; all eight release boots passed. The user explicitly
authorized continuing through all remaining stages without approval pauses;
see `continuous-build-authorization.md`. Stages remain separate releases/commits.

## Changes

Added a two-tick preemptive round-robin scheduler, kernel threads, private user
address spaces, ring-3 GDT descriptors, TSS kernel stacks, interrupt-frame context
switching and deferred exit cleanup. GEX1 is an original bounded/checksummed flat
executable format with a packer, loader and freestanding user startup/library.
The `int 0x80` ABI provides ticks, yield, sleep, exit, log, PID and a constrained
animation demonstration call. See `gex.md` and `syscalls.md`.

Two copies of the demo GEX run in distinct address spaces: one sleeps between
frames, the other performs a CPU-bound loop. A kernel worker sleeps/wakes in
parallel. User exceptions terminate only their task. The input lab remains usable
and displays both animations. The BIOS staging limit is now 448 KiB; its existing
segment-advancing sector loop is retained. GP debug tests now use selector 0x38,
since 0x18 is occupied by the newly added user descriptor.

## Validation

The release script runs `make clean && make iso`, then `make test`, with Clang
freestanding i686-elf `-Wall -Wextra -Werror`, NASM warnings-as-errors and LLD
fatal warnings. It then performs a second clean ISO build and byte comparison.
The final run passed without build warnings; the clean rebuild was byte-identical
and all six previous ISO hashes were preserved.
Actual QEMU TCG boots cover 64/256/512 MiB with keyboard set 2, 256 MiB with set 1,
and four kernel-fault variants at 256 MiB. The normal raw kernel is 61,492 bytes.

Normal tests check two moving user animations, rising preemption/worker counts,
sleep/yield, normal exit, rejected kernel-pointer and unknown-syscall arguments,
and a rejected truncated GEX. Separate user probes produce vector 13/error 0
for CLI, vector 14/error 5 for a supervisor-memory read, and vector 14/error 7
for a code write. The remaining processes keep running after all three probes.
Memory stress/accounting, IRQ preservation and input regression tests also run.

Host input tests wait for decoded motion and completed expected screen contents
with finite deadlines, because rendering now competes with scheduled user work.
They still verify exact text pixels, modifiers, repetition/release, scrolling,
mouse buttons and background restoration outside the two animated panels.
Two actual screenshots show both user animations moving. Fault variants verify
the complete panic pixels, CPU frame and halted state. Evidence is in
`dist/stage4-evidence/`; `dist/stage4-build-verification.log` records the actual
commands and results. The screenshot was also visually inspected.

## Artifact and VM command

```text
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage4-tasks.iso
```

Size: 1,851,392 bytes. The exact SHA-256 is in `dist/stage4-SHA256SUMS.txt`.
Changed/delivered files: `docs/stage-4-files.txt`.

```sh
qemu-system-i386 -cdrom dist/gathaos-stage4-tasks.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

VirtualBox/VMware manual configuration: legacy BIOS, EFI off, 256 MiB, VGA
(VBoxVGA in VirtualBox), 3D off, PS/2 input, no hard disk. These hypervisors and
physical hardware are unverified; actual automated boots use QEMU standard VGA.

## Manual checklist

- [ ] See two moving colored tiles labelled USER A and USER B.
- [ ] Type while animations continue; try modifiers, repeat, Enter and Ctrl+L.
- [ ] Move the mouse across the panels and text, including screen edges.
- [ ] Observe the expected USERFAULT probe logs without a kernel panic.
- [ ] Observe `SCHEDTEST: ... PASS` and `STAGE4: PASS` on serial.
- [ ] Optionally boot each DEBUG_FAULT image to inspect its panic diagnostics.

## Limits and next stage

Single CPU, 16 task slots, fixed executable address/maximum size and 64 KiB user
stacks. No FPU/SIMD context, signals, priorities, dynamic linker or NX. Syscalls
run with IRQs disabled; presentation/serial work can delay timer delivery.
Keyboard remains US ASCII, graphics software-buffered without VSync. Long-run
stability and leak stress belong to Stage 8. The GEX checks protect structure and
bounds; this release does not claim exhaustive malformed-image fuzz testing.

Stage 5 will add ATA PIO, the original GathaFS disk format/tooling, file syscalls
and persistence through a VM restart. No storage or filesystem is added here.

Commit: `Stage 4: tasks`.
