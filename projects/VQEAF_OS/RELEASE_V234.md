# VQEAF OS v2.3.4 — Pixel-art icon integration

Updated directly from VQEAF OS v2.3.3 Offline/Dry-run Full Source.

- 12 system icon designs from the latest user-generated image, each native
  transparent 36x36 for Home/Menu and 24x24 for list rows.
- Each icon now stores color locally in an RGB565 palette with bounded 1-byte
  horizontal RLE; total raw icon payload 8,260 bytes in Flash/rodata.
- No dynamic allocation or external decoder for icon runtime.
- Home shortcuts (WiFi/Music/Files), Menu (all 12 icons) and system lists
  display the new assets through existing C++ routes.
- Original portrait 240x320 layout, ESP32-S3 pins, .vqeaf theme handling,
  signed .qeapp package format and v2.3.3 build scripts preserved.
- Theme still controls selection background and focus frame. Art colors are
  deliberately fixed to the pixel icon design to preserve consistency.
- New source PNG pack, original reference sheet and deterministic offline icon
  compiler included in tools/pixel_icon_assets/.

## Observed checks

- PASS `python tools/test_pixel_icons_v234.py`: 22,464 pixel checks, 24 variants.
- PASS `python tools/test_board_host_smoke.py`: production C++ icon GUI routes.
- PASS `python tools/check_board_config.py`: N16R8 hardware configuration.
- PASS `python tools/build_pio.py --host-only`: board preflight, build guards,
  host C++ link and host icon UI smoke.
- PASS `python tools/build_offline.py --dry-run --check-only`: simulation only.
- PASS old-layout regression with 36px icon pixels excluded intentionally.

**Not verified:** PlatformIO target build, `.bin` generation, ESP32-S3 hardware
runtime, SPI display color calibration. See `docs/verification/VERIFY_ICON_INTEGRATION_V234.md`.

## Build

```powershell
cd VQEAF-OS
py -3 tools/test_board_host_smoke.py
py -3 tools/build_offline.py --dry-run --check-only
# Complete this on a machine with pre-cached PlatformIO libraries:
.\tools\build_offline.bat
```
