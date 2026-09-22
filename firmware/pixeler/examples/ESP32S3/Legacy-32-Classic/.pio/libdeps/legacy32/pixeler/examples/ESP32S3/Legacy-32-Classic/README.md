# Legacy-32-Classic E524546 hardware test

Firmware test for ESP32-S3-WROOM-1-N16R8, a 240x320 ST7789 display, ten active-low buttons and a 1-bit SDMMC card slot.

## Open in VS Code + PlatformIO

Open `Legacy-32-Classic.code-workspace` from the Pixeler repository root. The first workspace folder is the firmware project containing this `platformio.ini`, so the PlatformIO toolbar and project tasks are detected correctly.

Alternatively, use **File > Open Folder...** and open this `examples/ESP32S3/Legacy-32-Classic` directory directly. Do not open only the Pixeler repository root when you want to build this firmware example.

Build and upload from this directory:

```powershell
py -m platformio run
py -m platformio run --target upload
py -m platformio device monitor
```

If the screen backlight is active-low on a board revision, swap `LOW` and `HIGH` for `TFT_BL` in `src/main.cpp`. If colors are inverted, change `display->invertDisplay(false)` to `true`.
