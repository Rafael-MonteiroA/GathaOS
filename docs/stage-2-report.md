# Stage 2 report — Memory Management

Date: 2026-10-15. Status: **completed; waiting for the user's test and approval**.
The user authorized this stage after Stage 1. Stage 3 has not started.

## Implemented

- E820-based physical page management with eligible/busy bitmaps, 4 KiB pages,
  conservative reservations, 64-bit range clipping and allocation/free accounting.
- A lower entry trampoline and a higher-half linked kernel at the 0xc0000000
  offset. Code, stack, GDT and IDT execute/use high virtual addresses.
- Non-PAE 4 KiB supervisor paging, removal of low identity/null mappings,
  read-only text/rodata with CR0.WP, and an independently mapped framebuffer.
- `kmalloc`/`kfree` with 16-byte alignment, first-fit allocation, splitting,
  coalescing, on-demand mapped growth, physical-OOM rollback and trailing-page
  return. The initial mapping is 64 KiB; the virtual heap capacity is 16 MiB.
- Original minimal `libgatha` memcpy/memset for freestanding kernel use.
- Serial memory-map/accounting reports and boot allocation stress tests.
- Optional real page faults for null access and read-only writes, plus regression
  tests for the existing divide and general-protection diagnostics.

The screen pattern is intentionally unchanged. No Stage 3 graphics primitives,
double buffering, keyboard, mouse, window manager or application was added.

## Clean build and actual validation

The final kernel sources were built on Ubuntu 22.04 / WSL2 in a temporary native
filesystem checkout. This exact sequence succeeded:

```sh
make clean && make iso
make test
```

The build emitted **no warnings**, using C `-Wall -Wextra -Werror` with the
freestanding i686-elf target, NASM warnings-as-errors and LLD fatal warnings.
The final smoke suite completed **seven actual ISO boots** with QEMU 6.2.0,
TCG, legacy BIOS, standard VGA, no hard disk and no network device.

| Normal boot RAM | PMM usable frames | Free after initial heap / after all tests | Result |
|---|---:|---:|---|
| 64 MiB | 15,940 | 15,924 / 15,924 | PASS |
| 256 MiB | 65,092 | 65,076 / 65,076 | PASS |
| 512 MiB | 130,628 | 130,612 / 130,612 | PASS |

Each boot tested E820 range boundaries and overflow; mapping aliases and unmap;
complete physical-frame exhaustion and restoration; reserved/misaligned/double
free rejection; total and partial heap-growth failure rollback; 4,096 mixed heap
operations with alignment, metadata and byte-content verification; and heap
capacity exhaustion with 255 allocations of 64 KiB. All final free-frame counts
matched their baselines, and heap backing returned to 65,536 bytes.

The host independently checked normal usable-frame counts against configured
RAM, the 0x0019c000 kernel reservation end and QEMU's 128 KiB top reservation.
It checked QMP CPU state for high EIP/ESP and CR0.PG/WP, then verified timer counts
100 through 500 and the original IRQ register/flags preservation test.

Four additional 256 MiB boots passed all memory tests before deliberately faulting:

| Debug mode | CPU exception and observed result |
|---|---|
| page | Vector 14, error 2, CR2 0; fault EIP 0xc0103b64 |
| readonly | Vector 14, error 3, CR2 0xc0107000; fault EIP 0xc0103b64 |
| div0 | Vector 0, error 0; expected seven GPR sentinels and saved flags |
| gp | Vector 13, error 0x18; expected invalid-selector state |

Fault EIPs were checked against linked instruction symbols. CR0 was 0x80010011;
CR3 was 0x00192000. Panics left the CPU halted with IRQs disabled and stable
serial output. The page-fault screenshot was also visually inspected.

All seven boots used 1024x768x32 and passed comparison of **786,432 actual display
pixels each**, covering the normal pattern and complete panic register text.
All five linked variants had no undefined symbols. ELF inspection confirmed a
physical bootstrap entry at 0x100000 and main executable text at virtual
0xc0103000 / physical 0x00103000. The raw kernel payload is 36,864 bytes, below
the existing 64 KiB boot limit. xorriso confirmed BIOS El Torito fd1.4 bootability.

After testing, a second clean build of the normal and page-demo variants produced
byte-for-byte identical release ISOs. Their delivered Windows copies were also
compared. All three prior Stage 0/1 ISO checksums remained unchanged.

Evidence: [build log](../dist/stage2-build-verification.log),
[normal 256 MiB result](../dist/stage2-evidence/test-none-256/result.json),
[page-fault screenshot](../dist/stage2-evidence/test-page-256/screen.png),
[checksums](../dist/stage2-SHA256SUMS.txt), and [changed-file tree](stage-2-files.txt).
The full `dist/stage2-evidence/` includes all seven serial logs, raw PPM/PNG
captures, QMP CPU states and JSON results. The 256 MiB directories also contain
ELF headers, linker maps and undefined-symbol checks. JSON intentionally records
the temporary paths actually used during testing; hashes identify the delivered
copies of those tested images.

## Delivered artifacts

Both ISO files are **1,851,392 bytes** (1.765625 MiB):

```text
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage2-memory.iso
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage2-memory-page.iso
```

SHA-256:

```text
8cbbb13da468437811a01ac9ac496f1773e8050cebfa7cf9a4c6ba6b1561fffa  gathaos-stage2-memory.iso
847a117773cc4f1a815194625ea203f0ed63afa08af4164be87e18f255bcd671  gathaos-stage2-memory-page.iso
```

The page ISO is an intentional fault demo. The normal ISO continues running
after its successful boot tests. Readonly/div0/gp variants are test artifacts
under `build/`, reproducible through `make test`.

## Run and manual checklist

From the repository root in PowerShell with QEMU on PATH, or Ubuntu with a display:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage2-memory.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

For the intentional page-fault demonstration, run separately:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage2-memory-page.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

- [ ] Normal ISO boots and shows the existing color bars/checkerboard.
- [ ] Serial reports E820 regions, PMM frame counts and the high paging setup.
- [ ] All `MMTEST` results end in PASS, including `MMTEST: ALL PASS`.
- [ ] At 256 MiB, free frames return to 65,076 and heap backing to 65,536 bytes.
- [ ] `STAGE2: PASS` appears at 300 ticks; the timer continues afterward.
- [ ] Optionally boot with `-m 64M` or `-m 512M` and confirm the adjusted counts.
- [ ] Page demo runs memory tests, then shows `PANIC: PAGE FAULT` after 200 ticks.
- [ ] Its vector is 14 (0x0e), error 2 and CR2 zero, with readable high addresses.
- [ ] Report any issue; approve Stage 3 only after testing.

VirtualBox / VMware settings: 32-bit Other OS, legacy BIOS, EFI disabled,
256 MiB RAM, VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB
video RAM, 3D disabled and ISO attached to the boot optical drive. No disk is
required. These settings are for manual testing; those hypervisors remain untested.

## Known limits

- Only QEMU standard VGA with legacy BIOS was tested. Real hardware,
  VirtualBox, VMware and the 800x600 fallback remain unverified.
- The PMM caps its managed physical address range at 512 MiB; higher RAM is
  ignored. The direct map includes reserved holes, which must never be treated
  as allocatable RAM. Only PMM-returned frames are safe allocation candidates.
- The heap has a 16 MiB virtual limit, first-fit search and in-band metadata.
  There are no slabs, realloc, process heaps, allocation canaries or SMP locks.
- Tests cover the exercised workloads; they do not establish leak freedom for
  all future workloads. The exhaustion suite currently runs before other
  subsystems own dynamically allocated memory.
- Paging is non-PAE, supervisor-only and has no NX, swapping or process isolation.
  A usable kernel stack is still required; there is no stack guard/emergency
  double-fault stack. Invalid kernel writes can still corrupt writable state.
- The 64 KiB raw payload and 4 MiB bootstrap mapping limits remain enforced.
- Memory tests precede timer initialization, and the screen remains a static
  pattern. No input or recovery UI exists; close or restart the VM after panic.

## Next stage — not started

After approval, Stage 3 will add a double-buffered framebuffer driver, drawing
primitives and bitmap text rendering, PS/2 keyboard and mouse support, and an
interactive test screen for typing and moving a cursor. None of that work has
been started in this stage.

Commit message: `Stage 2: memory`.
