# Symbian S3 OS v0.5 - System Services Edition

Target: ESP32-S3-WROOM-1 N16R8, ST7789 portrait 240x320, 10-key keypad.

## New system features

### Quick panel
Open from Idle/Home with `OPTION`.

Items:
- WiFi radio on/off
- Backlight 20-100%
- Audio volume 0-100%
- Notification center
- Lock device

Use Up/Down to choose, Left/Right to adjust, START/SELECT to apply/open, A/B to return.

### Notification center
A fixed-size in-RAM queue stores the newest 12 system events. Current producers include:
- microSD initialization
- I2S audio initialization
- WiFi connected/disconnected
- keypad lock/unlock
- Notes save/clear
- WiFi radio changes from Quick panel

The queue is intentionally RAM-only so repeated events do not wear flash.

### Keypad lock and auto-lock
- Idle: A or B locks immediately.
- Long B from most screens locks immediately.
- Long A opens Notifications.
- START unlocks from the lock screen.
- Lock screen dims the TFT backlight after 10 seconds.
- Auto-lock timeout is configurable: Off, 30 s, 60 s, 2 min, 5 min.

### Notes
A single 63-character quick note is persisted in NVS/Preferences. The existing on-screen keyboard is reused so no extra text-entry runtime is required.

### Idle/Home additions
- unread notification count
- background music state
- Up opens Notifications
- OPTION opens Quick panel
- A/B locks the keypad

## Existing shortcuts retained
- Short MENU: Idle <-> 3x3 launcher / return to launcher
- Hold MENU: Idle/Home
- Hold OPTION: Settings
- Hold START: Music
- Hold SELECT: WiFi

## RAM policy
Notification history is capped at 12 entries. No framebuffer is introduced. Icons remain procedural TFT primitives.
