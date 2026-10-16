"""Drive the real PS/2 path and inspect desktop pixels and guest event results."""
import re
import time
from pathlib import Path
from input_test import read_screen,pixel

class Desktop:
    def __init__(self,request,evidence,serial,width,height):
        self.request,self.evidence,self.serial,self.width,self.height=request,evidence,serial,width,height
    def log(self): return self.serial.read_text().replace('\0','')
    def wait(self,predicate,description):
        deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            log=self.log()
            assert 'PANIC' not in log and 'FAIL' not in log,log[-3000:]
            if predicate(log): return log
            time.sleep(.04)
        raise AssertionError(description+'\n'+self.log()[-3000:])
    def event(self,kind,data): self.request('input-send-event',{'events':[{'type':kind,'data':data}]})
    def mouse(self):
        matches=re.findall(r'INPUT: mouse x=(\d+) y=(\d+) buttons=(\d+)',self.log())
        return tuple(map(int,matches[-1])) if matches else (self.width//2,self.height//2,0)
    def move(self,x,y):
        for _ in range(80):
            cx,cy,_=self.mouse()
            if (cx,cy)==(x,y): return
            dx=max(-80,min(80,x-cx));dy=max(-80,min(80,y-cy))
            self.request('input-send-event',{'events':[{'type':'rel','data':{'axis':'x','value':dx}},{'type':'rel','data':{'axis':'y','value':dy}}]})
            self.wait(lambda _:self.mouse()[:2]==(cx+dx,cy+dy),'mouse move')
        raise AssertionError('mouse target not reached')
    def button(self,down,button='left'):
        self.event('btn',{'down':down,'button':button});time.sleep(.12)
    def click(self,x,y,button='left'):
        self.move(x,y);self.button(True,button);self.button(False,button);time.sleep(.15)
    def key(self,name,down):
        self.event('key',{'down':down,'key':{'type':'qcode','data':name}});time.sleep(.04)
    def tap(self,name): self.key(name,True);self.key(name,False)
    def type(self,text):
        mapping={' ':'spc','\n':'ret','.':'dot','/':'slash','-':'minus','=':'equal',':':'semicolon'}
        for ch in text:
            shift=ch.isupper() or ch==':'
            if shift:self.key('shift',True)
            self.tap(mapping.get(ch,ch.lower()))
            if shift:self.key('shift',False)
    def windows(self):
        found={}
        for action,id_,owner,x,y,w,h in re.findall(r'WM: (\S+) id=(\d+) owner=(\d+) x=(\d+) y=(\d+) w=(\d+) h=(\d+)',self.log()):
            id_=int(id_)
            if action=='closed':found.pop(id_,None)
            else: found[id_]={'id':id_,'owner':int(owner),'x':int(x),'y':int(y),'w':int(w),'h':int(h)}
        return found
    def screen(self,name): return read_screen(self.request,self.evidence,name,self.width,self.height)
    def wait_pixel(self,x,y,color,name):
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            p=self.screen(name)
            if pixel(p,self.width,x,y)==color:return p
            time.sleep(.05)
        raise AssertionError(f'{name}: pixel {(x,y)} {pixel(p,self.width,x,y):06x} != {color:06x}')
    def close(self,w):
        self.click(w['x']+w['w']-16,w['y']+14)
        self.wait(lambda _:w['id'] not in self.windows(),'window close')
    def text_pixels(self,x,y,text,name):
        font={int(code):bytes(int(v,16) for v in values.split(',')) for values,code in re.findall(r'\{(0x[0-9a-f,x]+)\}, /\* ASCII (\d+) \*/',Path('gui/gfx/font.h').read_text())}
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            p=self.screen(name)
            if all(pixel(p,self.width,x+i*8+gx,y+gy)==(0x20344d if font[ord(c)][gy]&(128>>gx) else 0xffffff) for i,c in enumerate(text) for gy in range(16) for gx in range(8)):return
            time.sleep(.05)
        raise AssertionError('Rendered text mismatch: '+text)

def exercise_desktop(request,evidence,serial,width,height,scan_set):
    d=Desktop(request,evidence,serial,width,height)
    for marker in ['STAGE6: READY','STAGE6: PASS','STAGE4: PASS','GUITEST: invalid pointer and window rejected PASS',f'PS2: keyboard set={scan_set} translation=off','INPUTTEST: packet sign overflow resync key repeat release PASS']:
        d.wait(lambda log:marker in log,marker)
    d.wait(lambda _:len(d.windows())==2,'two initial user windows')
    d.screen('desktop')
    w=list(d.windows().values())[-1];x,y=w['x'],w['y']
    d.wait_pixel(x+4,y+3,0x187e83,'focused-window')
    d.click(x+24,y+32+126)
    d.wait(lambda log:'WIDGET: checkbox' in log,'checkbox event')
    d.click(x+50,y+32+170);d.key('ctrl',True);d.tap('a');d.key('ctrl',False);d.type('GathaOS')
    d.wait(lambda log:'WIDGET: single changed' in log,'textbox input')
    d.move(4,4);d.text_pixels(x+28,y+192,'GathaOS','typed-field')
    d.click(x+50,y+32+226);d.type('\nHello')
    d.wait(lambda log:'WIDGET: multi changed' in log,'multiline input')
    d.click(x+344,y+32+106);d.wait(lambda log:'WIDGET: list' in log,'list selection')
    d.click(x+470,y+32+239);d.wait(lambda log:'WIDGET: scroll' in log,'scrollbar')
    d.click(x+200,y+32+84);d.screen('dialog')
    d.click(x+350,y+32+250);d.wait(lambda log:'WIDGET: dialog closed' in log,'dialog acknowledgement')
    d.click(x+320,y+32+290,'right');d.screen('context-menu')
    d.click(x+350,y+32+330);d.screen('context-dialog')
    d.click(x+350,y+32+250)
    d.move(x+60,y+15);d.button(True);d.move(x+120,y+55);d.button(False)
    d.wait(lambda log:'WM: moved' in log,'drag')
    w=d.windows()[w['id']];assert (w['x'],w['y'])==(x+60,y+40)
    x,y=w['x'],w['y'];d.screen('dragged')
    d.click(x+w['w']-46,y+14)
    d.wait(lambda _:d.windows()[w['id']]['w']==width-8,'maximize')
    d.screen('maximized');d.click(width-54,16)
    d.wait(lambda _:d.windows()[w['id']]['w']==500,'restore')
    d.click(x+w['w']-76,y+14);d.wait(lambda log:'WM: minimized' in log,'minimize')
    d.screen('minimized');d.click(220,height-25)
    d.wait_pixel(x+4,y+3,0x187e83,'restored')
    d.click(40,height-22);d.screen('start-menu');d.click(60,height-96)
    d.wait(lambda _:len(d.windows())==3,'start menu launch')
    for w in reversed(list(d.windows().values())): d.close(w)
    d.click(50,52);d.wait(lambda _:len(d.windows())==1,'desktop icon launch')
    d.close(next(iter(d.windows().values())))
    d.move(4,4);baseline=d.screen('empty-desktop')
    d.move(width-10,height-60);d.move(4,4);restored=d.screen('cursor-restored')
    assert baseline[:width*(height-42)*3]==restored[:width*(height-42)*3],'cursor trails'
    return {'result':'PASS','windows_created':4,'checks':['user GUI pointer validation','focus','checkbox','single/multiline input','list','dialog','drag','maximize/restore','minimize/taskbar','start menu','desktop icon','close/reap','cursor restoration']}
