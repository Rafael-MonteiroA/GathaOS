"""Pack original boot code into a 1.44 MiB El Torito emulation image."""
import pathlib
import sys


def main():
    action, source, destination = sys.argv[1:]
    source, destination = pathlib.Path(source), pathlib.Path(destination)
    if action == "sizes":
        size = source.stat().st_size
        if not 0 < size <= 448 * 1024:
            raise SystemExit("Kernel exceeds the 448 KiB real-mode staging buffer")
        destination.write_text(
            f"%define KERNEL_BYTES {size}\n%define KERNEL_SECTORS {(size + 511) // 512}\n"
        )
    elif action == "image":
        stage1 = (source / "stage1.bin").read_bytes()
        stage2 = (source / "stage2.bin").read_bytes()
        kernel = (source / "kernel.bin").read_bytes()
        if len(stage1) != 512 or stage1[510:] != b"\x55\xaa":
            raise SystemExit("Invalid stage 1 boot sector")
        if len(stage2) != 8192 or not 0 < len(kernel) <= 448 * 1024:
            raise SystemExit("Invalid stage 2 or kernel size")
        payload = stage1 + stage2 + kernel
        destination.write_bytes(payload.ljust(1440 * 1024, b"\0"))
    else:
        raise SystemExit("Expected sizes or image")


if __name__ == "__main__":
    main()
