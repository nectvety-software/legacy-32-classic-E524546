# VQEAF OS v2.1 — ESP32-S3 handheld (240x320 portrait)

**VQEAF OS** replaces the user-facing Symbian-style launcher of the supplied
v2.0.1 firmware. It uses an original Retro-Go-inspired **portrait tab-and-list
launcher**; no Retro-Go code, copyrighted imagery or theme files are bundled.
All existing hardware mapping and primary system applications are retained.

## Hardware and build

- ESP32-S3-WROOM-1 N16R8, TFT ST7789 **240 x 320 portrait** (`TFT_ROTATION = 0`)
- Same 10 physical keys; pinout in `include/BoardConfig.h`
- SD MMC 1-bit + LittleFS + TFT_eSPI; PlatformIO / Arduino; monitor 115200
- `pio run -e vqeaf_os` compiles firmware; `pio run -e vqeaf_os -t upload`
  flashes firmware; `pio device monitor -b 115200` opens logs
- `cd VQEAF-OS && python tools/test_v14_build.py` for host-only syntax and link;
  **this is not an ESP32 PlatformIO build or a real-board test**

## Launcher (physical device)

Power on normally: VQEAF splash -> tab launcher. Hold **DOWN** or **A**
during power-on for recovery / Safe Mode. The six tabs are Home, Internet,
Applications, Media, System and Settings. Left/right switch tab; up/down choose
list entries, Start opens, OPTION opens dialog, B shows entry details, MENU
returns to Home launcher, and A closes dialogs or switches to Home tab. Hold
SELECT ~650ms to toggle Game/T9; when editing text, T9 numeric multi-tap is
available. Standby/lock screen and existing system applications still exist.

Icons in the launcher are procedural for built-in applications; signed installed
QEAPP/2 applications show their real bundled **32 x 32 RGB565** icon in the
selection preview if provided. Preview names/descriptions and selected bar
track the current item without blanking the entire display on each keypress.
An unmeasured battery icon is outline only (no fabricated voltage).

## Theme loading — `.vqeaf`

Copy VQEAF Theme Studio exports to `sd:/System/Themes/` or `sd:/Themes/`,
open Themes (Settings tab), select and apply. `vqeaf_night.vqeaf` and
`vqeaf_day.vqeaf` in `sd/Themes/` provide example palettes and optional
`launcher { ... }` extension settings. All regular Theme Studio `palette {}`
keys remain valid; unsupported backgrounds, shell simulation and advanced
animation data are safely skipped (not rendered on the embedded LCD).
Theme loading reads a bounded text prefix even for large embedded images.
Unsupported/missing files fall back to built-in VQEAF Night without changing
saved external-theme preference. A Studio re-export may drop the optional
firmware-only launcher overrides; the palette still works.

## Apps — signed `.qeapp` packages

**Existing QEAPP/2 format is preserved.** This build intentionally does **not**
introduce an incompatible ZIP or native executable format: QEAPP/2 is a signed,
bounded binary container with ECDSA P-256, supports `web` (HTTPS URL) and `text`
(bundled document), and verifies installed content at launch.
Copy trusted signed packages to `sd:/System/Apps/Inbox/`, use **Applications ->
App installer** to verify and install. Entries appear in the Applications tab.
To publish a new package, first provision your own publisher key as described
in `docs/QEAPP_V15_SIGNING.md`, then sign using `tools/build_qeapp.py`.
Do not put an untrusted private key in firmware, SD or public repositories.
**Arbitrary Lua/ELF/VXP payload execution is not supported** by this firmware.

## Compatibility and documentation

All older app implementation files, browser, WiFi, BLE, filesystem, crash
recovery and the signed package install pipeline remain in the project.
Existing NVS namespaces retain their historical names to avoid resetting
WiFi, preferences or recovery state. Old `.qeapp` QEAPP/1 packages are rejected,
as in the supplied v2.0.1 firmware; re-sign with your own QEAPP/2 key.
Existing legacy green theme remains available as `s60_green.vqeaf`, displayed
as **Legacy Lime**; it is no longer the fresh-install default.

- `docs/VQEAF_LAUNCHER_V21.md`: new UI architecture, skin schema and input map
- `docs/VQEAF_VERIFY_V21.md`: verification commands and limitations
- `docs/legacy/README_v201.md`: original v2.0.1 release notes (historical)
- `CHANGELOG.md`: legacy changelog + v2.1 entry
- See `docs/BOARD_TEST_V201.md` before connecting the device
