# Qeafbrowser v2.1.2 — PlatformIO build hardening

## Source log diagnosis

The supplied PlatformIO log shows the previous compile stopped at `PNG::openRAM()` because PNGdec 1.1.6 expects an `int (*)(PNGDRAW*)` callback. v2.1.1 already changed that callback to return `int`.

The same log also shows PlatformIO identifies the stock board profile as `ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` even though Qeafbrowser targets ESP32-S3-WROOM-1 N16R8. v2.1.2 hardens the stock board overrides so the actual build settings match N16R8.

## Changes in v2.1.2

- Added `board_build.psram_type = opi`.
- Added `board_build.flash_size = 16MB`.
- Changed partition table from `huge_app.csv` to `default_16MB.csv` so the 16 MB flash layout and LittleFS partition agree with the target.
- Kept `board_build.arduino.memory_type = qio_opi` and `board_build.flash_mode = qio`.
- Kept `-D BOARD_HAS_PSRAM=1`.
- Pinned `TJpg_Decoder` to 1.1.0 and PNGdec to 1.1.6 for deterministic APIs.
- Added a compile-time callback type assignment (`PNG_DRAW_CALLBACK *draw_cb = png_thumb_draw`) so future PNGdec signature changes fail at the exact adapter line.
- Added explicit `uint32_t` length conversion for `TJpgDec.drawJpg()` after a size guard.

## Build commands

Run a clean build after changing board memory settings:

```powershell
pio run -t clean
pio run
pio run -t upload
```

## Expected PlatformIO settings

The stock board name may still print `N8 (No PSRAM)` because that text comes from the base board manifest. The effective overrides are the important part: 16 MB flash, QIO flash, OPI PSRAM, `qio_opi` Arduino memory type, and `BOARD_HAS_PSRAM`.

## Validation performed here

The Linux simulator build/regression is run from the same `src/*.cpp`; it cannot validate the ESP32 cross-compiler or physical PSRAM. A new PlatformIO log is still the authoritative check for any next embedded-only error.
