"""Verify guest-requested shutdown and reset, including disk flush and second boot."""
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from desktop_test import Desktop
from apps_test import launch
from mkgathafs import validate

def run(iso,seed,mode):
    evidence=Path('build/test-power-'+mode).resolve();evidence.mkdir(parents=True,exist_ok=True)
    disk=evidence/'disk.img';shutil.copyfile(seed,disk)
    serial=evidence/'serial.log';serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='gatha-power-') as temp:
        endpoint=str(Path(temp)/'qmp.sock')
        cmd=[os.environ.get('QEMU','qemu-system-i386'),'-machine','pc,accel=tcg','-m','256M','-vga','std','-cdrom',str(iso),'-boot','d','-display','none','-monitor','none','-serial','file:'+str(serial),'-qmp','unix:'+endpoint+',server=on,wait=off','-drive',f'file={disk},format=raw,if=ide,index=0','-nic','none']
        with (evidence/'qemu.log').open('w') as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=log)
            try:
                deadline=time.monotonic()+90
                while time.monotonic()<deadline:
                    if serial.exists() and 'STAGE7: PASS' in serial.read_text():break
                    if process.poll() is not None:raise AssertionError('QEMU ended during boot')
                    time.sleep(.1)
                else:raise AssertionError('Boot timeout')
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(8);client.connect(endpoint);stream=client.makefile('rwb',buffering=0);json.loads(stream.readline())
                    def request(name,args=None):
                        stream.write((json.dumps({'execute':name,'arguments':args or {}})+'\n').encode())
                        while True:
                            response=json.loads(stream.readline())
                            if 'error' in response:raise RuntimeError(response)
                            if 'return' in response:return response['return']
                    request('qmp_capabilities');d=Desktop(request,evidence,serial,1024,768)
                    launch(d,0);d.type(mode+'\n')
                    if mode=='reboot':
                        d.wait(lambda text:text.count('STAGE7: PASS')>=2,'second guest boot after reboot')
                        assert serial.read_text().count('GathaOS booting...')==2
                        d.screen('rebooted')
                    else:
                        d.wait(lambda text:'POWER: halted' in text,'clean halt')
                        registers=request('human-monitor-command',{'command-line':'info registers'})
                        assert 'HLT=1' in registers
                        (evidence/'registers.txt').write_text(registers);d.screen('shutdown')
                    assert 'POWER: filesystem flushed' in serial.read_text()
                    request('quit');process.wait(timeout=5)
                validate(disk.read_bytes())
                (evidence/'result.json').write_text(json.dumps({'result':'PASS','mode':mode,'command':cmd,'boot_count':serial.read_text().count('GathaOS booting...')},indent=2)+'\n')
                print('POWER PASS: '+mode)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait()

if __name__=='__main__':
    for mode in ('shutdown','reboot'):run(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve(),mode)
