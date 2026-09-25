# Báo cáo kiểm thử VQEAF OS v2.4.2 — Ứng dụng, chủ đề, trình duyệt

**Loại kiểm thử:** GNU C++ host, dữ liệu package/theme gốc của dự án, microSD và HTTP giả lập, framebuffer 240×320 kết xuất bằng mã `SymbianUI.cpp` thực. Ảnh PNG **không chụp từ thiết bị thật**.

| Trường hợp | Kết quả | Bằng chứng |
|---|---|---|
| Regression 3 lõi v2.4.2 | **17/17 PASS** | `regression_v242.log` |
| Pixel Snake: game + chữ ký demo/cấu hình sản xuất | **6/6 PASS** | `regression_snake.log` (6 bước biên dịch/chạy) |
| Cài 2 ứng dụng mặc định `Welcome`, `Help Site` qua C++ installer | **PASS** | 2 thư mục cài + `receipt.bin` trên SD giả lập |
| Khởi tạo catalog lại sau cài | **PASS** | 2 ứng dụng tìm thấy sau mô phỏng khởi động lại |
| Sửa byte package rồi cài | **PASS: bị từ chối** | Log bộ cài |
| Nạp, kiểm tra palette `.vqeaf`; tệp sai định dạng | **PASS** | Invalid không vào catalog, S60 Green được nhận |
| Lưu theme ngoại lên NVS giả lập, đọc sau khởi tạo lại | **PASS** | `SettingsStore` C++ với `Preferences` test shim |
| HTML qua HTTP giả; mở link | **PASS** | Trang HTTP 200, 3 dòng, 1 link và đúng URL |
| Kết xuất UI bằng C++ RGB565 240×320 | **PASS: 5 khung hình** | PNG tại `screenshots/` |
| PlatformIO ESP32-S3, SD thật, WiFi/TLS thật, LCD vật lý | **CHƯA THỬ** | Chưa có thiết bị/toolchain tại đây |

**Dữ liệu:** `welcome.qeapp` và `help_site.qeapp` ký bằng public key mặc định; Snake demo được ký bằng khóa demo khác nên firmware bình thường từ chối đúng như thiết kế. **Không hạ mức xác minh chữ ký.**

**Giới hạn hình ảnh:** Ảnh tạo từ lớp render C++ thật và trạng thái đã xác nhận bởi từng test, nhưng luồng tương tác trên host được điều khiển bởi test harness thay vì nhấn nút vật lý. Chữ trong ảnh do font tham chiếu trên host. Đừng coi đó là xác nhận UI hoạt động hoàn chỉnh trên thiết bị.

**Cách chạy lại:** xem `README.md` trong bộ test. Tệp `.qeapp` và `.vqeaf` được lấy từ nguồn v2.4.2; không có mã firmware mới nào được nạp.
