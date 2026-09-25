# MEMORY — Qeafbrowser (Retro-Go UI rewrite)

Ghi chú dài hạn của project `projects/Qeafbrowser`. Việc theo ngày nằm ở
`memory/YYYY-MM-DD.md`.

## Bối cảnh

- Mục tiêu: viết lại `projects/Qeafbrowser_v1.7` thành launcher + GUI **giống
  Retro-Go** (tab, list, dialog, theme.json schema Retro-Go, font tỷ lệ có dấu)
  trên E524546 (ESP32-S3 N16R8 + ST7789 240x320 **dọc** + keypad Symbian + SD).
- Kiến trúc 2 lớp: `rg_display` (framebuffer PSRAM + checksum theo dòng, chỉ đẩy
  vùng dirty) → `rg_gui` (draw_text/dialog/list/tab). Immediate-mode, không timer.
- Giai đoạn G2 (lớp hiển thị + input + font + theme) đã có code; đang ở bước
  nạp lên máy để kiểm tra bằng mắt.

## Sự thật đã verify trên mạch (2026-09-24)

- PSRAM 8 MB nhận đủ: `ESP.getPsramSize()` = 8386279 (`qio_opi` đúng cho N16R8).
- Framebuffer 240x320 RGB565 = 153600 byte cấp được từ PSRAM.
- Theme loader chạy đúng thứ tự fallback SD → LittleFS → mặc định build-in.
- `ARDUINO_USB_CDC_ON_BOOT=0` ⇒ `Serial` = UART0 = cổng CH340 (COM3). Log app
  hiện trực tiếp trên COM3, không cần cắm cổng USB native.

## Bẫy môi trường / phần cứng (đọc trước khi debug)

- **CH340 rớt khỏi USB ~1.5–1.8 s sau reset**, tự quay lại sau vài chục giây.
  Hệ quả: log boot đứt giữa đường và upload hay fail
  (`Could not open COM3`, `PermissionError(13)`), phải retry nhiều lần.
  ⇒ **Không được kết luận "firmware treo ở dòng X" chỉ vì log dừng ở đó.**
  Luôn kiểm tra tool đọc port có báo lỗi hay không (`tools/serial_watch.py` in lỗi
  ra stderr). Đây đã từng làm tôi chẩn đoán sai thành "treo ở `rg_lcd.init()`".
- Thẻ SD: mount OK nhưng đọc timeout (`sdmmc_host_wait_for_event 0x107`).
- Không dùng `python -c` nhiều dòng qua Bash trên máy này (sandbox chặn); viết
  script vào `tools/` rồi chạy.

## Quy ước code

- MỌI hằng số (chân, kích thước, màu, phím) nằm ở `include/rg_config.h`.
  `include/pins.h` là GPIO bất khả xâm phạm (luật `docs/PROMPT.md`).
- Log boot theo kiểu v1.7: in từng bước + `Serial.flush()` ngay sau (UART TX mất
  dữ liệu đang chờ nếu code chết/treo ngay sau đó).
- Font tiếng Việt có dấu ở `include/fonts/lc_font_vn12.h` / `lc_font_vn16.h`,
  sinh bằng `tools/make_font.py`.
- Theme mẫu: `data/retro-go/themes/default/theme.json` (nạp qua `uploadfs`),
  bản chép tay cho thẻ SD ở `sd/retro-go/themes/default/theme.json`.

## Lệch cấu hình đã sửa

- `include/LGFX_ESP32S3_ST7789.h`: `cfg.pin_rst` **phải là `TFT_RESET`** (GPIO 3).
  Bản trước đặt `-1` với lý do "suspected COM6 drop" → panel không được reset cứng,
  ST7789 vào trạng thái không xác định → màn nhấp nháy, không hiện nội dung.
  Đã trả về `TFT_RESET` (2026-09-24), khớp Qeafbrowser_v1/v1.4/v1.7, E524546-OS,
  pochita, legacyOs/DESIGN.md. Nghi ngờ "COM drop do GPIO3" là **sai** — COM drop
  vẫn xảy ra khi GPIO3 chưa hề bị điều khiển.
- `main.cpp`: không dùng `RG_GUI_BOTTOM ± n`. `vpos()`/`hpos()` chỉ so sánh bằng
  đúng giá trị hằng; cộng trừ vào đó ra số khổng lồ và bị vẽ ngoài màn.

## Chẩn đoán màn hình trắng/đen (dùng lại khi gặp)

Bốn phép đo tách bạch nguyên nhân, không cần đoán:
1. log `lcd.init()` trả về gì + panel WxH sau `setRotation`;
2. self-test đổ 4 màu toàn màn (`RG_DISPLAY_SELFTEST` trong `rg_display.cpp`);
3. `rg_display_count_nonblack()` — fb có nội dung thật chưa;
4. `rg_display_flush_lines()` — dữ liệu có thực sự đẩy ra panel chưa;
   kèm heartbeat `redraws=` + `rg_input_raw()` để loại trừ phím kẹt gây redraw thừa.
