#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
readme = (ROOT / 'README.md').read_text(encoding='utf-8')
changelog = (ROOT / 'CHANGELOG.md').read_text(encoding='utf-8')

checks = []

def check(name, cond):
    if not cond:
        raise AssertionError(name)
    checks.append(name)
    print('PASS:', name)

check('legacy global AppContext ctx removed', 'AppContext ctx{' not in main)
check('application context renamed and file-local', 'static AppContext appCtx{' in main)
check('main.cpp contains no standalone ctx identifier', re.search(r'\bctx\b', main) is None)

runtime_globals = [
    'TFT_eSPI tft;', 'SymbianUI ui(tft);', 'InputManager input;',
    'SettingsStore settings;', 'StorageService storage;', 'MusicService music;',
    'TextKeyboard keyboard;', 'NotificationService notifications;',
    'LauncherApp launcher;', 'WiFiApp wifiApp;', 'BleApp bleApp;',
    'FilesApp filesApp;', 'CollectionApp collectionApp;', 'MusicApp musicApp;',
    'SettingsApp settingsApp;', 'ApplicationsApp applicationsApp;',
    'QuickPanelApp quickPanelApp;', 'NotificationCenterApp notificationApp;',
    'NotesApp notesApp;'
]
check('runtime objects use internal linkage', all(('static ' + decl) in main for decl in runtime_globals))
check('README preserves v0.5.1 linker hotfix documentation', 'v0.5.1 linker hotfix' in readme and 'libnet80211.a' in readme)
check('CHANGELOG documents ctx collision fix', '## v0.5.1 - Linker symbol hotfix' in changelog and 'multiple definition of ctx' in changelog)

print(f'PASS: {len(checks)}/{len(checks)} v0.5.1 linker regression gates')
