"""Ten minutes of real QEMU uptime, UI use, leak and idle-damage checks."""
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from desktop_test import Desktop
from apps_test import launch,click,field
from mkgathafs import validate,read_file

def run(iso,seed):
    evidence=Path('build/test-soak').resolve();evidence.mkdir(parents=True,exist_ok=True)
    disk=evidence/'disk.img';shutil.copyfile(seed,disk)
    serial=evidence/'serial.log';serial.unlink(missing_ok=True)
    samples=[]
    with tempfile.TemporaryDirectory(prefix='gatha-soak-') as temp:
        endpoint=str(Path(temp)/'qmp.sock')
        cmd=[os.environ.get('QEMU','qemu-system-i386'),'-machine','pc,accel=tcg','-m','256M','-vga','std','-cdrom',str(iso),'-boot','d','-display','none','-monitor','none','-serial','file:'+str(serial),'-qmp','unix:'+endpoint+',server=on,wait=off','-drive',f'file={disk},format=raw,if=ide,index=0','-nic','none']
        with (evidence/'qemu.log').open('w') as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=log)
            try:
                deadline=time.monotonic()+90
                while time.monotonic()<deadline:
                    if serial.exists() and 'STAGE8: PASS' in serial.read_text():break
                    if process.poll() is not None:raise AssertionError('Boot failed')
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
                    d.screen('welcome')
                    for w in reversed(list(d.windows().values())):d.close(w)
                    time.sleep(5)
                    def health():
                        values=re.findall(r'HEALTH: windows=(\d+) free=(\d+) heap=(\d+) frames=(\d+) pixels=(\d+) drops=(\d+)',d.log())
                        assert values,'Missing health telemetry'
                        return dict(zip(('windows','free','heap','frames','pixels','drops'),map(int,values[-1])))
                    baseline=health();assert baseline['windows']==0
                    start=time.monotonic();cycle=0
                    while time.monotonic()-start<600:
                        w=launch(d,2)
                        field(d,w,120,50,'/Documents/Soak.txt')
                        field(d,w,100,100,'GathaOS stability check')
                        d.key('ctrl',True);d.tap('s');d.key('ctrl',False)
                        click(d,w,120,14)
                        d.move(4,4);d.text_pixels(w['x']+22,w['y']+118,'GathaOS stability check','soak-editor')
                        c=launch(d,3);d.type('12.5+7.5=')
                        d.close(c);d.close(w)
                        w=launch(d,7)
                        x,y=w['x'],w['y'];d.move(x+90,y+15);d.button(True);d.move(x+170,y+65);d.button(False)
                        # QMP acknowledges injection before the guest consumes the release packet.
                        # Observe the completed move rather than racing the serial log.
                        d.wait(lambda _: (d.windows()[w['id']]['x'],d.windows()[w['id']]['y'])==(x+80,y+50), 'completed drag to expected coordinates')
                        w=d.windows()[w['id']]
                        d.click(w['x']+w['w']-46,w['y']+14)
                        d.wait(lambda _:d.windows()[w['id']]['w']==1016,'maximize')
                        d.click(1024-54,16)
                        d.wait(lambda _:d.windows()[w['id']]['w']==480,'restore')
                        d.click(w['x']+w['w']-76,w['y']+14)
                        d.click(140,743);d.close(w)
                        w=launch(d,9);before=len(d.log());click(d,w,450,24)
                        d.wait(lambda s:'BROWSER: title Try a form' in s[before:],'offline form page')
                        before=len(d.log());d.tap('tab');d.type('soak check');d.tap('ret')
                        d.wait(lambda s:'BROWSER: title Local form preview' in s[before:],'offline form submit')
                        d.close(w)
                        d.move(4,4);time.sleep(2)
                        sample=health();samples.append(sample)
                        assert sample['windows']==0 and sample['free']==baseline['free'] and sample['heap']==baseline['heap'],(baseline,sample)
                        assert sample['drops']==0,sample
                        cycle+=1
                        if cycle==1:
                            d.click(40,746);d.screen('start-menu');d.click(400,710)
                            w=launch(d,1);a=launch(d,7);d.move(930,680);d.screen('desktop-preview')
                            d.close(a);d.close(w);d.move(4,4);time.sleep(2)
                        idle_start=health();time.sleep(5);idle_end=health()
                        assert idle_end['pixels']-idle_start['pixels']<25000,(idle_start,idle_end)
                        assert idle_end['frames']-idle_start['frames']<=8,(idle_start,idle_end)
                        print('SOAK: cycle',cycle,'elapsed',round(time.monotonic()-start),'seconds; memory stable; idle damage bounded',flush=True)
                    elapsed=time.monotonic()-start
                    d.screen('soak-complete');request('quit');process.wait(timeout=5)
                data=disk.read_bytes();validate(data)
                assert read_file(data,'/Documents/Soak.txt')==b'GathaOS stability check'
                assert 'PANIC' not in serial.read_text() and 'FAIL' not in serial.read_text()
                (evidence/'result.json').write_text(json.dumps({'result':'PASS','elapsed_seconds':elapsed,'cycles':cycle,'baseline':baseline,'samples':samples,'checks':['real 600-second minimum','editor save/reopen pixels','calculator','browser form editing and local submission','overlapping windows','drag','maximize/restore','minimize/taskbar restore','reclaimed PMM/heap','zero event drops','bounded idle damage','disk validation'],'command':cmd},indent=2)+'\n')
            except Exception:
                if serial.exists():
                    (evidence/'failure-tail.txt').write_text(serial.read_text()[-12000:])
                raise
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait()

if __name__=='__main__':run(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve())
