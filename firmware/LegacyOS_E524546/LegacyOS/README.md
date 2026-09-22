# LegacyOS v1.0
## LEGACY-32 CLASSIC E524546 — Android-Style Handheld OS

```
╔══════════════════════════════════════════════════════╗
║        LEGACY-32 CLASSIC E524546 - LegacyOS v1.0    ║
║     ESP32-S3-WROOM-1 (N16R8) | ST7789 240x320       ║
╚══════════════════════════════════════════════════════╝
```

---

## Hardware Specifications

| Component | Details |
|-----------|---------|
| MCU | ESP32-S3-WROOM-1 (N16R8) — 240MHz dual-core |
| Flash | 16MB QIO |
| PSRAM | 8MB OPI |
| Display | ST7789 TFT 2" 240×320 SPI |
| Input | 10 buttons (D-Pad + A/B + Start/Select/Menu/Option) |
| Storage | MicroSD Card (SPI) |
| Radio | WiFi 2.4GHz + BLE 5.0 (built-in) |
| LoRa | Optional SX1276/78 (stub — set pins in LoRaManager.h) |

---

## Pin Wiring

### TFT ST7789
| Display Pin | ESP32-S3 GPIO |
|-------------|---------------|
| VCC | 3.3V |
| GND | GND |
| LEDK (BL) | GPIO 39 |
| D/C | GPIO 47 |
| CS | GPIO 14 |
| SCL | GPIO 48 |
| SDA | GPIO 12 |
| RESET | GPIO 3 |

### SD Card (SPI)
| SD Pin | ESP32-S3 GPIO |
|--------|---------------|
| VCC | 3.3V |
| GND | GND |
| CS (CD/DAT3) | GPIO 10 |
| MOSI (CMD) | GPIO 11 |
| CLK | GPIO 13 |
| MISO (DAT0) | GPIO 9 |

### Buttons (all INPUT_PULLUP — Active LOW)
| Button | GPIO |
|--------|------|
| KEY_UP | 7 |
| KEY_DOWN | 46 |
| KEY_LEFT | 45 |
| KEY_RIGHT | 6 |
| KEY_MENU | 18 |
| KEY_OPTION | 8 |
| KEY_SELECT | 16 |
| KEY_START | 17 |
| KEY_A | 15 |
| KEY_B | 5 |

---

## Required Libraries (Arduino Library Manager)

Install all via **Sketch → Include Library → Manage Libraries**:

| Library | Purpose | Install Name |
|---------|---------|--------------|
| **TFT_eSPI** | ST7789 display driver | `TFT_eSPI` |
| **ArduinoJson** | JSON parsing | `ArduinoJson` |
| **RadioLib** | LoRa (optional) | `RadioLib` |
| **EloquentLua** | Lua scripting (optional) | `EloquentLua` |
| **ESP8266Audio** | Music player (optional) | `ESP8266Audio` |

---

## Setup Instructions

### Step 1 — Copy User_Setup.h
```
Copy:   LegacyOS/User_Setup.h
   To:  Arduino/libraries/TFT_eSPI/User_Setup.h
```
This configures TFT_eSPI for the ST7789 with correct pins.

### Step 2 — Board Configuration
In Arduino IDE:
- **Board**: `ESP32S3 Dev Module`
- **Flash Size**: `16MB (128Mb)`
- **Flash Mode**: `QIO 80MHz`
- **PSRAM**: `OPI PSRAM`
- **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)`
- **USB CDC On Boot**: `Enabled`
- **Upload Speed**: `921600`

### Step 3 — Open Project
Open `LegacyOS.ino` in Arduino IDE.

### Step 4 — Upload
Press `Ctrl+U` to compile and upload.

---

## Features

### Home Screen
- Large clock widget (syncs via NTP when WiFi connected)
- WiFi & BLE status cards
- Storage status with SD card usage bar
- Bottom dock with quick-launch apps
- Swipe UP → Notification Panel
- Swipe DOWN → App Drawer
- MENU → Quick Settings

### App Drawer
- 4×3 icon grid with scrollable pages
- Launch any app with A button
- App categories: System, Tools, Games, Scripts

### Built-in Apps

| App | Description |
|-----|-------------|
| ⚙ Settings | Display, WiFi, BLE, System Info, Reboot |
| 📁 Files | SD card file browser with navigation |
| W WiFi | Scan, connect to WiFi networks |
| B Bluetooth | BLE UART advertising and scanning |
| = Calculator | Full calculator with D-pad navigation |
| > Terminal | FreeRTOS serial terminal + commands |
| R RetroEmu | ROM browser for GB/GBC/NES/GBA |
| L Lua IDE | Browse and run .lua scripts from SD |
| J JS Run | Browse and run .js scripts from SD |
| ~ LoRa | LoRa radio send/receive terminal |
| M Monitor | 4-page system monitor (CPU/Mem/Net/Tasks) |
| E Editor | Text editor with virtual D-pad keyboard |
| ♪ Music | SD card music player (MP3/WAV) |
| 🐍 Snake | Built-in classic Snake game |

### Navigation Controls

| Button | Function |
|--------|---------|
| D-Pad | Navigate menus and apps |
| A | Select / Confirm |
| B | Back / Cancel |
| START | App Drawer |
| SELECT | Secondary action (scan, keyboard, etc.) |
| MENU | Quick Settings / App options |
| OPTION | Additional options (delete, clear, shift) |
| MENU (hold) | Exit any running app → Home |

---

## SD Card Structure

```
/
├── scripts/
│   ├── lua/          ← .lua scripts
│   │   └── hello.lua
│   └── js/           ← .js scripts
│       └── hello.js
├── roms/
│   ├── gb/           ← Game Boy ROMs (.gb, .gbc)
│   ├── nes/          ← NES ROMs (.nes)
│   └── gba/          ← GBA ROMs (.gba)
├── music/            ← Audio files (.mp3, .wav, .flac)
├── saves/            ← Text editor saves
├── apps/             ← Reserved for future app packages
├── logs/
│   └── system.log
└── pictures/
```

---

## Lua Scripting API

Place `.lua` files in `/scripts/lua/` on the SD card.

```lua
-- Available LegacyOS Lua API:
print("Hello!")          -- Output to Lua IDE console
os_print("msg")          -- Same as print
os_delay(500)            -- Delay in milliseconds
-- More APIs added as EloquentLua integration grows
```

## JavaScript API

Place `.js` files in `/scripts/js/` on the SD card.

```javascript
// Available LegacyOS JS API:
print("Hello!");          // Output to JS console
console.log("msg");       // Same as print
os.uptime();              // Returns uptime in seconds
```

---

## Retro Emulation

For full emulation (currently stubs — display architecture is ready):

- **Game Boy**: Install `gnuboy-esp32` port, uncomment in `RetroEmu.h`
- **NES**: Install `nofrendo-esp32` port
- **GBA**: Limited support — requires external port

ROMs are loaded into 4MB PSRAM buffer from SD card.

---

## LoRa Radio

1. Connect an SX1276 or SX1278 module
2. Set pin numbers in `include/LoRaManager.h`
3. Install `RadioLib` from Library Manager
4. Uncomment the RadioLib code block in `LoRaManager::init()`

Default: **433 MHz**, SF9, BW125, 14dBm

---

## License

Open source for personal use and education.
LEGACY-32 CLASSIC E524546 hardware © respective owners.

---

*LegacyOS v1.0 — Built for the LEGACY-32 CLASSIC E524546*
*ESP32-S3-WROOM-1 (N16R8) — 240MHz | 16MB Flash | 8MB PSRAM*
