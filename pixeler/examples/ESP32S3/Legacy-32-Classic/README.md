# Pixeler for Legacy-32-Classic E524546

Full Pixeler firmware adapted from the upstream `Perfect-Devboard` example for:

- ESP32-S3-WROOM-1-N16R8
- 240x320 ST7789 SPI display
- ten active-low buttons with internal pull-ups
- SD card in 1-bit SDMMC mode

## Open in VS Code + PlatformIO

Open `Legacy-32-Classic.code-workspace` or open the Pixeler repository root directly.
The repository root now contains `platformio.ini`, so PlatformIO detects the
`legacy32` environment without needing to open a nested example directory.

The nested `examples/ESP32S3/Legacy-32-Classic/platformio.ini` is retained so the
device example can still be opened and built independently.

Build, upload and open the serial monitor from the repository root. In VS Code,
the equivalent commands are available from the PlatformIO toolbar:

```powershell
pio run
pio run --target upload
pio device monitor
```

The first full build can take several minutes because Pixeler contains graphics,
Lua, networking and game sources.

## Preserved features

The original Pixeler flow and contexts are retained: home/menu UI, Wi-Fi setup,
file browser, reader, file server, firmware screen, display brightness settings,
Lua support, Chess and Sokoban.

## Controls

- D-pad: navigate
- SELECT (GPIO 16): OK/confirm
- B (GPIO 5): back/cancel
- MENU, OPTION, START and A are registered and available to applications

## SD card

Format the card as FAT32. The firmware uses CLK=13, CMD=11 and DAT0=9.
DAT3/CS=10 is not used in 1-bit SDMMC mode.

## Notes

The MP3 screen from the reference board is disabled because this board definition
does not include a compatible audio codec/coprocessor pinout. Battery and RGB LED
features are also disabled.

If PlatformIO cannot find the serial port automatically, add for example
`upload_port = COM5` and `monitor_port = COM5` under `[env:legacy32]` in
`platformio.ini`, replacing COM5 with the port shown in Windows Device Manager.
