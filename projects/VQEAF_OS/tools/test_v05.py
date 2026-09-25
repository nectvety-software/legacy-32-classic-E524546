#!/usr/bin/env python3
"""Host-side structural regression gates for Symbian S3 OS v0.5."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def check_new_screen_ids() -> None:
    types = read("src/core/Types.h")
    for name in ["QuickPanel", "Notifications", "Notes", "Lock"]:
        assert f"  {name}," in types or f"  {name}\n" in types, name


def check_notification_service() -> None:
    h = read("src/services/NotificationService.h")
    cpp = read("src/services/NotificationService.cpp")
    m = re.search(r"MAX_ITEMS\s*=\s*(\d+)", h)
    assert m and 4 <= int(m.group(1)) <= 12
    assert "unreadCount()" in h
    assert "markAllRead" in h and "remove(int index)" in h
    assert "items[0] = SystemNotification" in cpp
    assert "if (used < MAX_ITEMS) ++used" in cpp


def check_quick_panel_and_lock() -> None:
    apps = read("src/apps/Apps.cpp")
    main = read("src/main.cpp")
    ui = read("src/core/SymbianUI.cpp")
    assert 'chrome("Quick panel"' in apps
    assert '"WiFi radio", "Backlight", "Audio volume", "Notifications", "Lock device"' in apps
    assert "ScreenId::Lock" in main
    assert "updateAutoLock" in main
    assert "lockTimeoutSec" in main
    assert "Press START to unlock" in ui
    assert "setBacklight(8)" in main


def check_notes_persistence() -> None:
    settings_h = read("src/services/SettingsStore.h")
    settings_cpp = read("src/services/SettingsStore.cpp")
    apps = read("src/apps/Apps.cpp")
    assert "String note() const" in settings_h
    assert "saveNote" in settings_h
    assert 'prefs.getString("note"' in settings_cpp
    assert 'prefs.putString("note"' in settings_cpp
    assert 'keyboard.open("Edit note"' in apps
    assert 'notifications.push("Notes"' in apps


def check_settings_scroll_and_timeout() -> None:
    apps = read("src/apps/Apps.cpp")
    settings_h = read("src/services/SettingsStore.h")
    settings_cpp = read("src/services/SettingsStore.cpp")
    assert "uint16_t lockTimeoutSec = 60" in settings_h
    assert 'prefs.getUShort("lockSec", 60)' in settings_cpp
    assert "SETTINGS_COUNT = 7" in apps
    assert "ctx.ui.scrollbar(SETTINGS_COUNT" in apps
    assert "Auto keypad lock" in apps
    assert "nextLockTimeout" in apps


def check_idle_shortcuts() -> None:
    main = read("src/main.cpp")
    ui = read("src/core/SymbianUI.cpp")
    assert "e.key == Key::Option) enterScreen(ScreenId::QuickPanel)" in main
    assert "e.key == Key::Up) enterScreen(ScreenId::Notifications)" in main
    assert "e.key == Key::Start)  { enterScreen(ScreenId::Music)" in main
    assert "e.key == Key::Option) { enterScreen(ScreenId::Settings)" in main
    assert 'e.key == Key::B)      { notifications.push("Keypad locked"' in main


def check_ble_duplicate_fixed() -> None:
    apps = read("src/apps/Apps.cpp")
    assert apps.count("NimBLEScan *scanner = NimBLEDevice::getScan();") == 1


def check_version_docs() -> None:
    readme = read("README.md")
    changelog = read("CHANGELOG.md")
    main = read("src/main.cpp")
    assert any(v in readme for v in ["Symbian S3 OS v1.1", "Symbian S3 OS v1.0", "Symbian S3 OS v0.9", "Symbian S3 OS v0.8", "Symbian S3 OS v0.7", "Symbian S3 OS v0.6", "Symbian S3 OS v0.5"])
    assert "v0.5.0 - System Services Edition" in changelog
    assert any(v in main for v in ["v1.1.0 theme services", "v1.0.1 S60 services", "v1.0 S60 services", "v0.9 shell services", "v0.8 system services", "v0.7 system services", "v0.6.1 S60 UI / low-flicker", "v0.6 Core Services", "v0.5 System Services"])
    assert (ROOT / "docs/UI_V05.md").exists()


def check_cpp11_initializer_safety() -> None:
    notif = read("src/services/NotificationService.h")
    apps_h = read("src/apps/Apps.h")
    assert "SystemNotification()" in notif and "SystemNotification(const String &t" in notif
    assert "PopupState()" in apps_h


def main() -> None:
    tests = [
        check_new_screen_ids,
        check_notification_service,
        check_quick_panel_and_lock,
        check_notes_persistence,
        check_settings_scroll_and_timeout,
        check_idle_shortcuts,
        check_ble_duplicate_fixed,
        check_version_docs,
        check_cpp11_initializer_safety,
    ]
    for test in tests:
        test()
        print(f"PASS: {test.__name__}")
    print(f"PASS: {len(tests)}/{len(tests)} v0.5 regression gates")

if __name__ == "__main__":
    main()
