# Stage 7 — Basic Applications

Date: 2026-10-16. Status: complete. The user authorized completion through Stage 8 without pauses.

## Implementation

Separate user-mode GEX programs provide GathaShell, Files, Text Editor,
Calculator, Settings, System Monitor, Paint and About. The Start menu launches
them; Files opens text, GBM2 drawings, folders and standalone GEX executables.
The seed includes executable files under /System/Apps. Built-in images also
remain embedded so older compatible GathaFS data disks can launch applications.

The shell implements all requested commands. Editor opens/saves ASCII text,
supports navigation and Shift selection. Calculator uses original fixed-point
arithmetic/runtime helpers. Settings changes session colors and displays RTC
date/time/resolution. Monitor reads process and memory statistics. Paint supports
pencil, eraser, line, filled rectangle, eight colors and an original indexed
bitmap format. See applications.md for exact controls and limitations.

GUI draw transactions prevent intermediate command chunks and bitmap updates
from being presented as incomplete frames. Launch paths, process enumeration,
indexed bitmap and power syscalls were added with user-memory validation.

## Test procedure

Clean build, make test and a second clean reproducibility build. Normal boots
at 64/256/512 MiB and set-1 keyboard launch and close every application via PS/2
input. Tests save/reopen text and compare actual font pixels, verify decimal
addition/division, change theme, view task statistics, save/reopen a drawing,
create/rename/delete files and open a file association. A disk GEX is launched
through Files. Shell commands perform real filesystem changes and their output
is checked. Independent host parsing verifies text bytes, bitmap stroke endpoints,
deleted entries and GathaFS allocation consistency after a fresh-process restart.
Four deliberate exception kernels verify complete panic screens/registers.
Additional boots exercise shutdown's clean halt and an actual reboot.

All fifteen boots passed. Clean builds were byte-identical, no build warnings
were emitted, no unresolved kernel symbols remained and all nine prior ISOs
were preserved. Raw kernel: 233,584 bytes. The host disk checks were also rerun
after correcting a metadata-only byte-count field (31 text bytes); the full
byte comparison had already passed. See disk-recheck.txt in the evidence.

Evidence is delivered under dist/stage7-evidence, with the command log and
SHA-256 manifest beside the ISO. Tests preserve the user's runtime disk by
working on private copies. Earlier stage artifacts remain unchanged.

## Artifacts and VM command

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage7-apps.iso

ISO size: 1,851,392 bytes. Seed: dist/gathaos-stage7-disk.img (33,554,432 bytes).
Writable runtime: dist/gathaos-disk.img. Existing runtime data is preserved;
to get the new sample /System/Apps folder, use a separate copy of the Stage 7 seed.
Changed/delivered tree: stage-7-files.txt.

```sh
qemu-system-i386 -cdrom dist/gathaos-stage7-apps.iso -boot d -m 256M -vga std -serial stdio -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0
```

VirtualBox/VMware: legacy BIOS, 256 MiB, VGA, 3D off, PS/2 and primary IDE disk.
Convert a copy of the raw image to the hypervisor's disk container. Only QEMU
standard VGA has been tested. Do not use -no-reboot when trying the shell reset.

## Manual checklist

- [ ] Launch and close each application from Start.
- [ ] Save a text file, reopen it and confirm its content after restarting QEMU.
- [ ] Browse Documents; create, rename, open and delete a temporary file.
- [ ] Run help, ls, cat, cp, mv, rm, ps, mem and uptime in Terminal.
- [ ] Calculate 12.5 + 7.5 and 9 / 4.
- [ ] Change wallpaper/accent; inspect the RTC and process list.
- [ ] Draw with each Paint tool, save a .gbm file and reopen it.
- [ ] Save work before issuing shutdown or reboot.

## Limits and next stage

ASCII only, editor 16,383 bytes, filesystem 192 KiB/file, eight windows and
sixteen task slots. No clipboard/undo. Calculator uses four fractional digits
and sequential operations, not expression precedence. Shell uses simple
space-separated arguments. Settings are session-only. Close exits immediately;
save edited work first. Shutdown currently halts with a safe-to-close screen.
No journaling or power-loss recovery; see gathafs.md.

Stage 8 provides boot/release polish, automatic power-off, the actual ten-minute
normal-use stability run, memory-leak checks and redraw-performance measurements.
Commit: Stage 7: apps.
