"""Real CD boots: multi-MiB kernel/BSS, maximum payload and rejected images."""
import array
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import mkboot


def command(*args):
    return subprocess.check_output(list(map(str, args)), text=True, stderr=subprocess.STDOUT)


def iso_offset(data):
    # The boot catalog records 2048-byte CD LBAs, but load count is in 512s.
    assert data[17*2048+7:17*2048+30] == b"EL TORITO SPECIFICATION"
    catalog = struct.unpack_from("<I", data, 17*2048+71)[0] * 2048
    assert data[catalog:catalog+2] == b"\x01\x00"
    assert sum(struct.unpack_from("<16H", data, catalog)) & 0xffff == 0
    assert data[catalog+30:catalog+34] == b"\x55\xaa\x88\x00"
    assert struct.unpack_from("<H", data, catalog+38)[0] == 4
    return struct.unpack_from("<I", data, catalog+40)[0] * 2048


def check_iso(path, kernel):
    data = path.read_bytes()
    start = iso_offset(data)
    pvd, lba, size, checksum = struct.unpack_from("<4I", data, start+8)
    assert pvd == 16 and lba*2048 == start
    assert size == 10240 + (len(kernel)+2047)//2048*2048
    payload = data[start+64:start+size]
    assert sum(struct.unpack("<"+"I"*(len(payload)//4), payload)) & 0xffffffff == checksum
    assert data[start+10240:start+10240+len(kernel)] == kernel
    return start


def make_iso(folder, kernel, memory_end):
    folder.mkdir(parents=True, exist_ok=True)
    (folder/"kernel-size.inc").write_text(mkboot.sizes(kernel, memory_end))
    command("nasm", "-Wall", "-Werror", "-I"+str(folder)+"/", "-f", "bin",
            "boot/stage2.asm", "-o", folder/"stage2.bin")
    (folder/"boot.img").write_bytes(mkboot.image(
        Path("build/none/stage1.bin").read_bytes(),
        (folder/"stage2.bin").read_bytes(), kernel))
    iso = folder/"boot.iso"
    command("bash", "scripts/make-iso.sh", folder/"boot.img", iso, folder)
    check_iso(iso, kernel)
    return iso


def boot(iso, seed, name, marker, ram=64, failure=False, extra=()):
    evidence = Path("build/test-boot-"+name).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    serial = evidence/"serial.log"
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix="gatha-boot-") as temp:
        endpoint = str(Path(temp)/"qmp.sock")
        disk = Path(temp)/"disk.img"
        shutil.copyfile(seed, disk)
        cmd = [os.environ.get("QEMU", "qemu-system-i386"), "-machine", "pc,accel=tcg",
               "-m", str(ram), "-vga", "std", "-cdrom", str(iso), "-boot", "d",
               "-display", "none", "-monitor", "none", "-no-reboot",
               "-serial", "file:"+str(serial), "-qmp", "unix:"+endpoint+",server=on,wait=off",
               "-drive", f"file={disk},format=raw,if=ide,index=0", "-nic", "none"]
        with (evidence/"qemu.log").open("w") as log:
            process = subprocess.Popen(cmd, stdout=log, stderr=log)
            try:
                deadline = time.monotonic()+90
                while time.monotonic() < deadline:
                    output = serial.read_text() if serial.exists() else ""
                    assert "FAIL" not in output and "PANIC" not in output, output
                    if marker in output:
                        break
                    assert process.poll() is None, output
                    time.sleep(.05)
                else:
                    raise AssertionError("Boot timeout: "+output)
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(5)
                    client.connect(endpoint)
                    stream = client.makefile("rwb", buffering=0)
                    json.loads(stream.readline())
                    def request(name, arguments=None):
                        stream.write((json.dumps({"execute": name, "arguments": arguments or {}})+"\n").encode())
                        while True:
                            response = json.loads(stream.readline())
                            assert "error" not in response, response
                            if "return" in response:
                                return response["return"]
                    request("qmp_capabilities")
                    if failure:
                        time.sleep(.25)
                        registers = request("human-monitor-command", {"command-line": "info registers"})
                        assert "HLT=1" in registers and "GathaOS booting..." not in output
                        assert serial.read_text() == output
                        (evidence/"registers.txt").write_text(registers)
                    else:
                        assert all(item in output for item in extra), output
                        assert "BOOT: CD EDD kernel CRC32 PASS" in output
                        request("screendump", {"filename": str(evidence/"screen.ppm")})
                    request("quit")
                    process.wait(timeout=5)
                result = {"result": "PASS", "case": name, "ram_mib": ram,
                          "rejected": failure, "command": cmd, "marker": marker,
                          "iso_sha256": hashlib.sha256(iso.read_bytes()).hexdigest()}
                (evidence/"result.json").write_text(json.dumps(result, indent=2)+"\n")
                print("BOOT LOADER PASS: "+name, flush=True)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def host_bounds():
    for size, end in ((0, 0x101000), (mkboot.MAX_KERNEL+1, 0x1000000),
                      (8192, 0x101000), (512, 0x100001), (512, 0x1001000)):
        try:
            mkboot.validate_size(size, end)
        except ValueError:
            pass
        else:
            raise AssertionError("Invalid kernel bounds accepted")
    mkboot.validate_size(mkboot.MAX_KERNEL, mkboot.MEMORY_LIMIT)
    print("BOOT HOST PASS: empty, oversized, BSS, alignment and maximum bounds", flush=True)


def large_kernel(folder):
    folder.mkdir(parents=True, exist_ok=True)
    data = array.array("I", (i ^ 0x735a91c6 for i in range(6*1024*1024//4)))
    if sys.byteorder != "little":
        data.byteswap()
    (folder/"data.bin").write_bytes(data.tobytes())
    (folder/"data.asm").write_text(
        'section .rodata align=4096\nglobal boot_probe_data\nboot_probe_data: incbin "'+
        str(folder/"data.bin")+'"\nsection .bss nobits align=4096\n'
        'global boot_probe_bss\nboot_probe_bss: resb 5*1024*1024\n'
        'section .note.GNU-stack noalloc noexec nowrite progbits\n')
    command("nasm", "-Wall", "-Werror", "-f", "elf32", folder/"data.asm", "-o", folder/"data.o")
    command("clang", "--target=i686-elf", "-std=c11", "-O2", "-ffreestanding",
            "-fno-builtin", "-fno-stack-protector", "-fno-pic", "-fno-pie", "-mno-sse",
            "-mno-sse2", "-mno-mmx", "-Wall", "-Wextra", "-Werror", "-Ikernel",
            "-c", "tools/boot_probe.c", "-o", folder/"probe.o")
    command("llvm-objcopy", "--redefine-sym", "kernel_main=normal_kernel_main",
            "build/none/kernel/main.o", folder/"main.o")
    objects = command("make", "-s", "--no-print-directory",
                      "--eval=print-objects:;@echo $(OBJECTS)", "print-objects").split()
    objects = [str(folder/"main.o") if o == "build/none/kernel/main.o" else o for o in objects]
    command("ld.lld", "-m", "elf_i386", "--fatal-warnings", "-T", "kernel/linker.ld",
            "-Map="+str(folder/"kernel.map"), "-o", folder/"kernel.elf",
            *objects, folder/"probe.o", folder/"data.o")
    command("llvm-objcopy", "-O", "binary", folder/"kernel.elf", folder/"kernel.bin")
    symbols = command("llvm-nm", "--defined-only", folder/"kernel.elf")
    end = int(re.search(r"([0-9a-f]+) . __kernel_phys_end", symbols)[1], 16)
    kernel = (folder/"kernel.bin").read_bytes()
    assert len(kernel) > 6*1024*1024 and end > 12*1024*1024
    return make_iso(folder, kernel, end), len(kernel), end


def main():
    iso, seed = (Path(p).resolve() for p in sys.argv[1:])
    host_bounds()
    normal = Path("build/none/kernel.bin").read_bytes()
    start = check_iso(iso, normal)
    work = Path("build/boot-loader").resolve()
    work.mkdir(parents=True, exist_ok=True)
    large, size, end = large_kernel(work/"large")
    boot(large, seed, "large-kernel", "STAGE7: PASS",
         extra=("BOOTTEST: 6 MiB data and 5 MiB BSS PASS",))
    boot(large, seed, "insufficient-ram", "BOOT ERROR: kernel memory unavailable", ram=8, failure=True)

    # A stand-alone payload tests exact transfer bounds without a kernel/BSS gap.
    (work/"stub.asm").write_text(
        'bits 32\norg 0x100000\nmov esi, message\n.next: lodsb\ntest al, al\n'
        'jz .halt\nmov ah, al\nmov dx, 0x3fd\n.wait: in al, dx\n'
        'test al, 0x20\njz .wait\nmov al, ah\nmov dx, 0x3f8\nout dx, al\n'
        'jmp .next\n.halt: cli\nhlt\njmp .halt\nmessage: db "BOOTTEST: payload entered",13,10,0\n')
    command("nasm", "-Wall", "-Werror", "-f", "bin", work/"stub.asm", "-o", work/"stub.bin")
    stub = (work/"stub.bin").read_bytes()
    for name, length in (("chunk-tail", 1024*1024+1), ("maximum", mkboot.MAX_KERNEL)):
        payload = (stub + bytes(range(256))*((length-len(stub)+255)//256))[:length]
        bound = (0x100000+length+4095)//4096*4096
        candidate = make_iso(work/name, payload, bound)
        boot(candidate, seed, name, "BOOTTEST: payload entered")

    original = bytearray(iso.read_bytes())
    for name, at in (("corrupt-first", 0), ("corrupt-chunk-edge", 32768),
                     ("corrupt-last", len(normal)-1)):
        data = original.copy()
        data[start+10240+at] ^= 0x40
        candidate = work/(name+".iso")
        candidate.write_bytes(data)
        boot(candidate, seed, name, "BOOT ERROR: kernel checksum", failure=True)
    bad = original.copy()
    struct.pack_into("<I", bad, start+16, 10240)
    candidate = work/"bad-metadata.iso"
    candidate.write_bytes(bad)
    boot(candidate, seed, "bad-metadata", "BOOT ERROR: CD image metadata", failure=True)
    candidate = work/"truncated.iso"
    candidate.write_bytes(original[:start+10240+2048])
    boot(candidate, seed, "truncated", "BOOT ERROR: kernel read", failure=True)
    # Stage 1 must follow the info table, independent of ISO mastering placement.
    relocated = work/"relocated"
    relocated.mkdir()
    (relocated/"iso").mkdir()
    # Boot images sort ahead of ordinary files. Extra directory metadata
    # shifts their starting LBA even when xorriso gives them highest priority.
    for index in range(128):
        (relocated/"iso"/f"extra-directory-{index:03}").mkdir()
    command("bash", "scripts/make-iso.sh", "build/none/boot.img",
            relocated/"boot.iso", relocated)
    relocated_start = check_iso(relocated/"boot.iso", normal)
    assert relocated_start != start, "Placement test did not move the boot image"
    boot(relocated/"boot.iso", seed, "relocated", "STAGE7: PASS")
    (Path("build/test-boot-large-kernel")/"sizes.json").write_text(
        json.dumps({"kernel_bytes": size, "kernel_memory_end": end,
                    "max_payload_bytes": mkboot.MAX_KERNEL,
                    "original_iso_lba": start//2048, "relocated_iso_lba": relocated_start//2048}, indent=2)+"\n")


if __name__ == "__main__":
    main()
