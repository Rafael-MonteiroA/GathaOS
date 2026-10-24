"""Submit real guest form controls and inspect the independent HTTP server bytes."""
import contextlib
import html
import json
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
import sys
import threading
import time
from urllib.parse import parse_qs,urlsplit
from network_test import Guest
from browser_test import browser,open_url,page_pixels,toolbar
from apps_test import click

def search_form(value=''):
    return ("<form action='/search?replace=this#result' method='get'>"
            f"<input type='search' name='q' value='{html.escape(value,quote=True)}' required maxlength='120'>"
            "<input type=hidden name=tag value='one&amp;two'><input type=hidden name=tag value='three'>"
            "<button name=go value=find>Search</button></form>")

class HTTP(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    seen=[]
    def log_message(self,*_):pass
    def do_GET(self):
        self.seen.append((self.path,dict(self.headers)))
        route=urlsplit(self.path).path;args=parse_qs(urlsplit(self.path).query,keep_blank_values=True)
        if route=='/search':
            q=args.get('q',[''])[0]
            body=f'<title>Search results</title><p>Results for: {html.escape(q)}</p>'+search_form(q)
        elif route=='/fields':
            body=("<p>Field rules</p><form action=/echo><input name=read value=locked readonly>"
                  "<input name=skip value=omitted disabled><input name=short maxlength=4>"
                  "<button name=go value=ok>Send</button></form>")
        elif route=='/multiple':
            body=("<p>First form</p><form action=/echo><input name=first><button>Send first</button></form>"
                  "<p>Second form</p><form action=/echo><input name=second><button>Send second</button></form>")
        elif route=='/disabled':
            body="<p>Disabled default</p><form action=/echo><input name=q><button disabled>Disabled</button><button>Send</button></form>"
        elif route=='/post':body='<p>Unsupported POST</p><form method=post action=/forbidden><input name=q><button>Go</button></form>'
        elif route=='/password':body='<p>Unsupported password</p><form action=/forbidden><input name=q><input type=password name=p><button>Go</button></form>'
        elif route=='/unsupported':body='<p>Unsupported select</p><form action=/forbidden><input name=q><select name=x><option>A</option></select><button>Go</button></form>'
        elif route=='/override':body='<p>Unsupported override</p><form action=/forbidden><input name=q><button formaction=/other>Go</button></form>'
        elif route=='/badchars':body='<p>Unsupported character</p><form action=/forbidden><input name=q><input type=hidden name=x value="caf&#233;"><button>Go</button></form>'
        elif route=='/scheme':body='<p>Unsupported scheme</p><form action="javascript:bad()"><input name=q><button>Go</button></form>'
        elif route=='/oversize':body='<p>Too much form data</p><form action=/forbidden><input name=q>'+f'<input type=hidden name=x value="{"%"*255}">'*2+'<button>Go</button></form>'
        elif route=='/failure':body='<p>Keep my input</p><form action=/binary><input name=q><button>Send</button></form>'
        elif route=='/cancel':body='<p>Cancel my request</p><form action=/slow><input name=q><button>Send</button></form>'
        elif route=='/slow':time.sleep(8);body='<p>Slow response</p>'
        elif route=='/binary':body='unsupported response'
        elif route=='/echo':body='<p>Form received</p>'
        else:body='<title>Search form</title><p>Search form</p>'+search_form()
        data=body.encode()
        self.send_response(200);self.send_header('Content-Type','application/octet-stream' if route=='/binary' else 'text/html; charset=utf-8');self.send_header('Content-Length',str(len(data)))
        try:self.end_headers();self.wfile.write(data)
        except (BrokenPipeError,ConnectionResetError):pass
        self.close_connection=True

def submit(g,act):
    before=len(g.d.log());act();g.wait(lambda s:'BROWSER: loaded' in s[before:]);return g.d.log()[before:]

def test(iso,seed):
    HTTP.seen=[];server=ThreadingHTTPServer(('127.0.0.1',0),HTTP)
    threading.Thread(target=server.serve_forever,daemon=True).start();prefix=f'http://10.0.2.2:{server.server_port}'
    try:
        with contextlib.closing(Guest(iso,seed,'forms',['-cpu','max','-rtc','base=utc','-nic','user,model=pcnet'])) as g:
            g.boot();browser(g);open_url(g,prefix+'/form');page_pixels(g,'Search form','form-initial')
            before=len(g.d.log());count=len(HTTP.seen);click(g.d,g.window,80,190)
            g.wait(lambda s:'BROWSER: form blocked Fill in the required field' in s[before:]);assert len(HTTP.seen)==count
            g.d.type('gatha os');g.d.move(4,4)
            g.d.text_pixels(g.window['x']+30,g.window['y']+178,'gatha os','form-typed')
            submit(g,lambda:g.d.tap('ret'));page_pixels(g,'Results for: gatha os','form-results')
            expected='/search?q=gatha+os&tag=one%26two&tag=three&go=find'
            assert HTTP.seen[-1][0]==expected,HTTP.seen[-1]
            # The same submitted URL must make a new request, not just an anchor jump.
            count=len(HTTP.seen);submit(g,lambda:click(g.d,g.window,80,190));assert len(HTTP.seen)==count+1 and HTTP.seen[-1][0]==expected
            toolbar(g,200,'BROWSER: loaded');assert HTTP.seen[-1][0]==expected
            toolbar(g,40,'BROWSER: loaded');toolbar(g,40,'BROWSER: loaded');page_pixels(g,'Search form','form-back')
            click(g.d,g.window,60,150);g.d.type('a +')
            g.d.key('shift',True);g.d.tap('7');g.d.key('shift',False);g.d.type('=')
            submit(g,lambda:click(g.d,g.window,80,190));assert HTTP.seen[-1][0]=='/search?q=a+%2B%26%3D&tag=one%26two&tag=three&go=find'
            print('FORMS: required, query bytes, resubmit and history PASS',flush=True)
            open_url(g,prefix+'/fields');g.d.tap('tab');g.d.type('wrong');g.d.tap('tab');g.d.type('abcdef')
            g.d.tap('tab');g.d.key('shift',True);g.d.tap('tab');g.d.key('shift',False)
            g.d.key('ctrl',True);g.d.tap('a');g.d.key('ctrl',False);g.d.type('efgh')
            submit(g,lambda:g.d.tap('ret'));assert HTTP.seen[-1][0]=='/echo?read=locked&short=efgh&go=ok',HTTP.seen[-1]
            open_url(g,prefix+'/multiple');click(g.d,g.window,60,250);g.d.type('second value')
            submit(g,lambda:g.d.tap('ret'));assert HTTP.seen[-1][0]=='/echo?second=second+value',HTTP.seen[-1]
            open_url(g,prefix+'/disabled');g.d.tap('tab');g.d.type('enabled');count=len(HTTP.seen);before=len(g.d.log());g.d.tap('ret')
            g.wait(lambda s:'BROWSER: form blocked Use an enabled submit button' in s[before:]);assert len(HTTP.seen)==count
            submit(g,lambda:click(g.d,g.window,80,230));assert HTTP.seen[-1][0]=='/echo?q=enabled'
            print('FORMS: focus, field rules and ownership PASS',flush=True)
            for route in ['post','password','unsupported','override','badchars','scheme','oversize']:
                open_url(g,prefix+'/'+route);count=len(HTTP.seen);before=len(g.d.log());g.d.tap('tab');g.d.tap('ret')
                g.wait(lambda s:'BROWSER: form blocked' in s[before:]);time.sleep(.2);assert len(HTTP.seen)==count
                g.d.screen('form-blocked-'+route)
            print('FORMS: unsupported and oversized submissions blocked PASS',flush=True)
            for route in ['failure','cancel']:
                open_url(g,prefix+'/'+route);g.d.tap('tab');g.d.type('keep this');before=len(g.d.log());g.d.tap('ret')
                if route=='cancel':g.wait(lambda s:'BROWSER: request' in s[before:]);g.d.tap('esc')
                g.wait(lambda s:'BROWSER: error' in s[before:]);g.d.move(4,4)
                g.d.text_pixels(g.window['x']+30,g.window['y']+178,'keep this','form-preserved-'+route)
            open_url(g,prefix+'/form');g.d.click(g.window['x']+g.window['w']-46,g.window['y']+14)
            g.d.wait(lambda _:g.d.windows()[g.window['id']]['w']==1016,'form maximize');g.window=g.d.windows()[g.window['id']]
            click(g.d,g.window,60,150);g.d.type('wide page');submit(g,lambda:g.d.tap('ret'));assert HTTP.seen[-1][0].startswith('/search?q=wide+page&')
            g.d.screen('form-maximized-results')
            count=len(HTTP.seen);open_url(g,'about:forms');g.d.tap('tab');g.d.type('local demo')
            submit(g,lambda:g.d.tap('ret'));page_pixels(g,'Local form preview','form-offline-preview');assert len(HTTP.seen)==count
            toolbar(g,40,'BROWSER: loaded');page_pixels(g,'Offline form demo - no data is sent to the internet.','form-offline-demo');assert len(g.d.windows())==1
            assert all(headers.get('User-Agent')=='GathaOS/13' for _,headers in HTTP.seen)
            assert not any(path.startswith('/forbidden') for path,_ in HTTP.seen)
            (g.root/'http-requests.json').write_text(json.dumps(HTTP.seen,indent=2)+'\n')
            g.finish(['real GET query bytes: spaces/reserved characters, hidden duplicate names, selected submit',
                      'required input focuses field without a request','typing and Enter/click submit','same URL resubmits',
                      'result pixels, back/reload','Tab/Shift+Tab/Ctrl+A, readonly, disabled, maxlength',
                      'separate form ownership','disabled default blocks Enter','unsupported and overflow forms send nothing',
                      'failure/cancellation retain edited input','maximized controls and no desktop click-through',
                      'offline form demo makes no request','process memory reclamation'])
    finally:server.shutdown();server.server_close()

if __name__=='__main__':test(*(Path(p).resolve() for p in sys.argv[1:3]))
