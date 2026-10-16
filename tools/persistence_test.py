"""Restart a fresh QEMU process on the same written disk and verify its bytes."""
from pathlib import Path
import subprocess
import time
from mkgathafs import validate, read_file

def verify(qemu,iso,disk,evidence,ram):
    serial = evidence/'restart-serial.log'
    serial.unlink(missing_ok=True)
    command = [qemu,'-machine','pc,accel=tcg','-m',f'{ram}M','-vga','std',
        '-cdrom',str(iso),'-boot','d','-display','none','-monitor','none',
        '-serial',f'file:{serial}','-drive',f'file={disk},format=raw,if=ide,index=0',
        '-no-reboot','-no-shutdown','-nic','none']
    with (evidence/'restart-qemu.log').open('w') as log:
        process = subprocess.Popen(command,stdout=log,stderr=log)
        try:
            deadline = time.monotonic()+90
            while time.monotonic() < deadline:
                output = serial.read_text() if serial.exists() else ''
                if 'PANIC' in output or 'FAIL' in output: raise RuntimeError(output)
                if 'STAGE4: PASS' in output and 'USERFS: all file syscalls PASS' in output: break
                if process.poll() is not None: raise RuntimeError('Restart QEMU exited early')
                time.sleep(0.1)
            else: raise RuntimeError('Persistence restart timed out: '+output)
            assert 'FSTEST: persisted data verified after restart PASS' in output
        finally:
            process.terminate()
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired: process.kill(); process.wait()
    image = disk.read_bytes(); validate(image)
    assert read_file(image,'/persist.bin') == bytes((i*37+11)&255 for i in range(9000))
    print('PERSISTENCE PASS: new QEMU process, guest readback and independent host disk verification')
    return {'result':'PASS','bytes_verified':9000,'restart_command':command}
