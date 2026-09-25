# Qeafbrowser — Retro-Go UI rewrite (ESP32-S3 E524546)

Viết lại Qeafbrowser_v1.7 thành launcher + trình duyệt giống Retro-Go,
**dọc 240x320** (ST7789, xác nhận của user thay cho mặc định ngang).
Tham chiếu layout/theme: https://github.com/ducalex/retro-go
Nguồn chức năng port từ: `projects/Qeafbrowser_v1.7`.

## Trạng thái: G2 hoàn thành (lớp hiển thị + input + font + theme)

```
include/
  pins.h                    GPIO bất khả xâm phạm (giữ nguyên v1.7)
  LGFX_ESP32S3_ST7789.h     cấu hình LovyanGFX (giữ nguyên v1.7)
  rg_config.h               MỌI hằng số trong 1 file (màn, layout, phím, đường dẫn)
  rg_display.h              lớp thấp: framebuffer PSRAM + checksum theo dòng + DMA flush
  rg_input.h                keypad: debounce, auto-repeat, hold SELECT>600ms đổi Game/T9
  rg_gui.h                  lớp cao: draw_text/rect/image/dialog giống Retro-Go
  rg_theme.h                loader theme.json ĐÚNG schema Retro-Go (THEMING.md)
  fonts/lc_font_vn12|16.h   font proportional Tahoma tiếng Việt, stream rg_font_t
src/
  rg_display.cpp  rg_input.cpp  rg_gui.cpp  rg_theme.cpp
  main.cpp                  màn hình test G2
data/retro-go/themes/default/theme.json   theme mặc định (nạp bằng uploadfs)
sd/retro-go/themes/default/theme.json     bản chép vào SD
lib/                        LovyanGFX 1.2.30 + TJpg_Decoder + PNGdec (vendored, offline)
tools/make_font.py          sinh lại font từ TTF (như v1.7)
```

## Build & nạp

```
pio run                  # build (đã verify: RAM 8.6%, Flash 7.3%)
pio run -t upload        # nạp firmware
pio run -t uploadfs      # nạp LittleFS (theme mặc định dự phòng khi không có SD)
pio device monitor -b 115200
```

## Thử trên thiết bị (G2 test screen)

1. Màn hình theo layout Retro-Go (thích ứng dọc): header 50px gồm ô logo 46x50
   (trái), banner tên app (phải logo, +8), dòng status (y=34); danh sách 8 mục
   tiếng Việt căn giữa dọc; footer gợi ý phím.
2. Phím 2/8: cuộn chọn (giữ = auto-repeat). Phím 4/6: đổi tông launcher_1..4.
3. Phím 7 (OPTION): mở hộp thoại demo chuẩn Retro-Go (tiêu đề, viền, mục 2 cột
   đổi giá trị bằng 4/6, mục disabled, mục message). 3 (Back)/1 (Menu): đóng.
4. Phím 16 (SELECT) giữ >600ms: đổi Game/T9 — xem log Serial; chế độ T9 bấm
   phím in ký tự số ra Serial.
5. Theme: chép nguyên thư mục theme Retro-Go bất kỳ vào SD
   `/retro-go/themes/<tên>/` (theme.json + *.png). Thiếu file/SD → tự dùng
   theme mặc định build-in, không crash.

## Kế hoạch các giai đoạn

| Giai đoạn | Nội dung | Trạng thái |
|-----------|----------|-----------|
| G1 | Khảo sát, thiết kế thư mục | xong |
| G2 | rg_display + rg_input + font + theme + màn test | **xong** |
| G3 | launcher_gui: tabs, PNG art (logo/banner/cover/background), preview, fav | kế tiếp |
| G4 | WiFi wizard + NTP + port http.cpp/T9 nhập URL | chưa |
| G5 | port wml.cpp render trang + điều hướng link + thumbnail | chưa |
| G6 | hoàn thiện: settings lưu flash, crash log, dọn code, tài liệu | chưa |
