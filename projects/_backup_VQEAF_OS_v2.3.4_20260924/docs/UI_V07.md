# Symbian S3 OS v0.7 UI / Recovery

## Status bar
The E524546 handheld has no SIM/modem. The portrait S60 chrome therefore renders only:

- WiFi RSSI bars when connected
- Battery glyph
- Time and screen title

Bluetooth and microSD status are intentionally not placed in the global status cluster.

## App opening screen
Launching from Idle, Menu, Applications, Library, File Manager, Quick Panel or Open applications first displays a short S60-style application startup panel. The panel uses the target app's 28x28 procedural pixel icon, `Dang mo ung dung`, and a small progress strip. Resuming a suspended UI from Open applications uses `Dang tiep tuc`.

## Recovery and Safe Mode
Recovery is shown automatically after a crash-like reset, and Safe Mode is automatically selected after two consecutive crash boots. Holding KEY_A while powering on forces Safe Mode. In Safe Mode the OS leaves audio, BLE, browser, and automatic WiFi connection inactive.

## Keypad
- MENU: Menu / Idle toggle
- Hold MENU: Open applications
- Hold A: Recovery
- Hold B: keypad lock
- Hold OPTION: Settings
- Hold START: Music (blocked in Safe Mode)
- Hold SELECT: WiFi
- START/SELECT: Open/OK
- A/B: Back/Cancel outside text input
