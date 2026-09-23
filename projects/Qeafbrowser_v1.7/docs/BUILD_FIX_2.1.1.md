# Qeafbrowser v2.1.1 — ESP32-S3 PlatformIO Build Fix

## Failure fixed

PlatformIO resolved PNGdec 1.1.6, whose `PNG_DRAW_CALLBACK` type is `int (PNGDRAW*)`. Qeafbrowser v2.1 used a `void png_thumb_draw(PNGDRAW*)`, causing `invalid conversion ... to int (*)(PNGDRAW*)`.

## Source change

- `png_thumb_draw` return type: `void` -> `int`.
- Every early path returns `1`; successful scanline handling ends with `return 1`.
- `PNG::openRAM` receives a validated `int` byte length.
- PNGdec pinned to `1.1.6`.

## PlatformIO cleanup

Removed duplicate/conflicting manual USB macros from `build_flags`. The existing `board_build.arduino.cdc_on_boot = 1` remains.

## Note on hardware banner

PlatformIO may still print the stock `esp32-s3-devkitc-1` board label as N8/No PSRAM even though this project overrides flash size and uses `qio_opi` plus `BOARD_HAS_PSRAM`. Confirm the physical module is ESP32-S3 N16R8 before relying on PSRAM at runtime.
