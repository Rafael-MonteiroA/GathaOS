"""Exercise GathaFS host format bounds, content and corruption rejection."""
from pathlib import Path
import struct
import tempfile
from mkgathafs import create,validate,read_file,BLOCK

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root/'Docs').mkdir()
    content = bytes((i*37+11)&255 for i in range(9000))
    (root/'Docs'/'sample.bin').write_bytes(content)
    image = create(root)
    assert read_file(image,'/Docs/sample.bin') == content
    assert create(root) == image
    for position,value in [(0,0),(BLOCK+1,255),(2*BLOCK+2*256+64,0),(2*BLOCK+256+8,127)]:
        broken = bytearray(image); broken[position] = value
        try: validate(broken)
        except ValueError: pass
        else: raise AssertionError(f'Accepted corruption at {position}')
    (root/'too-large').write_bytes(b'x'*(192*1024+1))
    try: create(root)
    except ValueError: pass
    else: raise AssertionError('Accepted oversized file')
print('GATHAFS HOST TEST: deterministic image, multiblock data, bounds, corruption PASS')
