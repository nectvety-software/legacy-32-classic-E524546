# VQEAF OS v2.5.0 — Feature-phone render core

**Phạm vi:** cải thiện độ phản hồi của giao diện Home/Menu/list và bảo toàn chất lượng Pixel Art RGB565 trên LCD ST7789 240×320. Dựa trên nguồn v2.4.4 có công cụ Serial Reset Logger. **Đây không phải thay đổi định dạng `.qeapp`, theme `.vqeaf`, GPU hoặc trình duyệt JavaScript.**

## Kiến trúc

| Hạng mục | Thay đổi | Giới hạn |
|---|---|---|
| Icon hệ thống 24/36px | Giải mã dữ liệu RLE/palette vào 1 dòng 36 RGB565 (72 B stack), `pushImage()` từng dòng; một giao dịch SPI/icon | Chỉ áp dụng lên vùng nền **đồng màu**, không áp dụng lên ảnh nền đa sắc/overlay cần alpha |
| Menu/Home/list | Ba tuyến vẽ icon hệ thống gọi `drawOpaque`; các chế độ vẽ truyền thống và icon tự thiết kế vẫn giữ `draw()` | Điều hướng menu từ v2.4.x đã có dirty cell, không phát minh lại compositor toàn màn hình |
| Màu RGB565 | `setSwapBytes(true)` trước `pushImage`, khôi phục trạng thái cũ sau giao dịch | Cần ảnh chụp LCD thật xác minh cách trình điều khiển phiên bản đã ghim hoạt động |
| Clock | `time(nullptr)` + `localtime_r()` thay `getLocalTime()` chờ thời gian | Vẫn hiển thị `--:--` nếu chưa đồng bộ NTP, không tạo giờ ảo |
| Mở màn hình | Interstitial khoảng 32ms thay 170ms | Công việc SD/WiFi, kiểm tra chữ ký vẫn có thể đồng bộ gây chậm |
| Gallery | `yield()` mỗi 16 MCU JPEG, PNG scanline hoặc BMP dòng kết xuất | Tránh bỏ đói scheduler/watchdog; không biến decode thành bất đồng bộ |
| Profiler | Tích lũy thời gian **toàn bộ `loop()`**, báo cáo 5 giây một lần nếu bật env chẩn đoán | Đây là loop latency, **không phải FPS** và bản thân Serial tạo overhead |

Không thêm framebuffer 240×320 (150 KiB), không thay GPIO, không thay tốc độ SPI 40 MHz hay xung CPU 240 MHz, không thay định dạng icon 24/36. Bảo toàn mã cài ứng dụng, browser, theme, xử lý phím và công cụ serial chẩn đoán đã có.

## Bài đo trên PC

`python tools/verify_v250.py` (cần Python 3 và g++). Mỗi phiên ghi `build_reports/v250/report.json`, `report.md` và log riêng.

Đối chiếu cặp renderer *trong cùng chương trình thực*: 12 icon × 2 kích thước × 6 nền = **144 tình huống**, **134.784 pixel RGB565**. So sánh toàn màn hình sau draw. Bộ mock đếm **30.846** đường ngang màu của renderer cũ so với **4.320** lệnh đẩy scanline của renderer opaque mới trong tổng các tình huống; mỗi icon mới có 1 startWrite/endWrite, giữ nguyên trạng thái swap byte. Hai đại lượng khác nhau về bytes truyền, không suy diễn độ tăng FPS từ tỷ lệ này.

## Biên dịch và thử nghiệm ESP32-S3

Trước tiên sao lưu firmware và cài đặt hiện có; không nạp chồng hai môi trường demo signature khác nhau nếu muốn giữ app đã ký.

```powershell
cd VQEAF-OS
py -3 tools\verify_v250.py
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

**A/B màu sắc**: so sánh `vqeaf_os` với `vqeaf_render_compat` (vẫn dùng trình vẽ span cũ). Sau khi đổi hình ảnh, chụp ảnh LCD Home, Menu (đặc biệt WiFi xanh, nhạc hồng, đỏ/cam, vùng rỗng) và đánh dấu nếu thấy hoán đổi đỏ/xanh, vệt ảnh hoặc màu sai ở cả 2 chế độ. Chỉ đánh giá hiệu năng khi màu tương đồng.

**Đo hiệu năng phần cứng**: build `pio run -e vqeaf_perf_diag -t upload`, monitor Serial 115200. Lần lượt di chuyển con trỏ Menu, kéo danh sách ứng dụng, vào/ra Gallery, mở browser, chơi Pixel Snake. Mỗi 5 giây hệ thống xuất:

```text
[VQEAF][PERF] samples=N avg_loop_us=N max_loop_us=N over16=N over33=N over100=N heap8=N largest8=N psram=N
```

Đây là thời gian cả vòng `loop()` tính bằng µs (gồm các lệnh `delay()` và I/O), số chu kỳ vượt 16/33/100 ms và bộ nhớ **thực tế trên máy**. So sánh **cùng tác vụ, cùng thẻ nhớ, nguồn điện và cùng nhiệt độ**, ghi log của cả bản chuẩn và chẩn đoán. Không dùng một giá trị trung bình ngắn để tuyên bố FPS toàn hệ thống. Nếu khởi động lại lúc cài `.qeapp`, dùng logger Serial Reset từ gói đi kèm và xem mã reset trong log.

**Rủi ro vẫn còn**: chứng thực HTTPS, thẻ SD chất lượng thấp, mở ảnh nhiều megapixel, giải mã hình, chữ ký ECDSA, bộ cài rollback và thiếu nguồn có thể gây giật hoặc reset. Bản này không loại bỏ các nguyên nhân vật lý đó; chỉ có dữ liệu khi chạy thiết bị mới xác định được.
