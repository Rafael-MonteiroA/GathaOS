# Stage 0 report — Toolchain and Boot

Date: 2026-10-15. Status: **completed; waiting for the user's test and approval**.
No Stage 1 implementation has started.

## Implemented

- Repository structure and English documentation, using the supplied full
  instructions as the root `AGENTS.md` (the starting folder had no repository).
- Ubuntu / WSL2 dependency setup with Clang's i686-elf cross target, NASM, LLD,
  LLVM objcopy, Make, xorriso, Python and QEMU.
- Original 512-byte boot sector and 8 KiB second-stage bootloader.
- Kernel reading from the boot medium, E820 collection, verified A20 enablement,
  VBE mode selection and transition to 32-bit protected mode.
- Freestanding C kernel, minimal COM1 `klog`, boot parameter validation and a
  static framebuffer pattern. No hosted libc or external guest runtime.
- El Torito floppy-emulation ISO pipeline, run script and headless smoke test.
- Immutable output policy: `make clean` preserves `dist/`; different bytes must
  use a new fix name. Identical rebuilds are accepted without replacing the ISO.

## Actual validation

`scripts/setup-deps.sh` was executed successfully on Ubuntu 22.04 in WSL2.
Two consecutive clean builds and smoke tests were executed in a temporary native
WSL checkout using exactly `make clean && make iso` followed by `make test`.
The build log contains no warnings. NASM and C compilation treat warnings as
errors; C uses `-Wall -Wextra -Werror` and the required freestanding flags.

QEMU 6.2.0, machine `pc,accel=tcg`, 256 MiB RAM, standard VGA, legacy SeaBIOS,
no hard disk and no network device booted the actual ISO. Observed serial output:

```text
BOOT: stage2
GathaOS booting...
BOOT: protected-mode A20=on
E820: entries=6
VBE: 1024x768x32 pitch=4096
STAGE0: PATTERN READY
STAGE0: PASS
```

The smoke test retrieved the QEMU display through QMP and compared all 786,432
pixels with the expected border, background, six bars and checkerboard. Both
runs passed. The raw QEMU PPM and losslessly encoded PNG are preserved. The
kernel ELF is 32-bit Intel 80386, entry point 0x100000; `llvm-nm` reported no
undefined symbols. xorriso confirmed a BIOS-bootable El Torito fd1.4 entry.

The second clean build produced an ISO identical to the first, byte for byte.
The delivered Windows copy has the same SHA-256:

```text
1bf015e7ce396b67c0a50e4cc96fd1dde923f637f406dcd73da43206441cd60c
```

ISO: `dist/gathaos-stage0-boot.iso`, **1,851,392 bytes** (1.765625 MiB).

Full delivered path:

```text
C:\Users\rafab\Documents\Codex\2026-10-15\leia-o-arquivo-agents-md-na\outputs\gathaos\dist\gathaos-stage0-boot.iso
```

Evidence: [build log](../dist/build-verification.log),
[serial log](../dist/stage0-evidence/serial.log),
[QEMU screenshot](../dist/stage0-evidence/screen.png),
[machine-readable result](../dist/stage0-evidence/result.json),
[El Torito inspection](../dist/stage0-evidence/el-torito.txt).
The JSON deliberately retains the actual temporary test paths; the hash connects
that tested image with the delivered copy. See [file tree](stage-0-files.txt).

## Run and manual checklist

From the repository root, in PowerShell with QEMU on PATH or Ubuntu with a display:

```sh
qemu-system-i386 -cdrom dist/gathaos-stage0-boot.iso -boot d -m 256M -vga std -serial stdio -no-reboot
```

- [ ] Boot the ISO and confirm the serial banner `GathaOS booting...`.
- [ ] Confirm `VBE: 1024x768x32 pitch=4096` and `STAGE0: PASS`.
- [ ] Confirm the navy background and light border are visible.
- [ ] Confirm the six red, amber, green, cyan, blue and purple bars.
- [ ] Confirm the gray / white checkerboard below the bars.
- [ ] Confirm the static screen stays visible until the VM is closed.
- [ ] Report the VM/version and any issue; approve Stage 1 only after testing.

VirtualBox / VMware configuration: legacy BIOS, EFI disabled, 32-bit Other OS,
256 MiB RAM, VGA-compatible graphics (VBoxVGA in VirtualBox), at least 8 MiB video
RAM, 3D disabled, ISO mounted as the boot optical drive. No hard disk is required.
These are proposed manual settings, not claims of testing on those hypervisors.

## Known limits

- Only QEMU with standard VGA and 256 MiB was boot-tested. Real hardware,
  VirtualBox and VMware are unverified.
- The 800x600x32 fallback is implemented but was not boot-tested on firmware
  lacking the preferred mode. UEFI-only boot is not supported.
- The embedded boot image uses a fixed 1.44 MiB floppy layout; the kernel payload
  must fit in 64 KiB. There is no ISO9660 filesystem reader.
- The E820 buffer holds 64 raw firmware entries and fails explicitly on overflow.
- Unsupported VBE/A20/BIOS configurations stop boot rather than trying more drivers.
- There is no keyboard, mouse, timer, exception handling, paging, allocator,
  filesystem, GUI toolkit or application. CPU interrupts stay disabled after boot.
- Graphics are a single static direct-framebuffer test; double buffering comes
  in Stage 3. Empty directory placeholders contain no later-stage implementation.
- Windows/WSL clock skew was seen when building on `/mnt/c`; final warning-free
  builds and tests used WSL's native filesystem.

## Next stage — not started

After explicit user approval, Stage 1 will add the kernel GDT/IDT infrastructure,
ISRs and exception handlers, readable panic output with registers, PIC remapping,
a 100 Hz PIT and expanded serial logging. Its tests will verify timer ticks and
a debug-gated deliberate divide-by-zero. No such work is included here.

Commit message for this completed stage: `Stage 0: boot`.
