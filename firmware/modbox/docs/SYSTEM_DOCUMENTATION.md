# ModBox System Documentation

## Mục lục
1. [Tổng quan hệ thống](#1-tổ-quan-hệ-thống)
2. [Màn hình chính](#2-màn-hình-chính)
3. [Ứng dụng WiFi Manager](#3-wifi-manager)
4. [Ứng dụng Setup](#4-setup)
5. [Ứng dụng CMD Terminal](#5-cmd-terminal)
6. [Điều khiển nút bấm](#6-điều-khiển-nút-bấm)

---

## 1. Tổng quan hệ thống

ModBox là thiết bị ESP32-S3 với màn hình TFT 240x320. Hệ thống hỗ trợ nhiều ứng dụng và tính năng:

### Thông số kỹ thuật
- **CPU**: ESP32-S3 Dual Core 240MHz
- **RAM**: 512KB Internal
- **Flash**: 16MB
- **Màn hình**: TFT 240x320 pixels
- **Kết nối**: WiFi, BLE

### Danh sách ứng dụng
| Icon | Tên | Mô tả |
|------|------|--------|
| WiFi | WiFi Manager | Quản lý kết nối WiFi |
| SD | SD Card | Quản lý thẻ nhớ |
| Paint | Paint | Ứng dụng vẽ |
| BLE | BLE Manager | Quản lý Bluetooth |
| CMD | CMD Terminal | Giao diện dòng lệnh |
| WEB | Web Server | Máy chủ web |
| Script | Script | Chạy script tự động |
| Retro | Retro Games | Trò chơi cổ điển |
| Setup | Settings | Cài đặt hệ thống |

---

## 2. Màn hình chính

### Layout
```
┌──────────────────────────┐
│ ModBox              [C/S]│ ← Thanh trạng thái (BLE)
├──────────────────────────┤
│                          │
│  [WiFi] [SD] [Paint]    │
│                          │
│  [BLE]  [CMD] [WEB]     │
│                          │
│  [Script][Retro][Setup] │
│                          │
└──────────────────────────┘
```

### Thanh trạng thái
- **C**: Client BLE kết nối (xanh)
- **S**: Server BLE hoạt động (xanh)
- **Đỏ**: Chưa kết nối

### Di chuyển
| Nút | Hành động |
|-----|-----------|
| UP | Di chuyển lên 3 ô |
| DOWN | Di chuyển xuống 3 ô |
| LEFT | Di chuyển sang trái |
| RIGHT | Di chuyển sang phải |
| A/SELECT | Mở ứng dụng |

---

## 3. WiFi Manager

### Menu chính
```
1. Quet WiFi      - Quét mạng xung quanh
2. Mang da luu    - Danh sách mạng đã lưu
3. Thuoc tinh     - Thông tin mạng
4. AP Mode        - Chế độ Access Point
5. Thoat          - Quay về menu chính
```

### 3.1 Quét WiFi
Hiển thị danh sách mạng WiFi xung quanh:
- **SSID**: Tên mạng
- **RSSI**: Cường độ tín hiệu (dBm)
- **[*]**: Mạng có mật khẩu
- **[OPEN]**: Mạng mở

**Điều khiển:**
| Nút | Hành động |
|-----|-----------|
| UP/DOWN | Chọn mạng |
| A | Quét lại |
| SELECT | Nhập mật khẩu |

### 3.2 Mạng đã lưu
Danh sách các mạng đã được kết nối trước đó.

**Điều khiển:**
| Nút | Hành động |
|-----|-----------|
| UP/DOWN | Chọn mạng |
| A | Kết nối trực tiếp |
| MENU | Mở menu tùy chọn |

**Menu tùy chọn:**
```
1. Ket noi     - Kết nối đến mạng
2. Xoa mang    - Xóa mạng khỏi danh sách
3. Ngat ket noi - Ngắt kết nối hiện tại
```

### 3.3 Thuộc tính
Hiển thị thông tin mạng WiFi:
- MAC Address
- IP Address
- Subnet Mask
- Gateway
- DNS Server
- BSSID
- Encryption

### 3.4 AP Mode
Chế độ Access Point cho phép thiết bị khác kết nối đến ModBox:
- **SSID mặc định**: ESP32-AP
- **IP mặc định**: 192.168.4.1

**Điều khiển:**
| Nút | Hành động |
|-----|-----------|
| A | Bật/Tắt AP Mode |

### 3.5 Virtual Keyboard
Bàn phím ảo để nhập mật khẩu WiFi:

```
┌──────────────────────────┐
│ Nhap Mat Khau            │
│ SSID: MyNetwork          │
├──────────────────────────┤
│ [1][2][3][4][5][6][7]...│
│ [Q][W][E][R][T][Y][U]...│
│ [A][S][D][F][G][H][J]...│
│ [Z][X][C][V][B][N][M]...│
├──────────────────────────┤
│ [DEL][SPACE     ][OK]   │
└──────────────────────────┘
```

**Điều khiển:**
| Nút | Hành động |
|-----|-----------|
| UP/DOWN/LEFT/RIGHT | Di chuyển con trỏ |
| A | Xác nhận ký tự |
| B | Quay lại |

**Nút đặc biệt:**
- **DEL**: Xóa ký tự cuối
- **SPACE**: Thêm khoảng trắng
- **OK**: Xác nhận và kết nối

---

## 4. Setup

### Menu cài đặt
```
1. Brightness    - Độ sáng màn hình
2. Dim Time     - Thời gian tắt dim
3. Orientation  - Hướng màn hình
4. UI Color     - Màu giao diện
5. UI Theme     - Chủ đề giao diện
6. WiFi         - Bật/Tắt WiFi
7. InstaBoot    - Khởi động nhanh
8. Beep Sound   - Âm thanh bíp
9. Startup App  - Ứng dụng khởi động
```

### 4.1 Brightness (Độ sáng)
Các mức độ sáng: `1%`, `25%`, `50%`, `75%`, `100%`

### 4.2 Dim Time (Thời gian tắt dim)
Thời gian tắt dim tự động: `10s`, `20s`, `30s`, `60s`, `Disabled`

### 4.3 Orientation (Hướng màn hình)
Các hướng: `Portrait 0`, `Landscape 90`, `Portrait 180`, `Landscape 270`

### 4.4 UI Color (Màu giao diện)
Các màu: `Green`, `Matrix`, `Blue`

### 4.5 UI Theme (Chủ đề)
Các chủ đề: `Green`, `Matrix`, `Blue`

### 4.6 WiFi [ON/OFF]
Bật hoặc tắt WiFi. WiFi mặc định: **OFF**

**Lưu ý:** WiFi Manager chỉ hoạt động khi WiFi được bật trong Setup.

### 4.7 InstaBoot [ON/OFF]
Bật/tắt khởi động nhanh vào ứng dụng đã chọn.

### 4.8 Beep Sound [ON/OFF]
Bật/tắt âm thanh bíp khi nhấn nút.

### 4.9 Startup App
Ứng dụng khởi động: `SubGHz`, `LoRa Ra-01SH`, `None`

### Điều khiển
| Nút | Hành động |
|-----|-----------|
| UP/DOWN | Di chuyển con trỏ |
| LEFT/RIGHT | Thay đổi giá trị |
| B/MENU | Quay lại (tự động lưu) |

**Tính năng:** Tất cả thay đổi được **tự động lưu** vào NVS Flash khi nhấn RIGHT/LEFT.

---

## 5. CMD Terminal

Giao diện dòng lệnh với các lệnh Unix-like.

### 5.1 Giao diện
```
┌──────────────────────────┐
│ CMD Terminal             │
├──────────────────────────┤
│ ModBox CMD v1.0          │
│ Type 'help' for commands │
│ ----------------------   │
│                          │
│ modbox:/spiffs$ _        │
├──────────────────────────┤
│ UP/DOWN: History | SEL:Kb│
│ A: Enter | B: Exit       │
└──────────────────────────┘
```

### 5.2 Lệnh File và Directory

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `ls [-la]` | Liệt kê file | `ls`, `ls -l`, `ls -a` |
| `cd <dir>` | Đổi thư mục | `cd /spiffs`, `cd ..` |
| `pwd` | In thư mục hiện tại | `pwd` |
| `mkdir <dir>` | Tạo thư mục | `mkdir data` |
| `touch <file>` | Tạo file | `touch config.txt` |
| `rm <file>` | Xóa file | `rm temp.txt` |
| `rm -rf <dir>` | Xóa thư mục | `rm -rf old_data` |
| `cp <src> <dst>` | Copy file | `cp a.txt b.txt` |
| `mv <src> <dst>` | Di chuyển/đổi tên | `mv old.txt new.txt` |
| `cat <file>` | Xem nội dung file | `cat config.txt` |
| `stat <file>` | Thông tin file | `stat readme.txt` |

### 5.3 Lệnh WiFi

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `scanap` | Quét mạng WiFi | `scanap` |
| `scanap -live` | Quét trực tiếp | `scanap -live` |
| `scanap -list` | Danh sách đã lưu | `scanap -list` |
| `connect "SSID" "pass"` | Kết nối WiFi | `connect "MyWiFi" "password123"` |
| `connect` | Kết nối lại mạng cuối | `connect` |
| `disconnect` | Ngắt kết nối | `disconnect` |

### 5.4 Lệnh Network Scan

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `scanlocal` | Quét thiết bị local | `scanlocal` |
| `scanarp` | Quét ARP | `scanarp` |
| `scanports <ip>` | Quét cổng thông dụng | `scanports 192.168.1.1` |
| `scanports <ip> all` | Quét tất cả cổng | `scanports 192.168.1.1 all` |
| `scanports <ip> start-end` | Quét dải cổng | `scanports 192.168.1.1 1-1024` |
| `scanssh <ip>` | Kiểm tra SSH | `scanssh 192.168.1.1` |

### 5.5 Lệnh Sweep

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `sweep` | Quét mặc định (10s) | `sweep` |
| `sweep -w <s>` | Thời gian quét WiFi | `sweep -w 15` |
| `sweep -b <s>` | Thời gian quét BLE | `sweep -b 20` |
| `sweep -w 15 -b 20` | Tùy chỉnh cả hai | `sweep -w 15 -b 20` |
| `sweep -h` | Trợ giúp | `sweep -h` |

### 5.6 Lệnh Capture (Chế độ chụp)

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `capture -probe` | Ghi probe requests | `capture -probe` |
| `capture -deauth` | Ghi khung hủy xác thực | `capture -deauth` |
| `capture -beacon` | Ghi beacon frames | `capture -beacon` |
| `capture -raw` | Chụp raw WiFi frames | `capture -raw` |
| `capture -eapol` | Ghi WPA handshakes | `capture -eapol` |
| `capture -pwn` | Chế độ Pwnagotchi | `capture -pwn` |
| `capture -wps` | Ghi WPS traffic | `capture -wps` |
| `capture -802154` | IEEE 802.15.4 frames | `capture -802154` |
| `capture stop` | Dừng chụp | `capture stop` |
| `capture list` | Danh sách file | `capture list` |
| `capture verify` | Kiểm tra file .pcap | `capture verify` |
| `capture status` | Trạng thái chụp | `capture status` |

### 5.8 Lệnh System

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `ifconfig` | Thông tin mạng | `ifconfig` |
| `ps` | Tiến trình đang chạy | `ps` |
| `top` | Thông tin hệ thống | `top` |
| `uptime` | Thời gian hoạt động | `uptime` |
| `free` | Thông tin bộ nhớ | `free` |
| `df` | Thông tin lưu trữ | `df` |

**Lệnh `top` hiển thị:**
```
=== System Info ===
CPU: ESP32-S3 @ 240MHz
Free Heap: 123456 bytes
Flash Size: 16777216 bytes
CPU Cores: 2
SDK Version: v4.4
```

### 5.9 Lệnh Basic

| Lệnh | Mô tả | Ví dụ |
|------|-------|-------|
| `whoami` | Tên người dùng | `whoami` |
| `date` | Ngày giờ hiện tại | `date` |
| `echo <text>` | In text | `echo Hello` |
| `clear` | Xóa màn hình | `clear` |
| `help` | Hiển thị help | `help` |
| `exit` | Thoát CMD | `exit` |

### 5.10 Virtual Keyboard CMD
```
┌──────────────────────────┐
│ Enter Command            │
│ modbox:/spiffs$          │
├──────────────────────────┤
│ [ ][1][2][3][4][5][6]...│
│ [Q][W][E][R][T][Y][U]...│
│ [A][S][D][F][G][H][J]...│
│ [Z][X][C][V][B][N][M]...│
│ [0][9][8][7][6][.][/]...│
├──────────────────────────┤
│ [DEL][SPACE     ][OK][CLR]│
└──────────────────────────┘
```

**Nút đặc biệt:**
- **DEL**: Xóa ký tự
- **SPACE**: Khoảng trắng
- **OK**: Xác nhận lệnh
- **CLEAR**: Xóa toàn bộ

### 5.11 Điều khiển CMD

| Nút | Hành động |
|-----|-----------|
| UP | Lịch sử lệnh (lên) |
| DOWN | Lịch sử lệnh (xuống) |
| SELECT | Mở bàn phím ảo |
| LEFT/RIGHT | Cuộn output |
| A | Execute lệnh |
| B/MENU | Thoát CMD |

---

## 6. Điều khiển nút bấm

### Sơ đồ nút
```
    [UP]                   
[LEFT][RIGHT]      [MENU]
    [DOWN]             [SELECT]
                          
                      [START]
[OPT]                          [A]
                               
                               
                      [B]      [KEY]
```

### Chức năng nút

| Nút | Menu chính | WiFi Manager | Setup | CMD |
|-----|------------|--------------|-------|-----|
| UP | Lên 3 ô | Chọn mục trên | Chọn trên | History up |
| DOWN | Xuống 3 ô | Chọn mục dưới | Chọn dưới | History down |
| LEFT | Sang trái | - | Giảm giá trị | Cuộn trái |
| RIGHT | Sang phải | - | Tăng giá trị | Cuộn phải |
| A | Mở app | Xác nhận | Xác nhận | Enter |
| B | - | Quay lại | Quay lại | Thoát |
| MENU | - | Quay lại | Quay lại | Thoát |
| SELECT | Mở app | - | - | Keyboard |
| START | - | - | - | - |
| OPT | - | - | - | - |

### Mã nút
```cpp
#define KEY_UP     7
#define KEY_DOWN   46
#define KEY_LEFT   45
#define KEY_RIGHT  6
#define KEY_MENU   18
#define KEY_OPTION 8
#define KEY_SELECT 16
#define KEY_START  17
#define KEY_A      15
#define KEY_B      5
```

---

## Phụ lục

### Lưu trữ NVS
Hệ thống sử dụng Preferences library để lưu trữ:
- Cài đặt trong namespace `modbox`
- WiFi credentials trong namespace `wifi`

### Màu sắc
```cpp
#define COLOR_BG     0x0000    // Đen
#define COLOR_WHITE   0xFFFF   // Trắng
#define COLOR_YELLOW  0xFD20   // Vàng
#define COLOR_GREEN   0x07E0   // Xanh lá
#define COLOR_RED     0xF800   // Đỏ
#define COLOR_BLUE    0x041F   // Xanh dương
#define COLOR_GRAY    0x8410   // Xám
```

### Kích thước màn hình
```cpp
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
```

---

## Phiên bản
- **Version**: 1.3.7
- **Ngày cập nhật**: 04/04/2026
- **Platform**: ESP32-S3
