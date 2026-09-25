#!/usr/bin/env python3
"""Regression gates for Symbian S3 OS v0.8 Shell Edition."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
def read(rel): return (ROOT / rel).read_text(encoding='utf-8')
checks=[]
def check(name, cond):
    if not cond: raise AssertionError(name)
    checks.append(name); print('PASS:', name)

types=read('src/core/Types.h')
main=read('src/main.cpp')
apph=read('src/apps/Apps.h')
apps=read('src/apps/Apps.cpp')
shell_h=read('src/services/ShellService.h')
shell=read('src/services/ShellService.cpp')
sys=read('src/services/SystemService.cpp')
ui=read('src/core/SymbianUI.cpp')
kbd_h=read('src/core/TextKeyboard.h')
readme=read('README.md')
changelog=read('CHANGELOG.md')
doc=read('docs/SHELL_V08.md')

check('Shell screen registered', re.search(r'\bShell,', types) is not None)
check('Shell service/app context registered', 'ShellService &shell;' in apph and 'class ShellApp' in apph)
check('main owns file-local shell service', 'static ShellService shellService;' in main and 'static ShellApp shellApp;' in main)
check('AppContext receives shell service', 'imageViewer, shellService};' in main)
check('shell initialized after storage mount', 'shellService.begin(storage, systemService);' in main)
check('shell enter/draw dispatch exists', 'case ScreenId::Shell:' in main and 'shellApp.handle(appCtx,e)' in main)
check('shell is not blocked in Safe Mode', 's == ScreenId::Shell' not in main[main.index('static bool disabledInSafeMode'):main.index('static bool shouldShowOpening')])
check('shell is an MRU task screen', 'case ScreenId::Shell:' in sys and 'return "Shell"' in sys)
check('shell has terminal icon', 'return "Term"' in sys and 'kind == "Term"' in ui)
check('Applications exposes shell', '"Shell", "Open apps"' in apps and 'ScreenId::Shell, ScreenId::TaskSwitcher' in apps)
check('Launcher options expose shell', '"Qeafbrowser", "Shell"' in apps)
check('opening-app lifecycle includes shell automatically', 'SystemService::isTaskScreen(to)' in main)

check('bounded output ring', 'MAX_LINES = 20' in shell_h and 'LINE_CHARS = 39' in shell_h)
check('bounded command history', 'HISTORY_MAX = 8' in shell_h and ('COMMAND_CHARS = 95' in shell_h or 'COMMAND_CHARS = 127' in shell_h))
check('no fork or exec command engine', 'fork(' not in shell and 'system(' not in shell and 'exec(' not in shell)
check('bounded ls output', 'shown < 14' in shell)
check('bounded cat output', 'emitted < 12' in shell)
check('microSD cwd normalization', 'normalizePath' in shell and 'currentDir' in shell_h)

for cmd in ['help','clear','uname','version','uptime','free','df','mount','pwd','cd','ls','cat','stat','date','wifi','ifconfig','ip','ps','dmesg','safe','reboot','echo','history']:
    check('shell command '+cmd, f'cmd == "{cmd}"' in shell or (cmd == 'clear' and 'cmd == "clear"' in shell))

check('wifi shell command can report/scan/connect/on/off', 'WiFi.scanNetworks' in shell and 'WiFi.begin(ssid.c_str()' in shell and 'WiFi.mode(WIFI_OFF)' in shell and 'WiFi.mode(WIFI_STA)' in shell)
check('safe mode shell command integrates recovery', 'setSafeMode(true)' in shell and 'setSafeMode(false)' in shell)
check('reboot uses deferred request', 'rebootRequested = true' in shell and 'takeRebootRequest()' in shell_h and 'ESP.restart()' in apps)
check('shell prompt and S60 chrome', 'ctx.ui.chrome("Shell"' in apps and 's3:' in apps and 'ctx.ui.softkeys("Options", "Command", "Back")' in apps)
check('START opens command editor', 'openCommand(ctx)' in apps)
check('UP recalls command history', 'historyAt(historyIndex)' in apps)
check('shared keyboard has slash and colon', 'COUNT = 52' in kbd_h and '?/:"' in kbd_h)
check('v0.8 docs identify S31/S3 architecture difference', 'ESP32-S31 RISC-V' in readme and 'ESP32-S3-WROOM-1 N16R8 (Xtensa LX7)' in doc)
check('v0.8 changelog present', 'v0.8.0 - Shell Edition' in changelog)

for p in sorted((ROOT/'src').rglob('*')):
    if p.suffix in {'.cpp','.h'}:
        t=p.read_text(encoding='utf-8')
        check('balanced braces '+str(p.relative_to(ROOT)), t.count('{') == t.count('}'))

print(f'PASS: {len(checks)}/{len(checks)} v0.8 gates')
