"""TLS integration tests boot the exact supplied ISO and exercise guest code."""
import contextlib
import sys
import ssl
import urllib.request
import json
import shutil
import struct
import time
from pathlib import Path
from network_test import Guest
from apps_test import launch,field,click
from mkgathafs import create

def local(iso,seed):
    from tls_fixtures import make
    from tls_peer import Server,Proxy
    folder=make('build/tls-pki');checks=[];servers=[];plan=[];payload=32781
    with contextlib.ExitStack() as stack:
        for name in ['good','expired','future','wrong-name','no-san','client-only','unknown-critical','must-staple','foreign']:
            s=stack.enter_context(contextlib.closing(Server(folder,name,payload=payload)));servers.append(s)
            plan.append((s.port,0 if name=='good' else -129,0,payload));checks.append(name)
        for fault in ['fragment','finished','tag','truncate','replay','length']:
            s=stack.enter_context(contextlib.closing(Server(folder,payload=payload)));p=stack.enter_context(contextlib.closing(Proxy(s,fault)));servers.extend([s,p])
            plan.append((p.port,0 if fault=='fragment' else -74,0,payload));checks.append(fault)
        for fault in ['tls13','cipher','abrupt','slow']:
            s=stack.enter_context(contextlib.closing(Server(folder,fault=fault,payload=payload)));servers.append(s)
            plan.append((s.port,-125 if fault=='slow' else -74,1 if fault=='slow' else 0,payload));checks.append(fault)
        # Repeated success after failures exercises connection and secret-buffer reclamation.
        s=stack.enter_context(contextlib.closing(Server(folder,payload=payload)));servers.append(s);plan.append((s.port,0,0,payload));checks.append('repeat-success')
        root=Path('build/tls-rootfs');shutil.copytree('build/none/rootfs',root,dirs_exist_ok=True)
        (root/'Documents/TLS-root.der').write_bytes((folder/'root.der').read_bytes())
        (root/'Documents/TLS-plan.bin').write_bytes(struct.pack('<I',len(plan))+b''.join(struct.pack('<IiII',*item) for item in plan))
        preserved=b'Existing document must survive a failed HTTPS request.\n'
        (root/'Documents/TLS-preserve.txt').write_bytes(preserved)
        private=Path('build/tls-private.img');private.write_bytes(create(root))
        with contextlib.closing(Guest(iso,private,'tls-local',['-cpu','max','-rtc','base=2026-10-21T12:00:00','-nic','user,model=pcnet'])) as g:
            g.boot();g.command_line(f'wget https://10.0.2.2:{plan[0][0]}/ /Documents/TLS-preserve.txt','TLS certificate rejected')
            slow_port=plan[checks.index('slow')][0]
            before=len(g.d.log());g.d.type(f'wget https://10.0.2.2:{slow_port}/ /Documents/TLS-preserve.txt\n');time.sleep(.7);g.d.tap('esc')
            g.wait(lambda s:'SHELL OUT: wget: Cancelled' in s[before:])
            g.d.type(f'wget https://10.0.2.2:{slow_port}/ /Documents/TLS-preserve.txt\n');time.sleep(.7);g.d.close(g.window);time.sleep(3)
            assert g.health()==g.baseline,(g.baseline,g.health());g.window=launch(g.d,0)
            g.d.close(g.window);w=launch(g.d,1);field(g.d,w,150,52,'/System/Tests');click(g.d,w,550,54);time.sleep(2);baseline=g.health();before=len(g.d.log())
            click(g.d,w,100,166);click(g.d,w,220,14)
            for index in range(len(plan)):
                log=g.wait(lambda s:f'TLS TEST: case {index} result=' in s[before:],150)
                assert f'TLS TEST: case {index} result={plan[index][1]}\n' in log
                print('TLS QEMU case PASS:',checks[index],flush=True)
            g.wait(lambda s:'TLS TEST: all protocol, certificate and integrity cases PASS' in s[before:]);time.sleep(3);assert g.health()==baseline,(baseline,g.health())
            for s in servers:
                if isinstance(s,Proxy):assert s.fault in s.events,(s.fault,s.events)
            (g.root/'server-events.json').write_text(json.dumps([s.events for s in servers],indent=2)+'\n')
            g.d.close(w);g.window=launch(g.d,0);g.command_line('echo HTTPS verification tests passed','HTTPS verification tests passed')
            g.finish(['production Terminal rejects private test CA','Escape and closing Terminal cancel TLS','failed HTTPS preserves destination','user security ABI bounds','entropy freshness',*checks,'memory/handle reclamation'],{'/Documents/TLS-preserve.txt':preserved})

def prerequisites(iso,seed):
    for name,cpu,rtc,message in [('entropy','qemu32','2026-10-21T12:00:00','Secure randomness unavailable'),('clock','max','1999-01-01T00:00:00','Invalid UTC clock')]:
        # A TCP listener is enough: prerequisite rejection must precede ClientHello.
        import socket,threading
        with socket.socket() as listener:
            listener.bind(('127.0.0.1',0));listener.listen(2);listener.settimeout(60);received=[]
            def accept():
                try:
                    s,_=listener.accept()
                    with s:s.settimeout(5);received.append(s.recv(1024))
                except OSError:pass
            thread=threading.Thread(target=accept,daemon=True);thread.start()
            with contextlib.closing(Guest(iso,seed,'tls-'+name,['-cpu',cpu,'-rtc','base='+rtc,'-nic','user,model=pcnet'])) as g:
                g.boot();g.command_line(f'wget https://10.0.2.2:{listener.getsockname()[1]}/ /Documents/no.txt',message)
                g.finish([name+' prerequisite fails closed','desktop still usable'])
            thread.join(timeout=6);assert not any(received),received

def live(iso,seed):
    url='https://www.google.com/robots.txt'
    context=ssl.create_default_context();context.maximum_version=ssl.TLSVersion.TLSv1_2
    context.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
    request=urllib.request.Request(url,headers={'User-Agent':'GathaOS/13','Accept':'*/*','Accept-Encoding':'identity'})
    with urllib.request.urlopen(request,context=context,timeout=30) as response:expected=response.read()
    with contextlib.closing(Guest(iso,seed,'tls-live',['-cpu','max','-rtc','base=utc','-nic','user,model=pcnet'])) as g:
        g.boot();g.command_line('wget '+url+' /Documents/Google.txt','SHELL OUT: wget OK',seconds=120)
        assert 'TLS: authenticated TLS 1.2' in g.d.log()
        g.command_line('cat /Documents/Google.txt','User-agent:')
        g.finish(['real Google HTTPS with SNI, RSA chain, UTC, X25519, AES-GCM and Finished verification','independent host response comparison'],{'/Documents/Google.txt':expected})

if __name__=='__main__':
    iso=Path(sys.argv[1]).resolve();seed=Path(sys.argv[2]).resolve();mode=sys.argv[3] if len(sys.argv)>3 else 'all'
    if mode=='live':live(iso,seed)
    if mode in ('all','local'):local(iso,seed)
    if mode in ('all','prerequisites'):prerequisites(iso,seed)
