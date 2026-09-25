#!/usr/bin/env python3
"""QEAPP/2 publisher trust, cryptographic tamper, installer and catalog tests.
Requires: g++, OpenSSL libcrypto headers, Python cryptography.
Firmware itself builds with GNU++11 and ESP32 framework mbedTLS.
"""
import hashlib
import struct
import subprocess
import tempfile
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
ROOT=Path(__file__).resolve().parent.parent
SRC=ROOT/'src/services'
STUB=ROOT/'tools/qeapp_host'

def cmd(*args):
    p=subprocess.run([str(x) for x in args],capture_output=True,text=True)
    if p.returncode: print(p.stdout,p.stderr);raise RuntimeError('Command failed: '+str(args[0]))
    if p.stdout: print(p.stdout.strip())
    return p

def compile_host(binary, test, header=None, cpp="c++11"):
    args=['g++','-std='+cpp,'-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
          '-DQEAPP_HOST_OPENSSL','-I'+str(STUB),'-I'+str(SRC)]
    if header: args+=['-DQEAPP_TRUST_KEY_HEADER="'+str(header)+'"']
    args += [str(SRC/'QeappFormat.cpp'),str(SRC/'QeappSignature.cpp'),
             str(SRC/'AppInstallerService.cpp'),str(STUB/test),'-lcrypto','-o',str(binary)]
    cmd(*args)

def main():
  with tempfile.TemporaryDirectory(prefix='qeapp-signed-') as td:
    d=Path(td)
    # Validate the firmware default/demo public key with the two shipped, signed packages.
    base=d/'demo_sd';base.mkdir()
    compile_host(d/'basic','test_installer.cpp')
    cmd(d/'basic',base,ROOT/'sd/System/Apps/Inbox/welcome.qeapp',ROOT/'sd/System/Apps/Inbox/help_site.qeapp')
    private=d/'publisher.pem';header=d/'test_trusted_key.h'
    cmd('python3',ROOT/'tools/qeapp_keys.py','--private',private,'--header',header)
    assert private.stat().st_mode&0o077==0, 'Signing key permissions too broad'
    otherPrivate=d/'other.pem';otherHeader=d/'other_trust.h'
    cmd('python3',ROOT/'tools/qeapp_keys.py','--private',otherPrivate,'--header',otherHeader)
    samples=[]
    for i in range(13):
      out=d/('app%d.qeapp'%i)
      cmd('python3',ROOT/'tools/build_qeapp.py','--id','app'+str(i),
          '--name','Sample app '+str(i),'--version','1.0.0', '--type','web',
          '--url','https://example.org/'+str(i),'--sign-key',private,'-o',out)
      samples.append(out)
    compile_host(d/'catalog','test_catalog.cpp',header,'c++17')
    cmd(d/'catalog',d/'catalog_sd',*samples)
    trusted=d/'welcome.qeapp'
    cmd('python3',ROOT/'tools/build_qeapp.py','--id','welcome','--name','Welcome',
        '--version','1.0.0','--type','text','--text',ROOT/'sd/System/Apps/Inbox/welcome.txt',
        '--icon',ROOT/'sd/System/Apps/Inbox/welcome_icon.png','--sign-key',private,'-o',trusted)
    untrusted=d/'other_signed.qeapp'
    cmd('python3',ROOT/'tools/build_qeapp.py','--id','welcome','--name','Welcome',
        '--version','1.0.0','--type','text','--text',ROOT/'sd/System/Apps/Inbox/welcome.txt',
        '--icon',ROOT/'sd/System/Apps/Inbox/welcome_icon.png','--sign-key',otherPrivate,'-o',untrusted)
    # Independent Python cryptographic cross-check, including exact signed range.
    binary=trusted.read_bytes();assert binary[:8]==b'QEAPP2\r\n'
    mlen,ilen,plen=struct.unpack_from('<III',binary,8)
    assert len(binary)==116+mlen+ilen+plen+76
    signed=binary[:-76];tail=binary[-76:];assert tail[:8]==b'QSIGP256'
    r=int.from_bytes(tail[12:44],'big');s=int.from_bytes(tail[44:76],'big')
    pub=serialization.load_pem_private_key(private.read_bytes(),None).public_key()
    pub.verify(utils.encode_dss_signature(r,s),hashlib.sha256(signed).digest(),ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    compile_host(d/'security','test_security.cpp',header,'c++17')
    cmd(d/'security',d/'security_sd',trusted,untrusted)
    print('QEAPP/2 secure publisher pin, SHA-256, ECDSA, installed receipt: PASS')
    print('NOTE: host OpenSSL verifies signing semantics; ESP32 target still requires PlatformIO/hardware build')
if __name__=='__main__':main()
