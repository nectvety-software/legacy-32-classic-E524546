## v2.0 - Fixed-Point D-Pad Inertia
- Added light velocity accumulation while the browse D-Pad is physically held.
- Added allocation-free integer friction after key release for a short decelerating coast.
- Added fast momentum shedding when the user reverses D-Pad direction.
- Added an 8-pixel bounded coast margin only when focus navigation already requires autoscroll.
- Preserved the existing fixed-point pixel-scroll target and final settle behavior.
- Added `sim/inertia_scroll_main.cpp` regression and screenshots for hold -> release -> coast -> settle.
- Host BSS remains unchanged versus v1.9 (277104 bytes).

## v1.9 - Pixel Smooth Content Scroll
- Added allocation-free fixed-point pixel scrolling for the main page.
- D-Pad focus autoscroll now eases over intermediate pixel positions instead of jumping by whole lines.
- Overview-to-page OK handoff animates to the selected content position using the same easing cadence.
- Main mini-map and scrollbar track visual pixel position during the transition.
- Header is repainted after content to safely clip partially visible blocks without an off-screen buffer.

# Qeafbrowser v1.7 — Overview Tiles + PSRAM/LittleFS Thumbnail Cache

- Refined Opera Mini 4 style overview cursor: double red keyline, corner handles, center crosshair, chunked D-Pad panning and x1..x8 zoom.
- Overview now uses page preview tiles based on real rendered pixel height (~one feature-phone viewport per tile), not fixed line counts.
- Cached JPEG/PNG thumbnails are sampled into overview tiles, so images appear in the miniature page preview.
- PSRAM thumbnail cache now uses LRU eviction with hit counters instead of always recycling slot 0.
- LittleFS thumbnail cache upgraded to v2 with dimensions, RGB565 format metadata and CRC32 validation; v1 cache files remain readable.
- First three page images are prefetched after HTML parsing so overview thumbnails are ready immediately.
- Regression gate forces more than six thumbnail keys to verify LRU eviction and reload from persistent cache.

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
- Cookie được lưu qua reboot trong `/Qeafbrowser/cookies.txt`.
- Chặn protocol không hỗ trợ như `javascript:`, `mailto:`, `data:` thay vì tạo URL sai.
- Báo MIME binary/unsupported và body bị truncate an toàn theo giới hạn PSRAM.

### Giới hạn còn lại

Đây là browser HTML/WML nhẹ theo kiểu Qeafbrowser/Opera Mini đời cũ, không phải engine Chromium/WebKit. JavaScript, CSS layout đầy đủ, video và WebAssembly chưa được thực thi. TLS hiện ưu tiên tương thích bằng `setInsecure()`; có thể thay bằng CA bundle ở bản tiếp theo.


## v1.4 - Opera Mini 4 Small Screen Rendering
- Added SSR image placeholder blocks scaled to 240px width for `<img alt=...>` content.
- Improved Desktop overview with zoom levels x1-x3 controlled by D-Pad left/right.
- OK on overview jumps into selected page area; up/down pans the overview viewport.
- Added qeafivels.com regression with hero image, product strip, overview zoom and screenshots.


## v1.5 - Real Thumbnail + Overview Cursor + Multi Zoom
- Added real JPEG/PNG thumbnail decode path in Linux simulator for `<img>` blocks (used by qeafivels regression).
- `<img>` tags now become dedicated image blocks with cached thumbnail rendering and blue focus frame.
- Overview cursor refined to a tighter Opera Mini 4 style viewport box with mini-map at top-right.
- Zoom levels extended to x1..x5 in Desktop overview via D-Pad LEFT/RIGHT.
- Mini-map added on browse screen right edge to show total page and current viewport.
- Added simulator image assets and updated qeafivels regression screenshots.


## v1.8 - Smooth Overview Motion
- Added allocation-free fixed-point easing for overview pan and zoom (17 ms animation tick).
- Added animated Opera Mini 4-style tile cursor that glides between preview page tiles.
- Mini-map and scrollbar now track the animated visual position instead of jumping directly to the target.
- D-Pad pan keeps the existing target-step behavior, while the visible viewport eases toward it over ~100-160 ms.
- No secondary framebuffer or sprite is allocated. Host regression BSS increased by only 32 bytes versus v1.7.
- Added `sim/overview_anim_main.cpp` regression capturing early/mid/settled animation frames.


## v2.1
- Added real-time NTP clock in the Opera Mini-style footer.
- Added automatic NTP start/retry when Wi-Fi/Internet becomes available and reconnects.
- Added configurable POSIX timezone (`timezone=ICT-7`, default UTC+7).
- The system RTC keeps the synchronized time after temporary network loss.
- Clock redraw is limited to a 60×18 px footer region once per second; no extra framebuffer.
- Added `sim/realtime_clock_main.cpp` regression and clock screenshots.

## v2.1.2 — ESP32-S3 PlatformIO build fix
- Fixed PNGdec 1.1.6 callback signature: `png_thumb_draw` now returns `int` and returns `1` to continue decoding.
- Added defensive PNG callback/input guards and explicit `size_t` -> `int` length validation for `PNG::openRAM`.
- Pinned `bitbank2/PNGdec@1.1.6` so the build uses the API this source targets.
- Removed duplicate/conflicting `ARDUINO_USB_MODE` / `ARDUINO_USB_CDC_ON_BOOT` command-line defines; `board_build.arduino.cdc_on_boot = 1` remains authoritative.
- No framebuffer or runtime image-cache allocation changes.
