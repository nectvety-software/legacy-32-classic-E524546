# LegacyOs v2.0

Hệ điều hành tùy chỉnh cho **ESP32-S3** + **ST7789 TFT**, dựa trên thiết kế CyberOS v2.0.

## Phần cứng yêu cầu

| Linh kiện | Chức năng |
|-----------|-----------|
| ESP32-S3-WROOM-1 (N16R8) | MCU chính |
| ST7789 TFT 1.54" (240x240) / 2.0" (240x320) | Màn hình hiển thị |
| microSD Card | Lưu trữ file |
| BME280 | Cảm biến nhiệt độ, độ ẩm, áp suất |
| Passive Buzzer | Âm thanh |
| 6 nút bấm (Joystick 4 hướng + OK + Back) | Điều khiển |
| Pin Li-ion + BQ24074 | Nguồn di động |

## GPIO Pin Mapping

### Màn hình ST7789 (SPI)

| Chức năng | GPIO | Ghi chú |
|-----------|------|---------|
| BL / LEDK | 39 | Đèn nền |
| D/C | 47 | Data / Command |
| CS | 14 | Chip Select |
| SCL / SCK | 48 | SPI Clock |
| SDA / MOSI | 12 | SPI MOSI |
| RESET | 3 | Reset màn hình |

### Nút bấm

| Chức năng | GPIO | Ghi chú |
|-----------|------|---------|
| UP | 40 | Joystick D |
| DOWN | 5 | Joystick B |
| LEFT | 4 | Joystick A |
| RIGHT | 45 | Joystick C |
| OK (A) | 37 | KEY2 |
| BACK (B) | 36 | KEY1 |

### microSD (SPI)

| Chức năng | GPIO | Ghi chú |
|-----------|------|---------|
| CS | 10 | Chip Select |
| MOSI | 11 | SPI MOSI |
| SCLK | 13 | SPI Clock |
| MISO | 9 | SPI MISO |
| DET | 38 | Card Detect |

### Cảm biến BME280 (I²C)

| Chức năng | GPIO | Ghi chú |
|-----------|------|---------|
| SDA | 18 | I²C Data |
| SCL | 6 | I²C Clock |

### Khác

| Chức năng | GPIO | Ghi chú |
|-----------|------|---------|
| Buzzer | 41 | PWM |
| PIN_BAT | 1 | ADC đo pin |

## Màu sắc giao diện

| Tên | Mã RGB565 | Mô tả |
|-----|-----------|-------|
| SLATE_BLACK | 0x0000 | Nền chính |
| COSMIC_BLUE | 0x1082 | Header, viền |
| ACTIVE_AMBER | 0xFDA0 | Highlight mục chọn |
| ELECTRIC_GRN | 0x07E0 | Giá trị tối ưu |
| BRIGHT_CYAN | 0x07FF | Thông tin phụ, đồ thị |
| BORDER_GREY | 0x5AEB | Đường phân chia |
| CRITICAL_RED | 0xF800 | Cảnh báo |

## Cấu trúc giao diện

```
+-----------------------------------+
| LEGACY OS CORE            Bat 98% |  Header (24px)
+-----------------------------------+
| [BUS]     [WFI]      [DIR]        |
|  DEVS     Wi-Fi      Files        |
|                                    |
| [SNS]     [TOL]      [SET]        |  Grid 3x3
| Weather   Tools      Settings      |
|                                    |
| [CPU]     [LOG]      [OSI]        |
| CPU Info  Sys Log    About         |
+-----------------------------------+
| JOYPAD Navigation    OK: SELECT   |  Footer (22px)
+-----------------------------------+
```

## Ứng dụng

| # | Icon | Tên | Chức năng |
|---|------|-----|-----------|
| 0 | [BUS] | DEVS | GPIO Debugger – giám sát & toggle chân |
| 1 | [WFI] | Wi-Fi | Quét mạng, kết nối WiFi |
| 2 | [DIR] | Files | Duyệt thẻ SD |
| 3 | [SNS] | Weather | Nhiệt độ, độ ẩm, áp suất + đồ thị |
| 4 | [TOL] | Tools | Công cụ nhanh (LCD test, buzzer) |
| 5 | [SET] | Settings | Cài đặt độ sáng, sleep, xoay màn |
| 6 | [CPU] | CPU Info | Thông tin hệ thống |
| 7 | [LOG] | Sys Log | Nhật ký khởi động |
| 8 | [OSI] | About | Giới thiệu |

## Cài đặt

### Yêu cầu

- Arduino IDE
- ESP32 Arduino Core 3.x
- Thư viện TFT_eSPI
- Thư viện SD (có sẵn trong ESP32 Core)

### Cấu hình TFT_eSPI

Sao chép nội dung file `tft_config.h` vào thư mục `<Arduino>/libraries/TFT_eSPI/User_Setup.h`, hoặc giữ nguyên `tft_config.h` trong thư mục dự án (được include tự động).

### Nạp chương trình

1. Mở `legacyOs.ino` trong Arduino IDE
2. Chọn board **ESP32S3 Dev Module**
3. Chọn cổng USB
4. Nhấn **Upload**

## Điều khiển

| Phím | Chức năng |
|------|-----------|
| UP / DOWN / LEFT / RIGHT | Di chuyển trong menu |
| OK (A) | Chọn / Xác nhận |
| BACK (B) | Quay lại / Thoát |
