"""Pack the original loader and kernel for an El Torito no-emulation CD."""
import pathlib
import subprocess
import sys
import zlib

SECTOR = 2048
KERNEL_BASE = 0x100000
MEMORY_LIMIT = 0x1000000
MAX_KERNEL = MEMORY_LIMIT - KERNEL_BASE


def validate_size(size, memory_end):
    if not 0 < size <= MAX_KERNEL:
        raise ValueError("Kernel payload must be between 1 byte and 15 MiB")
    if not KERNEL_BASE + size <= memory_end <= MEMORY_LIMIT or memory_end % 4096:
        raise ValueError("Kernel/BSS must fit the 16 MiB bootstrap mapping")


def sizes(kernel, memory_end):
    validate_size(len(kernel), memory_end)
    return (f"%define KERNEL_BYTES {len(kernel)}\n"
            f"%define KERNEL_SECTORS {(len(kernel) + SECTOR - 1) // SECTOR}\n"
            f"%define KERNEL_MEMORY_END {memory_end}\n"
            f"%define KERNEL_CRC32 0x{zlib.crc32(kernel):08x}\n")


def image(stage1, stage2, kernel):
    if len(stage1) != 512 or stage1[510:] != b"\x55\xaa":
        raise ValueError("Invalid stage 1 boot sector")
    if any(stage1[8:64]):
        raise ValueError("Boot info table must be zero before ISO mastering")
    if len(stage2) != 8192 or not 0 < len(kernel) <= MAX_KERNEL:
        raise ValueError("Invalid stage 2 or kernel size")
    payload = stage1.ljust(SECTOR, b"\0") + stage2 + kernel
    return payload.ljust((len(payload) + SECTOR - 1) // SECTOR * SECTOR, b"\0")


def main():
    action, source, destination = sys.argv[1:]
    source, destination = pathlib.Path(source), pathlib.Path(destination)
    if action == "sizes":
        symbols = {parts[2]: int(parts[0], 16) for line in subprocess.check_output(
            ["llvm-nm", "--defined-only", str(source.with_suffix(".elf"))],
            text=True).splitlines() if len(parts := line.split()) == 3}
        destination.write_text(sizes(source.read_bytes(), symbols["__kernel_phys_end"]))
    elif action == "image":
        destination.write_bytes(image(*((source / name).read_bytes() for name in
                                       ("stage1.bin", "stage2.bin", "kernel.bin"))))
    else:
        raise ValueError("Expected sizes or image")


if __name__ == "__main__":
    main()
