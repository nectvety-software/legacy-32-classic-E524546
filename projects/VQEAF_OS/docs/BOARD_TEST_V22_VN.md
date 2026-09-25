# VQEAF OS v2.2 — Hướng dẫn biên dịch và kiểm tra trên thiết bị

**Chưa thử nghiệm trên ESP32-S3 thật.** Dưới đây là quy trình bạn có thể thực hiện sau khi sao lưu firmware/SD gốc.

## 1. Chuẩn bị

- Thiết bị ESP32-S3-WROOM-1 N16R8 và ST7789 240×320 dọc dùng sơ đồ GPIO hiện tại; không tự chuyển các chân trong `include/BoardConfig.h`.
- Cài PlatformIO Core và driver/USB tương ứng; kết nối board qua cáp truyền dữ liệu, SD FAT32 nếu kiểm tra theme.
- Giữ nguyên `platformio.ini` để firmware sử dụng `TFT_eSPI`, Arduino, PSRAM OPI như phiên bản được cung cấp.

## 2. Build và nạp

```powershell
cd VQEAF-OS
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Host regression (máy tính đã có g++ và Python 3): `python tools/test_ui_v22.py`. Lệnh này không thay thế biên dịch PlatformIO.

## 3. Test giao diện

1. Khởi động bình thường: Splash `VQEAF OS` → **Home/General** như ảnh tham chiếu (đồng hồ, WiFi, thông báo, ba ô nhanh).
2. Bấm MENU: **3×4 menu** theo thứ tự WiFi/Bluetooth/Music, File mgr/Gallery/Internet, Shell/Recovery/Settings, Themes/Apps/Library. Chỉ redraw hai ô cũ/mới khi thay đổi lựa chọn, không nháy toàn màn.
3. Bấm MENU hoặc A/B trong menu trở lại Home. Trong app, bấm MENU về lưới. Trong lưới, OPTION → Retro Explorer để kiểm tra launcher dạng tab (tùy chọn); A trong Explorer đưa về Menu.
4. D-pad lên/xuống/trái/phải trên lưới: 12 mục không tràn chỉ số và không va thanh footer. Giữ SELECT >600 ms: chuyển Game/T9 và không tự nhấn OK; dùng Input Editor thử backspace.
5. Chép `sd/System/Themes/vqeaf_reference_lime.vqeaf` vào `SD:/System/Themes/` và apply trong `Menu → Themes`. So sánh Home/Grid/Settings/Footer; thử VQEAF Night và VQEAF Day. Theme hỏng/mất SD phải fallback an toàn.
6. Truy cập WiFi, BLE, File manager, Music, Gallery, Browser, Settings, Shell, Calculator, Stopwatch; so sánh ảnh chụp 240×320 với các bảng `docs/UI_PIXEL_ATLAS_V22.md`, đặc biệt app có nội dung nhiều hơn một trang.
7. Giữ DOWN khi cấp nguồn: phải tới Recovery/Safe Mode; không phụ thuộc theme trên SD. Kiểm tra tháo SD an toàn khi nhạc đã dừng.
8. Mở installer với QEAPP/2 **đã ký bằng key được cấu hình trong chính firmware của bạn**; gói unsigned/tampered bị từ chối. Không thay public key nhận dạng publisher chỉ để chạy demo.

## 4. Báo cáo kết quả

Ghi lại log PlatformIO `SUCCESS/FAILED`, Serial 115200, màn hình ảnh thật Home/Menu/Settings/Themes và 16 màn, dung lượng heap/PSRAM sau 5–10 phút, nhiệt/reset/watchdog, trạng thái WiFi/TLS/mất thẻ SD. Nếu gặp lỗi build, gửi **dòng lỗi đầu tiên**, file/line và phiên bản PlatformIO cùng board package.

**Không phát hành BIN/chứng nhận hiệu năng trước khi các kiểm tra thật PASS.**
