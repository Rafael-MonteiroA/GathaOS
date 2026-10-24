"""Independent host zlib creates vectors; no host library enters the guest."""
import gzip
import os
from pathlib import Path
import random
import struct
import subprocess
import zlib

def compress(plain,window=15):
    encoder=zlib.compressobj(wbits=window)
    return encoder.compress(plain)+encoder.flush()

def gzip_options(plain):
    raw=compress(plain,-15)
    header=b'\x1f\x8b\x08\x1f'+b'\0'*4+b'\0\xff'+b'\x03\0abc'+b'page.html\0comment\0'
    return header+struct.pack('<H',zlib.crc32(header)&65535)+raw+struct.pack('<II',zlib.crc32(plain),len(plain))

def main():
    Path('build').mkdir(exist_ok=True);vectors=[];seeds=[];rng=random.Random(1401951)
    def add(enc,data,plain=b'',capacity=None,error=0):
        vectors.append((enc,len(plain) if capacity is None else capacity,data,error,plain))
    for length in [0,1,2,3,7,31,128,257,258,259,4096,32768,65537,524288,4194304]:
        for plain in [b'A'*length,bytes(rng.randrange(256) for _ in range(length))]:
            for level,strategy in [(0,zlib.Z_DEFAULT_STRATEGY),(6,zlib.Z_FIXED),(9,zlib.Z_DEFAULT_STRATEGY)]:
                for enc,window in [(1,31),(2,15)]:
                    compressor=zlib.compressobj(level,zlib.DEFLATED,window,8,strategy)
                    encoded=compressor.compress(plain)+compressor.flush();add(enc,encoded,plain)
                    if length:add(enc,encoded,capacity=length-1,error=27)
                    if 7<=length<=4096:seeds.append((enc,encoded))
    plain=b'Original GathaOS decoder. '*3000
    add(1,gzip_options(plain),plain)
    add(1,gzip.compress(b'first')+gzip.compress(b' second'),b'first second')
    add(1,gzip.compress(b'')*32,b'')
    add(1,gzip.compress(b'')*33,error=90)
    def named_empty(n):
        member=gzip.compress(b'',mtime=0)
        return member[:3]+b'\x08'+member[4:10]+b'x'*(n-len(member)-1)+b'\0'+member[10:]
    # The second callback lands in the next member's magic bytes; its error
    # must remain cancellation/timeout, never become a malformed-header error.
    add(1,named_empty(8192)+named_empty(8191)+gzip.compress(b'')+named_empty(8192)*3,b'')
    for window in range(9,16):add(2,compress(plain,window),plain)
    for enc,window in [(1,31),(2,15)]:
        compressor=zlib.compressobj(wbits=window);encoded=b''
        for chunk in [b'first '*100,b'second '*100,b'last '*100]:encoded+=compressor.compress(chunk)+compressor.flush(zlib.Z_SYNC_FLUSH)
        encoded+=compressor.flush();add(enc,encoded,b'first '*100+b'second '*100+b'last '*100)
        encoded=compress(b'truncation specimen '*10,window)
        for cut in range(len(encoded)):add(enc,encoded[:cut],capacity=1024,error=1)
        add(enc,encoded+b'junk',capacity=1024,error=1)
        for offset in [-1,-4,-5]:
            damaged=bytearray(encoded);damaged[offset]^=1;add(enc,damaged,capacity=1024,error=1)
    dictionary=b'dictionary';c=zlib.compressobj(zdict=dictionary)
    add(2,c.compress(b'dictionary dictionary')+c.flush(),capacity=1024,error=95)
    add(2,compress(b'raw fallback forbidden',-15),capacity=1024,error=1)
    # Explicit invalid block type and stored-block length complement.
    for raw in [b'\x07',b'\x01\x01\0\x01\0X']:
        add(1,b'\x1f\x8b\x08'+b'\0'*7+raw+b'\0'*8,capacity=1024,error=1)
    bad=bytearray(gzip_options(b'header'));bad[12]^=1;add(1,bad,capacity=1024,error=1)
    add(1,b'\x1f\x8b\x08\x08'+b'\0'*6+b'x'*8193,capacity=1024,error=90)
    add(1,b'\x1f\x8b\x08\x20'+b'\0'*6,capacity=1024,error=1)
    add(1,gzip.compress(b'a'*524289),capacity=524288,error=27)
    # Mutants must agree with an independent decoder, with the same output cap
    # and complete-input rule. Seeds here contain exactly one gzip/zlib member.
    for _ in range(20000):
        enc,seed=rng.choice(seeds);data=bytearray(seed)
        for _ in range(rng.randrange(1,5)):
            action=rng.randrange(3);at=rng.randrange(len(data)+1)
            if action==0 and at<len(data):data[at]^=1<<rng.randrange(8)
            elif action==1:data[at:at]=rng.randbytes(rng.randrange(1,5))
            else:del data[at:at+rng.randrange(1,5)]
        try:
            dec=zlib.decompressobj(31 if enc==1 else 15);out=dec.decompress(data,65537)
            if not dec.eof or dec.unused_data or dec.unconsumed_tail or len(out)>65536:raise zlib.error('bound or trailing bytes')
            add(enc,data,out,65536)
        except zlib.error:add(enc,data,capacity=65536,error=1)
    path=Path('build/inflate-vectors.bin')
    with path.open('wb') as stream:
        stream.write(struct.pack('<I',len(vectors)))
        for enc,cap,data,error,plain in vectors:
            stream.write(struct.pack('<IIIII',enc,cap,len(data),error,len(plain)));stream.write(data);stream.write(plain)
    subprocess.run(['clang','-std=c11','-O1','-g','-no-pie','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','tools/inflate_native_test.c','libgatha/inflate.c','-o','build/inflate-native-test'],check=True)
    subprocess.run(['build/inflate-native-test',str(path)],check=True,timeout=180,
                   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':main()
