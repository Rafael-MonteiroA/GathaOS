"""Certificate validation positives/negatives plus sanitizer parser mutations."""
import ctypes as c
from pathlib import Path
import subprocess
from tls_fixtures import make,NOW

def main():
    folder=make('build/test-pki')
    sources=['libgatha/tls/x509.c',*[str(p) for p in Path('libgatha/crypto').glob('*.c')]]
    subprocess.run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',*sources,'-o','build/x509-test.so'],check=True)
    lib=c.CDLL(str(Path('build/x509-test.so').resolve()))
    class DER(c.Structure):_fields_=[('p',c.c_void_p),('n',c.c_uint)]
    def verify(names,host=b'localhost',root='root',now=NOW,mutation=False):
        data=[(folder/(n+'.der')).read_bytes() for n in names]
        if mutation:data[0]=data[0][:-1]+bytes([data[0][-1]^1])
        buffers=[c.create_string_buffer(d) for d in data];chain=(DER*len(data))(*[DER(c.addressof(b),len(d)) for b,d in zip(buffers,data)])
        trust=(folder/(root+'.der')).read_bytes();anchor=c.create_string_buffer(trust);anchors=(DER*1)(DER(c.addressof(anchor),len(trust)));out=c.create_string_buffer(520)
        return lib.x509_chain(chain,len(data),anchors,1,host,c.c_uint32(now),out)
    for host in [b'localhost',b'LOCALHOST',b'10.0.2.2',b'127.0.0.1',b'one.unit.test']:
        assert verify(['good','intermediate'],host)==0,host
    for host in [b'other.invalid',b'unit.test',b'a.b.unit.test',b'10.0.2.3',b'localhost.attacker',b'']:
        assert verify(['good','intermediate'],host)!=0,host
    for name in ['expired','future','wrong-name','no-san','client-only','unknown-critical','must-staple','sha1','foreign','ca-leaf']:
        assert verify([name,'intermediate'])!=0,name
    for issuer in ['bad-ca','constraints']:assert verify(['good',issuer])!=0,issuer
    assert verify(['good','intermediate'],root='root-short')!=0
    assert verify(['good','intermediate'],mutation=True)!=0
    assert verify(['good'])!=0
    assert verify(['good','intermediate'],now=0)!=0
    for p in Path('certs').glob('*.der'):
        data=p.read_bytes();b=c.create_string_buffer(data);out=c.create_string_buffer(2048);assert lib.x509_parse(DER(c.addressof(b),len(data)),out)==0,p
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-I.','tools/x509_fuzz.c',*sources,'-o','build/x509-fuzz'],check=True)
    subprocess.run(['build/x509-fuzz',str(folder/'good.der')],check=True)
    print('X509 PASS: chains, SAN/IP/wildcards, validity, CA/path/EKU/critical constraints, signatures, DER bounds and sanitizer mutations',flush=True)
if __name__=='__main__':main()
