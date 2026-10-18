"""Exercise GathaFS host format bounds, content and corruption rejection."""
from pathlib import Path
import struct
import tempfile
from mkgathafs import create,validate,read_file,BLOCK,MAX_SIZE

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
    legacy = create(root,version=1)
    assert read_file(legacy,'/Docs/sample.bin') == content
    # Exercise direct/indirect boundary and the exact maximum, including tails.
    for size in (47*BLOCK,47*BLOCK+1,48*BLOCK,MAX_SIZE):
        large = bytes(range(256))*(size//256)+bytes(range(size%256))
        (root/'large.bin').write_bytes(large)
        image = create(root)
        assert read_file(image,'/large.bin') == large
    nodes = validate(image)
    large_node = next(n for n in nodes if n[6].startswith(b'large.bin\0'))
    pointer = large_node[-1]
    for value,offset in [(9,0),(large_node[7],0),(pointer,0),(8192,0),(10,1023*4)]:
        broken = bytearray(image);struct.pack_into('<I',broken,pointer*BLOCK+offset,value)
        try:validate(broken)
        except ValueError:pass
        else:raise AssertionError('Accepted corrupt indirect block')
    (root/'too-large').write_bytes(b'x'*(MAX_SIZE+1))
    try: create(root)
    except ValueError: pass
    else: raise AssertionError('Accepted oversized file')
print('GATHAFS HOST TEST: v1/v2, deterministic image, 4 MiB, indirect boundaries/corruption PASS')
