#!/usr/bin/env python3
"""Actual C++ QEAPP/2 manager with a POSIX SD shim and ephemeral signer.
Tests are HOST tests; do not label them real ESP32 hardware results.
"""
import subprocess
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
SRC=ROOT/'src/services'
HOST=ROOT/'tools/qeapp_host'

def run(*args):
    p=subprocess.run([str(s) for s in args],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if p.returncode:
        raise AssertionError(f"command failed {args[0]}:\n{p.stdout}")
    return p.stdout

def main():
    with tempfile.TemporaryDirectory(prefix='vqeaf-app-manager-') as tmp:
        root=Path(tmp)
        key=root/'ephemeral.pem'
        header=root/'trust_key.h'
        print(run('python3',ROOT/'tools/qeapp_keys.py','--private',key,'--header',header))
        samples=[]
        for idx,version in enumerate(('1.0.0','1.1.0','1.2.0')):
            content=root/f'doc{idx}.txt'
            content.write_text(f'VQEAF signed test document {version}',encoding='utf-8')
            pkg=root/f'v{idx}.qeapp'
            print(run('python3',ROOT/'tools/build_qeapp.py','--id','welcome','--name','Welcome',
                '--version',version,'--type','text','--text',content,
                '--sign-key',key,'-o',pkg).splitlines()[0])
            samples.append(pkg)
        exe=root/'test_upgrade'
        args=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
            '-DQEAPP_HOST_OPENSSL',f'-DQEAPP_TRUST_KEY_HEADER="{header}"',
            '-I'+str(HOST),'-I'+str(SRC),
            *[str(SRC/n) for n in ('QeappFormat.cpp','QeappVersion.cpp',
                                    'QeappSignature.cpp','AppInstallerService.cpp','QeappDataService.cpp')],
            str(HOST/'test_upgrade.cpp'),'-lcrypto','-o',str(exe)]
        run(*args)
        print(run(exe,root/'sd',*samples).strip())
        print('Target ESP32 build/hardware test: NOT RUN')

if __name__=='__main__':main()
