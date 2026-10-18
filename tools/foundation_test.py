"""Boot real guests, launch the large GEX via Files, and check fault reclamation."""
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
from apps_test import launch, click, field, command
from desktop_test import Desktop
from mkgathafs import create, read_file, validate

def run(iso, disk, ram, label, legacy=False, persisted=False):
    evidence=Path('build/test-foundation-'+label).resolve();evidence.mkdir(parents=True,exist_ok=True)
    serial=evidence/'serial.log';serial.unlink(missing_ok=True)
    (evidence/'result.json').unlink(missing_ok=True)
    def wait(predicate,seconds=90):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            output=serial.read_text() if serial.exists() else ''
            assert 'PANIC' not in output and 'FAIL' not in output,output[-4000:]
            if predicate(output):return output
            assert process.poll() is None,'QEMU exited'
            time.sleep(.1)
        raise AssertionError('Timed out: '+output[-4000:])
    with tempfile.TemporaryDirectory(prefix='gatha-foundation-') as temp:
        endpoint=str(Path(temp)/'qmp.sock')
        cmd=[os.environ.get('QEMU','qemu-system-i386'),'-machine','pc,accel=tcg','-m',str(ram)+'M','-vga','std',
             '-cdrom',str(iso),'-boot','d','-display','none','-monitor','none','-serial','file:'+str(serial),
             '-qmp','unix:'+endpoint+',server=on,wait=off','-drive',f'file={disk},format=raw,if=ide,index=0','-nic','none']
        with (evidence/'qemu.log').open('w') as log:
            process=subprocess.Popen(cmd,stdout=log,stderr=log)
            try:
                wait(lambda s:'STAGE8: PASS' in s)
                time.sleep(6)
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(10);client.connect(endpoint);stream=client.makefile('rwb',buffering=0);json.loads(stream.readline())
                    def request(name,args=None):
                        stream.write((json.dumps({'execute':name,'arguments':args or {}})+'\n').encode())
                        while True:
                            response=json.loads(stream.readline())
                            if 'error' in response:raise RuntimeError(response)
                            if 'return' in response:return response['return']
                    request('qmp_capabilities');d=Desktop(request,evidence,serial,1024,768)
                    for w in reversed(list(d.windows().values())):d.close(w)
                    w=launch(d,1);field(d,w,150,52,'/System/Tests');click(d,w,550,54)
                    def health():
                        h=re.findall(r'HEALTH: windows=(\d+) free=(\d+) heap=(\d+)',d.log())
                        assert h;return tuple(map(int,h[-1]))
                    time.sleep(2);baseline=health();assert baseline[0]==1
                    for attempt in range(2):
                        # foundation.gex is the only entry in this fixture directory.
                        before=d.log().count('FOUNDATION: checks PASS; intentional user page fault follows')
                        click(d,w,100,122);click(d,w,220,14)
                        output=wait(lambda s:s.count('FOUNDATION: checks PASS; intentional user page fault follows')>before)
                        pid=int(re.findall(r'TASK: loaded pid=(\d+) name=GEX program',output)[-1])
                        wait(lambda s:f'USERFAULT: pid={pid} vector=14' in s)
                        time.sleep(2)
                        assert health()==baseline,(label,baseline,health())
                    if persisted:assert 'FOUNDATION: persisted large file PASS' in output
                    assert ('FOUNDATION: physical OOM rollback PASS' if ram==64 else 'FOUNDATION: 128 MiB heap capacity PASS') in output
                    d.screen('after-fault-recovery');d.close(w)
                    if not legacy:
                        w=launch(d,2);field(d,w,130,50,'/FoundationTest/EditorLarge.txt')
                        click(d,w,120,14);d.wait(lambda s:'EDITOR: opened' in s,'large editor document')
                        d.move(4,4);d.text_pixels(w['x']+22,w['y']+118,'Large document test','large-editor')
                        click(d,w,220,14);d.wait(lambda s:'EDITOR: saved' in s,'large editor save');d.close(w)
                    else:
                        w=launch(d,1);field(d,w,150,52,'/System/Legacy');click(d,w,550,54)
                        click(d,w,100,122);click(d,w,220,14)
                        d.wait(lambda _:len(d.windows())==2,'GEX v1 application')
                        d.screen('legacy-gex')
                        for window in reversed(list(d.windows().values())):d.close(window)
                    c=launch(d,3);d.type('2+3=');d.wait(lambda s:'CALC: result 5\n' in s,'desktop after user fault');d.close(c)
                    if not legacy:
                        w=launch(d,0)
                        command(d,'cp /FoundationTest/large.bin /FoundationTest/Copy.bin')
                        assert 'SHELL OUT: cp OK' in d.log()
                        d.close(w)
                    request('quit');process.wait(timeout=5)
                image=disk.read_bytes();validate(image)
                expected=192*1024 if legacy else 4*1024*1024
                content=read_file(image,'/FoundationTest/large.bin')
                assert len(content)==expected and content==bytes((i*37+11)&255 for i in range(expected))
                if not legacy:
                    assert read_file(image,'/FoundationTest/Copy.bin')==content
                    text=bytearray(b''.join([b'x'*63+b'\n']*(220*1024//64)));text[:19]=b'Large document test'
                    assert read_file(image,'/FoundationTest/EditorLarge.txt')==text
                result={'result':'PASS','ram_mib':ram,'legacy':legacy,'fresh_process_persistence':persisted,
                        'probe_runs':2,'baseline':baseline,'large_file_bytes':expected,'command':cmd,
                        'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),
                        'checks':['5 MiB BSS','malloc/calloc/realloc/overflow','fragmentation','128 MiB heap or physical OOM rollback',
                                  'page table reclamation','file boundary/limit/pointers','safe save busy guard','fault fd/memory cleanup',
                                  'desktop survives fault','independent full file verification']}
                (evidence/'result.json').write_text(json.dumps(result,indent=2)+'\n')
                print('FOUNDATION QEMU PASS:',label,flush=True)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait()

def main():
    iso=Path(sys.argv[1]).resolve();seed=Path(sys.argv[2]).resolve()
    for ram in (64,256,512):
        evidence=Path('build/test-foundation-'+str(ram)).resolve();evidence.mkdir(parents=True,exist_ok=True)
        disk=evidence/'disk.img';shutil.copyfile(seed,disk)
        run(iso,disk,ram,str(ram))
        if ram==256:run(iso,disk,ram,'restart',persisted=True)
    # The old ABI places the stack at 8 MiB; rebuild only its small header.
    # Code is identical, and uses no new syscall in this selected About app.
    root=Path('build/legacy-root');shutil.copytree('build/none/rootfs',root,dirs_exist_ok=True)
    legacy=root/'System/Legacy';legacy.mkdir(parents=True,exist_ok=True)
    data=bytearray(Path('build/none/about.gex').read_bytes());struct.pack_into('<I',data,4,1)
    (legacy/'about-v1.gex').write_bytes(data)
    folder=Path('build/test-foundation-legacy').resolve();folder.mkdir(parents=True,exist_ok=True)
    disk=folder/'disk.img';disk.write_bytes(create(root,version=1))
    run(iso,disk,64,'legacy',legacy=True)

if __name__=='__main__':main()
