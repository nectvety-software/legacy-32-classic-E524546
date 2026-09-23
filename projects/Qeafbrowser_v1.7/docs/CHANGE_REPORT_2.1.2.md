# Qeafbrowser v2.1.2 — Change Report

## Files changed

### `src/main.cpp`
- Corrected `TJpg_Decoder` result handling: `JDR_OK` is zero, so JPEG success is now checked explicitly with `!= JDR_OK`.
- Checks both `getJpgSize()` and `drawJpg()` return values.
- Added explicit `uint32_t` bounds/casts for JPEG RAM buffer sizes.
- Added serial diagnostics for PNG/JPEG open/decode failures.
- Added PSRAM boot diagnostics: detected state, total size and free size.
- Changed document/network buffer fallback to allocate independently, avoiding loss/leak of a successful PSRAM allocation if only one allocation fails.
- Updated About version to v2.1.2.

### `platformio.ini`
- Uses custom board ID `qeafbrowser-n16r8`.
- Keeps 16 MB flash, `qio_opi`, OPI PSRAM and LittleFS configuration explicit.
- Keeps PNGdec pinned to `1.1.6` and TJpg_Decoder to `^1.1.0`.

### `boards/qeafbrowser-n16r8.json`
- New PlatformIO custom board profile for ESP32-S3-WROOM-1 N16R8.
- Declares 16 MB Quad flash and 8 MB Octal PSRAM.
- Declares `memory_type=qio_opi`, `psram_type=opi` and `BOARD_HAS_PSRAM`.

### Documentation
- `docs/BUILD_FIX_2.1.2.md`
- `docs/ARDUINO_API_SMOKE_2.1.2.txt`
- `docs/CHANGE_REPORT_2.1.2.md`

## Verification

- Headless Linux build + keypad regression: PASS.
- Qeafivels regression including thumbnail cache/LRU: PASS.
- Forced `ARDUINO` API/type smoke compile: PASS.
- Custom board JSON syntax validation: PASS.

A real `pio run` / `pio run -t upload` still needs to be run on the user's Windows PlatformIO installation because the ESP32 PlatformIO toolchain is not present in this execution environment.
