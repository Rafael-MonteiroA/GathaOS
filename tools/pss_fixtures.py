"""Host-only PSS PKI, signed by cryptography and checked by OpenSSL."""
import base64
from pathlib import Path
import subprocess
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding
from tls_fixtures import make as rsa_fixtures

RSA_OID = bytes.fromhex("2a864886f70d010101")
PSS_OID = bytes.fromhex("2a864886f70d01010a")
MGF_OID = bytes.fromhex("2a864886f70d010108")


def der(tag, data):
    length = len(data)
    count = (length.bit_length()+7)//8
    return bytes([tag])+(bytes([length]) if length < 128 else bytes([128+count])+length.to_bytes(count, "big"))+data


def value(encoded):
    n = encoded[1]
    at = 2
    if n & 128:
        count = n & 127
        n = int.from_bytes(encoded[at:at+count], "big")
        at += count
    assert at+n == len(encoded)
    return encoded[at:]


def fields(data):
    result = []
    while data:
        n, at = data[1], 2
        if n & 128:
            count = n & 127
            n = int.from_bytes(data[at:at+count], "big")
            at += count
        result.append(data[:at+n])
        data = data[at+n:]
    return result


def integer(n):
    data = n.to_bytes(max(1, (n.bit_length()+7)//8), "big")
    return der(2, (b"\0" if data[0] & 128 else b"")+data)


def hash_id(name, null=True):
    last = {"sha256": 1, "sha384": 2, "sha512": 3}[name]
    return der(0x30, der(6, bytes.fromhex("6086480165030402")+bytes([last]))+(b"\x05\0" if null else b""))


def pss_id(name="sha256", salt=32, mgf=None, trailer=None, null=True, explicit_salt=False):
    params = der(0xa0, hash_id(name, null))+der(0xa1, der(0x30, der(6, MGF_OID)+hash_id(mgf or name, null)))
    if salt != 20 or explicit_salt:
        params += der(0xa2, integer(salt))
    if trailer is not None:
        params += der(0xa3, integer(trailer))
    return der(0x30, der(6, PSS_OID)+der(0x30, params))


def rewrite(encoded, signer, name="sha256", salt=32, algorithm=None, spki=None, pkcs=False):
    outer = fields(value(encoded))
    body = fields(value(outer[0]))
    if spki is not None:
        old = fields(value(body[6]))
        body[6] = der(0x30, spki+old[1])
    if algorithm is None:
        algorithm = (der(0x30, der(6, RSA_OID[:-1]+bytes([{"sha256":11,"sha384":12,"sha512":13}[name]]))+b"\x05\0")
                     if pkcs else pss_id(name, salt))
    body[2] = algorithm
    tbs = der(0x30, b"".join(body))
    h = {"sha256":hashes.SHA256, "sha384":hashes.SHA384, "sha512":hashes.SHA512}[name]()
    sig = signer.sign(tbs, padding.PKCS1v15() if pkcs else padding.PSS(padding.MGF1(h), salt), h)
    return der(0x30, tbs+algorithm+der(3, b"\0"+sig))


def pem(data):
    encoded = base64.b64encode(data)
    return b"-----BEGIN CERTIFICATE-----\n"+b"\n".join(encoded[i:i+64] for i in range(0, len(encoded), 64))+b"\n-----END CERTIFICATE-----\n"


def make(folder):
    folder = rsa_fixtures(folder)
    keys = {name:serialization.load_pem_private_key((folder/(name+".key")).read_bytes(), None)
            for name in ("root", "intermediate", "leaf")}
    base = {name:(folder/(name+".der")).read_bytes() for name in ("good", "intermediate", "root", "wrong-name", "expired")}
    data = {}
    data["pss-intermediate"] = rewrite(base["intermediate"], keys["root"], name="sha384", salt=48)
    data["restricted-intermediate"] = rewrite(base["intermediate"], keys["root"], spki=pss_id())
    data["unrestricted-root"] = rewrite(base["root"], keys["root"], spki=der(0x30, der(6, PSS_OID)))
    data["restricted-root"] = rewrite(base["root"], keys["root"], spki=pss_id())
    for name, hn in (("sha256",32), ("sha384",48), ("sha512",64)):
        data[name] = rewrite(base["good"], keys["intermediate"], name=name, salt=hn)
    data["salt-zero"] = rewrite(base["good"], keys["intermediate"], salt=0)
    data["salt-default"] = rewrite(base["good"], keys["intermediate"], salt=20)
    data["salt-maximum"] = rewrite(base["good"], keys["intermediate"], salt=222)
    data["explicit-defaults"] = rewrite(base["good"], keys["intermediate"], salt=20, algorithm=pss_id(salt=20, trailer=1, explicit_salt=True))
    data["hash-null-absent"] = rewrite(base["good"], keys["intermediate"], algorithm=pss_id(null=False))
    data["pss-leaf"] = rewrite(base["good"], keys["intermediate"], spki=der(0x30, der(6, PSS_OID)))
    data["restricted-leaf"] = rewrite(base["good"], keys["intermediate"], spki=pss_id())
    data["pss-wrong-name"] = rewrite(base["wrong-name"], keys["intermediate"])
    data["pss-expired"] = rewrite(base["expired"], keys["intermediate"])
    data["bad-mgf"] = rewrite(base["good"], keys["intermediate"], algorithm=pss_id(mgf="sha384"))
    data["bad-salt"] = rewrite(base["good"], keys["intermediate"], algorithm=pss_id(salt=31))
    data["bad-trailer"] = rewrite(base["good"], keys["intermediate"], algorithm=pss_id(trailer=2))
    data["missing-params"] = rewrite(base["good"], keys["intermediate"], algorithm=der(0x30, der(6, PSS_OID)))
    data["default-sha1"] = rewrite(base["good"], keys["intermediate"], algorithm=der(0x30, der(6, PSS_OID)+der(0x30,b"")))
    params = fields(value(fields(value(pss_id()))[1]))
    for name, body in (("duplicate-hash", params[:1]+params), ("reordered", params[::-1]),
                       ("missing-mgf", params[:1]+params[2:]), ("unknown-field", params+[der(0xa4, integer(0))]),
                       ("negative-salt", params[:2]+[der(0xa2,der(2,b"\xff"))]),
                       ("oversized-salt", params[:2]+[der(0xa2,integer(513))])):
        data[name] = rewrite(base["good"], keys["intermediate"], algorithm=der(0x30,der(6,PSS_OID)+der(0x30,b"".join(body))))
    parts = fields(value(data["sha256"]))
    data["outer-mismatch"] = der(0x30, parts[0]+pss_id(salt=33)+parts[2])
    for name, encoded in data.items():
        (folder/(name+".der")).write_bytes(encoded)
        (folder/(name+".pem")).write_bytes(pem(encoded))
    # Preserve the same RSA private numbers while marking a PKCS8 key as PSS.
    raw = keys["leaf"].private_bytes(serialization.Encoding.DER, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    components = fields(value(raw))
    for name, algorithm in (("pss-leaf", der(0x30,der(6,PSS_OID))), ("restricted-leaf", pss_id())):
        encoded = der(0x30, components[0]+algorithm+components[2])
        b64 = base64.b64encode(encoded)
        (folder/(name+".key")).write_bytes(b"-----BEGIN PRIVATE KEY-----\n"+
            b"\n".join(b64[i:i+64] for i in range(0,len(b64),64))+b"\n-----END PRIVATE KEY-----\n")
    for name in ("sha256","sha384","sha512","salt-zero","salt-default","salt-maximum",
                 "explicit-defaults","hash-null-absent","pss-leaf","restricted-leaf"):
        subprocess.run(["openssl","verify","-CAfile",str(folder/"root.pem"),"-untrusted",
                        str(folder/"pss-intermediate.pem"),str(folder/(name+".pem"))], check=True, stdout=subprocess.DEVNULL)
    return folder
