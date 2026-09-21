# Pochitta OS — legacy-32-classic E524546

Firmware PlatformIO/VS Code cho bo mạch:

- MCU: ESP32-S3-WROOM-1 N16R8
- Flash: 16 MB
- PSRAM: 8 MB OPI
- Màn hình: TFT ST7789 2.0 inch, 240×320, SPI
- Phím: 10 nút active-low dùng `INPUT_PULLUP`
- Thẻ nhớ: SD_MMC 1-bit

## Sơ đồ chân

### ST7789

| Tín hiệu | GPIO |
|---|---:|
| LEDK / Backlight | 39 |
| D/C | 47 |
| CS | 14 |
| SCL / SCLK | 48 |
| SDA / MOSI | 12 |
| RESET | 3 |

### Nút bấm

| Nút | GPIO |
|---|---:|
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

Mặc định: `A/SELECT/START/RIGHT` để mở hoặc xác nhận; `B/LEFT` để quay lại; `MENU` về màn hình chính.

### SD card — SD_MMC 1-bit

| Tín hiệu | GPIO |
|---|---:|
| DAT3 / CD | 10 |
| CMD | 11 |
| CLK | 13 |
| DAT0 | 9 |

Firmware chạy SD_MMC ở chế độ 1-bit nên sử dụng CLK, CMD và DAT0. GPIO10 được giữ lại dưới tên `SD_MMC_D3_CD_PIN` để tham chiếu phần cứng.

## Biên dịch và nạp

```powershell
pio run
pio run -t upload
pio device monitor
```

Nạp dữ liệu LittleFS:

```powershell
pio run -t uploadfs
```

Nếu cổng USB không tự xuất hiện, giữ BOOT, nhấn RESET, thả RESET rồi thả BOOT trước khi upload.

## File cấu hình chính

- `include/board_config.h`: toàn bộ chân phần cứng và tùy chọn thiết bị.
- `platformio.ini`: cấu hình ESP32-S3 N16R8 và TFT_eSPI.

## Ghi chú điện

- Các nút được giả định nối GPIO xuống GND khi nhấn.
- Không đưa tín hiệu 5 V trực tiếp vào GPIO ESP32-S3.
- Khe SD cần mức logic 3.3 V và pull-up phù hợp trên CMD/DAT0.
