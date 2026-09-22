# v1.3 — Opera Mini 4 / Qeafivels mode

- Added Opera Mini 4.5-inspired request headers.
- Added Qeafivels Speed Dial entry.
- Added HTTPS lock indicator.
- Added modern HTML skip rules for template/iframe/object.
- Added simulator redirect + semantic fixture for https://qeafivels.com/ -> https://www.qeafivels.com/.
- Added dedicated Qeafivels keypad browsing regression/screenshots.

# Changelog

## 1.2.0 — Java/Symbian Keypad Focus

- Thêm focus rectangle xanh ôm sát link/text/block, bỏ kiểu highlight đỏ cả hàng.
- Group paragraph/heading nhiều dòng bằng parser `block-id`; group link nhiều dòng bằng `line0..line1`.
- D-Pad điều hướng theo focus block; OK mở link đang focus; viewport tự cuộn theo block.
- Sửa anchor parser để link đóng thẻ vẫn giữ đúng `link-id`.
- Sửa lỗi `cur_off` làm mất ký tự đầu của heading sau các empty block liên tiếp.
- Bổ sung mock HTML test `keypad.test`, portable POSIX simulator shim và `headless_main.cpp`.
- Test framebuffer 240×320: heading, paragraph, link wrap, LEFT/RIGHT, OK mở link, auto-scroll đều PASS.

## 1.1.0 — Real Web Core

- Thêm HTTPS/TLS bằng `WiFiClientSecure` trên ESP32-S3.
- Nâng request lên HTTP/1.1; hỗ trợ redirect tương đối tối đa 6 hop.
- Thêm `url_normalize_input`, `url_resolve`, `url_encode_query`.
- URL không có scheme mặc định dùng HTTPS.
- Sửa link `/path` không còn bị ép về HTTP khi trang gốc là HTTPS.
- Thêm Back/Forward stack 12 trang.
- Search Google dùng HTTPS và percent/form encoding.
- HTML parser lấy `<title>`, bỏ script/style/noscript/svg/canvas, sửa mất khoảng trắng giữa từ.
- Hỗ trợ `alt`/`aria-label` cho ảnh ở chế độ text.
- Cookie được lưu qua reboot trong `/ESPBrowser/cookies.txt`.
- Chặn protocol không hỗ trợ như `javascript:`, `mailto:`, `data:` thay vì tạo URL sai.
- Báo MIME binary/unsupported và body bị truncate an toàn theo giới hạn PSRAM.

### Giới hạn còn lại

Đây là browser HTML/WML nhẹ theo kiểu DMAX/Opera Mini đời cũ, không phải engine Chromium/WebKit. JavaScript, CSS layout đầy đủ, video và WebAssembly chưa được thực thi. TLS hiện ưu tiên tương thích bằng `setInsecure()`; có thể thay bằng CA bundle ở bản tiếp theo.


## v1.4 - Opera Mini 4 Small Screen Rendering
- Added SSR image placeholder blocks scaled to 240px width for `<img alt=...>` content.
- Improved Desktop overview with zoom levels x1-x3 controlled by D-Pad left/right.
- OK on overview jumps into selected page area; up/down pans the overview viewport.
- Added qeafivels.com regression with hero image, product strip, overview zoom and screenshots.
