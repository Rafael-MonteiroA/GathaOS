"""Exercise each real user-mode application from the desktop launch menu."""
import re
import time
from desktop_test import Desktop

def launch(d,index):
    before=set(d.windows());d.click(40,d.height-22);d.click(60,d.height-326+index*30)
    d.wait(lambda _:bool(set(d.windows())-before),f'launch application {index}')
    return d.windows()[(set(d.windows())-before).pop()]

def click(d,w,x,y): d.click(w['x']+2+x,w['y']+32+y)
def field(d,w,x,y,text):
    click(d,w,x,y);d.key('ctrl',True);d.tap('a');d.key('ctrl',False);d.type(text)
def command(d,text):
    marker='SHELL: command '+text.split()[0]+' completed'
    count=d.log().count(marker);d.type(text+'\n');d.wait(lambda log:log.count(marker)>count,text)

def exercise_apps(request,evidence,serial,width,height,scan_set):
    d=Desktop(request,evidence,serial,width,height)
    d.wait(lambda log:'STAGE7: PASS' in log,'stage seven desktop')
    assert f'PS2: keyboard set={scan_set} translation=off' in d.log()
    for w in reversed(list(d.windows().values())): d.close(w)
    w=launch(d,2)
    field(d,w,130,50,'/Documents/Test.txt')
    field(d,w,100,100,'Hello GathaOS\nSaved from ring3.')
    d.key('ctrl',True);d.tap('s');d.key('ctrl',False)
    d.wait(lambda log:'EDITOR: saved' in log,'save editor text');d.screen('editor-saved')
    click(d,w,120,14);d.wait(lambda log:'EDITOR: opened' in log,'reopen editor text')
    d.move(4,4);d.text_pixels(w['x']+22,w['y']+118,'Hello GathaOS','editor-reopened');d.close(w)
    w=launch(d,3);d.type('12.5+7.5=')
    d.wait(lambda log:'CALC: result 20\n' in log,'decimal addition');d.screen('calculator')
    d.type('c9/4=');d.wait(lambda log:'CALC: result 2.25\n' in log,'decimal division');d.close(w)
    w=launch(d,4);click(d,w,200,98);d.wait(lambda log:'SETTINGS: wallpaper changed' in log,'wallpaper')
    click(d,w,200,176);d.wait(lambda log:'SETTINGS: accent changed' in log,'accent');d.screen('settings');d.close(w)
    w=launch(d,5);d.wait(lambda log:'SYSMON: refreshed' in log,'system monitor');d.screen('system-monitor');d.close(w)
    w=launch(d,6);click(d,w,110,60)
    d.move(w['x']+2+40,w['y']+32+110);d.button(True);d.move(w['x']+2+160,w['y']+32+170);d.button(False)
    d.wait(lambda log:'PAINT: stroke' in log,'pencil stroke');click(d,w,495,432)
    d.wait(lambda log:'PAINT: saved' in log,'save bitmap');d.screen('paint-saved')
    click(d,w,450,22);click(d,w,410,432);d.wait(lambda log:'PAINT: opened' in log,'reopen bitmap');d.screen('paint-reopened');d.close(w)
    w=launch(d,1);field(d,w,150,52,'/Documents');click(d,w,550,54)
    field(d,w,120,344,'Temp.txt');click(d,w,50,382);d.wait(lambda log:'FILES: created' in log,'file creation')
    # Stable inode-order entries: Welcome, Test, Drawing, Temp.
    click(d,w,100,110+3*24+12);field(d,w,120,344,'Renamed.txt');click(d,w,290,382)
    d.wait(lambda log:'FILES: renamed' in log,'file rename');click(d,w,100,110+3*24+12);click(d,w,410,382)
    click(d,w,420,265);d.wait(lambda log:'FILES: deleted' in log,'delete confirmation');d.screen('files')
    click(d,w,100,110+24+12);click(d,w,220,14)
    d.wait(lambda _:len(d.windows())==2,'file association opens editor');d.screen('file-open-editor')
    for window in reversed(list(d.windows().values())):d.close(window)
    w=launch(d,1);field(d,w,150,52,'/System/Apps');click(d,w,550,54);click(d,w,100,122);click(d,w,220,14)
    d.wait(lambda log:'name=GEX program' in log and len(d.windows())==2,'GEX file launch')
    d.screen('gex-from-disk')
    for window in reversed(list(d.windows().values())):d.close(window)
    w=launch(d,0)
    for text in ['help','ls','cd /Documents','ls','cat Test.txt','mkdir Scratch','cp Test.txt Copy.txt','mv Copy.txt Moved.txt','cat Moved.txt','rm Moved.txt','rm Scratch','echo shell works','ps','mem','uptime','clear']:
        command(d,text)
    d.screen('terminal');d.close(w)
    for marker in ['mkdir OK','cp OK','mv OK','rm OK','Hello GathaOS','Saved from ring3.','shell works']:
        assert 'SHELL OUT: '+marker in d.log(),marker
    w=launch(d,7);d.screen('about');d.close(w)
    d.wait(lambda log:len(re.findall(r'TASK: exit pid=\d+ code=0x00000000',log))>=10,'application exits')
    assert 'PANIC' not in d.log()
    return {'result':'PASS','apps':['terminal','files','editor','calculator','settings','sysmon','paint','about'],'checks':['start-menu processes','save/reopen text','decimal math','theme','task/memory view','bitmap save/reopen','file create/rename/delete/association','shell filesystem commands','all app exits']}

def verify_disk(path):
    from mkgathafs import read_file,validate
    import struct
    image=path.read_bytes();nodes=validate(image)
    assert read_file(image,'/Documents/Test.txt')==b'Hello GathaOS\nSaved from ring3.'
    drawing=read_file(image,'/Documents/Drawing.gbm')
    assert len(drawing)==41008 and struct.unpack_from('<4I',drawing)==(0x324d4247,256,160,8)
    pixels=drawing[48:]
    assert pixels[13*256+14]==2 and pixels[43*256+74]==2,'Saved stroke endpoints'
    assert all(c<8 for c in pixels) and pixels[0]==0
    names={n[6].split(b'\0',1)[0] for n in nodes if n[0]}
    assert not names.intersection({b'Temp.txt',b'Renamed.txt',b'Copy.txt',b'Moved.txt',b'Scratch'})
    return {'result':'PASS','text_bytes':len(read_file(image,'/Documents/Test.txt')),'bitmap_bytes':len(drawing),'bitmap_stroke_verified':True}
