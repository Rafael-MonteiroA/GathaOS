"""Boot the original guest and exercise networking through its real Terminal UI."""
import contextlib
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
from apps_test import launch,field,click
from desktop_test import Desktop
from mkgathafs import read_file, validate

PAYLOAD=bytes((i*37+11)&255 for i in range(98317))
LARGE=bytes((i*17+5)&255 for i in range(1024*1024+13))
class HTTP(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    seen=[]
    def log_message(self,*_):pass
    def do_GET(self):
        self.seen.append((self.path,dict(self.headers)))
        try:
            if self.path=='/slow':time.sleep(5)
            if self.path=='/length' or self.path=='/large':
                data=PAYLOAD if self.path=='/length' else LARGE
                self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
            elif self.path=='/chunked':
                self.wfile.write(b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n')
                for offset in range(0,len(PAYLOAD),777):
                    data=PAYLOAD[offset:offset+777];self.wfile.write(f'{len(data):x};test=yes\r\n'.encode()+data+b'\r\n')
                self.wfile.write(b'0\r\nX-Test: done\r\n\r\n')
            elif self.path=='/close':self.wfile.write(b'HTTP/1.0 200 OK\r\n\r\n'+PAYLOAD)
            elif self.path=='/zero':self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n')
            elif self.path=='/short':self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Length: 20\r\n\r\nshort')
            elif self.path=='/conflict':self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n')
            elif self.path=='/duplicate':self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\nxx')
            elif self.path=='/oversize':self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Length: 4194305\r\n\r\n')
            elif self.path=='/badchunk':self.wfile.write(b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nx\r\n')
            elif self.path=='/gzip':self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 2\r\n\r\nxx')
            else:self.wfile.write(b'HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n')
            self.close_connection=True
        except (ConnectionError,BrokenPipeError):pass

class Guest:
    def __init__(self,iso,seed,label,network):
        self.iso=iso;self.root=Path('build/test-network-'+label).resolve();self.root.mkdir(parents=True,exist_ok=True)
        self.serial=self.root/'serial.log';self.serial.unlink(missing_ok=True);(self.root/'result.json').unlink(missing_ok=True)
        self.disk=self.root/'disk.img';shutil.copyfile(seed,self.disk)
        self.temp=tempfile.TemporaryDirectory(prefix='gatha-net-');self.endpoint=str(Path(self.temp.name)/'qmp')
        self.command=[os.environ.get('QEMU','qemu-system-i386'),'-machine','pc,accel=tcg','-m','256M','-vga','std',
                      '-cdrom',str(iso),'-boot','d','-display','none','-monitor','none','-serial','file:'+str(self.serial),
                      '-qmp','unix:'+self.endpoint+',server=on,wait=off','-drive',f'file={self.disk},format=raw,if=ide,index=0']+network
        self.logfile=(self.root/'qemu.log').open('w');self.process=subprocess.Popen(self.command,stdout=self.logfile,stderr=self.logfile)
        self.client=None;self.stream=None;self.d=None
    def wait(self,predicate,seconds=40):
        deadline=time.monotonic()+seconds
        while time.monotonic()<deadline:
            log=self.serial.read_text() if self.serial.exists() else ''
            assert 'PANIC' not in log and 'FAIL' not in log,log[-3000:]
            if predicate(log):return log
            assert self.process.poll() is None,'QEMU exited'
            time.sleep(.05)
        raise AssertionError('Network wait timed out: '+log[-3500:])
    def boot(self,connected=True):
        self.wait(lambda s:'STAGE8: PASS' in s and (not connected or 'NET: DHCP bound' in s))
        time.sleep(5)
        self.client=socket.socket(socket.AF_UNIX);self.client.settimeout(10);self.client.connect(self.endpoint)
        self.stream=self.client.makefile('rwb',buffering=0);json.loads(self.stream.readline());self.request('qmp_capabilities')
        self.d=Desktop(self.request,self.root,self.serial,1024,768)
        for w in reversed(list(self.d.windows().values())):self.d.close(w)
        time.sleep(2);self.baseline=self.health();self.window=launch(self.d,0)
    def health(self):
        values=re.findall(r'HEALTH: windows=(\d+) free=(\d+) heap=(\d+)',self.serial.read_text());return tuple(map(int,values[-1]))
    def request(self,name,args=None):
        self.stream.write((json.dumps({'execute':name,'arguments':args or {}})+'\n').encode())
        while True:
            response=json.loads(self.stream.readline())
            if 'error' in response:raise RuntimeError(response)
            if 'return' in response:return response['return']
    def command_line(self,text,expected=None,seconds=90):
        before=len(self.d.log());marker='SHELL: command '+text.split()[0]+' completed'
        self.d.type(text+'\n');output=self.wait(lambda s:marker in s[before:],seconds)[before:]
        if expected:assert expected in output,(text,output)
        print('NETWORK command PASS:',text,flush=True);return output
    def finish(self,checks,files=None):
        time.sleep(.3);self.d.screen('network-desktop');self.d.close(self.window);time.sleep(3)
        assert self.health()==self.baseline,(self.baseline,self.health())
        self.request('quit');self.process.wait(timeout=5)
        image=self.disk.read_bytes();validate(image)
        for path,expected in (files or {}).items():assert read_file(image,path)==expected,path
        result={'result':'PASS','checks':checks,'iso_sha256':hashlib.sha256(self.iso.read_bytes()).hexdigest(),
                'command':self.command,'baseline':self.baseline,'files':{p:hashlib.sha256(b).hexdigest() for p,b in (files or {}).items()}}
        (self.root/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('NETWORK QEMU PASS:',self.root.name,flush=True)
    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
        if self.stream:self.stream.close()
        if self.client:self.client.close()
        self.logfile.close();self.temp.cleanup()

def nat(iso,seed):
    server=ThreadingHTTPServer(('127.0.0.1',0),HTTP);threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        with contextlib.closing(Guest(iso,seed,'nat',['-nic','user,model=pcnet'])) as g:
            g.boot();g.command_line('net','PCnet: connected (DHCP)')
            # Two real user faults must reclaim every DNS/TCP handle and page.
            g.d.close(g.window);w=launch(g.d,1);field(g.d,w,150,52,'/System/Tests');click(g.d,w,550,54)
            time.sleep(2);baseline=g.health()
            for _ in range(2):
                before=len(g.d.log());click(g.d,w,100,146);click(g.d,w,220,14)
                output=g.wait(lambda s:'NETWORK ABI: bounds, exhaustion and stale handles PASS' in s[before:])[before:]
                pid=int(re.findall(r'TASK: loaded pid=(\d+) name=GEX program',output)[-1])
                g.wait(lambda s:f'USERFAULT: pid={pid} vector=14' in s);time.sleep(2)
                assert g.health()==baseline,(baseline,g.health())
            g.d.close(w);g.window=launch(g.d,0)
            prefix=f'http://10.0.2.2:{server.server_port}'
            files={}
            for route,payload in [('length',PAYLOAD),('chunked',PAYLOAD),('close',PAYLOAD),('large',LARGE),('zero',b'')]:
                path='/Documents/'+route+'.bin';g.command_line(f'wget {prefix}/{route} {path}','SHELL OUT: wget OK');files[path]=payload
            for route in ['short','conflict','duplicate','badchunk','missing','oversize','gzip']:
                output=g.command_line(f'wget {prefix}/{route} /Documents/length.bin')
                assert 'SHELL OUT: wget:' in output and 'wget OK' not in output,output
            g.command_line('wget ftp://example.com/ /Documents/length.bin','Unsupported protocol')
            g.command_line('wget http://10.0.2.2:1/ /Documents/length.bin','Connection refused')
            before=len(g.d.log());g.d.type(f'wget {prefix}/slow /Documents/length.bin\n');time.sleep(.7);g.d.tap('esc')
            g.wait(lambda s:'SHELL OUT: wget: Cancelled' in s[before:]);print('NETWORK cancellation PASS',flush=True)
            g.command_line(f'wget {prefix}/length /Documents/again.bin','SHELL OUT: wget OK');files['/Documents/again.bin']=PAYLOAD
            assert all(headers.get('User-Agent')=='GathaOS/11' for _,headers in HTTP.seen)
            g.finish(['DHCP NAT','ARP','TCP checksums/flow control','HTTP content-length/chunks/close/empty','1 MiB binary download',
                      'malformed/truncated/oversized/unsupported responses preserve destination','unsupported scheme rejection','refused connection',
                      'Escape cancellation','repeated connections','process memory reclamation','independent disk bytes'],files)
    finally:server.shutdown();server.server_close()

def offline(iso,seed):
    with contextlib.closing(Guest(iso,seed,'offline',['-nic','none'])) as g:
        g.boot(False);g.command_line('net','Offline: no supported network adapter')
        g.command_line('nslookup example.com','Network offline')
        g.command_line('wget http://10.0.2.2/ /Documents/no.txt','Network offline')
        g.finish(['no NIC boots normally','network operations fail promptly','desktop remains usable'])

def live(iso,seed):
    url='http://example.com/'
    request=urllib.request.Request(url,headers={'User-Agent':'GathaOS/10','Accept':'*/*','Accept-Encoding':'identity'})
    with urllib.request.urlopen(request,timeout=30) as response:expected=response.read()
    assert b'Example Domain' in expected
    with contextlib.closing(Guest(iso,seed,'live',['-nic','user,model=pcnet'])) as g:
        g.boot();g.command_line('nslookup example.com','Address: ')
        g.command_line('wget '+url+' /Documents/Internet.html','SHELL OUT: wget OK')
        g.command_line('cat /Documents/Internet.html','<!doctype html>')
        g.finish(['real external DNS','real external HTTP','host comparison of complete response'],{'/Documents/Internet.html':expected})

def wire(iso,seed):
    from network_peer import Peer,CONTENT
    with contextlib.closing(Peer()) as peer:
        with contextlib.closing(Guest(iso,seed,'wire',['-netdev',f'socket,id=lan,connect=127.0.0.1:{peer.port}',
                                                    '-device','pcnet,netdev=lan,mac=52:54:00:12:34:56'])) as g:
            g.boot();g.command_line('nslookup lookup.test','Address: 10.0.2.2')
            g.command_line('nslookup missing.test','Name or file not found')
            g.command_line('nslookup truncated.test','Network message exceeds supported limits')
            g.command_line('nslookup silent.test','Connection timed out')
            g.command_line('wget http://lookup.test/proof /Documents/proof.txt','wget OK')
            g.command_line('wget http://10.0.2.2:81/ /Documents/proof.txt','Connection timed out')
            peer.dhcp_enabled=False;before=len(g.d.log())
            g.wait(lambda s:'NET: DHCP acquiring' in s[before:],30)
            g.command_line('net','PCnet: waiting for DHCP')
            g.command_line('nslookup lookup.test','Network offline')
            peer.dhcp_enabled=True;before=len(g.d.log());g.wait(lambda s:'NET: DHCP bound' in s[before:],40)
            g.command_line('nslookup lookup.test','Address: 10.0.2.2')
            assert not peer.errors,peer.errors
            for event in ['offer','bound','renew','syn-retransmit','drop-request','tcp-proof']:assert event in peer.events,(event,peer.events)
            (g.root/'peer-events.json').write_text(json.dumps(peer.events,indent=2)+'\n')
            g.finish(['DHCP wrong transaction rejection, renewal, expiry and reacquisition','DNS CNAME','wrong DNS ID and compression cycle rejected',
                      'NXDOMAIN','truncated DNS error','DNS timeout','SYN and data retransmission','TCP out-of-order/duplicate/overlap',
                      'TCP checksum rejection','invalid RST rejection','connection timeout preserves file'],{'/Documents/proof.txt':CONTENT})

if __name__=='__main__':
    iso=Path(sys.argv[1]).resolve();seed=Path(sys.argv[2]).resolve();mode=sys.argv[3] if len(sys.argv)>3 else 'all'
    if mode in ('all','nat'):nat(iso,seed)
    if mode in ('all','offline'):offline(iso,seed)
    if mode in ('all','wire'):wire(iso,seed)
    if mode=='live':live(iso,seed)
