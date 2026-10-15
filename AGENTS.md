# GathaOS — Build Instructions for Codex

You are building **GathaOS**, a complete, original, from-scratch operating system with a graphical user interface and a set of basic applications.

> Scope for now: **a working OS with a GUI and basic apps. Nothing more.** No networking, no package manager, no Wi-Fi, no 3D, no compatibility layers. Do not add features outside this scope.

---

## 1. Hard Rules

1. **Not Linux. Not a Linux derivative.** Do not use the Linux kernel, BSD kernels, busybox, GNU coreutils, X11, Wayland, GTK, Qt, SDL, or any existing OS/userland. Do not copy code from existing kernels.
2. **No hosted libc.** Everything runs freestanding. Write your own minimal `libc`-like library (`libgatha`) for the kernel and for user programs.
3. **Own bootloader.** Write a custom two-stage bootloader (NASM). Do not use GRUB, Limine, or SYSLINUX.
4. **Own filesystem (GathaFS)**, own executable format (`.gex`), own syscall ABI, own GUI toolkit, own window manager.
5. Third-party code is allowed only for **build tooling** (NASM, GCC/Clang cross-compiler, `make`, `xorriso`, QEMU, Python for build scripts). Fonts must be self-made or public-domain (e.g. a hand-written 8x16 bitmap font), with the license noted.
6. Never skip a stage or merge stages. Never continue to the next stage without my approval (see section 4).

---

## 2. Technical Targets

| Item | Decision |
|---|---|
| Architecture | x86 (32-bit protected mode). Keep code clean enough to port to x86_64 later |
| Languages | NASM (boot, low-level stubs) + freestanding C (C11/C17). No C++ |
| Toolchain | `i686-elf-gcc` cross-compiler (or clang with `--target=i686-elf`), `nasm`, `ld`, `make` |
| Graphics | VBE linear framebuffer (set up in the bootloader), 32-bit color, 1024x768 minimum, double buffered |
| Input | PS/2 keyboard and PS/2 mouse |
| Storage | ATA PIO driver (IDE) first; GathaFS on a disk image |
| Test environment | QEMU (`qemu-system-i386`), running from WSL2 or Windows |
| Output | Bootable **ISO** (El Torito) for each stage |

Development host is Windows with WSL2, PowerShell and VS Code. All build commands must work inside WSL2 (Ubuntu). Provide a `scripts/setup-deps.sh` that installs everything required (`nasm`, `make`, `xorriso`, `qemu-system-x86`, cross-compiler build or package).

---

## 3. Repository Layout

```
gathaos/
├── AGENTS.md                # this file
├── README.md                # what GathaOS is, how to build and run
├── Makefile                 # top-level: `make`, `make iso`, `make run`, `make test`, `make clean`
├── boot/
│   ├── stage1.asm           # 512-byte boot sector
│   └── stage2.asm           # loads kernel, enables A20, VBE, protected mode
├── kernel/
│   ├── arch/x86/            # GDT, IDT, ISRs, IRQs, PIC, PIT, context switch
│   ├── mm/                  # PMM, paging, kernel heap
│   ├── drivers/             # serial, framebuffer, ps2 kbd/mouse, ata, rtc
│   ├── fs/                  # VFS + GathaFS
│   ├── sched/               # tasks, scheduler
│   ├── syscall/             # syscall table and dispatcher
│   └── main.c
├── libgatha/                # freestanding libc-like library (string, stdio, malloc, syscalls)
├── gui/
│   ├── gfx/                 # drawing primitives, fonts, bitmaps, blitting
│   ├── wm/                  # window manager, compositor, events
│   └── toolkit/             # buttons, labels, text boxes, lists, menus, scrollbars
├── apps/
│   ├── terminal/
│   ├── files/
│   ├── editor/
│   ├── calculator/
│   ├── settings/
│   ├── sysmon/
│   └── paint/
├── tools/                   # host tools: mkgathafs, gex packer, image converters
├── scripts/                 # setup-deps.sh, make-iso.sh, run-qemu.sh, smoke-test.sh
├── docs/                    # architecture, memory map, syscall list, GathaFS spec, stage reports
└── dist/                    # build outputs (ISO per stage) — see section 4
```

---

## 4. Stage Workflow (MANDATORY)

The project is built in **stages**. After finishing **each** stage you must do all of the following, in order:

1. **Build clean** from scratch (`make clean && make iso`). No warnings with `-Wall -Wextra`.
2. **Run the automated smoke test** (`make test`) that boots the ISO in QEMU headless and checks serial output for the stage's expected markers.
3. **Generate the stage ISO** at `dist/gathaos-stage<N>-<name>.iso` (example: `dist/gathaos-stage3-drivers.iso`). Keep the ISOs of previous stages; never overwrite them.
4. **Show me the files.** Print:
   - the tree of files created or changed in this stage,
   - the full path of the ISO and its size,
   - the exact command to run it in a VM, for example:
     `qemu-system-i386 -cdrom dist/gathaos-stage3-drivers.iso -m 256M -vga std -serial stdio`
   - a note on how to load it in VirtualBox / VMware (BIOS mode, 256 MB RAM, VGA, no disk required unless the stage says so).
5. **Write a stage report** at `docs/stage-<N>-report.md` with: what was done, what to test manually (a checklist for me), known issues, and what the next stage will do.
6. **Commit** with message `Stage <N>: <name>`.
7. **STOP and wait for my approval.** Do not start the next stage until I reply that I tested it. If I report bugs, fix them first and regenerate the ISO as `...-stage<N>-<name>-fix<K>.iso`.

If something cannot be completed in a stage, say so clearly instead of faking it. Never claim a feature works unless it was actually booted and verified.

---

## 5. Stages

### Stage 0 — Toolchain and Boot
- `setup-deps.sh`, Makefile, ISO pipeline (El Torito), `run-qemu.sh`.
- Stage 1 bootloader (512 bytes) → stage 2 bootloader: load kernel from the boot medium, enable A20, query memory map (E820), set VBE mode (1024x768x32, fallback 800x600), enter protected mode, jump to the kernel.
- Kernel prints "GathaOS booting..." on serial (COM1) and draws a solid-color screen with a test pattern.
- **Test:** ISO boots in QEMU, serial shows the boot banner, screen shows the pattern.

### Stage 1 — Kernel Core
- GDT, IDT, exception handlers (with a readable panic screen showing registers), PIC remap, PIT timer (100 Hz), serial logging (`klog`), `panic()`.
- **Test:** timer ticks counted on serial; deliberate divide-by-zero shows the panic screen (behind a debug flag).

### Stage 2 — Memory Management
- Physical memory manager (bitmap or buddy), paging with higher-half kernel, kernel heap (`kmalloc`/`kfree`), memory map report.
- **Test:** allocation stress test on boot, results on serial.

### Stage 3 — Graphics and Input Drivers
- Framebuffer driver with double buffering, drawing primitives (pixel, line, rect, rounded rect, gradients, alpha blend), bitmap font renderer (8x16 minimum), PS/2 keyboard (scancode set 1/2, modifiers, key repeat), PS/2 mouse (3-byte packets, cursor sprite).
- **Test:** a screen where I can type text and move a mouse cursor smoothly.

### Stage 4 — Tasks, Scheduler, Syscalls, User Mode
- Preemptive round-robin scheduler, kernel threads, ring 3 user mode, syscall interface (`int 0x80`), `.gex` executable format and loader, `libgatha` basics, `sleep`, `yield`, `exit`.
- **Test:** two user programs animating on screen at the same time.

### Stage 5 — Storage and Filesystem
- ATA PIO driver, VFS layer, **GathaFS** (superblock, inode table, bitmap, directories, files, timestamps; documented in `docs/gathafs.md`), `mkgathafs` host tool to build the disk image from a folder, file syscalls (`open/read/write/close/seek/readdir/mkdir/unlink/stat`).
- The ISO must carry the system disk image or ship a companion `dist/gathaos-disk.img` that is attached in QEMU (document which one and how).
- **Test:** boot, list files, create/read/write a file, persist across reboot (when using the disk image).

### Stage 6 — Window Manager and GUI Toolkit
- Compositor with window stacking, dragging, focus, minimize/maximize/close, title bars, per-window back buffers, damage-rectangle redraw, event queue (mouse, keyboard, window events).
- Toolkit: label, button, checkbox, text box (single/multi-line), list view, menu bar, context menu, scrollbar, dialog boxes.
- Desktop: wallpaper, desktop icons, taskbar with window list and clock (RTC), start menu that launches apps.
- Visual quality matters: consistent theme, clean spacing, smooth cursor, no flicker.
- **Test:** open several windows of a demo app, drag them, switch focus, use the start menu.

### Stage 7 — Basic Applications
Each app runs as a separate user-mode `.gex` program using the toolkit:
- **Terminal**: GathaShell with built-in commands (`ls`, `cd`, `cat`, `mkdir`, `rm`, `cp`, `mv`, `echo`, `clear`, `ps`, `mem`, `uptime`, `help`, `reboot`, `shutdown`).
- **File Manager**: browse folders, create/rename/delete, open files with the right app.
- **Text Editor**: open/edit/save text files, basic cursor movement and selection.
- **Calculator**: standard operations.
- **Settings**: wallpaper/theme color, resolution info, date/time view.
- **System Monitor**: tasks list, memory usage, CPU ticks.
- **Paint**: simple drawing canvas with a few tools and colors; save as a simple own bitmap format.
- **About GathaOS** dialog.
- **Test:** every app launches from the start menu, works, and closes without crashing the system.

### Stage 8 — Polish and Release Candidate
- Boot splash, clean shutdown/reboot (ACPI or fallback), stability pass (no crash during 10 minutes of normal use), memory leak check, performance pass on redraw, final `README.md`, full docs, final ISO `dist/gathaos-1.0-rc1.iso`.

---

## 6. Code and Quality Standards

- Small, readable functions; one responsibility per file; comments explain **why**, not what.
- All code, identifiers, comments, commit messages and docs in **English**.
- Every kernel subsystem logs through `klog` to serial so problems can be diagnosed from `-serial stdio`.
- No undefined behavior: use `-ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror` where applicable.
- Document the memory map, boot protocol, syscall table and GathaFS format in `docs/`.
- Keep the build reproducible: one command (`make iso`) produces the ISO from a clean checkout.
- Add automated tests where possible (serial-marker smoke tests; host-side unit tests for `mkgathafs` and pure-logic code).

---

## 7. How to Start

1. Read this whole file.
2. Create the repository layout and `README.md`.
3. Begin **Stage 0** only.
4. Finish Stage 0 following the workflow in section 4, show me the files and ISO, and **wait for my approval**.