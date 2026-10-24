"""Real PS/2 browser interactions against controlled HTTP and external HTTPS."""
import contextlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
import sys
import threading
import time
from network_test import Guest
from apps_test import launch,click

INDEX=b"<title>Browser test</title><p>Browser test page</p><p><a href='next?from=index'>Open next page</a></p><script>HIDDEN SCRIPT</script>"
NEXT=b"<title>Second page</title><p>Second page content</p><p><a href='/long#bottom'>Jump to bottom</a></p>"
LONG=b"<title>Long page</title>"+b''.join(f'<p>Line {i:03}</p>'.encode() for i in range(80))+b"<p id='bottom'>Bottom anchor</p>"
class HTTP(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    seen=[]
    def log_message(self,*_):pass
    def do_GET(self):
        self.seen.append((self.path,dict(self.headers)))
        try:
            code=200;kind='text/html; charset=utf-8';body=INDEX;extra=[]
            if self.path=='/slow':time.sleep(8)
            if self.path=='/start':code=302;extra=[('Location','/dir/index')];body=b''
            elif self.path=='/loop':code=301;extra=[('Location','/loop')];body=b''
            elif self.path=='/badredirect':code=302;extra=[('Location','javascript:bad()')];body=b''
            elif self.path=='/dir/next?from=index':body=NEXT
            elif self.path=='/long':body=LONG
            elif self.path=='/plain':body=b'<b>Literal text</b>\nSecond line';kind='text/plain'
            elif self.path=='/binary':body=b'not a page';kind='application/octet-stream'
            elif self.path=='/missing':code=404;body=b'<p>Page not found</p>'
            elif self.path=='/displaylimit':body=b'x'*65537;kind='text/plain'
            elif self.path=='/oversize':
                self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 524289\r\n\r\n');self.close_connection=True;return
            elif self.path=='/short':
                self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 20\r\n\r\nx');self.close_connection=True;return
            elif self.path=='/chunked':
                self.wfile.write(b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\n\r\n')
                for start in range(0,len(INDEX),17):
                    part=INDEX[start:start+17];self.wfile.write(f'{len(part):x}\r\n'.encode()+part+b'\r\n')
                self.wfile.write(b'0\r\n\r\n');self.close_connection=True;return
            self.send_response(code)
            for key,value in extra:self.send_header(key,value)
            self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(body)))
            self.end_headers();self.wfile.write(body);self.close_connection=True
        except (OSError,ConnectionError):pass

def browser(g):
    g.d.close(g.window);g.window=launch(g.d,9);g.wait(lambda s:'BROWSER: ready' in s)

def open_url(g,url,expected='BROWSER: loaded',wait=True):
    before=len(g.d.log())
    g.d.key('ctrl',True);g.d.tap('l');g.d.key('ctrl',False);g.d.type(url+'\n')
    if wait:return g.wait(lambda s:expected in s[before:],120)[before:]
    return before

def page_pixels(g,text,name):
    g.d.move(4,4);g.d.text_pixels(g.window['x']+22,g.window['y']+152,text,name)

def toolbar(g,x,expected):
    before=len(g.d.log());click(g.d,g.window,x,24);return g.wait(lambda s:expected in s[before:],120)[before:]

def local(iso,seed):
    server=ThreadingHTTPServer(('127.0.0.1',0),HTTP);threading.Thread(target=server.serve_forever,daemon=True).start()
    prefix=f'http://10.0.2.2:{server.server_port}'
    try:
        with contextlib.closing(Guest(iso,seed,'browser-local',['-cpu','max','-rtc','base=utc','-nic','user,model=pcnet'])) as g:
            g.boot();browser(g);g.d.screen('browser-home')
            open_url(g,prefix+'/start');page_pixels(g,'Browser test page','browser-index')
            assert 'BROWSER: loaded '+prefix+'/dir/index' in g.d.log()
            before=len(g.d.log());click(g.d,g.window,60,144)
            g.wait(lambda s:'BROWSER: loaded '+prefix+'/dir/next?from=index' in s[before:]);page_pixels(g,'Second page content','browser-next')
            toolbar(g,40,'BROWSER: loaded');page_pixels(g,'Browser test page','browser-back')
            toolbar(g,120,'BROWSER: loaded');page_pixels(g,'Second page content','browser-forward')
            count=sum(path=='/dir/next?from=index' for path,_ in HTTP.seen)
            toolbar(g,200,'BROWSER: loaded');assert sum(path=='/dir/next?from=index' for path,_ in HTTP.seen)==count+1
            before=len(g.d.log());click(g.d,g.window,60,144);g.wait(lambda s:'BROWSER: loaded '+prefix+'/long#bottom' in s[before:])
            g.d.screen('browser-anchor');g.d.tap('home');page_pixels(g,'Line 000','browser-scroll-top')
            g.d.tap('pgdn');page_pixels(g,'Line 019','browser-scroll-page')
            g.d.tap('home');g.d.click(g.window['x']+g.window['w']-46,g.window['y']+14)
            g.d.wait(lambda _:g.d.windows()[g.window['id']]['w']==1016,'browser maximize');g.window=g.d.windows()[g.window['id']]
            page_pixels(g,'Line 000','browser-maximized');g.d.click(970,16)
            g.d.wait(lambda _:g.d.windows()[g.window['id']]['w']==780,'browser restore');g.window=g.d.windows()[g.window['id']]
            open_url(g,prefix+'/plain');page_pixels(g,'<b>Literal text</b>','browser-plain')
            open_url(g,prefix+'/missing');page_pixels(g,'Page not found','browser-404')
            open_url(g,prefix+'/chunked');page_pixels(g,'Browser test page','browser-chunked')
            for route in ['/binary','/loop','/badredirect','/oversize','/short']:
                open_url(g,prefix+route,'BROWSER: error');page_pixels(g,'Browser test page','browser-preserved-'+route[1:])
            open_url(g,'file:///System/Apps','BROWSER: error');page_pixels(g,'Browser test page','browser-scheme-rejected')
            for stop in ['esc','button']:
                before=open_url(g,prefix+'/slow',wait=False);g.wait(lambda s:'BROWSER: request' in s[before:])
                if stop=='esc':g.d.tap('esc')
                else:click(g.d,g.window,280,24)
                g.wait(lambda s:'BROWSER: error Open: Cancelled' in s[before:]);page_pixels(g,'Browser test page','browser-cancel-'+stop)
            # The scheduler must keep another real application responsive during I/O.
            before=open_url(g,prefix+'/slow',wait=False);g.wait(lambda s:'BROWSER: request' in s[before:])
            calc=launch(g.d,3);g.d.type('2+3=');g.wait(lambda s:'CALC: result 5' in s[before:]);g.d.close(calc)
            g.d.close(g.window);time.sleep(3);assert g.health()==g.baseline
            browser_window=launch(g.d,9);g.window=browser_window
            open_url(g,prefix+'/displaylimit');g.d.screen('browser-display-limit')
            toolbar(g,350,'BROWSER: loaded');g.d.screen('browser-home-final')
            assert all(headers.get('User-Agent')=='GathaOS/15' for _,headers in HTTP.seen)
            g.finish(['start menu browser','redirect and relative link','back/forward/reload','anchor/scroll',
                      'maximize/restore reflow','pixel-verified HTML and literal plain text','HTTP 404 display','chunked page',
                      'MIME/scheme/redirect/size/truncation failure preserves page','Escape/Stop cancellation',
                      'calculator responsive during fetch','close during fetch reclaims memory','bounded display','home'])
    finally:server.shutdown();server.server_close()

def offline(iso,seed):
    with contextlib.closing(Guest(iso,seed,'browser-offline',['-cpu','max','-nic','none'])) as g:
        g.boot(False);browser(g);open_url(g,'http://example.com/','BROWSER: error Open: Network offline')
        g.d.screen('browser-offline');g.finish(['offline launch and home','offline network error','page retained','memory reclamation'])

def certificate(iso,seed):
    from tls_fixtures import make
    from tls_peer import Server
    folder=make('build/browser-pki')
    with contextlib.closing(Server(folder)) as server,contextlib.closing(Guest(iso,seed,'browser-certificate',
            ['-cpu','max','-rtc','base=utc','-nic','user,model=pcnet'])) as g:
        g.boot();browser(g);open_url(g,f'https://10.0.2.2:{server.port}/','BROWSER: error Open: TLS certificate rejected')
        assert not any(event[0]=='request' for event in server.events)
        g.finish(['untrusted HTTPS certificate rejected before HTTP','no bypass button','home retained','memory reclamation'])

def live(iso,seed):
    with contextlib.closing(Guest(iso,seed,'browser-live',['-cpu','max','-rtc','base=utc','-nic','user,model=pcnet'])) as g:
        g.boot();browser(g);open_url(g,'http://example.com/')
        g.d.screen('browser-example');assert 'BROWSER: title Example Domain' in g.d.log()
        open_url(g,'https://www.google.com/robots.txt');page_pixels(g,'User-agent: *','browser-google-https')
        assert 'TLS: authenticated TLS 1.2' in g.d.log()
        g.finish(['real internet DNS/HTTP Example Domain','real Google HTTPS verified certificate','pixel-verified robots.txt rendering'])

if __name__=='__main__':
    iso,seed=map(lambda p:Path(p).resolve(),sys.argv[1:3]);mode=sys.argv[3] if len(sys.argv)>3 else 'all'
    if mode in ('all','local'):local(iso,seed)
    if mode in ('all','offline'):offline(iso,seed)
    if mode in ('all','certificate'):certificate(iso,seed)
    if mode=='live':live(iso,seed)
