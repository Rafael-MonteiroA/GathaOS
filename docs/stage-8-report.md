# Stage 8 - Polish and Release Candidate

Date: 2026-10-16. Verification in progress; results will be recorded before release.
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

Pending completion of the clean build, smoke matrix, power tests, soak and
second clean reproducibility build. The first attempt passed all short tests,
then stopped at a host drag-position assertion after 525 seconds of reported
soak progress. The host now waits for the guest move-completion event, and the
full suite is being repeated. No release status is inferred from compilation.

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

Stages 0 through 8 complete the requested sequence once the verification above
passes. There is no automatically planned Stage 9; further changes follow user
feedback on this release candidate.
