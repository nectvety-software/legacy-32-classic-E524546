# Qeafbrowser 1.2 — Keypad Focus Test Report

Ngày test: 2026-09-22

## Mục tiêu

Kiểm tra cơ chế duyệt web kiểu Java/Symbian trên màn 240×320 không cảm ứng: focus rectangle xanh nhảy giữa heading, paragraph, link và block bằng D-Pad; phím giữa OK kích hoạt link; viewport tự cuộn theo focus.

## Cách chạy

Harness `sim/headless_main.cpp` chạy trực tiếp firmware `src/main.cpp`, `src/wml.cpp`, `src/http.cpp`, `src/store.cpp` qua shim Arduino/LGFX. Network transport dùng mock HTTP/HTTPS xác định để test parser/render/navigation; request vẫn đi qua `http_get()` và URL resolver thật của firmware. Không dùng ảnh UI dựng thủ công.

Lệnh build Linux/CI:

```bash
g++ -O2 -std=gnu++17 -Isim/shims -Isim -Iinclude \
  -o sim/qeafbrowser_headless \
  sim/headless_main.cpp sim/sim_arduino.cpp \
  src/main.cpp src/wml.cpp src/http.cpp src/store.cpp
./sim/qeafbrowser_headless
```

## Kết quả

| Gate | Kết quả |
|---|---|
| HTML parser lấy title/nội dung | PASS |
| Heading nhiều dòng = 1 focus block | PASS |
| Paragraph wrap nhiều dòng = 1 focus block | PASS |
| Link wrap nhiều dòng = 1 focus block | PASS |
| Viền xanh ôm sát block, không tô kín nội dung | PASS |
| D-Pad DOWN/RIGHT sang block sau | PASS |
| D-Pad UP/LEFT sang block trước | PASS |
| OK mở link `/article.html` | PASS |
| Relative URL resolve thành `https://keypad.test/article.html` | PASS |
| History ghi URL + title `Article Opened` | PASS |
| Auto-scroll khi focus xuống cuối viewport | PASS |
| Lỗi mất ký tự đầu heading | FIXED / PASS |

History sau test xác nhận trang con được mở bằng OK: `https://keypad.test/article.html` với title `Article Opened`.

## Screenshot

- `docs/screenshots/keypad_focus/01_focus_heading.png`
- `docs/screenshots/keypad_focus/02_focus_paragraph.png`
- `docs/screenshots/keypad_focus/03_focus_long_link.png`
- `docs/screenshots/keypad_focus/04_left_previous_block.png`
- `docs/screenshots/keypad_focus/05_right_long_link.png`
- `docs/screenshots/keypad_focus/06_ok_open_article.png`
- `docs/screenshots/keypad_focus/07_autoscroll_lower_block.png`
- `docs/screenshots/keypad_focus/keypad_focus_contact_sheet.png`

## Giới hạn test

Môi trường container hiện tại không có DNS/Internet ra ngoài và không có ESP32-S3 vật lý, nên test này xác nhận toàn bộ luồng parser → focus → render → keypad → URL resolver → HTTP mock, nhưng chưa xác nhận TLS/CA và timing keypad trên bo mạch thật. HTTPS thật vẫn dùng `WiFiClientSecure` trên ESP32-S3 như bản 1.1.
