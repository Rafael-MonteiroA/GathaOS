"""Ephemeral test PKI. No test CA or private key enters a release trust store."""
from datetime import datetime,timezone
import ipaddress
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID,ExtendedKeyUsageOID,ObjectIdentifier

BEFORE=datetime(2025,1,1);AFTER=datetime(2030,1,1);NOW=int(datetime(2026,10,21,12,tzinfo=timezone.utc).timestamp())
def make(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=True)
    keys={name:rsa.generate_private_key(public_exponent=65537,key_size=2048) for name in ('root','intermediate','leaf','foreign')}
    names={name:x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'Gatha test '+name)]) for name in keys}
    def cert(name,key,issuer,signer,ca=False,path=None,sans=True,usage=True,eku=True,before=BEFORE,after=AFTER,extra=None,algorithm=None):
        builder=x509.CertificateBuilder().subject_name(names[name]).issuer_name(names[issuer]).public_key(keys[key].public_key()).serial_number(x509.random_serial_number()).not_valid_before(before).not_valid_after(after)
        builder=builder.add_extension(x509.BasicConstraints(ca=ca,path_length=path),critical=True)
        if usage:builder=builder.add_extension(x509.KeyUsage(digital_signature=not ca,content_commitment=False,key_encipherment=False,data_encipherment=False,key_agreement=False,key_cert_sign=ca,crl_sign=ca,encipher_only=False,decipher_only=False),critical=True)
        if not ca and sans:
            san=sans if isinstance(sans,list) else [x509.DNSName('localhost'),x509.DNSName('*.unit.test'),x509.IPAddress(ipaddress.ip_address('10.0.2.2')),x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]
            builder=builder.add_extension(x509.SubjectAlternativeName(san),critical=False)
        if not ca and eku:builder=builder.add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH if eku is True else ExtendedKeyUsageOID.CLIENT_AUTH]),critical=False)
        if extra:builder=builder.add_extension(*extra)
        return builder.sign(keys[signer],algorithm or hashes.SHA256())
    certificates={}
    certificates['root']=cert('root','root','root','root',True,1)
    certificates['intermediate']=cert('intermediate','intermediate','root','root',True,0)
    certificates['good']=cert('leaf','leaf','intermediate','intermediate')
    certificates['expired']=cert('leaf','leaf','intermediate','intermediate',after=datetime(2026,1,1))
    certificates['future']=cert('leaf','leaf','intermediate','intermediate',before=datetime(2028,1,1))
    certificates['wrong-name']=cert('leaf','leaf','intermediate','intermediate',sans=[x509.DNSName('other.invalid')])
    certificates['no-san']=cert('leaf','leaf','intermediate','intermediate',sans=False)
    certificates['client-only']=cert('leaf','leaf','intermediate','intermediate',eku='client')
    certificates['unknown-critical']=cert('leaf','leaf','intermediate','intermediate',extra=(x509.UnrecognizedExtension(ObjectIdentifier('1.2.3.4.5'),b'\x05\0'),True))
    certificates['must-staple']=cert('leaf','leaf','intermediate','intermediate',extra=(x509.TLSFeature([x509.TLSFeatureType.status_request]),False))
    certificates['sha1']=cert('leaf','leaf','intermediate','intermediate',algorithm=hashes.SHA1())
    certificates['foreign']=cert('leaf','leaf','foreign','foreign')
    certificates['bad-ca']=cert('intermediate','intermediate','root','root',False)
    certificates['root-short']=cert('root','root','root','root',True,0)
    certificates['constraints']=cert('intermediate','intermediate','root','root',True,0,extra=(x509.NameConstraints(permitted_subtrees=[x509.DNSName('.unit.test')],excluded_subtrees=None),True))
    certificates['ca-leaf']=cert('leaf','leaf','intermediate','intermediate',True,0)
    for name,key in keys.items():
        (root/(name+'.key')).write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.TraditionalOpenSSL,serialization.NoEncryption()))
    for name,crt in certificates.items():
        (root/(name+'.der')).write_bytes(crt.public_bytes(serialization.Encoding.DER))
        chain=crt.public_bytes(serialization.Encoding.PEM)
        if name not in ('root','intermediate','foreign'):chain+=certificates['intermediate'].public_bytes(serialization.Encoding.PEM)
        (root/(name+'.pem')).write_bytes(chain)
    return root
