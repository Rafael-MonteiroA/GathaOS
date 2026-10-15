"""Boot the actual ISO, verify serial markers and every captured screen pixel."""
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib


def expected_pixel(x, y, width, height):
    if x < 8 or y < 8 or x >= width - 8 or y >= height - 8:
        return 0xE2E8F0
    if height // 4 <= y < height // 2 and width // 8 <= x < width * 7 // 8:
        colors = [0xEF4444, 0xF59E0B, 0x22C55E, 0x06B6D4, 0x3B82F6, 0xA855F7]
        return colors[(x - width // 8) * 6 // (width * 3 // 4)]
    if height * 5 // 8 <= y < height * 7 // 8 and width // 8 <= x < width * 7 // 8:
        return 0xFFFFFF if (x // 32 + y // 32) & 1 else 0x334155
    return 0x142238


def verify_screen(path, width, height):
    with path.open("rb") as stream:
        if stream.readline() != b"P6\n":
            raise RuntimeError("Expected QEMU P6 screenshot")
        if stream.readline().split() != [str(width).encode(), str(height).encode()]:
            raise RuntimeError("Unexpected screen dimensions")
        if stream.readline() != b"255\n":
            raise RuntimeError("Unexpected screen depth")
        pixels = stream.read()
    expected = bytearray()
    for y in range(height):
        for x in range(width):
            rgb = expected_pixel(x, y, width, height)
            expected.extend(rgb.to_bytes(3, "big"))
    if pixels != expected:
        raise RuntimeError("Screen pixels differ from the Stage 0 test pattern")
    # Encode the unmodified QEMU screenshot as PNG using only the Python stdlib.
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
            ">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    path.with_suffix(".png").write_bytes(png)


def main():
    iso = Path(sys.argv[1]).resolve()
    evidence = Path("build/test").resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    serial = evidence / "serial.log"
    serial.unlink(missing_ok=True)
    qemu = os.environ.get("QEMU", "qemu-system-i386")
    with tempfile.TemporaryDirectory(prefix="gatha-qmp-") as temporary:
        endpoint = str(Path(temporary) / "qmp.sock")
        command = [qemu, "-machine", "pc,accel=tcg", "-m", "256M", "-vga", "std",
                   "-cdrom", str(iso), "-boot", "d", "-display", "none", "-monitor", "none",
                   "-serial", f"file:{serial}", "-qmp", f"unix:{endpoint},server=on,wait=off",
                   "-no-reboot", "-no-shutdown", "-nic", "none"]
        with (evidence / "qemu.log").open("w") as log:
            process = subprocess.Popen(command, stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 30
                while time.monotonic() < deadline:
                    output = serial.read_text() if serial.exists() else ""
                    if "ERROR" in output or "FAIL" in output:
                        raise RuntimeError(output)
                    if "STAGE0: PASS" in output:
                        break
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited before boot completed")
                    time.sleep(0.1)
                else:
                    raise RuntimeError(f"Boot timed out; serial output:\n{output}")
                for marker in ["BOOT: stage2", "GathaOS booting...", "BOOT: protected-mode A20=on",
                               "STAGE0: PATTERN READY", "STAGE0: PASS"]:
                    if marker not in output:
                        raise RuntimeError(f"Missing serial marker: {marker}")
                if not re.search(r"E820: entries=([1-9]|[1-5][0-9]|6[0-4])\n", output):
                    raise RuntimeError("Missing valid E820 count")
                mode = re.search(r"VBE: (1024x768|800x600)x32 pitch=(\d+)", output)
                if not mode:
                    raise RuntimeError("Missing supported VBE mode")
                width, height = map(int, mode[1].split("x"))
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(5)
                    client.connect(endpoint)
                    stream = client.makefile("rwb", buffering=0)
                    json.loads(stream.readline())
                    def request(name, arguments=None):
                        stream.write((json.dumps({"execute": name, "arguments": arguments or {}}) + "\n").encode())
                        while True:
                            response = json.loads(stream.readline())
                            if "error" in response:
                                raise RuntimeError(response)
                            if "return" in response:
                                return response["return"]
                    request("qmp_capabilities")
                    screenshot = evidence / "screen.ppm"
                    request("screendump", {"filename": str(screenshot)})
                    verify_screen(screenshot, width, height)
                    request("quit")
                process.wait(timeout=5)
                result = {"result": "PASS", "iso": str(iso),
                          "sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
                          "resolution": [width, height], "pixels_checked": width * height,
                          "qemu": subprocess.check_output([qemu, "--version"], text=True).splitlines()[0],
                          "command": command, "serial": output}
                (evidence / "result.json").write_text(json.dumps(result, indent=2) + "\n")
                print(output, end="")
                print(f"SMOKE PASS: ISO boot and {width * height:,} screen pixels verified")
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


if __name__ == "__main__":
    main()
