# Modbox Icon Display Fix
## Vấn đề: Các icon không hiển thị

### Nguyên nhân:
- Các mảng icon tĩnh (`epd_bitmap_wifi`, `epd_bitmap_ble`, v.v.) được khởi tạo rỗng `{}`
- Hệ thống tải icon từ SPIFFS nhưng UI sử dụng các mảng tĩnh rỗng

### Giải pháp đã áp dụng:

1. **Thay đổi khai báo icon arrays** từ `const uint16_t[] = {}` thành `uint16_t[]` (có thể ghi)
2. **Thêm hàm `populateStaticArrays()`** để sao chép dữ liệu đã tải vào các mảng tĩnh
3. **Sửa đường dẫn SPIFFS** từ `/assets/epd_bitmap_.bin` thành `/epd_bitmap_.bin`
4. **Thêm app test icon** để kiểm tra hiển thị

### Cách khắc phục:

#### Bước 1: Upload file icon lên SPIFFS
1. Mở sketch `SPIFFS_Upload_Icons.ino`
2. Thay thế mảng `iconData[]` bằng dữ liệu icon thực tế của bạn
3. Upload sketch lên ESP32
4. Chờ thông báo "Upload complete"
5. Chuyển về sketch `modbox.ino` chính

#### Bước 2: Kiểm tra icon
1. Upload sketch `modbox.ino`
2. Chọn app "IconTest" từ menu chính
3. Kiểm tra xem các icon có hiển thị không
4. Nếu không hiển thị, kiểm tra Serial Monitor để xem lỗi

#### Bước 3: Tạo dữ liệu icon
Nếu bạn chưa có file `epd_bitmap_.bin`, bạn cần:
1. Tạo các icon 48x48 pixel với định dạng RGB565
2. Ghép chúng thành một file binary theo thứ tự:
   - WiFi icon
   - SD icon
   - Paint icon
   - BLE icon
   - Terminal icon
   - Web icon
   - Script icon
   - Retro icon
   - Setup icon

### Kiểm tra Serial Monitor:
- "SPIFFS initialized" - SPIFFS hoạt động
- "Loaded X icons and populated static arrays" - Icon đã tải thành công
- "Icon file not found" - File icon không tồn tại trong SPIFFS

### App test:
- Chọn "IconTest" từ menu để kiểm tra hiển thị icon
- Nhấn MENU để thoát khỏi app test

### Lưu ý:
- Đảm bảo file icon có đúng kích thước: 9 icon × 48×48×2 bytes = 41,472 bytes
- Mỗi icon là 48×48 pixel với định dạng 16-bit RGB565
- Thứ tự icon phải khớp với thứ tự trong code