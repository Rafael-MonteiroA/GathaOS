"""Cross-check the original primitives against independent host implementations."""
import ctypes as c
import hashlib
import hmac
import os
from pathlib import Path
import subprocess
from cryptography.hazmat.primitives.asymmetric import x25519, rsa, padding
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

def main():
    Path('build').mkdir(exist_ok=True)
    sources=[str(p) for p in Path('libgatha/crypto').glob('*.c')]
    subprocess.run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',*sources,'-o','build/crypto-test.so'],check=True)
    lib=c.CDLL(str(Path('build/crypto-test.so').resolve()))
    buf=lambda n:c.create_string_buffer(n)
    class AES(c.Structure):_fields_=[('round',c.c_ubyte*176)]
    class RSA(c.Structure):_fields_=[('modulus',c.c_ubyte*512),('size',c.c_uint),('exponent',c.c_uint32)]
    for size in [0,1,3,55,56,63,64,65,111,112,127,128,129,1000,65536]:
        data=os.urandom(size)
        for ident,fun in [(4,hashlib.sha256),(5,hashlib.sha384),(6,hashlib.sha512)]:
            out=buf(64);n=lib.crypto_hash(ident,data,size,out);assert out.raw[:n]==fun(data).digest()
        key=os.urandom(size%140);out=buf(32);lib.hmac256(key,len(key),data,size,out);assert out.raw==hmac.digest(key,data,'sha256')
    key=os.urandom(48);seed=os.urandom(64);label=b'key expansion';a=hmac.digest(key,label+seed,'sha256');expected=b''
    while len(expected)<104:expected+=hmac.digest(key,a+label+seed,'sha256');a=hmac.digest(key,a,'sha256')
    out=buf(104);assert lib.tls_prf(key,len(key),label,seed,len(seed),out,104)==0;assert out.raw==expected[:104]
    for _ in range(30):
        private=x25519.X25519PrivateKey.generate();peer=x25519.X25519PrivateKey.generate().public_key()
        scalar=private.private_bytes(serialization.Encoding.Raw,serialization.PrivateFormat.Raw,serialization.NoEncryption());point=peer.public_bytes(serialization.Encoding.Raw,serialization.PublicFormat.Raw)
        out=buf(32);assert lib.x25519(out,scalar,point)==0;assert out.raw==private.exchange(peer)
    scalar=bytes.fromhex('77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a')
    out=buf(32);assert lib.x25519(out,scalar,b'\x09'+b'\0'*31)==0;assert out.raw.hex()=='8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a'
    for point in [b'\0'*32,b'\x01'+b'\0'*31]:assert lib.x25519(out,scalar,point)!=0
    for n in [0,1,15,16,17,31,255,1024,16384]:
        key=os.urandom(16);iv=os.urandom(12);aad=os.urandom(13);plain=os.urandom(n);aes=AES();lib.aes128_init(c.byref(aes),key)
        block=os.urandom(16);out=buf(16);lib.aes128_block(c.byref(aes),block,out);assert out.raw==Cipher(algorithms.AES(key),modes.ECB()).encryptor().update(block)
        expected=AESGCM(key).encrypt(iv,plain,aad);data=c.create_string_buffer(plain,max(n,1));tag=buf(16)
        lib.gcm_encrypt(c.byref(aes),iv,aad,len(aad),data,n,tag);assert data.raw[:n]+tag.raw==expected
        bad=bytes([tag.raw[0]^1])+tag.raw[1:];assert lib.gcm_decrypt(c.byref(aes),iv,aad,len(aad),data,n,bad)!=0;assert data.raw[:n]==expected[:n]
        assert lib.gcm_decrypt(c.byref(aes),iv,aad,len(aad),data,n,tag)==0;assert data.raw[:n]==plain
    for bits in [2048,3072,4096]:
        private=rsa.generate_private_key(public_exponent=65537,key_size=bits);mod=private.public_key().public_numbers().n.to_bytes(bits//8,'big');public=RSA();public.size=len(mod);public.exponent=65537;public.modulus[:len(mod)]=mod
        for ident,algorithm in [(4,hashes.SHA256()),(5,hashes.SHA384()),(6,hashes.SHA512())]:
            data=os.urandom(333);digest=hashlib.new(algorithm.name,data).digest();sig=private.sign(data,padding.PKCS1v15(),algorithm)
            assert lib.rsa_verify(c.byref(public),ident,digest,sig,len(sig))==0
            bad=bytes([sig[0]^1])+sig[1:];assert lib.rsa_verify(c.byref(public),ident,digest,bad,len(bad))!=0
            assert lib.rsa_verify(c.byref(public),ident,digest,mod,len(mod))!=0
    print('CRYPTO PASS: SHA-256/384/512, HMAC, PRF, X25519, AES-128, GCM, RSA 2048/3072/4096 and negative vectors',flush=True)
if __name__=='__main__':main()
