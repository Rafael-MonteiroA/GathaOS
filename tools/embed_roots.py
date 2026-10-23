"""Encode reviewed public trust data. Builds are offline and never fetch roots."""
import hashlib
from pathlib import Path
import sys
def main():
    output=Path(sys.argv[1]);paths=sorted(Path('certs').glob('*.der'))
    expected=dict(line.split()[::-1] for line in Path('certs/SHA256SUMS').read_text().splitlines())
    lines=['/* Generated public certificate data; no private keys. */']
    for i,p in enumerate(paths):
        data=p.read_bytes();assert hashlib.sha256(data).hexdigest()==expected[p.name],p
        lines.append(f'static const uint8_t trust_{i}[]={{')
        lines.extend('    '+','.join(f'0x{v:02x}' for v in data[j:j+16])+',' for j in range(0,len(data),16));lines.append('};')
    lines.append('static const struct der trust_roots[]={'+','.join(f'{{trust_{i},sizeof(trust_{i})}}' for i in range(len(paths)))+'};')
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text('\n'.join(lines)+'\n')
if __name__=='__main__':main()
