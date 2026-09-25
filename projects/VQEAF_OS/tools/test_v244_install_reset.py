#!/usr/bin/env python3
"""Focused source and compiled-C++ host gates. Hardware reset testing separate."""
from pathlib import Path
import subprocess
import tempfile
import sys
ROOT=Path(__file__).resolve().parents[1]

def run(cmd):
 p=subprocess.run([str(x) for x in cmd],cwd=ROOT,text=True,capture_output=True,timeout=120)
 if p.returncode:raise RuntimeError(f"{cmd}:\n{p.stdout[-3000:]}\n{p.stderr[-3000:]}")
 return p.stdout

def main():
 src=(ROOT/'src/services/AppInstallerService.cpp').read_text()
 ui=(ROOT/'src/apps/Apps.cpp').read_text()
 assert 'std::unique_ptr<char[]> manifest' in src
 assert 'std::unique_ptr<RecoveryNames> names' in src
 assert 'static void printBootInstallDiagnostics' in (ROOT/'src/services/AppInstallerService.h').read_text()
 assert 'AppInstallerService::printBootInstallDiagnostics();' in (ROOT/'src/main.cpp').read_text()
 # After reload(), the file list MUST NOT verify up to 12 x ECDSA signatures.
 listing=ui[ui.index('void AppInstallerApp::reload('):ui.index('void AppInstallerApp::enter(')]
 assert '.inspect(' not in listing
 assert 'Select to verify signature' in ui
 assert not any('uint16_t pixels[1024]' in x for x in ui.splitlines())
 with tempfile.TemporaryDirectory() as td:
  out=Path(td)/'installer.o'
  # Compile the ESP32-only diagnostics path against dedicated platform shims.
  run(['g++','-std=c++17','-O1','-Wall','-Wextra','-Werror',
     '-Wno-deprecated-declarations','-DARDUINO_ARCH_ESP32','-DQEAPP_HOST_OPENSSL',
     '-include','tools/esp32_installer_compile/Serial.h',
     '-Itools/esp32_installer_compile','-Itools/qeapp_host','-Isrc/services',
     '-c','src/services/AppInstallerService.cpp','-o',out])
 print('PASS: ESP32-only instrumentation compiles against host shims')
 print('PASS: bounded heap workspaces, lazy inbox, icon buffer, boot reset hook')
 print(run([sys.executable,'tools/test_v24_app_manager.py']).splitlines()[-2])
 print('Hardware ESP32-S3 reset reproduction: NOT RUN')
if __name__=='__main__':main()
