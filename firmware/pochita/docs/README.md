# SD Card Manager cho ESP32-S3

## Giới thiệu
SD Card Manager là một thư viện quản lý thẻ SD cho ESP32-S3, hỗ trợ các thao tác cơ bản và nâng cao như đọc, ghi, xóa file, quản lý thư mục, benchmark tốc độ, và nhiều tính năng khác.

## Tính năng

### ✅ Core Features
- **Tự động phát hiện** SD Card với nhiều tần số SPI
- **Hỗ trợ nhiều loại thẻ** (SD, SDHC, MMC)
- **Quản lý file và thư mục** (tạo, xóa, di chuyển, sao chép)
- **Benchmark tốc độ** đọc/ghi
- **Quản lý cache** thông minh
- **Kiểm tra tính hợp** file tên

### ✅ Các hàm chính
- `begin()` - Khởi tạo SD Card
- `writeFile(path, content)` - Ghi file
- `readFile(path)` - Đọc file  
- `appendFile(path, content)` - Nối thêm file
- `removeFile(path)` - Xóa file
- `renameFile(path1, path2)` - Đổi tên file
- `copyFile(src, dest)` - Sao chép file
- `fileExists(path)` - Kiểm tra file tồn tại
- `createDir(path)` - Tạo thư mục
- `listDir(path)` - Liệt kê thư mục
- `listFiles(path)` - Lấy danh sách file
- `testCard()` - Kiểm tra chức năng
- `benchmark()` - Đo benchmark
- `formatCard()` - Format thẻ SD

## 📁 Cấu trứng

### Kết nối vật lý (Wiring)
```
ESP32-S3    →    SD Card Module
VCC (3.3V) →    VCC
GND         →    GND
GPIO5      →    CS (Chip Select)
GPIO18     →    SCK (Clock)
GPIO19     →    MISO (Data In)
GPIO23     →    MISO (Data Out)
```

### Các pin CS có thể dùng
- GPIO5 (mặc định)
- GPIO4, 2, 15, 13, 21, 22

## 📱 File Structure
```
sd_card_manager_fixed.h    - Header file
sd_card_manager_fixed.cpp  - Implementation file  
sd_card_final_fixed.ino    - Main Arduino file
```

## 🛠️ Cài đặt trong Arduino IDE

### 1. Cài đặt thư viện
- Vào **Tools → Manage Libraries**
- Tìm và cài đặt **"SD" by Arduino, ESP32**

### 2. Cấu hình board
- Board: **ESP32-S3 Dev Module**
- Upload Speed: **921600**
- CPU Frequency: **240MHz**
- PSRAM: **Enabled**

### 3. Mở file chính
- Mở file `sd_card_final_fixed.ino` trong Arduino IDE
- Cập nhật thông tin WiFi và CS pin nếu cần

## 🚀 Sử dụng

### Commands qua Serial Monitor (115200 baud)
```
help                    - Hiển thị commands
info                    - Thông tin SD card  
test                    - Test chức năng
benchmark               - Test tốc độ
list                    - Liệt kê file/thư mục
write <path>            - Ghi file mới
read <path>             - Đọc nội dung file
delete <path>           - Xóa file
mkdir <path>            - Tạo thư mục
rmdir <path>            - Xóa thư mục
copy <src> <dest>       - Sao chép file
clear                   - Clear màn hình
quit                    - Thoát
```

### Ví dụ sử dụng
```
> help
=== SD Card Manager Commands ===
help                    - Show this help
info                    - Show SD card information
test                    - Test SD card functionality
benchmark               - Run read/write benchmark
list                    - List all files and directories
write <path>            - Write test file
read <path>             - Read file content
delete <path>           - Delete file
mkdir <path>            - Create directory
rmdir <path>            - Remove directory
clear                   - Clear screen
quit                    - Exit program
=====================================

> info
=== SD Card Information ===
Card Size: 15.9 GB
Used Space: 1.2 MB (7.5%)
Free Space: 14.7 GB
Card Type: SDHC
==========================

> write /test.txt
Writing file: /test.txt
✓ Written 42 bytes

> read /test.txt
Reading file: /test.txt
✓ Read 42 bytes
File content:
Test content at 12345

> list
=== Directory Listing ===
Path: /
  [DIR] System Volume Information
  [FILE] test.txt - 42 bytes
========================
```

## 🔍 Khắc phục sự cố

### 1. SD Card không được phát hiện
```
✗ SD Card initialization failed
SPI Configuration:
  MISO: GPIO23
  MOSI: GPIO19  
  SCK: GPIO18
  CS: GPIO5
  VCC: 3.3V
  GND: GND

Troubleshooting:
  1. Check wiring connections
  2. Try different CS pin (4, 5, 2, 15, 13, 21, 22)
  3. Format SD Card as FAT32
  4. Try different SD Card
  5. Check for short circuits
```

### 2. Lỗi khi compile
- Đảm bảo đã cài đặt thư viện SD
- Kiểm tra phiên bản ESP32 core
- Kiểm tra cấu hình board

### 3. Lỗi khi truy cập file
- Kiểm tra SD Card đã được format (FAT32)
- Kiểm tra dung lượng trống
- Kiểm tra tên file hợp lệ (không có ký tự đặc biệt)

## ⚡ Tối ưu cho ESP32-S3

- Sử dụng PSRAM để tăng performance
- Giới hạn các thao tác đọc/ghi
- Dọn xóa các file không cần thiết
- Sử dụng buffer lớn hơn cho các file lớn

## 📊 Demo Output
```
=== SD Card Manager Demo ===
CS Pin: 5
Make sure SD Card is connected to this pin

✓ SD Card ready!

=== Running Demo Operations ===

=== SD Card Information ===
Card Size: 15.9 GB
Used Space: 1.0 MB (6.3%)
Free Space: 14.9 GB
Card Type: SDHC
==========================

1. Testing SD Card...
✓ All tests passed

2. Creating directory...
✓ Directory created

3. Writing file...
✓ Written 67 bytes

4. Reading file...
✓ Read 67 bytes
--- File Content ---
Hello ESP32-S3!
This is a test file.
SD Card is working!
--------------------

5. Appending to file...
✓ Appended 16 bytes

6. Listing directory...
=== Directory Listing ===
Path: /demo
  [FILE] test.txt - 83 bytes
========================

7. Running benchmark...
=== SD Card Benchmark ===
Write Speed: 8192.0 bytes/sec
Read Speed: 16384.0 bytes/sec
========================

8. Copying file...
✓ File copied

9. Listing all files...
Found 2 files:
  test.txt
  test_copy.txt

=== Demo Complete ===
SD Card is working properly!
```

## 📄 Yêu cầu hệ thống

- ESP32-S3 với PSRAM (khuyến nghị 8MB+)
- Thẻ SD (khuyến nghị Class 10, tối đa 32GB)
- Màn hình cảm ứng (tùy chọn)
- Dây cắm (nếu có thể)
- Nguồn điện ổn định (3.3V-5V)

## 📖 License
MIT License - Xem file LICENSE để biết thêm chi tiết

---

**Lưu ý:** Đây là phiên bản đã sửa lỗi. Chỉ cần mở file `sd_card_final_fixed.ino` trong Arduino IDE và nạp code!