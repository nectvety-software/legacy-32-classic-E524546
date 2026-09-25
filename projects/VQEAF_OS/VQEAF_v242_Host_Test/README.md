# VQEAF OS v2.4.2 — C++ Host Acceptance Kit

Bộ test này kiểm tra lõi thật nhưng chạy trên **máy tính** với giả lập microSD và HTTP. Ảnh PNG lấy từ framebuffer 240×320 do `src/core/SymbianUI.cpp` vẽ qua host TFT; font là bộ tham chiếu của test host. **KHÔNG phải ảnh chụp từ ESP32-S3**.

## Nguồn và cách tái chạy

1. Giải nén `VQEAF_OS_v2.4.2_Core_Recovery_Full_Source.zip`, tìm thư mục `VQEAF-OS`.
2. Trên Linux/WSL, cài `g++`, OpenSSL headers, Python 3, `cryptography` và `Pillow`. Gõ `python3 -m pip install cryptography Pillow` trong môi trường Python của bạn.
3. Chạy `VQEAF_SOURCE_DIR=/duong/dan/VQEAF-OS python3 run_acceptance.py` trong thư mục bộ test này. Đầu ra: `screenshots/`, `logs/`, `emulated_sd/`.
4. Tại gốc mã nguồn, chạy `python3 tools/verify_v242.py` rồi `python3 tools/test_pixel_snake.py` để xác nhận bộ regression đầy đủ.

**Test trường hợp thực:**
- Bộ cài `AppInstallerService` kiểm tra chữ ký và cài `welcome.qeapp` + `help_site.qeapp` bằng gói thực từ dự án lên thư mục giả lập SD. Cả 2 có `receipt.bin` và xuất hiện trong catalog sau tạo lại đối tượng dịch vụ (mô phỏng khởi động lại); bản bị sửa byte bị từ chối.
- Bộ đọc theme `ThemeFileService` quét bản `.vqeaf` thực và loại tệp sai cấu trúc, sau đó `SettingsStore` lưu/chọn đường dẫn theme và đọc lại từ NVS giả lập qua đối tượng mới; `SymbianUI::setExternalTheme` vẽ trang sau áp dụng.
- `BrowserService` tải và parse HTML có link qua server giả `FakeHttp`, thực thi mở link và đối chiếu URL.
- `tools/test_pixel_snake.py` xác nhận game Snake được cài qua khóa demo riêng và **bị từ chối đúng** trên cấu hình khóa mặc định.

**Chưa kiểm tra:** build PlatformIO target, LCD ST7789 vật lý, thẻ microSD thật, OTA, HTTPS trực tuyến và khả năng chạy website JavaScript nặng.

## Kiểm thử trên thiết bị thật

```powershell
cd VQEAF-OS
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Chạy trong Shell: `corediag`, `sddiag rw`, `tlsdiag valid`. Thử cài `welcome.qeapp` từ `System/Apps/Inbox` và áp dụng `s60_green.vqeaf` từ `System/Themes`. Để thử Snake, nạp **bản riêng** `vqeaf_snake_demo`; không trộn khóa demo vào firmware production. Chụp ảnh trực tiếp màn hình ST7789 rồi đối chiếu với `screenshots/` trong bộ test. Nên sao lưu dữ liệu microSD trước khi kiểm tra thẻ.
