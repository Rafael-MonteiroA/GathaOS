"""Boot Stage 3 ISOs; verify memory, input, graphics and exception regressions."""
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
import shutil
from persistence_test import verify as verify_persistence
from input_test import exercise


def expected_pixel(x, y, width, height):
    if x < 8 or y < 8 or x >= width - 8 or y >= height - 8:
        return 0xE2E8F0
    if height // 4 <= y < height // 2 and width // 8 <= x < width * 7 // 8:
        colors = [0xEF4444, 0xF59E0B, 0x22C55E, 0x06B6D4, 0x3B82F6, 0xA855F7]
        return colors[(x - width // 8) * 6 // (width * 3 // 4)]
    if height * 5 // 8 <= y < height * 7 // 8 and width // 8 <= x < width * 7 // 8:
        return 0xFFFFFF if (x // 32 + y // 32) & 1 else 0x334155
    return 0x142238


def panic_pixels(output, width, height):
    font = {}
    source = Path("kernel/panic_font.h").read_text()
    for character, values in re.findall(r"\['(.)'\]\s*=\s*\{([0-9,]+)\}", source):
        font[character] = list(map(int, values.split(",")))
    if len(font) != 40:
        raise RuntimeError("Unexpected panic font table")
    lines = output.split("GATHAOS KERNEL PANIC\n", 1)[1].split("PANIC: HALTED", 1)[0].splitlines()
    lines.insert(0, "GATHAOS KERNEL PANIC")
    pixels = bytearray()
    for y in range(height):
        pixels.extend(bytes.fromhex("991b1b" if y < 64 else "200e16") * width)
    for number, line in enumerate(lines):
        y = 24 if number == 0 else 84 + (number - 1) * 32
        for column, character in enumerate(line.upper()):
            for gy, bits in enumerate(font.get(character, [0] * 7)):
                for gx in range(5):
                    if not bits & (1 << (4 - gx)):
                        continue
                    for dy in range(2):
                        for dx in range(2):
                            px, py = 32 + column * 12 + gx * 2 + dx, y + gy * 2 + dy
                            if px >= width or py >= height:
                                raise RuntimeError("Panic text is clipped")
                            index = (py * width + px) * 3
                            pixels[index:index + 3] = b"\xf8\xfa\xfc"
    return pixels


def verify_screen(path, width, height, output, fault):
    with path.open("rb") as stream:
        if stream.readline() != b"P6\n":
            raise RuntimeError("Expected QEMU P6 screenshot")
        if stream.readline().split() != [str(width).encode(), str(height).encode()]:
            raise RuntimeError("Unexpected screen dimensions")
        if stream.readline() != b"255\n":
            raise RuntimeError("Unexpected screen depth")
        pixels = stream.read()
    if fault == "none":
        expected = bytearray()
        for y in range(height):
            for x in range(width):
                rgb = expected_pixel(x, y, width, height)
                expected.extend(rgb.to_bytes(3, "big"))
    else:
        expected = panic_pixels(output, width, height)
    if pixels != expected:
        raise RuntimeError("Displayed pixels differ from the expected pattern or panic text")
    # Encode the unmodified QEMU screenshot as PNG using only the Python stdlib.
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(
            ">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    path.with_suffix(".png").write_bytes(png)


def verify_frame(output, fault, elf):
    symbols = subprocess.check_output(["llvm-nm", "--defined-only", str(elf)], text=True)
    if fault == "none":
        if "debug_trigger_" in symbols:
            raise RuntimeError("Normal kernel contains deliberate fault code")
        return {}
    fields = {name: int(value, 16) for name, value in re.findall(r"\b([A-Z0-9]+)=0x([0-9A-F]{8})", output)}
    expected = {"VECTOR": {"div0": 0, "gp": 13, "page": 14, "readonly": 14}[fault],
                "ERROR": {"div0": 0, "gp": 0x38, "page": 2, "readonly": 3}[fault],
                "CS": 8, "SS": 16, "DS": 16, "ES": 16, "FS": 16, "GS": 16}
    symbol = {"div0": "debug_divide_instruction", "gp": "debug_gp_instruction",
              "page": "debug_page_instruction", "readonly": "debug_readonly_instruction"}[fault]
    match = re.search(r"^([0-9a-fA-F]+) [A-Za-z] " + symbol + r"$", symbols, re.M)
    if not match:
        raise RuntimeError("Missing expected fault instruction symbol")
    expected["EIP"] = int(match[1], 16)
    if fault == "div0":
        expected.update(EAX=0x13579BDF, EBX=0x2468ACE0, ECX=0, EDX=0,
                        ESI=0x11223344, EDI=0x55667788, EBP=0x99AABBCC)
    elif fault == "gp":
        expected["EAX"] = 0x38
    elif fault == "page":
        expected["CR2"] = 0
    elif fault == "readonly":
        word = re.search(r"^([0-9a-fA-F]+) [A-Za-z] debug_readonly_word$", symbols, re.M)
        if not word:
            raise RuntimeError("Missing read-only fault target")
        expected["CR2"] = int(word[1], 16)
    for name, value in expected.items():
        if fields.get(name) != value:
            raise RuntimeError(f"Bad saved register {name}: {fields.get(name)} != {value}")
    flags_mask = 0x601 if fault == "div0" else 0x200
    if fields.get("EFLAGS", 0) & flags_mask != flags_mask:
        raise RuntimeError("Interrupted flags were not saved correctly")
    if not 0xC0080000 <= fields.get("ESP", 0) < 0xC0090000 or fields["ESP"] & 3:
        raise RuntimeError("Bad interrupted ESP")
    if fields.get("CR0", 0) & 0x80010001 != 0x80010001 or fields["EIP"] < 0xC0000000:
        raise RuntimeError("Expected higher-half protected mode with paging and write protection")
    return fields


def main():
    iso = Path(sys.argv[1]).resolve()
    fault = sys.argv[2]
    elf = Path(sys.argv[3])
    ram = int(sys.argv[4])
    if fault not in ("none", "div0", "gp", "page", "readonly"):
        raise SystemExit("Invalid fault test mode")
    scan_set = int(os.environ.get("PS2_SET", "2"))
    evidence = Path(f"build/test-{fault}-{ram}-set{scan_set}").resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / "result.json").unlink(missing_ok=True)
    serial = evidence / "serial.log"
    serial.unlink(missing_ok=True)
    qemu = os.environ.get("QEMU", "qemu-system-i386")
    disk = evidence / 'disk.img'
    if fault == 'none': shutil.copyfile(os.environ.get('DISK','dist/gathaos-stage5-disk.img'),disk)
    with tempfile.TemporaryDirectory(prefix="gatha-qmp-") as temporary:
        endpoint = str(Path(temporary) / "qmp.sock")
        command = [qemu, "-machine", "pc,accel=tcg", "-m", f"{ram}M", "-vga", "std",
                   "-cdrom", str(iso), "-boot", "d", "-display", "none", "-monitor", "none",
                   "-serial", f"file:{serial}", "-qmp", f"unix:{endpoint},server=on,wait=off",
                   "-no-reboot", "-no-shutdown", "-nic", "none"]
        if fault == 'none': command += ['-drive',f'file={disk},format=raw,if=ide,index=0']
        with (evidence / "qemu.log").open("w") as log:
            process = subprocess.Popen(command, stdout=log, stderr=log)
            try:
                tick_times = {}
                deadline = time.monotonic() + 90
                while time.monotonic() < deadline:
                    output = serial.read_text() if serial.exists() else ""
                    if "BOOT ERROR" in output or "FAIL" in output or (fault == "none" and "PANIC" in output):
                        raise RuntimeError(output)
                    for value in re.findall(r"PIT: ticks=(\d+)\n", output):
                        tick_times.setdefault(int(value), time.monotonic())
                    if (fault == "none" and len(tick_times) >= 5) or (fault != "none" and "PANIC: HALTED" in output):
                        break
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited before boot completed")
                    time.sleep(0.1)
                else:
                    raise RuntimeError(f"Boot timed out; serial output:\n{output}")
                for marker in ["BOOT: stage2", "BOOT: CD EDD kernel CRC32 PASS", "GathaOS booting...", "BOOT: protected-mode A20=on",
                               "GDT: kernel-owned CS=0x08 DS=0x10",
                               "IDT: 256 ring-0 interrupt gates installed",
                               "PIC: IRQ0-15 remapped to 32-47; only IRQ0 unmasked",
                               "PIT: channel=0 mode=2 divisor=11932 target_hz=100",
                               "IRQ: register-and-flags preservation PASS", "STAGE2: TIMER RUNNING",
                               "PAGING: higher_half=0xC0000000 identity=off null=unmapped wp=on",
                               "HEAP: base=0xE0000000 limit_mib=16 alignment=16 initial_bytes=65536",
                               "MMTEST: E820 rounding reservations overflow clipping PASS",
                               "MMTEST: paging aliases protections unmap PASS",
                               "MMTEST: heap OOM partial-growth rollback PASS",
                               "MMTEST: heap mixed_operations=4096 alignment content coalescing PASS",
                               "MMTEST: heap exhaustion_blocks=255 trim_and_reuse PASS", "MMTEST: ALL PASS"]:
                    if marker not in output:
                        raise RuntimeError(f"Missing serial marker: {marker}")
                expected_ticks = [100, 200, 300, 400, 500] if fault == "none" else [100, 200]
                if [t // 100 * 100 for t in tick_times] != expected_ticks or any(t % 100 > 30 for t in tick_times):
                    raise RuntimeError(f"Unexpected tick progression: {list(tick_times)}")
                if fault == "none":
                    if "STAGE3: PASS" not in output or not 2.5 < list(tick_times.values())[-1] - list(tick_times.values())[0] < 12:
                        raise RuntimeError("Timer cadence or success marker is invalid")
                else:
                    reason = {"div0": "DIVIDE ERROR", "gp": "GENERAL PROTECTION",
                              "page": "PAGE FAULT", "readonly": "PAGE FAULT"}[fault]
                    if "PANIC: " + reason not in output or "STAGE2: PASS" in output:
                        raise RuntimeError("Unexpected exception path")
                pmm = re.search(r"PMM: managed_limit_mib=512 usable_frames=(\d+) free_frames=(\d+) reserved_end=0x([0-9A-F]+)", output)
                exhaustion = re.search(r"MMTEST: pmm exhaustion_frames=(\d+) restored_frames=(\d+) PASS", output)
                final = re.search(r"MMTEST: final_free_frames=(\d+) heap_bytes=65536 PASS", output)
                if not pmm or not exhaustion or not final:
                    raise RuntimeError("Incomplete memory accounting")
                total = int(pmm[1])
                reserved = int(pmm[3], 16)
                # QEMU's PC E820 reserves the final 128 KiB of RAM for ACPI/BIOS.
                expected_total = (min(ram, 512) * 1024 * 1024 - 128 * 1024 - reserved) // 4096
                if total != expected_total or total != int(pmm[2]):
                    raise RuntimeError(f"Unexpected usable RAM count: {total} != {expected_total}")
                if not (int(exhaustion[1]) == int(exhaustion[2]) == int(final[1]) == total - 16):
                    raise RuntimeError("PMM or heap leaked frames")
                fields = verify_frame(output, fault, elf)
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
                    input_result = {}
                    if fault == "none":
                        from apps_test import exercise_apps
                        input_result = exercise_apps(request, evidence, serial, width, height, scan_set)
                        output = serial.read_text()
                    else:
                        screenshot = evidence / "screen.ppm"
                        request("screendump", {"filename": str(screenshot)})
                        verify_screen(screenshot, width, height, output, fault)
                    registers = request("human-monitor-command", {"command-line": "info registers"})
                    (evidence / "cpu-registers.txt").write_text(registers)
                    cr0 = re.search(r"CR0=([0-9a-fA-F]+)", registers)
                    eip = re.search(r"EIP=([0-9a-fA-F]+)", registers)
                    esp = re.search(r"ESP=([0-9a-fA-F]+)", registers)
                    if not cr0 or int(cr0[1], 16) & 0x80010001 != 0x80010001:
                        raise RuntimeError("Hardware paging/write protection is not active")
                    if not eip or not esp:
                        raise RuntimeError("Missing CPU addresses")
                    addresses = [int(eip[1],16),int(esp[1],16)]
                    if not (min(addresses) >= 0xC0000000 or
                            (fault == 'none' and all(0x400000 <= v <= 0xc000000 for v in addresses) and
                             re.search(r'CS\s*=001b',registers))):
                        raise RuntimeError("CPU is outside expected kernel/user address spaces")
                    if fault != "none":
                        time.sleep(0.3)
                        if serial.read_text() != output or "HLT=1" not in registers:
                            raise RuntimeError("Panic did not leave the CPU halted")
                        live_flags = re.search(r"EFL=([0-9a-fA-F]+)", registers)
                        if not live_flags or int(live_flags[1], 16) & 0x200:
                            raise RuntimeError("Panic left IRQs enabled")
                    request("quit")
                process.wait(timeout=5)
                persistence = {}
                if fault == 'none':
                    if 'USERFS: all file syscalls PASS' not in output or 'STAGE5: DISK PASS' not in output:
                        raise RuntimeError('Missing filesystem syscall results')
                    persistence = verify_persistence(qemu,iso,disk,evidence,ram)
                    from apps_test import verify_disk
                    input_result['disk_verification']=verify_disk(disk)
                result = {"result": "PASS", "mode": fault, "ram_mib": ram, "iso": str(iso),
                          "sha256": hashlib.sha256(iso.read_bytes()).hexdigest(),
                          "resolution": [width, height], "pixels_checked": width * height,
                          "input_test": input_result,
                          "persistence_test": persistence,
                          "qemu": subprocess.check_output([qemu, "--version"], text=True).splitlines()[0],
                          "command": command, "serial": output, "exception_registers": fields,
                          "usable_frames": total, "final_free_frames": int(final[1]),
                          "tick_arrival_seconds": {str(t): round(v - next(iter(tick_times.values())), 3) for t, v in tick_times.items()}}
                (evidence / "result.json").write_text(json.dumps(result, indent=2) + "\n")
                print('\n'.join(line for line in output.splitlines() if not line.startswith('INPUT:')), end="\n")
                print(f"SMOKE PASS ({fault}, {ram} MiB, set {scan_set}): memory, IRQs, display and input/exception checks")
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
