"""Host-only OpenSSL TLS server and record fault injector for the real guest."""
import contextlib
import socket
import ssl
import threading
import time

def exact(s,n):
    data=b''
    while len(data)<n:
        block=s.recv(n-len(data))
        if not block:raise EOFError()
        data+=block
    return data

class Server:
    def __init__(self,folder,name='good',fault='',payload=32781):
        self.fault=fault;self.payload=bytes((i*37+11)&255 for i in range(payload));self.events=[];self.stop=threading.Event()
        self.context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version=ssl.TLSVersion.TLSv1_2;self.context.maximum_version=ssl.TLSVersion.TLSv1_2
        self.context.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256');self.context.load_cert_chain(folder/(name+'.pem'),folder/'leaf.key')
        if fault=='tls13':self.context.minimum_version=ssl.TLSVersion.TLSv1_3;self.context.maximum_version=ssl.TLSVersion.TLSv1_3
        if fault=='cipher':self.context.set_ciphers('ECDHE-RSA-AES256-GCM-SHA384')
        self.context.set_alpn_protocols(['http/1.1']);self.sock=socket.socket();self.sock.bind(('127.0.0.1',0));self.sock.listen(4);self.sock.settimeout(.2);self.port=self.sock.getsockname()[1]
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def run(self):
        while not self.stop.is_set():
            try:client,_=self.sock.accept()
            except socket.timeout:continue
            except OSError:break
            threading.Thread(target=self.handle,args=(client,),daemon=True).start()
    def handle(self,client):
        with client:
            client.settimeout(30)
            try:
                if self.fault=='slow':time.sleep(4);return
                with self.context.wrap_socket(client,server_side=True) as stream:
                    self.events.append(('handshake',stream.version(),stream.cipher()[0]))
                    request=b''
                    while b'\r\n\r\n' not in request and len(request)<4096:
                        data=stream.recv(4096)
                        if not data:return
                        request+=data
                    self.events.append(('request',request.decode('ascii')))
                    if self.fault=='abrupt':stream.sendall(self.payload);return
                    # Multiple records include boundaries that do not match TCP segments.
                    for offset in range(0,len(self.payload),7777):stream.sendall(self.payload[offset:offset+7777])
                    try:stream.unwrap()
                    except (OSError,ssl.SSLError):pass
            except (OSError,ssl.SSLError) as e:self.events.append(('closed',type(e).__name__))
    def close(self):self.stop.set();self.sock.close();self.thread.join(timeout=2)

class Proxy:
    def __init__(self,server,fault):
        self.server=server;self.fault=fault;self.events=[];self.stop=threading.Event();self.sock=socket.socket();self.sock.bind(('127.0.0.1',0));self.sock.listen(4);self.sock.settimeout(.2);self.port=self.sock.getsockname()[1]
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def run(self):
        while not self.stop.is_set():
            try:client,_=self.sock.accept()
            except socket.timeout:continue
            except OSError:break
            threading.Thread(target=self.handle,args=(client,),daemon=True).start()
    def handle(self,client):
        with client,contextlib.closing(socket.create_connection(('127.0.0.1',self.server.port))) as upstream:
            client.settimeout(30);upstream.settimeout(30)
            def forward():
                try:
                    while True:
                        data=client.recv(65536)
                        if not data:break
                        upstream.sendall(data)
                except OSError:pass
                try:upstream.shutdown(socket.SHUT_WR)
                except OSError:pass
            threading.Thread(target=forward,daemon=True).start()
            encrypted=False;changed=False
            try:
                while True:
                    header=exact(upstream,5);data=exact(upstream,int.from_bytes(header[3:5],'big'));kind=header[0]
                    if kind==20:encrypted=True
                    if kind==22 and not encrypted and (self.fault=='inspect' or self.fault.startswith('pss-')):
                        at=0
                        while at+4<=len(data):
                            size=int.from_bytes(data[at+1:at+4],'big')
                            if at+4+size>len(data):break
                            if data[at]==12:
                                assert size>=40 and data[at+4:at+8]==b'\x03\x00\x1d\x20'
                                pos=at+4+36;scheme=int.from_bytes(data[pos:pos+2],'big')
                                self.events.append(('signature-scheme',scheme))
                                if self.fault!='inspect' and not changed:
                                    changed=True;self.events.append(self.fault);patched=bytearray(data)
                                    if self.fault=='pss-signature':patched[at+4+size-1]^=1
                                    elif self.fault=='pss-key-kind':patched[pos:pos+2]=(0x0809 if scheme==0x0804 else 0x0804).to_bytes(2,'big')
                                    elif self.fault=='pss-unoffered':patched[pos:pos+2]=b'\x08\x0c'
                                    elif self.fault=='pss-hash':patched[pos:pos+2]=b'\x08\x05'
                                    elif self.fault=='pss-pkcs-kind':patched[pos:pos+2]=b'\x04\x01'
                                    elif self.fault=='pss-length':patched[pos+3]^=1
                                    else:raise AssertionError(self.fault)
                                    data=bytes(patched)
                            at+=4+size
                    target=(self.fault=='finished' and encrypted and kind==22) or (self.fault in ('tag','truncate','replay') and kind==23)
                    if not changed and target:
                        changed=True;self.events.append(self.fault)
                        if self.fault=='truncate':client.sendall(header+data[:10]);return
                        if self.fault=='replay':client.sendall(header+data+header+data);continue
                        data=data[:-1]+bytes([data[-1]^1])
                    if self.fault=='fragment' and kind==22 and not encrypted:
                        for start in range(0,len(data),37):
                            part=data[start:start+37];client.sendall(header[:3]+len(part).to_bytes(2,'big')+part)
                        self.events.append('fragment');continue
                    if self.fault=='length' and not changed:
                        header=header[:3]+b'\xff\xff';changed=True;self.events.append('length')
                    client.sendall(header+data)
            except (EOFError,OSError):pass
            finally:
                for stream in (client,upstream):
                    try:stream.shutdown(socket.SHUT_RDWR)
                    except OSError:pass
    def close(self):self.stop.set();self.sock.close();self.thread.join(timeout=2)
