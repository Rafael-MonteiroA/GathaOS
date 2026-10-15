# Stage 1 report — Kernel Core

Date: 2026-10-15. Status: **completed; waiting for the user's test and approval**.
Stage 1 was authorized after the Stage 0 delivery. Stage 2 has not started.

## Implemented

- Kernel-owned flat GDT with code/data selectors 0x08/0x10 and segment reloads.
- A 256-entry IDT and original NASM interrupt stubs, with normalized CPU error
  codes, saved general/segment registers, aligned C calls and IRETD restoration.
- Exception naming and `panic()` with readable framebuffer and serial output:
  vector, error code, EIP, EFLAGS, GPRs, interrupted ESP, segments and CR0/CR2/CR3.
- An original hand-authored diagnostic font, with its permission grant recorded.
- PIC remapping to vectors 32-47, masking, EOI and spurious IRQ7/IRQ15 handling.
- PIT channel 0 at a nominal 100 Hz, divisor 11932, and tick logging from the
  idle main loop. An IRQ self-check verifies GPR and flags preservation.
- Kernel COM1 initialization, bounded serial polling and interrupt-state-preserving
  `klog`, decimal and hexadecimal output.
- Build-time `DEBUG_FAULT=div0` and `DEBUG_FAULT=gp` modes. Fault instructions
  are absent from the normal kernel. Each mode uses separate object directories.
- Expanded headless smoke tests that boot actual ISOs and inspect screen pixels,
  serial logs, fault EIP symbols, saved registers and the CPU's halted state.

No allocator, paging, scheduler, filesystem, input driver or GUI toolkit was added.
The panic text drawer is limited to diagnostics required by this stage.

## Clean build and actual boot validation

The final source was copied to a temporary native Ubuntu 22.04 / WSL2 directory
to avoid Windows-mounted filesystem clock skew. The following sequence was run
twice, with successful exit status and **no build warnings**:

```sh
make clean && make iso
make test
```

C used `-Wall -Wextra -Werror`, the i686-elf target and freestanding flags. NASM
and LLD also treated warnings as errors. The second clean build verified that
the existing normal and divide-demo ISOs were byte-for-byte identical.

Each test suite booted three ISOs in QEMU 6.2.0 using TCG, legacy BIOS, standard
VGA, 256 MiB RAM, no hard disk and no network device. All six final verification
boots passed. Every mode reached 1024x768x32 and returned six E820 records.

| Boot mode | Observed and checked result |
|---|---|
| Normal | GDT/IDT/PIC/PIT initialization; IRQ GPR/CF/IF/DF preservation; ticks 100, 200, 300, 400, 500; `STAGE1: PASS` at 300 |
| Divide error | After 200 ticks, vector 0, error 0, fault EIP 0x00100B73; seven expected GPR values; EFLAGS 0x647; readable panic; CPU halted with IRQs off |
| General protection | After 200 ticks, vector 13, error 0x18, fault EIP 0x00100B59; expected selectors and EAX; readable panic; CPU halted with IRQs off |

In the last normal run, tick markers 100 through 500 arrived at relative host
times 0.000, 1.008, 2.014, 3.020 and 4.025 seconds. This confirms approximate
cadence in the tested VM; it is not a hardware clock-accuracy benchmark.

The test compared **all 786,432 pixels per boot** against the normal pattern or
the complete panic text and register display. The divide-error PNG was also
visually inspected. QMP CPU state confirmed `HLT=1` and IF clear after each panic;
serial output remained unchanged afterward. Exception EIP was independently
checked against the linked ELF symbol. All three ELF files had no undefined
symbols. xorriso confirmed the BIOS El Torito fd1.4 boot entry.

Evidence is in [stage1-evidence](../dist/stage1-evidence/), with separate
`test-none`, `test-div0` and `test-gp` directories. Each holds the serial log,
QEMU log, raw PPM, lossless PNG, CPU registers, ELF inspection and JSON results.
The JSON keeps the actual temporary paths used during testing; SHA-256 connects
those tested files to the delivered copies. See also the
[build log](../dist/stage1-build-verification.log) and
[changed-file tree](stage-1-files.txt).

## Delivered ISOs

Both Stage 1 release ISOs are **1,851,392 bytes** (1.765625 MiB).

```text
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage1-core.iso
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage1-core-div0.iso
```

SHA-256:

```text
d278a80f6ddcc6f63c60610c869b26c2995eed15857adf8a2464758d2baacb6a  gathaos-stage1-core.iso
5a4bb8d6bf68b3d248534cf1a4a58124f078330e6a6588ff0a1c11d16434903d  gathaos-stage1-core-div0.iso
```

The Stage 0 ISO remains unchanged; its verified SHA-256 is
`1bf015e7ce396b67c0a50e4cc96fd1dde923f637f406dcd73da43206441cd60c`.
The GP image is a temporary test artifact under `build/`, reproducible through
`make test` or `make iso DEBUG_FAULT=gp`.

## Run and manual checklist

From the repository root in PowerShell with QEMU on PATH, or Ubuntu with a display:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage1-core.iso -boot d -m 256M -vga std -serial stdio -no-reboot
qemu-system-i386 -cdrom dist/gathaos-stage1-core-div0.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

Run these commands separately, closing the first VM before starting the second.

- [ ] Normal ISO boots and retains the navy/color-bar/checkerboard pattern.
- [ ] Serial reports GDT, IDT, PIC and PIT initialization without a panic.
- [ ] Serial shows IRQ preservation PASS and tick counts advancing by 100.
- [ ] `STAGE1: PASS` appears after 300 ticks and counting continues afterward.
- [ ] Divide-demo ISO reaches 200 ticks, then displays `PANIC: DIVIDE ERROR`.
- [ ] The panic screen shows vector/error zero and readable register values.
- [ ] The panic stays visible; restarting the VM repeats the intentional fault.
- [ ] Report any issue and explicitly approve Stage 2 only after testing.

For VirtualBox / VMware: 32-bit Other OS, legacy BIOS, EFI disabled, 256 MiB RAM,
VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB video RAM, 3D off,
ISO mounted in the boot optical drive. No hard disk is required. These settings
are supplied for manual testing; neither hypervisor was tested here.

## Known limits

- Only QEMU's standard VGA with legacy BIOS and 256 MiB RAM was verified.
  VirtualBox, VMware, real hardware and the 800x600 fallback remain unverified.
- Exception handlers require a usable kernel stack. There is no emergency
  double-fault stack/task yet; stack corruption can still prevent diagnostics.
- Only divide error and general protection were deliberately triggered. Other
  exception vectors and spurious PIC IRQ cases have not been injected.
- The frame ABI is ring-0-only. No TSS, ring-3 transitions or context switching.
- The 32-bit tick counter's approximately 497-day wrap has not been boot-tested.
- No input or recovery controls exist. Restart or close the VM after a panic.
- The original 64 KiB kernel payload cap and fixed El Torito boot image remain.
- Font output is uppercase diagnostic text; no general font or GUI API exists.

## Next stage — not started

After explicit approval, Stage 2 will implement a physical memory manager,
higher-half paging, a kernel heap, a memory map report and boot-time allocation
stress tests. No Stage 2 implementation is included in this commit.

Commit message: `Stage 1: core`.
