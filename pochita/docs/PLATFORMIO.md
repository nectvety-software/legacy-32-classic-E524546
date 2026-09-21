# PochitaOS tren PlatformIO

## Mo va build

1. Mo thu muc goc `pochita` bang VS Code + PlatformIO IDE.
2. Chon environment `pochita_esp32s3`.
3. Build, sau do Upload. Serial Monitor su dung 115200 baud.

Dong lenh tuong duong:

```bash
pio run
pio run -t upload
pio device monitor
```

PlatformIO tu dong tai cac thu vien LovyanGFX, TJpg_Decoder va PNGdec duoc khoa trong `platformio.ini`.

## Cau hinh phan cung

Pin phan cung nam trong `src/component/BoardPins.h`. Driver ST7789 dung LovyanGFX va duoc cau hinh trong `src/component/Display.h`:

| Tin hieu | GPIO |
|---|---:|
| TFT MOSI/SDA | 12 |
| TFT SCLK/SCL | 48 |
| TFT CS | 14 |
| TFT DC | 47 |
| TFT RST | 3 |
| TFT BL | 39 |
| SD CS | 10 |
| SD MOSI | 11 |
| SD SCLK | 13 |
| SD MISO | 9 |

Man hinh mac dinh la ST7789 240x320. Neu board revision cua ban dung chan hoac driver khac, chi sua cac `build_flags` TFT trong `platformio.ini`.

## Bo nho

Environment mac dinh dung ESP32-S3-WROOM-1 N16R8: 16 MB Flash va 8 MB OPI PSRAM (`qio_opi`) voi partition `huge_app.csv`.
