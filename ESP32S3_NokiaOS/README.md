# ESP32-S3 Nokia Classic OS

Hệ điều hành phong cách Nokia Classic cho ESP32-S3 với giao diện người dùng đơn giản, dễ sử dụng.

## Tính năng chính

- **WiFi Manager**: Kết nối, quét mạng, cấu hình AP
- **Bluetooth**: BLE và Classic Bluetooth Serial
- **Media Player**: Phát WAV, MOD, XM tracker files
- **File Manager**: Duyệt SD card, xóa, đổi tên, sao chép file
- **Web Browser**: Trình duyệt web đơn giản với HTML parser
- **GPIO Control**: Điều khiển trực tiếp các chân GPIO
- **Lua Interpreter**: Máy ảo Lua 5.1 tích hợp
- **3D Viewer**: Xem file OBJ 3D (Blender export)
- **Settings**: Cấu hình hệ thống đầy đủ
- **Apps**: Calculator, Notes, Clock, Games

## Phần cứng yêu cầu

### Board
- **ESP32-S3-WROOM-1 (N16R8)**
  - Flash: 16MB
  - PSRAM: 8MB (Octal mode)
  - CPU: Dual-core Xtensa LX7 @ 240MHz

### Màn hình TFT
- **ST7789 2.0 inch 240x320 pixels**
- Kết nối SPI

| TFT Pin | ESP32-S3 Pin |
|---------|-------------|
| VCC     | 3.3V        |
| GND     | GND         |
| LEDK    | GPIO 39     |
| D/C     | GPIO 47     |
| CS      | GPIO 14     |
| SCL     | GPIO 48     |
| SDA     | GPIO 12     |
| RESET   | GPIO 3      |

### Nút bấm (10 nút)

| Chức năng | GPIO |
|-----------|------|
| KEY_UP    | 7    |
| KEY_DOWN  | 46   |
| KEY_LEFT  | 45   |
| KEY_RIGHT | 6    |
| KEY_MENU  | 18   |
| KEY_OPTION| 8    |
| KEY_SELECT| 16   |
| KEY_START | 17   |
| KEY_A     | 15   |
| KEY_B     | 5    |

### SD Card (SDIO 4-bit mode)

| SD Pin | ESP32-S3 Pin |
|--------|-------------|
| VCC    | 3.3V        |
| GND    | GND         |
| CD/DAT3| GPIO 10     |
| CMD    | GPIO 11     |
| CLK    | GPIO 13     |
| DAT0   | GPIO 9      |

## Cài đặt

### Arduino IDE

1. Cài đặt ESP32 board package (>= 2.0.14)
2. Cài đặt thư viện:
   - TFT_eSPI (>= 2.5.43)
   - ArduinoJson (>= 6.21.0)
   - SD (built-in)

3. Copy `User_Setup.h` vào thư mục `TFT_eSPI`
4. Chọn board: **ESP32-S3 DevKitC-1**
5. Partition Scheme: **16MB (3MB APP/9.9MB FATFS)**
6. Upload code

### PlatformIO

```ini
[env:esp32-s3-nokia]
platform = espressif32 @ 6.4.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = dio
board_upload.maximum_ram_size = 524288
board_upload.maximum_size = 16777216
board_build.partitions = default_16MB.csv

build_flags = 
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCONFIG_SPIRAM_CACHE_WORKAROUND

lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    bblanchon/ArduinoJson @ ^6.21.0
```

## Sử dụng

### Điều hướng
- **MENU**: Mở menu chính / Về màn hình chính
- **UP/DOWN**: Di chuyển trong menu
- **SELECT**: Chọn mục
- **B**: Quay lại
- **OPTION**: Tùy chọn bổ sung

### Các ứng dụng

#### WiFi Manager
- Tự động quét mạng
- Kết nối với mật khẩu
- Hiển thị IP, RSSI

#### 3D Viewer
- Hỗ trợ file .obj từ Blender
- Xoay: UP/DOWN (X), LEFT/RIGHT (Y)
- Zoom: A/B

#### Lua Interpreter
- Biên dịch và chạy script Lua
- Hỗ trợ GPIO, WiFi, delay từ Lua
- Lưu script trên SD card

#### Media Player
- Phát file WAV 16-bit stereo
- Phát file tracker MOD/XM
- Visualizer hiệu ứng sóng

## Lưu ý kỹ thuật

### PSRAM
- Sử dụng 8MB PSRAM cho buffer lớn
- Cần enable trong menuconfig/board settings
- Dùng cho 3D mesh, audio buffer, web cache

### SDIO
- SD card dùng SDIO 4-bit mode (nhanh hơn SPI)
- Cần pull-up resistors (10kΩ) trên các đường tín hiệu
- Tránh dùng strapping pins (GPIO 0, 45, 46)

### TFT
- SPI tốc độ 80MHz
- Backlight PWM điều chỉnh độ sáng
- Rotation 0 (portrait) cho giao diện Nokia

## Đóng góp

Dự án mã nguồn mở, chào đón đóng góp!

## License

MIT License

## Credits

- TFT_eSPI by Bodmer
- Lua 5.1 architecture
- Nokia UI design inspiration
