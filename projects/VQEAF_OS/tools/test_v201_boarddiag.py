#!/usr/bin/env python3
"""v2.0.1 host diagnostics. This is not physical hardware or live TLS proof."""
from pathlib import Path
import tempfile, subprocess, sys, re, shutil, zipfile
ROOT=Path(__file__).resolve().parents[1]

def run(args):
    result=subprocess.run([str(x) for x in args],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if result.returncode:
        print(result.stdout[-6000:]); raise SystemExit(result.returncode)
    print(result.stdout.strip()[-800:]);return result

def main():
    maincpp=(ROOT/'src/main.cpp').read_text()
    board=(ROOT/'src/services/BoardDiagnostics.cpp').read_text()
    assert 'diagPoll();' in maincpp
    assert 'event=REMOVED' in maincpp and 'event=MOUNTED' in maincpp
    assert 'TrustedTls::configure' in board
    assert 'client.setInsecure' not in board
    assert 'testSdReadWrite' in board and '.s3_diag_scratch.bin' in board
    assert 'expectedTrusted' in board and 'INCONCLUSIVE' in board
    with tempfile.TemporaryDirectory(prefix='s3-v201-') as directory:
        base=Path(directory)
        exe=base/'diag_host'
        run(['g++','-std=c++11','-Itools/storage_host','-Itools/host_stubs',
             '-Iinclude','-Isrc','tools/boarddiag_host/test_diag.cpp',
             'src/services/BoardDiagnostics.cpp','src/services/TrustedTls.cpp',
             'src/services/StorageService.cpp','-o',exe])
        (base/'card').mkdir()
        run([exe,base/'card'])
    print('PASS v2.0.1 host diagnostics; physical card removal and live TLS are PENDING')

if __name__=='__main__':main()
