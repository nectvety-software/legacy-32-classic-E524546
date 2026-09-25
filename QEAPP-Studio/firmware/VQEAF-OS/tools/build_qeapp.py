#!/usr/bin/env python3
"""Create deterministic QEAPP/2 packages for Symbian S3 OS v1.4.

Examples:
python tools/build_qeapp.py --id welcome --name 'Welcome' --version 1.0.0 \
  --type text --text docs/welcome.txt -o welcome.qeapp
python tools/build_qeapp.py --id browser_home --name 'Website' --version 1.0.0 \
  --type web --url https://example.org/ -o website.qeapp
Optional --icon /path/to/32x32.png requires Pillow. This stores RGB565 LE.
"""
import argparse
import hashlib
import re
import struct
from pathlib import Path

MAGIC = b'QEAPP2\r\n'
MAX_PAYLOAD = 256 * 1024


def parse_args():
    a = argparse.ArgumentParser(description='Build bounded, declarative QEAPP/2 package')
    a.add_argument('--id', required=True)
    a.add_argument('--name', required=True)
    a.add_argument('--version', required=True)
    a.add_argument('--type', choices=('web', 'text', 'lua'), required=True)
    a.add_argument('--url')
    a.add_argument('--text', type=Path)
    a.add_argument('--lua', type=Path, help='main.lua source (experimental vqeaf_lua_beta only)')
    a.add_argument('--enable-lua-experimental',action='store_true')
    a.add_argument('--icon', type=Path)
    a.add_argument('--sign-key',type=Path,required=True,help='private publisher PEM key, NEVER distribute')
    a.add_argument('--key-id',type=lambda t:int(t,0),default=None)
    a.add_argument('-o', '--output', type=Path, required=True)
    return a.parse_args()


def main():
    a = parse_args()
    if a.key_id is None:a.key_id=0x544c5541 if a.type=='lua' else 0x31534351
    if not re.fullmatch(r'[a-z0-9_-]{1,24}', a.id):
        raise SystemExit('Invalid id: lowercase a-z, 0-9, underscore, dash; <=24')
    if not a.name.isascii() or not 1 <= len(a.name) <= 40 or any(ord(c)<32 or ord(c)>126 for c in a.name):
        raise SystemExit('Name must be 1..40 printable ASCII chars')
    if not re.fullmatch(r'[0-9]+(?:\.[0-9]+)*', a.version) or len(a.version) > 19:
        raise SystemExit('Version must contain numeric dotted components')
    if a.type == 'web':
        if not a.url or not a.url.startswith('https://') or ' ' in a.url or len(a.url)>192:
            raise SystemExit('HTTPS --url required, <=192 bytes')
        if a.text or a.lua:
            raise SystemExit('Web app cannot embed payload')
        manifest = f'id={a.id}\nname={a.name}\nversion={a.version}\ntype=web\nentry={a.url}\n'.encode('ascii')
        payload = b''
    elif a.type == 'lua':
        if not a.enable_lua_experimental or not a.lua or a.url or a.text:
            raise SystemExit('Experimental Lua requires --lua, --enable-lua-experimental and no --text/--url')
        payload=a.lua.read_bytes()
        if not 0 < len(payload) <= 64*1024 or b'\x00' in payload:
            raise SystemExit('Lua source must be UTF-8 text and <=64KiB')
        try:payload.decode('utf-8')
        except UnicodeDecodeError as exc:raise SystemExit('Lua source must be UTF-8') from exc
        manifest=f'id={a.id}\nname={a.name}\nversion={a.version}\ntype=lua\n'.encode('ascii')
    else:
        if not a.text or a.url or a.lua:
            raise SystemExit('Text app requires --text and no --url/--lua')
        payload = a.text.read_bytes()
        if not 0<len(payload)<=MAX_PAYLOAD:
            raise SystemExit('Text payload must be 1..262144 bytes')
        manifest = f'id={a.id}\nname={a.name}\nversion={a.version}\ntype=text\n'.encode('ascii')
    icon=b''
    if a.icon:
        try:
            from PIL import Image
        except ImportError as exc:
            raise SystemExit('Pillow is required when --icon is provided') from exc
        im=Image.open(a.icon).convert('RGB')
        if im.size!=(32,32):
            raise SystemExit('Icon must be exactly 32x32 RGB pixels')
        data=bytearray()
        for r,g,b in im.get_flattened_data() if hasattr(im,"get_flattened_data") else im.getdata():
            rgb=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
            data.extend(struct.pack('<H',rgb))
        icon=bytes(data)
    if len(manifest)>2048:
        raise SystemExit('Manifest exceeds 2048 bytes')
    h=lambda block:hashlib.sha256(block).digest()
    hdr=MAGIC+struct.pack('<III',len(manifest),len(icon),len(payload))+h(manifest)+h(icon)+h(payload)
    assert len(hdr)==116
    a.output.parent.mkdir(parents=True, exist_ok=True)
    # Signature covers the complete header and all three exact sections.
    # The final 76-byte trailer contains magic, 4-byte key ID and raw r||s.
    try:
        from cryptography.hazmat.primitives import hashes,serialization
        from cryptography.hazmat.primitives.asymmetric import ec,utils
    except ImportError as exc:
        raise SystemExit('Install cryptography: pip install cryptography') from exc
    private=serialization.load_pem_private_key(a.sign_key.read_bytes(),password=None)
    if not isinstance(private,ec.EllipticCurvePrivateKey) or private.curve.name!='secp256r1':
        raise SystemExit('Expected an ECDSA secp256r1 (P-256) private key')
    contents=hdr+manifest+icon+payload
    # Prehashed lets the signer and firmware agree on SHA256(contents).
    signature_der=private.sign(hashlib.sha256(contents).digest(),ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    r,s=utils.decode_dss_signature(signature_der)
    trailer=b'QSIGP256'+struct.pack('<I',a.key_id)+r.to_bytes(32,'big')+s.to_bytes(32,'big')
    assert len(trailer)==76
    a.output.write_bytes(contents+trailer)
    print(f'Package: {a.output}  ({a.output.stat().st_size} bytes)')
    print('Manifest SHA256:', h(manifest).hex())
    print('Icon SHA256:    ', h(icon).hex())
    print('Payload SHA256: ', h(payload).hex())
    print('ECDSA P-256 signature present, publisher key ID:', hex(a.key_id))
    print('Firmware must pin the matching publisher public key.')

if __name__=='__main__':
    main()
