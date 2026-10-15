"""Pack an original fixed-address GEX1 executable; optionally embed as C data."""
from pathlib import Path
import struct
import subprocess
import sys

def checksum(data):
    n = 2166136261
    for value in data: n = ((n ^ value) * 16777619) & 0xffffffff
    return n

def main():
    elf, raw, output = map(Path, sys.argv[1:4])
    symbols = {parts[2]: int(parts[0],16) for line in subprocess.check_output(
        ['llvm-nm','--defined-only',str(elf)],text=True).splitlines() if len(parts := line.split()) == 3}
    data = raw.read_bytes(); base = 0x400000
    assert len(data) == symbols['__user_file_end'] - base
    header = struct.pack('<8I',0x31584547,1,base,symbols['_start']-base,
                         symbols['__user_rx_end']-base,len(data),symbols['__user_mem_end']-base,checksum(data))
    output.write_bytes(header+data)
    if len(sys.argv) > 4:
        name = sys.argv[4]
        output.with_suffix('.c').write_text('#include <stdint.h>\nconst uint8_t '+name+'[] = {\n'+
            ','.join(str(x) for x in header+data)+'\n};\nconst unsigned '+name+'_size = '+str(len(header+data))+';\n')

if __name__ == '__main__': main()
