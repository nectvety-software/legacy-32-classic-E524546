#!/usr/bin/env python3
"""Static routing contract, complementary to the live C++ pixel suite."""
from pathlib import Path
import re
R=Path(__file__).resolve().parents[1]
catalog=(R/'src/core/UiIconCatalog.h').read_text()
ui=(R/'src/core/SymbianUI.cpp').read_text()
app=(R/'src/apps/LauncherGrid.cpp').read_text()
main=(R/'src/main.cpp').read_text()
expected=['WiFi','Bluetooth','Music','Files','Gallery','Internet','Shell','Recovery','Settings','Themes','Apps','Library']
assert re.findall(r'VqeafIcons::Id::([A-Za-z]+)',catalog.split('GRID_ASSETS[12]')[1].split('};')[0])==expected
assert re.findall(r'VqeafIcons::Id::([A-Za-z]+)',catalog.split('HOME_ASSETS[3]')[1].split('};')[0])==['WiFi','Music','Files']
assert 'VqeafIcons::drawOpaque(tft, expected, iconX, iconY, 36' in ui
assert 'VqeafIcons::drawOpaque(tft, id, iconX, r.y + 6, 36' in ui
assert 'VqeafIcons::drawOpaque(tft, standardId, 12, y + 9, 24' in ui
assert 'const char *const (&launcherIcon)[12] = UiIconCatalog::GRID_IDS' in app
assert 'ctx.ui.gridItem(old,' in app and 'ctx.ui.gridItem(index,' in app
assert 'ui.idleShortcutDelta(old,idleShortcut)' in main
assert (R/'src/services/QeappSignature.cpp').exists()
assert (R/'src/services/ThemeFileService.cpp').exists()
assert 'TFT_ROTATION = 0' in (R/'include/BoardConfig.h').read_text()
print('PASS: Home/Menu production routing, 36/24px asset IDs, partial redraw, theme and QEAPP unchanged')
