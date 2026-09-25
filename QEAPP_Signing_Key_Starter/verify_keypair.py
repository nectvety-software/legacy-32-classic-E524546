from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization,hashes
import re
root=Path(__file__).parent
k=serialization.load_pem_private_key((root/'qeapp_private.pem').read_bytes(),password=None)
k2=serialization.load_pem_private_key((root/'qeapp_private.key').read_bytes(),password=None)
p=serialization.load_pem_public_key((root/'qeapp_public.pem').read_bytes())
p2=serialization.load_pem_public_key((root/'qeapp_public.key').read_bytes())
assert isinstance(k,ec.EllipticCurvePrivateKey) and k.curve.name=='secp256r1'
expected=k.public_key().public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)
assert k2.public_key().public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)==expected
assert p.public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)==expected
assert p2.public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)==expected
h=(root/'QeappTrustKey.h').read_text()
raw=bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})',h.split('QEAPP_TRUST_PUBKEY[65]')[1]))
assert raw==expected
message=b'QEAPP/2 test: verify custom publisher key pair'
s=k.sign(message,ec.ECDSA(hashes.SHA256()))
p.verify(s,message,ec.ECDSA(hashes.SHA256()))
print('PASS: private .pem / private .key / public .pem / public .key / firmware header are same ECDSA P-256 key pair')
print('NOTE: this test does not verify the trust anchor already flashed on a device')
