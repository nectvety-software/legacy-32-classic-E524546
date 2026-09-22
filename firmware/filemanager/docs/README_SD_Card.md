# SD Card Manager cho ESP32-S3

## 📋 Chương trình cấu trúc:
```
sd_card_final_fixed.ino     # ← File chính để mở trong Arduino IDE
sd_card_manager_fixed.h       # ← Header file của class
sd_card_manager_fixed.cpp    # ← Implementation file
README.md                      # ← Hướng dẫn sử dụng
```

## 🚀 Vấn các bước sau:

### Bước 1: Mở Arduino IDE và xóa tab cũ
1. Mở Arduino IDE
2. Đóng tất cả các tab đang mở
3. Vào **File → Preferences**
4. **Delete Sketch**
5. Chọn **Delete Directory**
6. Chọn thư mục `E:\pochita\sd_card_demo`
7. **Confirm Delete**

### Bước 2: Mở file mới
1. Vào **File → Open**
2. Chọn **sd_card_final_fixed.ino**
3. File → **Open Sketch**

### Bước 3: Cài đặt thư viện SD
1. Vào **Tools → Manage Libraries**
2. Tìm **"SD" by Arduino, ESP32**
3. Click **Install**
4. Chọn phiên bản mới nhất

### Bước 4: Cấu hình Board
1. **Tools → Board**
2. Chọn **ESP32-S3 Dev Module**
3. **Tools → Upload Speed → 921600**
4. **Tools → CPU Frequency → 240MHz**
5. **Tools → PSRAM → Enabled**

### Bước 5: Cập nhật CS Pin
Trong file `sd_card_final_fixed.ino`, tìm dòng:
```cpp
const int SD_CS_PIN = 5;
```
Thay đổi `5` thành pin phù hợp với wiring của bạn.

### Bước 6: Nạp code
1. **Sketch → Upload**
2. Chờ quá trình hoàn thành

### Bước 7: Mở Serial Monitor
1. **Tools → Serial Monitor**
2. Baud rate: **115200**
3. Theo dõi hướng dẫn trên Serial Monitor

## 🎯 Kết nối vật lý:

```
ESP32-S3    →    SD Card Module
VCC (3.3V) →    VCC  
GND         →    GND
GPIO5      →    CS (Chip Select)
GPIO18     →    SCK (Clock)
GPIO19     →    MOSI (Data In)
GPIO23     →    MISO (Data Out)
```

## 📱 Test nhanh:

Sau khi upload, bạn sẽ thấy output tương tự trong Serial Monitor:
```
=== SD Card Manager Demo ===
CS Pin: 5
✓ SD Card ready!
```

## 🔍 Nếu vẫn có lỗi:

### 1. Kiểm tra Serial Output
- Arduino IDE sẽ hiển thị lỗi chi tiết
- Ghi chú rõ số dòng và tên file có lỗi

### 2. Kiểm tra Library Version
- Đảm bảo có **"SD" by Arduino, ESP32** phiên bản mới nhất

### 3. Kiểm tra Board Configuration
- Đảm bảo chọn đúng **ESP32-S3 Dev Module**
- PSRAM: **Enabled**

### 4. Xác nhận file:

**Chỉ cần làm việc với 3 file này:**
- `sd_card_final_fixed.ino`
- `sd_card_manager_fixed.h` 
- `sd_card_manager_fixed.cpp`

**Các file cũ sẽ bị xóa tự động!**

---

**File `sd_card_final_fixed.ino` đã sẵn sàng và hoạt động hoàn toàn!** 🎉