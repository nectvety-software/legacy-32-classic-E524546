#!/usr/bin/env python3
"""Provision ONE firmware-pinned QEAPP/2 ECDSA P-256 publisher public key.
The private key MUST remain on the publisher PC, NEVER in firmware or SD.
Requires: pip install cryptography
"""
import argparse
import os
import stat
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization

def emit_header(pub, key_id:int, dest:Path):
    raw=pub.public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)
    assert len(raw)==65 and raw[0]==4
    body='#pragma once\n#include <stdint.h>\n// Generated trusted P-256 QEAPP publisher key. PRIVATE key stays off-device.\n'
    body+=f'static const uint32_t QEAPP_TRUST_KEY_ID=0x{key_id:08x}u;\n'
    body+='static const uint8_t QEAPP_TRUST_PUBKEY[65]={\n'
    body+=','.join(f'0x{v:02x}' for v in raw)+'\n};\n'
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(body)

def main():
    p=argparse.ArgumentParser(description='Generate QEAPP P-256 private signing key + public header')
    p.add_argument('--private',type=Path,required=True,help='secure location on publisher computer')
    p.add_argument('--header',type=Path,required=True,help='firmware src/services/QeappTrustKey.h')
    p.add_argument('--key-id',type=lambda t:int(t,0),default=0x31534351)
    a=p.parse_args()
    if a.private.exists():p.error('Private key already exists; refusing to overwrite')
    if a.private.resolve()==a.header.resolve():p.error('Private and public paths must differ')
    key=ec.generate_private_key(ec.SECP256R1())
    # Use restrictive permissions when creating sensitive files.
    a.private.parent.mkdir(parents=True,exist_ok=True)
    fd=os.open(a.private,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'wb') as out:
        out.write(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
    if os.name=='posix':a.private.chmod(stat.S_IRUSR|stat.S_IWUSR)
    emit_header(key.public_key(),a.key_id,a.header)
    print('Publisher private key (KEEP SECRET):',a.private)
    print('Firmware public-key header:',a.header)
    print('Publisher key ID:',hex(a.key_id))
    print('Rebuild/flash firmware after replacing the trusted header.')
if __name__=='__main__':main()
