"""Independent Ethernet test peer; deliberately inject loss and malformed packets."""
import socket
import struct
import threading
import time

HOST=bytes([10,0,2,2]);GUEST=bytes([10,0,2,15]);MAC=bytes.fromhex('525400123456');PEER=bytes.fromhex('525400abcdef')
CONTENT=b'Original network stack: retransmission, overlap and checksum validation.\n'
def checksum(data):
    if len(data)%2:data+=b'\0'
    total=sum(struct.unpack('!'+str(len(data)//2)+'H',data))
    while total>>16:total=(total&65535)+(total>>16)
    return (~total)&65535
def name(text):return b''.join(bytes([len(part)])+part.encode() for part in text.split('.'))+b'\0'

class Peer:
    def __init__(self):
        self.listener=socket.socket();self.listener.bind(('127.0.0.1',0));self.listener.listen(1);self.port=self.listener.getsockname()[1]
        self.listener.settimeout(.5);self.client=None;self.stopped=False;self.errors=[];self.events=[];self.sessions={};self.dhcp_enabled=True
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def send(self,kind,payload,destination=MAC):
        packet=destination+PEER+struct.pack('!H',kind)+payload
        self.client.sendall(struct.pack('!I',len(packet))+packet)
    def ip(self,protocol,payload,destination=GUEST):
        header=bytearray(struct.pack('!BBHHHBBH4s4s',0x45,0,20+len(payload),7,0x4000,64,protocol,0,HOST,destination))
        struct.pack_into('!H',header,10,checksum(header));self.send(0x800,header+payload,b'\xff'*6 if destination==b'\xff'*4 else MAC)
    def udp(self,source_port,port,data,broadcast=False):
        destination=b'\xff'*4 if broadcast else GUEST
        p=bytearray(struct.pack('!HHHH',source_port,port,len(data)+8,0)+data)
        struct.pack_into('!H',p,6,checksum(HOST+destination+struct.pack('!BBH',0,17,len(p))+p) or 65535)
        self.ip(17,p,destination)
    def tcp(self,port,seq,ack,flags,data=b'',bad=False):
        p=bytearray(struct.pack('!HHIIBBHHH',80,port,seq,ack,5<<4,flags,8192,0,0)+data)
        struct.pack_into('!H',p,16,checksum(HOST+GUEST+struct.pack('!BBH',0,6,len(p))+p))
        if bad:p[-1]^=1
        self.ip(6,p)
    def dhcp(self,p):
        if not self.dhcp_enabled:return
        options={};at=240
        while at<len(p):
            code=p[at];at+=1
            if code==255:break
            if code==0:continue
            n=p[at];at+=1;options[code]=p[at:at+n];at+=n
        kind=options[53][0];assert kind in (1,3)
        reply=bytearray(240);reply[:4]=bytes([2,1,6,0]);reply[4:8]=p[4:8];reply[16:20]=GUEST;reply[28:34]=p[28:34];reply[236:240]=bytes.fromhex('63825363')
        reply+=bytes([53,1,2 if kind==1 else 5,54,4])+HOST+bytes([1,4,255,255,255,0,3,4])+HOST+bytes([6,4])+HOST+bytes([51,4])+struct.pack('!I',20)+b'\xff'
        wrong=bytearray(reply);wrong[4]^=0xff;wrong[16:20]=bytes([10,0,2,99]);self.udp(67,68,wrong,True)
        self.udp(67,68,reply,True);self.events.append('offer' if kind==1 else 'renew' if p[12:16]==GUEST else 'bound')
    def dns(self,port,p):
        labels=[];at=12
        while p[at]:n=p[at];at+=1;labels.append(p[at:at+n].decode());at+=n
        hostname='.'.join(labels);question=p[12:at+5]
        self.events.append('dns:'+hostname)
        if hostname in ('silent.test','pending.test'):return
        flags=0x8183 if hostname=='missing.test' else 0x8380 if hostname=='truncated.test' else 0x8180
        if flags!=0x8180:self.udp(53,port,struct.pack('!HHHHHH',struct.unpack_from('!H',p)[0],flags,1,0,0,0)+question);return
        alias=name('alias.test');records=b'\xc0\x0c'+struct.pack('!HHIH',5,1,60,len(alias))+alias+alias+struct.pack('!HHIH',1,1,60,4)+HOST
        reply=bytearray(struct.pack('!HHHHHH',struct.unpack_from('!H',p)[0],flags,1,2,0,0)+question+records)
        wrong=bytearray(reply);wrong[0]^=255;wrong[-4:]=bytes([10,0,2,99]);self.udp(53,port,wrong)
        cycle=bytearray(reply);offset=12+len(question);cycle[offset:offset+2]=struct.pack('!H',0xc000|offset);cycle[-4:]=bytes([10,0,2,99]);self.udp(53,port,cycle)
        self.udp(53,port,reply)
    def receive(self,frame):
        if len(frame)<14:return
        kind=struct.unpack_from('!H',frame,12)[0];p=frame[14:]
        if kind==0x806:
            if len(p)>=28 and p[6:8]==b'\0\1' and p[24:28]==HOST:
                self.send(0x806,struct.pack('!HHBBH',1,0x800,6,4,2)+PEER+HOST+MAC+GUEST);self.events.append('arp')
            return
        if kind!=0x800:return
        assert p[0]==0x45 and checksum(p[:20])==0,'Guest IPv4 checksum'
        length=struct.unpack_from('!H',p,2)[0];protocol=p[9];source,destination=p[12:16],p[16:20];p=p[20:length]
        if protocol==17:
            sport,dport,size,total=struct.unpack_from('!HHHH',p);p=p[:size]
            assert not total or checksum(source+destination+struct.pack('!BBH',0,17,size)+p)==0,'Guest UDP checksum'
            if dport==67:self.dhcp(p[8:])
            elif dport==53:self.dns(sport,p[8:])
        elif protocol==6:
            assert checksum(source+destination+struct.pack('!BBH',0,6,len(p))+p)==0,'Guest TCP checksum'
            sport,dport,seq,ack=struct.unpack_from('!HHII',p);flags=p[13];data=p[(p[12]>>4)*4:]
            if dport!=80:return
            s=self.sessions.setdefault(sport,{'syn':0,'request':0,'delivered':False})
            if flags&2:
                s['syn']+=1
                if s['syn']==1:self.events.append('drop-syn');return
                self.tcp(sport,70000,seq+1,18);self.events.append('syn-retransmit');return
            if data:
                s['request']+=1
                if s['request']==1:self.events.append('drop-request');return
                if s['delivered']:return
                assert data.startswith(b'GET /proof HTTP/1.1\r\n'),data
                s['delivered']=True;ack=seq+len(data)
                response=b'HTTP/1.1 200 OK\r\nContent-Length: '+str(len(CONTENT)).encode()+b'\r\n\r\n'+CONTENT
                self.tcp(sport,70031,ack,24,response[30:]) # Out of order: must not be appended.
                self.tcp(sport,70001,ack,24,b'corrupt',bad=True)
                self.tcp(sport,999999,ack,20) # Out-of-window reset must be ignored.
                self.tcp(sport,70001,ack,24,response[:30])
                self.tcp(sport,70001,ack,24,response[:30]) # Duplicate.
                self.tcp(sport,70021,ack,24,response[20:50]) # Partial overlap.
                self.tcp(sport,70051,ack,25,response[50:])
                self.events.append('tcp-proof')
    def run(self):
        try:
            while not self.stopped:
                try:self.client,_=self.listener.accept();break
                except socket.timeout:continue
            if not self.client:return
            self.client.settimeout(.5);buffer=bytearray()
            while not self.stopped:
                try:part=self.client.recv(65536)
                except socket.timeout:continue
                if not part:break
                buffer+=part
                while len(buffer)>=4:
                    size=struct.unpack_from('!I',buffer)[0];assert size<=65536
                    if len(buffer)<4+size:break
                    frame=bytes(buffer[4:4+size]);del buffer[:4+size];self.receive(frame)
        except (ConnectionError,OSError) as error:
            if not self.stopped:self.errors.append(repr(error))
        except Exception as error:self.errors.append(repr(error))
    def close(self):
        self.stopped=True
        if self.client:self.client.close()
        self.listener.close();self.thread.join(timeout=2)
