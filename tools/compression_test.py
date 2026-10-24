"""Real QEMU Browser/Terminal requests; independent host compression and bytes."""
import contextlib
import gzip
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
from pathlib import Path
import random
import sys
import threading
import time
import zlib
from network_test import Guest
from browser_test import browser,open_url,page_pixels
from apps_test import launch,click
from inflate_test import compress,gzip_options

PAGE=b"<title>Compressed page</title><p>Compressed page works</p><form action='/search'><input name='q'><button>Search</button></form>"
BINARY=random.Random(14).randbytes(131073)
class HTTP(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    seen=[]
    def log_message(self,*_):pass
    def do_GET(self):
        self.seen.append((self.path,dict(self.headers)))
        try:
            plain=PAGE;encoding='gzip';extra=[];kind='text/html';framing='length'
            if self.path.startswith('/search?'):plain=b'<title>Search response</title><p>Compressed search received</p>'
            if self.path in ['/binary','/binary-deflate']:plain=BINARY;kind='application/octet-stream'
            if self.path=='/cancel':plain=b'a'*4194304;kind='application/octet-stream'
            if self.path=='/bomb':plain=b'a'*524289
            if self.path=='/empty':plain=b'';kind='text/plain'
            if self.path in ['/deflate','/binary-deflate','/bad-adler','/raw','/dictionary']:encoding='deflate'
            body=gzip.compress(plain,mtime=0) if encoding=='gzip' else compress(plain)
            if self.path=='/options':body=gzip_options(plain)
            if self.path=='/members':body=gzip.compress(plain[:30])+gzip.compress(plain[30:])
            if self.path=='/chunked':framing='chunked'
            if self.path=='/close':framing='close'
            if self.path=='/identity':encoding='identity';body=plain
            if self.path=='/alias':encoding='X-GZip'
            if self.path=='/unknown':encoding='br'
            if self.path=='/chain':encoding='gzip, deflate'
            if self.path=='/duplicate':extra=[('Content-Encoding','gzip')]
            if self.path=='/bad-crc':body=body[:-8]+bytes([body[-8]^1])+body[-7:]
            if self.path=='/bad-size':body=body[:-1]+bytes([body[-1]^1])
            if self.path=='/bad-adler':body=body[:-1]+bytes([body[-1]^1])
            if self.path=='/short':body=body[:-2]
            if self.path=='/trailing':body+=b'junk'
            if self.path=='/raw':body=compress(plain,-15)
            if self.path=='/dictionary':
                encoder=zlib.compressobj(zdict=b'dictionary');body=encoder.compress(plain)+encoder.flush()
            if self.path=='/wire-limit':
                self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 589825\r\n\r\n');return
            if self.path=='/bad-trailer':framing='chunked'
            self.send_response(200)
            for key,value in extra:self.send_header(key,value)
            self.send_header('Content-Type',kind);self.send_header('Content-Encoding',encoding)
            if framing=='length':self.send_header('Content-Length',str(len(body)))
            if framing=='chunked':self.send_header('Transfer-Encoding','chunked')
            self.end_headers()
            if framing=='chunked':
                for start in range(0,len(body),11):
                    part=body[start:start+11];self.wfile.write(f'{len(part):x}\r\n'.encode()+part+b'\r\n')
                self.wfile.write(b'0\r\n'+(b'Content-Encoding: identity\r\n' if self.path=='/bad-trailer' else b'X-Check: done\r\n')+b'\r\n')
            else:self.wfile.write(body)
        except (OSError,ConnectionError):pass
        finally:self.close_connection=True

def run(iso,seed):
    HTTP.seen=[];server=ThreadingHTTPServer(('127.0.0.1',0),HTTP)
    threading.Thread(target=server.serve_forever,daemon=True).start();prefix=f'http://10.0.2.2:{server.server_port}'
    try:
        with contextlib.closing(Guest(iso,seed,'compression',['-cpu','max','-rtc','base=utc','-nic','user,model=pcnet'])) as g:
            g.boot();browser(g)
            for route in ['gzip','deflate','chunked','close','members','options','alias','identity']:
                log=open_url(g,prefix+'/'+route);page_pixels(g,'Compressed page works','compression-'+route)
                if route!='identity':assert 'HTTP: decoded ' in log,log
            # Submit a real form and inspect the server's query, then its gzip response.
            g.d.tap('tab');g.d.type('gatha os\n');g.wait(lambda s:'BROWSER: title Search response' in s)
            assert any(path=='/search?q=gatha+os' for path,_ in HTTP.seen)
            page_pixels(g,'Compressed search received','compression-search')
            for route in ['unknown','chain','duplicate','bad-crc','bad-size','bad-adler','short','trailing',
                          'raw','dictionary','bomb','wire-limit','bad-trailer']:
                open_url(g,prefix+'/'+route,'BROWSER: error');page_pixels(g,'Compressed search received','compression-rejected-'+route)
            open_url(g,prefix+'/empty');assert 'HTTP: decoded gzip ' in g.d.log();g.d.screen('compression-empty')
            g.d.close(g.window);g.window=launch(g.d,0);files={}
            for route in ['binary','binary-deflate']:
                path='/Documents/'+route+'.bin'
                g.command_line(f'wget {prefix}/{route} {path}','SHELL OUT: wget OK');files[path]=BINARY
            for route in ['bad-crc','bad-adler','short','duplicate']:
                output=g.command_line(f'wget {prefix}/{route} /Documents/binary.bin')
                assert 'SHELL OUT: wget:' in output and 'wget OK' not in output
            # Wait for decoding, not just network I/O, before injecting Escape.
            before=len(g.d.log());g.d.type(f'wget {prefix}/cancel /Documents/binary.bin\n')
            g.wait(lambda s:'HTTP: decoding gzip' in s[before:]);assert 'wget completed' not in g.d.log()[before:]
            g.d.tap('esc');g.wait(lambda s:'SHELL OUT: wget: Cancelled' in s[before:])
            before=len(g.d.log());g.d.type(f'wget {prefix}/cancel /Documents/binary.bin\n')
            g.wait(lambda s:'HTTP: decoding gzip' in s[before:]);assert 'wget completed' not in g.d.log()[before:]
            g.d.close(g.window);time.sleep(3);assert g.health()==g.baseline
            g.window=launch(g.d,0);g.command_line(f'wget {prefix}/binary /Documents/again.bin','SHELL OUT: wget OK');files['/Documents/again.bin']=BINARY
            assert all(h.get('Accept-Encoding')=='gzip, deflate' and h.get('User-Agent')=='GathaOS/14' for _,h in HTTP.seen)
            (g.root/'http-requests.json').write_text(json.dumps(HTTP.seen,indent=2)+'\n')
            g.finish(['gzip/zlib decode over length/chunked/close framing','gzip optional headers and concatenated members',
                      'case-insensitive x-gzip and identity','pixel-verified pages and compressed GET form response',
                      'empty compressed body','unsupported/duplicate coding, checksums, truncation and bounds preserve page/file',
                      'forbidden coding trailer rejection','byte-exact binary downloads','cancel and close during decompression',
                      'memory reclamation and subsequent request','independent request headers and query'],files)
    finally:server.shutdown();server.server_close()
if __name__=='__main__':run(*map(lambda p:Path(p).resolve(),sys.argv[1:3]))
