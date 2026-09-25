#!/usr/bin/env python3
"""v2.0 host CI: actual source compilation, SD fault injection, TLS CA inventory.
No network or physical ESP32; live TLS handshakes must be tested on the board.
"""
from pathlib import Path
import hashlib, re, ssl, subprocess, sys, tempfile
ROOT = Path(__file__).resolve().parents[1]
EXPECTED = [
    '96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6', # ISRG X1
    'd947432abde7b7fa90fc2e6b59101b1280e0e1c7e4e40fa3c6887fff57a7f4cf', # GTS R1
    '8d25cd97229dbf70356bda4eb3cc734031e24cf00fafcfd32dc76eb5841c7ea8', # GTS R2
    '34d8a73ee208d9bcdb0d956520934b4e40e69482596e8b6f73c8426b010a6f48', # GTS R3
    '349dfa4058c5e263123b398ae795573c4e1313c83fe68f93556cd5e8031b3c7d', # GTS R4
    'cb3ccbb76031e5e0138f8dd39a23f9de47ffc35e43c1144cea27d46a5ab1cb5f', # DigiCert G2
    'ebd41040e4bb3ec742c9e381d31ef2a41a48b6685c96e7cef3c1df6cd4331c99', # GlobalSign
]

def run(*args, ok=True):
    c = subprocess.run([str(x) for x in args], cwd=ROOT, capture_output=True, text=True)
    if ok and c.returncode:
        print(c.stdout[-3000:], c.stderr[-3000:], file=sys.stderr)
        raise RuntimeError('Command failed: ' + ' '.join(map(str, args)))
    return c

def main():
    services = ROOT/'src/services'
    cert_cpp = (services/'TrustedTls.cpp').read_text()
    pem = cert_cpp.split('R"S3TLS(', 1)[1].split(')S3TLS"', 1)[0]
    certs = re.findall(r'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----', pem, re.S)
    assert len(certs) == 7, 'Unexpected firmware root count'
    hashes = [hashlib.sha256(ssl.PEM_cert_to_DER_cert(c)).hexdigest() for c in certs]
    assert hashes == EXPECTED, 'One of the firmware trust anchors unexpectedly changed'
    for name in ('BrowserService.cpp','ShellService.cpp'):
        body = (services/name).read_text()
        assert 'setInsecure(' not in body
        assert 'TrustedTls::configure' in body
    assert 'Unsafe HTTPS downgrade blocked' in (services/'BrowserService.cpp').read_text()
    assert 'Downloads require verified HTTPS' in (services/'BrowserService.cpp').read_text()
    assert 'tls20_' in (services/'BrowserService.cpp').read_text()
    print('PASS: fixed CA fingerprints, no insecure clients, blocked HTTPS downgrade')

    with tempfile.TemporaryDirectory(prefix='s3-v20-') as tmp:
        tmp = Path(tmp)
        bundle = tmp/'roots.pem'; bundle.write_text('\n'.join(certs)+'\n')
        for i, cert in enumerate(certs):
            root = tmp/f'root{i}.pem'; root.write_text(cert+'\n')
            run('openssl','verify','-CAfile',bundle,root)
        # Prove the bundle rejects an arbitrary untrusted self-signed signer.
        untrusted = tmp/'untrusted.pem'; key = tmp/'untrusted.key'
        run('openssl','req','-x509','-newkey','rsa:2048','-nodes',
            '-days','1','-subj','/CN=Untrusted Local Test',
            '-keyout',key,'-out',untrusted)
        assert run('openssl','verify','-CAfile',bundle,untrusted,ok=False).returncode != 0
        print('PASS: OpenSSL accepts all seven anchors and rejects unknown signer')
        binary = tmp/'tls_host'
        run('g++','-std=c++11','-Itools/host_stubs','-Iinclude','-Isrc',
            'tools/test_v20_tls.cpp','src/services/TrustedTls.cpp','-o',binary)
        assert 'PASS' in run(binary).stdout
        print('PASS: compiled production TLS adapter and wall-clock gate')
        # Full v1.9 regression includes C++ simulated SD writes and package signing.
        # test_atomic.cpp now additionally injects corruption and hot-plug events.
        r = run(sys.executable, 'tools/test_v19_stability.py')
        assert 'v2.0 atomic readback, removal' in r.stdout
        assert any(f'{count}/{count} host C++11' in r.stdout for count in range(22, 50))
        assert 'TLS' not in r.stderr.upper()  # no missing TLS symbols
        print('PASS: real StorageService.cpp SD fault injection and full regression')
        print('PASS: at least 22 host C++11 production units and synthetic full link')
    print('NOTE: target PlatformIO, real network certificates and physical SD hotplug NOT verified')

if __name__ == '__main__':
    main()
