#!/usr/bin/env python3
"""Native executable model tests + structural integration checks; not target-board validation."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='s3-v18-') as d:
    binary=str(Path(d)/'v18_test')
    subprocess.run(['g++','-std=gnu++11','-Wall','-Wextra','-Werror',
                    str(r/'src/services/CalculatorEngine.cpp'),str(r/'tools/test_v18_core.cpp'),'-o',binary],check=True)
    subprocess.run([binary],check=True)
main=(r/'src/main.cpp').read_text()
apps=(r/'src/apps/Apps.cpp').read_text()
svc=(r/'src/services/MusicService.cpp').read_text()
board=(r/'include/BoardConfig.h').read_text()
checks={
 'portrait panel': 'SCREEN_W = 240' in board and 'SCREEN_H = 320' in board,
 'calculator screen routed': 'case ScreenId::Calculator:' in main and 'calculatorApp.draw' in main,
 'stopwatch screen routed': 'case ScreenId::Stopwatch:' in main and 'stopwatchApp.tick' in main,
 'utility apps in Applications': '"Calculator","Stopwatch"' in apps,
 'music auto-next in loop': 'musicApp.tick(appCtx, screen == ScreenId::Music)' in main,
 'WAV end drains DMA': 'finishDeadline=millis()+150UL' in svc,
 'bounded playlist no heap and nonrepeating shuffle': 'PlaylistNavigator playlist' in (r/'src/apps/Apps.h').read_text(),
 'music repeats Off/All/One': all(x in (r/'src/services/PlaylistNavigator.h').read_text() for x in ['Repeat::Off','Repeat::All','Repeat::One']),
 'gallery slideshow tick': 'galleryApp.tick(appCtx, screen == ScreenId::Gallery)' in main,
 'gallery outside library path': 'if(!found&&ctx.storage.mounted())' in apps,
 'BLE strongest results': 'power>devs[weakest].rssi' in apps,
 'BLE details has return state': 'if (details)' in apps and 'details=false; draw(ctx)' in apps,
 'safe mode audio guard unchanged': 'if (!systemService.safeMode())' in main,
 'TFT no full-screen sprite': 'TFT_eSprite' not in apps,
}
for label,ok in checks.items():
    if not ok:raise AssertionError(label)
    print('PASS:',label)
print(f'PASS: {len(checks)} v1.8 source integration checks')
