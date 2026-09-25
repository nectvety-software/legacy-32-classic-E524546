#!/usr/bin/env python3
"""Regression gates for Symbian S3 OS v0.9 Shell file/network edition."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def read(rel): return (ROOT/rel).read_text(encoding='utf-8')
checks=[]
def check(name, cond):
    if not cond: raise AssertionError(name)
    checks.append(name); print('PASS:',name)
sh=read('src/services/ShellService.cpp'); hh=read('src/services/ShellService.h'); apps=read('src/apps/Apps.cpp'); main=read('src/main.cpp'); rd=read('README.md'); ch=read('CHANGELOG.md')
check('v0.9 shell/network feature line retained','NETWORK MONITOR' in sh and 'SYSTEM MONITOR' in sh)
check('bounded terminal','MAX_LINES = 20' in hh and 'LINE_CHARS = 39' in hh and 'COMMAND_CHARS = 127' in hh)
check('quoted argument parser','splitArgs' in sh and "ch == '\"'" in sh and "ch == '\\''" in sh)
for cmd in ['mkdir','rm','rmdir','touch','cp','mv','hexdump','write','append']:
    check('file command '+cmd, f'cmd == "{cmd}"' in sh or (cmd=='hexdump' and 'cmd == "hexdump"' in sh))
check('copy uses bounded 512-byte buffer','uint8_t buffer[512]' in sh)
check('copy removes partial destination on failure','remove(dstPath)' in sh)
check('rm refuses root','refusing to remove root' in sh)
check('rmdir is non-recursive','fs().rmdir' in sh and 'directory may not be empty' in sh)
check('hexdump is bounded','uint8_t bytes[8]' in sh and 'row < 8' in sh)
for cmd in ['netmon','nslookup','ping','wget','top']:
    check('network/monitor command '+cmd, f'cmd == "{cmd}"' in sh or (cmd=='netmon' and 'cmd == "netmon"' in sh))
check('DNS uses hostByName','WiFi.hostByName' in sh)
check('ICMP ping has compile-time fallback','SYMBIAN_SHELL_HAS_ICMP' in sh and 'TCP probe used' in sh)
check('wget streams to File','http.writeToStream(&out)' in sh and 'networkDownloadBytes' in sh)
check('HTTPS wget explicitly insecure','client.setInsecure()' in sh)
check('ifconfig shows network details','subnetMask' in sh and 'gatewayIP' in sh and 'dnsIP' in sh and 'macAddress' in sh)
check('net monitor tracks signal/download','NETWORK MONITOR' in sh and 'downloaded:' in sh and 'last net op:' in sh)
check('system monitor tracks heap/psram','SYSTEM MONITOR' in sh and 'heap min:' in sh and 'psram:' in sh)
check('S60 shell menu exposes monitors','Network monitor' in apps and 'System monitor' in apps and 'File commands' in apps)
check('no recursive delete command','rm -rf' not in sh and 'recursive delete' not in sh.lower())
check('no arbitrary process execution','fork(' not in sh and 'system(' not in sh and 'exec(' not in sh)
check('documentation updated',('# Symbian S3 OS v1.1' in rd or '# Symbian S3 OS v1.0' in rd or '# Symbian S3 OS v0.9' in rd) and 'v0.9.0 - Shell File + Network Tools' in ch and (ROOT/'docs/SHELL_V09.md').exists())
for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        t=p.read_text(encoding='utf-8')
        check('balanced braces '+str(p.relative_to(ROOT)), t.count('{')==t.count('}'))
print(f'PASS: {len(checks)}/{len(checks)} v0.9 gates')
