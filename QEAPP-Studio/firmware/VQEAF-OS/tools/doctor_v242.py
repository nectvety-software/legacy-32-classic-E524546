#!/usr/bin/env python3
"""Read-only VQEAF v2.4.2 host package/trust and card-layout doctor.

This script never modifies or signs an app. Supply --sd-root for a mounted
microSD directory; without it checks only the bundled sample packages.
Live TLS, SD write health and LCD still require real ESP32-S3 field checks.
"""
from __future__ import annotations
import argparse
import hashlib
import re
import struct
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def public_key(header: Path):
    text=header.read_text()
    m=re.search(r'QEAPP_TRUST_KEY_ID\s*=\s*(0x[\da-fA-F]+)',text)
    n=re.search(r'QEAPP_TRUST_PUBKEY\s*\[\s*65\s*\]\s*=\s*\{([^}]+)\}',text,re.S)
    if not m or not n: raise ValueError('Trusted firmware public key header not recognized')
    raw=bytes(int(x,16) for x in re.findall(r'0x[\da-fA-F]+',n.group(1)))
    if len(raw)!=65 or raw[0]!=4:raise ValueError('Malformed P-256 public key')
    return int(m.group(1),16),raw


def verify_pkg(path: Path, key_id: int, pub: bytes) -> tuple[bool,str]:
    try:
        data=path.read_bytes()
        if len(data)<116+1+76 or data[:8]!=b'QEAPP2\r\n':
            return False,'Not QEAPP/2 or truncated'
        sizes=struct.unpack_from('<III',data,8)
        mlen,ilen,plen=sizes
        if not 0<mlen<=2048 or ilen not in (0,2048) or plen>256*1024:
            return False,'Unsupported section dimensions'
        body_end=116+mlen+ilen+plen
        if body_end+76!=len(data):return False,'Truncated/extra signed bytes'
        manifest=data[116:116+mlen]
        icon=data[116+mlen:116+mlen+ilen]
        payload=data[116+mlen+ilen:body_end]
        for content,expected in [(manifest,data[20:52]),(icon,data[52:84]),(payload,data[84:116])]:
            if hashlib.sha256(content).digest()!=expected:return False,'SHA256 section changed'
        trailer=data[body_end:]
        if trailer[:8]!=b'QSIGP256':return False,'Signature marker missing'
        stored_id=struct.unpack_from('<I',trailer,8)[0]
        if stored_id!=key_id:
            return False,f'Signing key mismatch: package=0x{stored_id:08X}, firmware=0x{key_id:08X} (select matching firmware or re-sign with your key)'
        from cryptography.hazmat.primitives import hashes
        from cryptography.hazmat.primitives.asymmetric import ec,utils
        key=ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(),pub)
        sig=utils.encode_dss_signature(int.from_bytes(trailer[12:44],'big'),int.from_bytes(trailer[44:76],'big'))
        key.verify(sig,hashlib.sha256(data[:body_end]).digest(),ec.ECDSA(utils.Prehashed(hashes.SHA256())))
        return True,'verified section hashes + ECDSA P-256 signature'
    except ImportError:
        return False,'Install cryptography to verify P-256 signatures'
    except Exception as ex:
        return False,f'Invalid signature or package: {type(ex).__name__}'


def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--key-header',type=Path,default=ROOT/'src/services/QeappTrustKey.h')
    a.add_argument('--package',type=Path,action='append',help='Repeat to check selected packages')
    a.add_argument('--sd-root',type=Path,help='Optional mounted FAT card folder, read-only check')
    args=a.parse_args()
    key_id,pub=public_key(args.key_header)
    print(f'[KEY] firmware trust ID=0x{key_id:08X} from {args.key_header.name}')
    pkgs=args.package or [ROOT/'sd/System/Apps/Inbox/welcome.qeapp',ROOT/'sd/System/Apps/Inbox/help_site.qeapp']
    good=True
    for p in pkgs:
        ok,reason=verify_pkg(p,key_id,pub)
        print(f'[{"PASS" if ok else "FAIL"}] QEAPP {p.name}: {reason}')
        good &= ok
    if args.sd_root:
        if not args.sd_root.is_dir():
            print(f'[FAIL] Card path unavailable: {args.sd_root}'); good=False
        else:
            for rel in ['System/Themes','System/Apps/Inbox','System/Apps/Installed','System/Cache/Web']:
                exists=(args.sd_root/rel).is_dir()
                print(f'[{"PASS" if exists else "WARN"}] /{rel}: {"exists" if exists else "missing (firmware creates if writable)"}')
            for p in sorted((args.sd_root/'System/Themes').glob('*.vqeaf')):
                b=p.read_bytes()
                structurally=0<len(b)<=512*1024 and b.lstrip(b'\xef\xbb\xbf\x20\r\n').startswith(b'@vqeaf 1.') and b'</theme>' in b[-192:]
                print(f'[{"OK HEADER" if structurally else "WARN"}] Theme {p.name}: '+('test Apply for full palette validation' if structurally else 'invalid header/tail/size'))
    print('[INFO] Physical SD read/write: run Shell `sddiag rw` on device')
    print('[INFO] TLS: wait for NTP, then run Shell `tlsdiag valid` on device')
    print('[INFO] All runtime checks: run Shell `corediag` at 115200 baud')
    return 0 if good else 1

if __name__=='__main__':raise SystemExit(main())
