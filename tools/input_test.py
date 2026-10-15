"""Exercise actual emulated PS/2 devices through QMP; verify visible results."""
from pathlib import Path
import re
import struct
import time
import zlib


def read_screen(request, evidence, name, width, height):
    path = evidence / (name + '.ppm')
    request('screendump', {'filename': str(path)})
    with path.open('rb') as stream:
        assert stream.readline() == b'P6\n'
        assert stream.readline().split() == [str(width).encode(), str(height).encode()]
        assert stream.readline() == b'255\n'
        pixels = stream.read()
    assert len(pixels) == width * height * 3
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    rows = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    path.with_suffix('.png').write_bytes(png + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
    return pixels


def pixel(pixels, width, x, y):
    i = (y * width + x) * 3
    return int.from_bytes(pixels[i:i + 3], 'big')


def verify_text(pixels, width, height, lines, caret):
    font = {int(code): bytes(int(v, 16) for v in values.split(',')) for values, code in
            re.findall(r'\{(0x[0-9a-f,x]+)\}, /\* ASCII (\d+) \*/', Path('gui/gfx/font.h').read_text())}
    assert len(font) == 95
    for y in range(height - 448):
        line, gy = divmod(y, 20)
        for x in range(width - 96):
            col, gx = divmod(x, 8)
            c = lines[line][col] if line < len(lines) and col < len(lines[line]) else ' '
            expected = 0xe2e8f0 if gy < 16 and font[ord(c)][gy] & (128 >> gx) else 0x101e30
            if (col, line) == caret and gx < 2 and gy < 16:
                expected = 0x5eead4
            actual = pixel(pixels, width, 48 + x, 360 + y)
            if actual != expected:
                raise RuntimeError(f'Text pixel mismatch at ({x},{y}): {actual:06x} != {expected:06x}; expected {lines!r}')


def exercise(request, evidence, serial, width, height, scan_set):
    start = serial.read_text()
    for marker in [f'PS2: keyboard set={scan_set} translation=off', 'STAGE3: READY',
                   'GFXTEST: clipping alpha endpoints diagonal PASS',
                   'INPUTTEST: packet sign overflow resync key repeat release PASS']:
        assert marker in start, marker
    def event(kind, data):
        request('input-send-event', {'events': [{'type': kind, 'data': data}]})
    def key(name, down):
        event('key', {'down': down, 'key': {'type': 'qcode', 'data': name}})
        time.sleep(0.035)
    def tap(name, shift=False):
        if shift: key('shift', True)
        key(name, True); key(name, False)
        if shift: key('shift', False)
    def clear():
        key('ctrl', True); tap('l'); key('ctrl', False)
    def move(dx, dy):
        request('input-send-event', {'events': [
            {'type': 'rel', 'data': {'axis': 'x', 'value': dx}},
            {'type': 'rel', 'data': {'axis': 'y', 'value': dy}}]})
        time.sleep(0.06)
    def latest_mouse():
        values = re.findall(r'INPUT: mouse x=(\d+) y=(\d+) buttons=(\d+)', serial.read_text())
        return tuple(map(int, values[-1])) if values else (width // 2, height // 2, 0)
    def move_to(x, y):
        for _ in range(40):
            cx, cy, _ = latest_mouse()
            if (cx, cy) == (x, y): return
            move(max(-100, min(100, x-cx)), max(-100, min(100, y-cy)))
        raise RuntimeError(f'Mouse failed to reach {(x,y)}; got {latest_mouse()}')
    initial = read_screen(request, evidence, 'screen', width, height)
    card = (width - 96) // 3
    second = 48 + card + 16
    third = second + card + 16
    probes = {(0, height-1): 0x0b1424, (52, 211): 0x18283d, (80, 230): 0x5eead4,
              (150, 230): 0x60a5fa, (198, 250): 0xfbbf24,
              (second, 211): 0x38bdf8, (second, 254): 0xa78bfa,
              (third, 220): 0x3b82f6, (third+50, 220): 0x987ad6}
    for point, expected in probes.items():
        assert pixel(initial, width, *point) == expected, f'Primitive sample {point}'
    move_to(8, 140)
    for name, shift in [('g',True),('a',False),('t',False),('h',False),('a',False),
                        ('o',True),('s',True),('spc',False),('3',False),('1',True),('ret',False),
                        ('a',False),('b',False),('c',False),('backspace',False),('tab',False),('z',False)]:
        tap(name, shift)
    tap('caps_lock'); tap('q'); tap('w', True); tap('caps_lock')
    key('shift', True); key('shift_r', True); key('shift', False); tap('e'); key('shift_r', False)
    for modifier in ('ctrl', 'ctrl_r', 'alt', 'alt_r'):
        key(modifier, True); tap('q'); key(modifier, False)
    tap('print'); tap('pause')
    time.sleep(0.15)
    typed = read_screen(request, evidence, 'typed', width, height)
    verify_text(typed, width, height, ['GathaOS 3!', 'ab  zQwE'], (8, 1))
    # Movement over both artwork and text must restore every background pixel.
    baseline = typed
    move_to(100,230)
    move_to(width-1,height-1); move(100,100)
    assert latest_mouse()[:2] == (width-1,height-1), 'Bottom/right edge clamp'
    move_to(0,0); move(-100,-100)
    assert latest_mouse()[:2] == (0,0), 'Top/left edge clamp'
    move_to(400,390)
    for _ in range(30): move(2,1)
    assert latest_mouse()[:2] == (460,420), 'Continuous relative motion'
    move_to(8,140)
    restored = read_screen(request, evidence, 'cursor-restored', width, height)
    assert baseline == restored, 'Cursor left trails or corrupted underlying text'
    for button, mask, index in [('left',1,0), ('middle',4,1), ('right',2,2)]:
        event('btn', {'down':True, 'button':button}); time.sleep(0.1)
        assert latest_mouse()[2] == mask
        pressed = read_screen(request, evidence, 'button-'+button, width, height)
        assert pixel(pressed,width,width-216+index*48,height-52) == 0x168b80
        event('btn', {'down':False, 'button':button}); time.sleep(0.1)
        assert latest_mouse()[2] == 0
    clear()
    before = len(serial.read_text())
    key('r', True); time.sleep(0.8); key('r', False); time.sleep(0.15)
    repeats = re.findall(r'INPUT: key=19 down=1 ascii=114 mods=0 repeat=([01])', serial.read_text()[before:])
    assert 5 <= len(repeats) <= 20 and repeats[0] == '0' and all(r == '1' for r in repeats[1:])
    stopped = len(re.findall(r'INPUT: key=19 down=1', serial.read_text()))
    time.sleep(0.3)
    assert len(re.findall(r'INPUT: key=19 down=1', serial.read_text())) == stopped
    repeated = read_screen(request, evidence, 'repeat', width, height)
    verify_text(repeated, width, height, ['r' * len(repeats)], (len(repeats),0))
    # Scroll beyond the visible area and confirm only the newest lines remain.
    clear()
    rows = (height - 448) // 20
    for _ in range(rows + 3): tap('x'); tap('ret')
    scrolled = read_screen(request, evidence, 'scrolled', width, height)
    verify_text(scrolled, width, height, ['x'] * (rows-1), (0,rows-1))
    clear()
    for c in 'GathaOS 3!':
        tap('spc' if c == ' ' else '1' if c == '!' else c.lower(), c.isupper() or c == '!')
    time.sleep(0.1)
    final = read_screen(request, evidence, 'final', width, height)
    verify_text(final, width, height, ['GathaOS 3!'], (10,0))
    output = serial.read_text()
    assert not any(word in output for word in ('PANIC', 'FAIL', 'dropped bytes'))
    return {'scancode_set': scan_set, 'typed_lines': ['GathaOS 3!', 'ab  zQwE'],
            'repeat_characters': len(repeats), 'scroll_rows': rows+3,
            'cursor_restore_pixels_compared': width*height,
            'buttons_tested': ['left','middle','right'], 'result':'PASS'}
