# Stage 3 report — Graphics and Input Drivers

Date: 2026-10-15. Status: **completed; waiting for the user's test and approval**.
The user authorized this stage after Stage 2. Stage 4 has not started.

## Implemented

- Framebuffer driver with a heap-backed RGB back buffer, pitch/channel-aware
  presentation and software double buffering over the BIOS VBE front buffer.
- Pixel, line, rectangle, rounded rectangle, gradient and alpha-blend primitives.
- Original 8x16 font for all 95 printable ASCII characters, with recorded license.
- PS/2 controller setup, native keyboard scancode sets 1 and 2, independent
  left/right modifiers, Caps Lock and software key repeat (500 ms / 40 ms).
- Standard three-byte PS/2 mouse, signed motion, bounds, three buttons and an
  original cursor sprite with background restoration.
- IRQ1/IRQ12 routing through a bounded input byte queue; main-loop decoding and
  rendering. Initial PIC setup and the timer preservation probe remain intact.
- A graphical input lab: shapes, typed text, Enter/Backspace/Tab, wrapping,
  scrolling, Ctrl+L and visible modifier/button indicators. Presentation is
  limited to 50 Hz. This stage does not introduce desktop windows or applications.

See `graphics-and-input.md` for API contracts, recovery and supported input.

## Clean build and actual boot tests

In a native Ubuntu 22.04 / WSL2 temporary checkout, this sequence succeeded:

```sh
make clean && make iso
make test
```

The build used freestanding Clang i686-elf, `-Wall -Wextra -Werror`, NASM
warnings-as-errors and LLD fatal warnings. **No build warnings** were emitted.
The release suite booted eight ISOs in QEMU 6.2.0 / TCG / legacy BIOS / standard
VGA, with no hard disk or network device. All selected 1024x768x32.

| Boot | RAM | Keyboard set | Result |
|---|---:|---:|---|
| Normal interactive | 256 MiB | 2 | PASS |
| Normal interactive | 64 MiB | 2 | PASS |
| Normal interactive | 512 MiB | 2 | PASS |
| Alternate keyboard | 256 MiB | 1 | PASS |
| Null page write | 256 MiB | — | PASS: expected page fault |
| Read-only write | 256 MiB | — | PASS: expected protection fault |
| Divide by zero | 256 MiB | — | PASS: expected register frame |
| Invalid selector | 256 MiB | — | PASS: expected general protection |

Every boot passed the earlier allocation/exhaustion/rollback tests, independent
PMM accounting, higher-half EIP/ESP and CR0.PG/WP checks, IRQ register/flag
preservation and timer checks. Normal set-2 usable/free-after-memory-test counts
were 15,934/15,918 at 64 MiB, 65,086/65,070 at 256 MiB and 130,622/130,606 at
512 MiB. The back buffer is allocated afterward and consumes 3,145,728 bytes
plus heap metadata/page rounding. These free counts are the memory-test baseline,
not the final graphics heap footprint.

The four interactive boots sent real QMP keyboard/mouse events into the emulated
devices. Tests checked:

- Visible `GathaOS 3!` and `ab  zQwE` text after case changes, Enter, Backspace,
  Tab, Caps Lock and Shift XOR Caps, using every pixel in the text area.
- Both Shift keys held together, releasing one while the other remains down;
  left/right Control and Alt suppressing text; Print Screen/Pause without stuck
  modifiers.
- A held `r` repeating ten characters in each final run, followed by no further
  repetition after release. Actual text pixels matched the counted characters.
- Nineteen entered lines scrolling through the sixteen-row text area; Ctrl+L
  clearing it; final retyping and caret display.
- Cursor travel across graphics/text, all screen corners, overrun clamping and
  thirty consecutive relative moves. Returning the cursor restored **all
  786,432 display pixels** to the saved baseline, without trails.
- Left/middle/right button events and corresponding on-screen indicator colors.
- Primitive samples: rounded corner/interior, rectangle, diagonal, gradient
  endpoints and a known alpha result. Boot self-tests add clipped writes,
  alpha endpoints, signed -256 motion, overflow and packet timeout/resync.
- No input-drop diagnostic or panic during these exercised workloads.

All four fault boots compared the complete panic display with expected text,
matched fault EIPs to ELF symbols and checked CPU error codes, CR2 where relevant,
halted state and disabled IRQs. Their screen path bypasses the graphics demo.
The interactive screenshot was visually inspected as well.

ELF inspections found no undefined symbols in the normal/fault release builds.
The raw normal kernel is **53,248 bytes**, below the unchanged 64 KiB loader
limit. xorriso confirmed a bootable BIOS El Torito 1.44 MiB emulated-floppy image.
A second `make clean && make iso` produced an identical ISO. Its Windows copy
was byte-compared, and all five previous Stage 0/1/2 ISO hashes stayed unchanged.

Evidence: [build/rebuild log](../dist/stage3-build-verification.log),
[normal result](../dist/stage3-evidence/test-none-256-set2/result.json),
[set-1 result](../dist/stage3-evidence/test-none-256-set1/result.json),
[actual typed screen](../dist/stage3-evidence/test-none-256-set2/typed.png),
[checksum](../dist/stage3-SHA256SUMS.txt), [changed-file tree](stage-3-files.txt).
The evidence directory contains all eight boot results, serial logs, actual
PNG/PPM captures, QMP CPU states and ELF/map checks. JSON records the temporary
paths actually tested; hashes identify the delivered ISO copies.

## Delivered ISO and run command

ISO size: **1,851,392 bytes** (1.765625 MiB).

```text
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage3-drivers.iso
SHA-256: d428737b1e8eccfefbc78efd86601d588ab7de3d590c042b016159c2cd32a670
```

From the repository root, with QEMU on PATH:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage3-drivers.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

VirtualBox / VMware: Other 32-bit OS, legacy BIOS (EFI off), 256 MiB RAM,
VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB video RAM, 3D off,
PS/2 keyboard/mouse and ISO attached to the boot optical drive. No disk required.
These are manual-test settings; those hypervisors have not been verified.

## Manual checklist

- [ ] Boot the normal ISO and see the input lab with three graphics samples.
- [ ] Click inside QEMU to capture input; Ctrl+Alt releases the capture.
- [ ] Type letters/numbers using the US keyboard layout; try Shift and Caps Lock.
- [ ] Try Enter, Backspace and Tab; type enough lines to scroll.
- [ ] Hold a letter for repeat, then release it and confirm repetition stops.
- [ ] Press Ctrl+L to clear the text.
- [ ] Move the cursor across text and shapes and into every corner.
- [ ] Press left/middle/right mouse buttons and observe the matching indicators.
- [ ] Check serial for `MMTEST: ALL PASS`, `STAGE3: READY` and `STAGE3: PASS`.
- [ ] Report any issue and approve Stage 4 only after testing.

## Known limits

- Tested only on QEMU standard VGA/BIOS at 1024x768. The 800x600 fallback,
  physical hardware, VirtualBox and VMware remain unverified.
- Software presentation has no vertical-sync/page-flip guarantee. The 50 Hz
  redraw cap and tested motion are not a frame-time benchmark or long soak test.
- US printable ASCII only; no ABNT2/Unicode, hardware keyboard LED updates,
  numpad numeric mode, arrow navigation or saved text. Text is a driver demo.
- Standard PS/2 mouse only: no wheel, extra buttons or USB HID. Device/controller
  command failures panic; no hotplug. Arbitrary lost-byte recovery is limited
  by the PS/2 protocol. Queue-loss recovery exists but was not fault-injected.
- Existing memory, boot payload, supervisor-only paging and panic stack limits
  remain. There is no scheduler, userspace or filesystem.

## Next stage — not started

After approval, Stage 4 will implement tasks, a preemptive round-robin scheduler,
kernel threads, ring-3 user mode, the syscall ABI and `.gex` loader. Its test will
run two user programs animating simultaneously. None of this is included here.

Commit message: `Stage 3: drivers`.
