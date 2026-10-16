# Stage 6 — Window Manager and GUI Toolkit

Date: 2026-10-16. Status: complete. The user authorized continuous progression through Stage 8.

## Changes

Kernel compositor with stacking/focus, dragging, minimize, maximize/restore,
close requests, title bars, per-window RGB buffers, clipped damage redraw and
bounded/coalesced event queues. Window buffers are reclaimed at process exit.
Desktop wallpaper, icon, taskbar window list, RTC clock and Start menu launch
separate Widgets GEX processes. The original user toolkit includes labels,
buttons, checkboxes, single/multiline text boxes, selection/navigation, list,
scrollbar, menu bar, context menu and dialogs. GUI syscalls validate pointers,
ownership, dimensions, command counts and coordinates; see gui.md.

The ATA polling budget was increased and failure diagnostics added after an
initial candidate failed a filesystem write before reaching the desktop. The
next candidate boot passed storage and persistence. The first scrollbar test
found that its integer mapping could not reach the final item; the mapping was
corrected. Text pixel checks move the mouse off the glyphs before comparing.

## Validation and evidence

Release validation uses a clean build with warnings-as-errors, make test, a second
clean reproducibility build, undefined-symbol/ELF checks and verification of all
previous ISO hashes. The desktop tests send actual PS/2 input via QMP and check
rendered text pixels, widget event results, four window launches, focus, dragging,
minimize/taskbar restore, maximize/restore, menus, dialog, scrollbar and cleanup.
The final screenshot is visually inspected. The complete final command output,
per-boot result JSON, serial logs and screenshots are delivered in dist.

All twelve boots passed: normal 64/256/512 MiB and keyboard set 1, each followed
by a fresh-process persistence boot, plus four deliberate kernel-fault boots.
The two clean builds were byte-identical, with no build warnings and all eight
earlier ISOs unchanged. Raw kernel: 94,284 bytes. SHA-256 manifest:
dist/stage6-SHA256SUMS.txt; complete log: dist/stage6-build-verification.log.

## Artifacts and run command

ISO: C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage6-desktop.iso

ISO size: 1,851,392 bytes. Companion immutable seed:
dist/gathaos-stage6-disk.img, 33,554,432 bytes. Runtime copy:
dist/gathaos-disk.img. Changed/delivered tree: stage-6-files.txt.

```sh
qemu-system-i386 -cdrom dist/gathaos-stage6-desktop.iso -boot d -m 256M -vga std -serial stdio -drive file=dist/gathaos-disk.img,format=raw,if=ide,index=0
```

VirtualBox/VMware: legacy BIOS, EFI off, 256 MiB, VGA, 3D off, PS/2 input and
primary-master IDE disk. Convert a copy of the raw disk as required by the
hypervisor. Only QEMU standard VGA has been boot-tested.

## Manual checklist

- [ ] Drag the two windows, focus each, minimize and restore from the taskbar.
- [ ] Maximize/restore and close; launch more through Start and the desktop icon.
- [ ] Type in both fields; try arrows, Shift selection and Ctrl+A.
- [ ] Toggle the checkbox, select a list item and use the scrollbar.
- [ ] Open Help, close its dialog and try the right-click context menu.

## Known limits and next stage

Eight windows, sixteen tasks, ASCII input, no clipboard or wheel support,
no force-kill UI and no hardware VSync. Maximum window allocation can fail
gracefully under heap pressure. The RTC is displayed as UTC. GathaFS retains
Stage 5's size bounds and lack of power-loss journaling. Long-run leak and
redraw-performance validation is reserved for Stage 8.

Stage 7 adds the seven basic applications and About, each in a user-mode GEX.
Commit: Stage 6: desktop.
