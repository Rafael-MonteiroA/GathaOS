# Stage 5 — Storage and Filesystem

Completed 2026-10-16. The user authorized continuing through Stage 8 without
approval pauses; stages retain separate artifacts, reports and commits.

## Implementation

Original primary-master ATA PIO driver, RTC timestamps, GathaFS superblock,
allocation bitmap, 128 inodes, directories, file data and a per-process VFS with
eight descriptors. File syscalls support open/read/write/close/seek/readdir,
mkdir/unlink/stat/rename/sync with user-pointer validation. The deterministic
host image builder creates a 32 MiB companion disk from rootfs. See gathafs.md
for the exact format and syscalls.md for the ABI.

## Actual validation

Clean build and make test passed with warnings treated as errors. Four normal
boots (64/256/512 MiB, plus keyboard set 1) each passed file and input tests and
then started a fresh QEMU process on the same disk. All four second boots read
back the 9,000-byte persisted file. Independent host validation checked its
contents and allocation structure. Four additional deliberate kernel-fault
boots passed full panic-screen checks. Host format/corruption tests passed.
The user-mode probe exercised every file syscall, including invalid pointers
and descriptors. There were twelve actual boots in total.

A second clean build produced the same ISO bytes. All seven earlier ISO hashes
were preserved. The normal raw kernel is 73,780 bytes. Evidence, screenshots,
serial logs, command lines and results are in dist/stage5-evidence; the complete
build log and SHA-256 manifest accompany the ISO.

## Deliverables

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage5-storage.iso

ISO size: 1,851,392 bytes. Disk: dist/gathaos-stage5-disk.img (immutable seed),
33,554,432 bytes. dist/gathaos-disk.img is the writable runtime copy. Preserve
this copy to retain changes. Changed/delivered tree: stage-5-files.txt.

```sh
qemu-system-i386 -cdrom dist/gathaos-stage5-storage.iso -boot d -m 256M -vga std -serial stdio -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0
```

VirtualBox/VMware: legacy BIOS, 256 MiB, VGA, 3D off, PS/2, CD ISO and primary
IDE disk. Convert a copy of the raw disk to the hypervisor's supported disk
format. These hypervisors are not tested; automated evidence is from QEMU.

## Manual checklist

- [ ] Boot with the IDE disk and observe GATHAFS / USERFS PASS markers.
- [ ] Shut down QEMU and restart with the same disk; see persisted data verified.
- [ ] Verify both user animations and typing/mouse still work.
- [ ] Keep a backup of the writable disk before experimenting.

## Limits and next stage

No journal or power-loss atomicity; persistence was tested after clean VM
termination. ATA errors are reported but interrupted metadata writes are not
transactionally recovered. Files are limited to 192 KiB, names to 39 ASCII bytes,
paths to 255 bytes and eight open descriptors per process. Diagnostic boot files
/persist.bin and temporary test names are reserved. No user-facing file browser
yet. Keyboard is US ASCII; real hardware and other hypervisors remain untested.

Stage 6 adds the window manager, desktop and GUI toolkit. Commit: Stage 5: storage.
