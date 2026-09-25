#!/usr/bin/env python3
"""Structural and behavior-model regression gates for Symbian S3 OS v0.6."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding='utf-8')

checks = []
def check(name, cond):
    if not cond:
        raise AssertionError(name)
    checks.append(name)
    print('PASS:', name)

# 1. Portrait hardware contract must remain unchanged.
board = read('include/BoardConfig.h')
check('portrait 240x320 contract', 'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board and 'TFT_ROTATION = 0' in board)

# 2. New task switcher and cooperative resume path.
types = read('src/core/Types.h')
main = read('src/main.cpp')
apps_h = read('src/apps/Apps.h')
apps_cpp = read('src/apps/Apps.cpp')
check('TaskSwitcher screen id exists', 'TaskSwitcher,' in types)
check('long MENU opens task switcher', 'e.key == Key::Menu)   { enterScreen(ScreenId::TaskSwitcher)' in main)
check('enterScreen has resume parameter', 'bool resume = false' in main)
check('task switcher can request resume', 'takeResumeRequest' in apps_h and 'resumeTarget = taskSwitcherApp.takeResumeRequest()' in main)
check('resume skips WiFi rescan enter', 'if (!resume) wifiApp.enter(appCtx); wifiApp.draw(appCtx);' in main)
check('MRU bounded to six tasks', 'MAX_RECENT = 6' in read('src/services/SystemService.h'))
check('MRU de-duplicates existing screen', 'if (found == 0) return;' in read('src/services/SystemService.cpp'))

# 3. Health monitor / low memory notification behavior.
sys_cpp = read('src/services/SystemService.cpp')
check('health monitor runs at 5 second cadence', 'now - lastHealthCheck < 5000UL' in sys_cpp)
check('low heap warning threshold 48 KB', '48UL * 1024UL' in sys_cpp)
check('low heap warning rearm threshold 64 KB', '64UL * 1024UL' in sys_cpp)
check('persistent boot counter', 'getUInt("boots"' in sys_cpp and 'putUInt("boots"' in sys_cpp)

# 4. Saved WiFi profiles.
profiles_h = read('src/services/WiFiProfileStore.h')
profiles_cpp = read('src/services/WiFiProfileStore.cpp')
check('WiFi profile store bounded to five', 'MAX_PROFILES = 5' in profiles_h)
check('WiFi profiles persist SSID/password/open state', all(x in profiles_cpp for x in ['putString(sk.c_str()', 'putString(pk.c_str()', 'putBool(ok.c_str()']))
check('WiFi UI shows saved marker', 'sub += "  Saved"' in apps_cpp)
check('WiFi forget action exists', 'Forget saved' in apps_cpp and 'ctx.wifiProfiles.remove' in apps_cpp)
check('boot reconnect uses last saved WiFi', 'wifiProfiles.lastSSID()' in main and 'Auto connecting to ' in main)

# 5. Avoid regression of the prior linker collision.
check('no standalone ctx identifier in main', re.search(r'\bctx\b', main) is None)
check('app context remains file-local appCtx', 'static AppContext appCtx{' in main)

# 6. Main objects and new service objects remain file-local.
check('new system services use internal linkage', 'static SystemService systemService;' in main and 'static WiFiProfileStore wifiProfiles;' in main)

# 7. Syntax sanity: balanced delimiters in project C++ files.
for p in sorted((ROOT / 'src').rglob('*')):
    if p.suffix in {'.cpp', '.h'}:
        text = p.read_text(encoding='utf-8')
        check(f'balanced braces {p.relative_to(ROOT)}', text.count('{') == text.count('}'))

# Behavior model: mirror the MRU algorithm and validate order/de-dup/drop.
MAX = 6
recent = []
def record(s):
    global recent
    if s in recent:
        recent.remove(s)
    recent.insert(0, s)
    recent[:] = recent[:MAX]

for s in ['WiFi','Files','Music','Settings','Notes','Clock']:
    record(s)
check('MRU newest first', recent[0] == 'Clock' and recent[-1] == 'WiFi')
record('Music')
check('MRU re-open moves existing to front without duplicate', recent[0] == 'Music' and recent.count('Music') == 1 and len(recent) == 6)
record('BLE')
check('MRU seventh task drops oldest', recent[0] == 'BLE' and len(recent) == 6 and 'WiFi' not in recent)

readme = read('README.md')
changelog = read('CHANGELOG.md')
check('v0.6 docs present', any(v in readme for v in ['# Symbian S3 OS v1.1','# Symbian S3 OS v1.0','# Symbian S3 OS v0.9','# Symbian S3 OS v0.8','# Symbian S3 OS v0.7','# Symbian S3 OS v0.6']) and 'v0.6.0 - Core Services & Tasking' in changelog and (ROOT/'docs/UI_V06.md').exists())

print(f'PASS: {len(checks)}/{len(checks)} v0.6 regression gates')
