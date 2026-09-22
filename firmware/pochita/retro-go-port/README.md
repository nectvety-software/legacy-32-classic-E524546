# Retro-Go port for POCHITA E524546

This port targets the `legacy-32-classic E524546` handset:

- ESP32-S3-WROOM-1 N16R8
- ST7789 240x320 portrait LCD
- LCD SPI: MOSI 12, CLK 48, CS 14, DC 47, RESET 3, backlight 39
- microSD SPI: MISO 9, MOSI 11, CLK 13, CS 10
- MAX98357A: DATA 40, BCLK 41, LRCLK 42
- ten active-low GPIO buttons with the same Nokia-style logical mapping as POCHITA OS

Retro-Go is a complete ESP-IDF multi-application firmware, not an Arduino
library. It cannot be linked into the current POCHITA sketch as a normal
PlatformIO dependency. This folder supplies a hardware target so a real
Retro-Go image can be built for this exact board.

## Prepare

Install ESP-IDF 4.4 through 5.3, clone Retro-Go, then run:

```powershell
.\retro-go-port\install-port.ps1 -RetroGoPath D:\path\to\retro-go
```

## Build and flash

From the Retro-Go repository:

```powershell
python rg_tool.py --target=pochita-e524546 build-img
python rg_tool.py --target=pochita-e524546 --port=COM5 install
```

Flashing the Retro-Go image replaces the active POCHITA firmware. Keep the
PlatformIO POCHITA build so it can be restored later. A future dual-boot build
requires a shared custom partition table and two independently built ESP-IDF
applications; ROM emulation cannot safely execute as an arbitrary SD `.bin`
inside the Arduino process.

## SD card layout

Use the standard Retro-Go folders. Only use ROM dumps that you are legally
allowed to use.

```text
/roms/nes/
/roms/gb/
/roms/gbc/
/roms/sms/
/roms/gg/
/roms/md/
/retro-go/
```

The POCHITA OpenRhynn package is a native POCHITA game and is intentionally
identified by its `POCHITA-RHYNN-V1` header. It is not a Mega Drive ROM and
must not be passed to the Genesis emulator.

