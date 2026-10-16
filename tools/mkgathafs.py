"""Original GathaFS v1 builder, reader and structural verifier (host only)."""
from pathlib import Path
import struct
import sys
BLOCK = 4096
INODES = 128
DIRECT = 48
BLOCKS = 8192
STAMP = 1767225600
NODE = struct.Struct('<6I40s48I')

def hash_bytes(data):
    h = 2166136261
    for byte in data: h = ((h ^ byte)*16777619) & 0xffffffff
    return h

def create(folder):
    image = bytearray(BLOCKS*BLOCK)
    bitmap = bytearray(BLOCK)
    for i in range(10): bitmap[i//8] |= 1 << (i%8)
    header = struct.pack('<11I',0x31534647,1,BLOCK,BLOCKS,INODES,1,2,10,0,STAMP,0)
    image[:48] = header+struct.pack('<I',hash_bytes(header))
    records = [(None,0,'/',2)]
    def descend(path,parent):
        for child in sorted(path.iterdir(),key=lambda p:p.name):
            if child.is_symlink(): raise ValueError('Symlinks are not supported')
            encoded = child.name.encode('ascii')
            if not 0 < len(encoded) < 40 or child.name in ('.','..'): raise ValueError('Invalid filename')
            index = len(records)
            if index >= INODES: raise ValueError('Inode table full')
            records.append((child,parent,child.name,2 if child.is_dir() else 1))
            if child.is_dir(): descend(child,index)
    descend(folder,0)
    next_block = 10
    for i,(path,parent,name,kind) in enumerate(records):
        data = path.read_bytes() if kind == 1 else b''
        if len(data) > DIRECT*BLOCK: raise ValueError('File exceeds 192 KiB')
        blocks = []
        for offset in range(0,len(data),BLOCK):
            if next_block >= BLOCKS: raise ValueError('Disk full')
            block = next_block; next_block += 1
            blocks.append(block); bitmap[block//8] |= 1 << (block%8)
            chunk = data[offset:offset+BLOCK]
            image[block*BLOCK:block*BLOCK+len(chunk)] = chunk
        node = NODE.pack(1,kind,parent,len(data),STAMP,STAMP,name.encode('ascii'),*(blocks+[0]*(DIRECT-len(blocks))))
        image[2*BLOCK+i*256:2*BLOCK+(i+1)*256] = node
    image[BLOCK:2*BLOCK] = bitmap
    validate(image)
    return image

def validate(image):
    if len(image) < 10*BLOCK: raise ValueError('Truncated volume')
    h = struct.unpack_from('<12I',image)
    if h[:3] != (0x31534647,1,BLOCK) or not 16 <= h[3] <= 32768 or len(image) != h[3]*BLOCK or h[4:9] != (128,1,2,10,0) or h[10] or h[11] != hash_bytes(image[:44]):
        raise ValueError('Invalid superblock')
    nodes = [NODE.unpack_from(image,2*BLOCK+i*256) for i in range(INODES)]
    used = set(range(10)); names = set()
    for i,n in enumerate(nodes):
        if not n[0]: continue
        if n[1] not in (1,2) or n[2] >= INODES or not nodes[n[2]][0] or nodes[n[2]][1] != 2 or n[3] > DIRECT*BLOCK:
            raise ValueError('Invalid inode')
        if b'\0' not in n[6]: raise ValueError('Unterminated name')
        name = n[6].split(b'\0',1)[0]
        if not name or (i and (b'/' in name or name in (b'.',b'..'))) or (n[2],name) in names: raise ValueError('Invalid name')
        names.add((n[2],name))
        ancestors = set(); parent = i
        while parent:
            if parent >= INODES or parent in ancestors or not nodes[parent][0]: raise ValueError('Directory cycle')
            ancestors.add(parent); parent = nodes[parent][2]
        count = (n[3]+BLOCK-1)//BLOCK
        if n[1] == 2 and n[3]: raise ValueError('Directory data not allowed')
        for j,block in enumerate(n[7:]):
            if j >= count:
                if block: raise ValueError('Trailing block reference')
            elif block < 10 or block >= h[3] or block in used: raise ValueError('Duplicate/invalid data block')
            else: used.add(block)
    if not nodes[0][0] or nodes[0][1:3] != (2,0) or nodes[0][6].split(b'\0',1)[0] != b'/': raise ValueError('Invalid root')
    for block in range(BLOCK*8):
        if bool(image[BLOCK+block//8] & (1 << (block%8))) != (block in used): raise ValueError('Bitmap mismatch')
    return nodes

def read_file(image,path):
    nodes = validate(image); current = 0
    for component in path.strip('/').split('/'):
        found = [i for i,n in enumerate(nodes) if n[0] and n[2] == current and n[6].split(b'\0',1)[0] == component.encode('ascii')]
        if len(found) != 1: raise ValueError('File not found: '+path)
        current = found[0]
    n = nodes[current]
    if n[1] != 1: raise ValueError('Not a file')
    return b''.join(image[b*BLOCK:(b+1)*BLOCK] for b in n[7:] if b)[:n[3]]

def main():
    if sys.argv[1] == 'check':
        validate(Path(sys.argv[2]).read_bytes()); print('GATHAFS CHECK PASS'); return
    if sys.argv[1] == 'read':
        sys.stdout.buffer.write(read_file(Path(sys.argv[2]).read_bytes(),sys.argv[3])); return
    source,target = map(Path,sys.argv[1:3]); data = create(source)
    target.parent.mkdir(parents=True,exist_ok=True)
    if target.exists() and target.read_bytes() != data: raise SystemExit('Refusing to replace existing disk image: '+str(target))
    if not target.exists(): target.write_bytes(data)
    print(f'GATHAFS: {target}, {len(data)} bytes')

if __name__ == '__main__': main()
