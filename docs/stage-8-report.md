# Stage 8 - Polish and Release Candidate

Date: 2026-10-16. Status: complete.
The user authorized all remaining stages and requested a cleaner early-2000s look.

## Changes

A restrained blue/gray desktop, original folder/menu artwork, quiet wallpaper
ribbons, light client panels, subtly beveled buttons and drawn caption controls.
The font keeps its fixed 8x16 cells but uses a compact six-by-nine body instead
of vertically stretched glyphs. Start rows highlight under the pointer. About
welcomes the user without exposing low-level implementation details.

A boot splash appears after memory tests while storage/input startup completes.
The established power fallback flushes storage and halts with a safe-to-close
screen; reboot resets through the 8042. The interactive QEMU script now permits
that reset. Diagnostic background workers exit after six seconds. Paint validates
loaded drawings in a temporary buffer; invalid data cannot replace the canvas.
File deletion confirmation blocks interaction with underlying controls.

The release adds per-second memory/window/redraw/event-drop telemetry and a real
600-second minimum host-timed soak. Full architecture, current GUI/syscall notes,
visual design, application behavior and build/VM instructions are documented.

## Verification

The clean build and complete make test matrix passed: sixteen QEMU boots,
including 64/256/512 MiB RAM, keyboard sets 1/2, four fresh-process persistence
boots, four deliberate kernel faults, clean halt, reset/reboot and the soak.
All eight applications were exercised through actual PS/2 events; rendered text,
saved files and bitmap pixels were verified independently.

The soak completed 617.05 seconds and 29 cycles. Each cycle
saved/reopened text, used Calculator, dragged/maximized/restored/minimized/closed
windows, and returned to the same 64248 free physical frames and
3149824 mapped heap bytes. Event drops remained zero. Five-second idle
samples stayed below 25,000 presented pixels and at most eight frames, verifying
that idle presentation remains limited to the clock rather than the whole screen.

The first soak attempt stopped at a host drag-position assertion after 525
seconds of reported progress. The test was updated to wait for the guest's move
completion instead of inspecting immediately after QMP injection. The full suite
was then repeated successfully. This test change did not alter the guest ISO.

A second clean build reproduced the identical ISO. No compiler warnings or
unresolved kernel symbols remained. All prior-stage ISOs were checksum-verified
and preserved. Raw kernel: 241,776 bytes, below the 448 KiB limit.
VirtualBox 7.2.8 also booted the ISO; the visible GUI/Qt console was verified.

Evidence: dist/stage8-evidence, dist/stage8-build-verification.log and
stage8-SHA256SUMS.txt. Tests use private cloned disks. The separate VirtualBox
VM GathaOS-Clean-2000 uses its own writable VDI; earlier user disks are preserved.

## Artifacts

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage8-release.iso
Final alias: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-1.0-rc1.iso
Each ISO: 1,851,392 bytes. SHA-256: 6965d7b8fe955ec28750af9282d710346a9758ce0afd2207ed2260d75dd2e793
Seed: dist/gathaos-stage8-disk.img (33,554,432 bytes).
Changed/generated file tree: stage-8-files.txt.

Run from the repository root with a compatible working disk:

```sh
qemu-system-i386 -cdrom dist/gathaos-1.0-rc1.iso -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0 -boot d -m 256M -vga std -serial stdio
```

VirtualBox/VMware: legacy BIOS, 256 MiB RAM, VGA-compatible graphics, 3D off,
PS/2 input, optical boot ISO and GathaFS disk at IDE primary master. Use VBoxVGA
and 64 MiB video RAM in VirtualBox. VMware remains untested. Fresh installs must
copy the seed to a separate working disk. Never replace an existing data disk.

## Manual checklist

- [ ] Boot the new ISO with a compatible GathaFS primary-master IDE disk.
- [ ] Check font readability, blue default desktop and title buttons.
- [ ] Open Start; move over rows and launch several applications.
- [ ] Drag, minimize, restore, maximize and close overlapping windows.
- [ ] Save/reopen a note and a drawing; inspect Files and Calculator.
- [ ] Try all three wallpapers and accent colors in Settings.
- [ ] Save work, run shutdown in Terminal, wait for safe-to-close, then close VM.
- [ ] Boot again and verify saved documents; try reboot from Terminal.

## Known limits

QEMU is the automated verification target. The user reported Stage 7 operating
well in VirtualBox; the new release also booted in VirtualBox 7.2.8 and its
visible GUI console was verified. The complete automated suite uses QEMU.
Shutdown is a clean halt fallback, not automatic ACPI power-off. Save edits before
closing apps or shutting down. The theme is session-only. The display has no
VSync, antialiasing or runtime mode changes. Input is US ASCII and PS/2 only.
The fixed resource/file/application limits in README and applications.md remain.
GathaFS is not journaled; a sudden VM power cut can damage metadata. Boot self-tests
reserve /persist.bin, /testdir, /user-test.txt and /userdir for diagnostics.
The 800x600 firmware fallback is implemented but this release suite uses 1024x768.

## Next stage

Stages 0 through 8 complete the requested sequence. There is no automatically planned Stage 9; further changes follow user
feedback on this release candidate.
