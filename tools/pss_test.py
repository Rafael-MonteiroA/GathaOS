"""Independent RSA-PSS vectors, malformed encodings and sanitizer replay."""
import ctypes as c
import hashlib
import os
from pathlib import Path
import struct
import subprocess
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa


class RSA(c.Structure):
    _fields_ = [("modulus", c.c_ubyte*512), ("size", c.c_uint), ("exponent", c.c_uint32)]


def main():
    Path("build").mkdir(exist_ok=True)
    sources = [str(p) for p in Path("libgatha/crypto").glob("*.c")]
    subprocess.run(["clang", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-shared", "-fPIC", *sources, "-o", "build/pss-test.so"], check=True)
    lib = c.CDLL(str(Path("build/pss-test.so").resolve()))
    records = []
    def verify(key, ident, salt, digest, sig, expected):
        result = lib.rsa_pss_verify(c.byref(key), ident, salt, digest, sig, len(sig))
        assert (result == 0) == expected, (ident, salt, len(sig), expected)
        records.append(struct.pack("<6I", key.size, key.exponent, ident, salt, len(sig), int(expected))+
                       bytes(key.modulus)+digest.ljust(64, b"\0")+sig.ljust(512, b"\0"))
    for bits in (2048, 3072, 4096):
        private = rsa.generate_private_key(public_exponent=65537, key_size=bits)
        numbers = private.private_numbers()
        modulus = numbers.public_numbers.n.to_bytes(bits//8, "big")
        key = RSA(); key.modulus[:len(modulus)] = modulus; key.size = len(modulus); key.exponent = 65537
        for ident, algorithm in ((4, hashes.SHA256()), (5, hashes.SHA384()), (6, hashes.SHA512())):
            hn = algorithm.digest_size
            for salt in (0, 1, 20, hn, bits//8-hn-2):
                for length in (0, 111, 8193):
                    message = os.urandom(length)
                    digest = hashlib.new(algorithm.name, message).digest()
                    sig = private.sign(message, padding.PSS(padding.MGF1(algorithm), salt), algorithm)
                    verify(key, ident, salt, digest, sig, True)
                    verify(key, ident, salt+1, digest, sig, False)
                    verify(key, ident, salt, bytes([digest[0]^1])+digest[1:], sig, False)
                    verify(key, ident, salt, digest, bytes([sig[0]^1])+sig[1:], False)
            digest = hashlib.new(algorithm.name, b"encoding").digest()
            sig = private.sign(b"encoding", padding.PSS(padding.MGF1(algorithm), hn), algorithm)
            encoded = pow(int.from_bytes(sig, "big"), 65537, numbers.public_numbers.n).to_bytes(bits//8, "big")
            for position in (0, 1, bits//8-hn-2, bits//8-hn-1, bits//8-2, bits//8-1):
                bad = bytearray(encoded); bad[position] ^= 1
                forged = pow(int.from_bytes(bad, "big"), numbers.d, numbers.public_numbers.n).to_bytes(bits//8, "big")
                verify(key, ident, hn, digest, forged, False)
            bad = b"\x80"+bytes(bits//8-1)
            forged = pow(int.from_bytes(bad, "big"), numbers.d, numbers.public_numbers.n).to_bytes(bits//8, "big")
            verify(key, ident, hn, digest, forged, False)
            for size in (0, 1, bits//8-1):
                verify(key, ident, hn, digest, sig[:size], False)
            verify(key, ident, hn, digest, modulus, False)
            verify(key, ident, 0xffffffff, digest, sig, False)
            verify(key, 3, hn, digest, sig, False)
            # A valid PKCS1 signature and a different MGF hash must not pass.
            verify(key, ident, hn, digest, private.sign(b"encoding", padding.PKCS1v15(), algorithm), False)
            other = hashes.SHA512() if ident != 6 else hashes.SHA256()
            verify(key, ident, hn, digest, private.sign(b"encoding", padding.PSS(padding.MGF1(other), hn), algorithm), False)
    path = Path("build/pss-vectors.bin")
    path.write_bytes(struct.pack("<I", len(records))+b"".join(records))
    subprocess.run(["clang", "-std=c11", "-O1", "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror", "-I.",
                    "tools/pss_native_test.c", *sources, "-o", "build/pss-native-test"], check=True)
    subprocess.run(["build/pss-native-test", str(path)], check=True)
    print(f"PSS HOST PASS: {len(records)} independent vectors, 2048/3072/4096, SHA256/384/512, exact salts and malformed encodings", flush=True)


if __name__ == "__main__":
    main()
