# Symbian S3 OS v0.6 - Core Services & Tasking

## Display contract

The physical device is portrait. Runtime dimensions remain exactly **240x320** with `TFT_ROTATION = 0`.

## S60 task model

Long-pressing **MENU** opens `Open applications`, matching the behavior of classic S60 devices more closely than v0.5's long-MENU-to-Home shortcut.

The task switcher keeps at most six recent app screen IDs. It is an MRU list: reopening an existing app moves it to slot 0 instead of duplicating it. Idle, Splash, Lock, Launcher, Quick Panel and the task switcher itself are not retained as tasks.

Selecting a recent app uses the resume path. Stateful apps therefore keep their current in-memory UI state:

- WiFi/BLE lists are not automatically rescanned.
- File Manager keeps the current folder and cursor.
- Music keeps the current library cursor/player state.
- Settings and Applications keep the current selection.
- Notifications and Notes keep their current list/editor state.

This is cooperative UI task switching, not pre-emptive multitasking. Background Music and system services continue from the main event loop.

## WiFi profile lifecycle

`WiFiProfileStore` stores up to five profiles in Preferences/NVS. The most recently saved profile is moved to the first slot. The last successfully connected saved SSID is remembered.

At boot, if `WiFi auto reconnect` is enabled, the OS attempts to connect to the remembered profile. A network already in the saved profile list connects without opening the password keyboard. `Options > Forget saved` removes credentials for the highlighted SSID.

Passwords are stored as NVS strings. Enable ESP32 flash/NVS encryption for deployments where credential-at-rest protection is required.

## System health service

`SystemService` checks heap health every five seconds. It records the lowest observed free heap. A low-memory notification is emitted once when free heap drops below 48 KB and is armed again only after memory recovers above 64 KB. This avoids notification storms while still exposing memory pressure during testing.

System Info shows:

- current heap and PSRAM,
- microSD state,
- I2S audio state,
- persistent boot count,
- uptime,
- minimum observed heap,
- number of saved WiFi profiles,
- notification count,
- automatic lock timeout.

## Keypad

- MENU: launcher / return to Idle from launcher
- Hold MENU: Recent Apps
- OPTION on Idle: Quick Panel
- Hold OPTION: Settings
- Hold START: Music
- Hold SELECT: WiFi
- Hold A: Notifications
- Hold B: Lock
- START or SELECT: confirm/open
- A or B: back/cancel outside text entry
