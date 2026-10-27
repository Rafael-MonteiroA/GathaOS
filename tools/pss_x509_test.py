"""PSS certificate parameter policy, restricted keys and parser mutations."""
import ctypes as c
from pathlib import Path
import subprocess
from pss_fixtures import make
from tls_fixtures import NOW


def main():
    folder=make("build/pss-pki")
    sources=["libgatha/tls/x509.c", *map(str,Path("libgatha/crypto").glob("*.c"))]
    subprocess.run(["clang","-std=c11","-O2","-Wall","-Wextra","-Werror","-shared","-fPIC",*sources,"-o","build/pss-x509.so"],check=True)
    lib=c.CDLL(str(Path("build/pss-x509.so").resolve()))
    class DER(c.Structure):
        _fields_=[("p",c.c_void_p),("n",c.c_uint)]
    def check(leaf,issuer="pss-intermediate",root="root",success=True):
        data=[(folder/(n+".der")).read_bytes() for n in (leaf,issuer,root)]
        buffers=[c.create_string_buffer(d) for d in data]
        values=[DER(c.addressof(b),len(d)) for b,d in zip(buffers,data)]
        chain=(DER*2)(*values[:2]);anchor=(DER*1)(values[2]);out=c.create_string_buffer(1024)
        result=lib.x509_chain(chain,2,anchor,1,b"localhost",c.c_uint32(NOW),out)
        assert (result==0)==success,(leaf,issuer,root,result)
    good=("sha256","sha384","sha512","salt-zero","salt-default","salt-maximum",
          "explicit-defaults","hash-null-absent","pss-leaf","restricted-leaf")
    for name in good:
        check(name)
        check(name,root="unrestricted-root")
    for name in ("pss-wrong-name","pss-expired","bad-mgf","bad-salt","bad-trailer",
                 "missing-params","default-sha1","duplicate-hash","reordered",
                 "missing-mgf","unknown-field","negative-salt","oversized-salt","outer-mismatch"):
        check(name,success=False)
    check("sha256","restricted-intermediate")
    check("sha384","restricted-intermediate",success=False)
    check("salt-default","restricted-intermediate",success=False)
    check("salt-maximum","restricted-intermediate")
    check("good","restricted-intermediate",success=False)
    check("sha256","restricted-intermediate","restricted-root")
    check("sha256","pss-intermediate","restricted-root",success=False)
    subprocess.run(["clang","-std=c11","-O1","-g","-no-pie","-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer","-Wall","-Wextra","-Werror","-I.",
                    "tools/x509_fuzz.c",*sources,"-o","build/pss-x509-fuzz"],check=True)
    for name in ("sha256","restricted-leaf"):
        subprocess.run(["build/pss-x509-fuzz",str(folder/(name+".der"))],check=True)
    print("PSS X509 PASS: OpenSSL-verified chains, SHA256/384/512, salts, RSAE/PSS key restrictions, malformed parameters and sanitizer mutations",flush=True)


if __name__=="__main__":
    main()
