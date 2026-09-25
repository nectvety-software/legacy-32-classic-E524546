# Qeafbrowser v2.2 — Giao diện Symbian S60, chữ ĐẬM kiểu Nokia

Toàn bộ khung UI được vẽ lại theo chuẩn **Symbian S60 (3rd Edition)** và dùng font
**đậm nét** như Nokia 2700: mọi chuỗi của giao diện đều được thicken thêm 1 px
stem, cho ra nét chữ 2 px đúng chất feature phone.

Ảnh kiểm tra: `sim/s60_out/*.bmp` (240×320, mở bằng `sim/inspect_bmp.py` hoặc
bất kỳ trình xem BMP). Xem mục *Kiểm thử* ở dưới để chạy lại.

## Theme nằm ở đâu

| File | Nội dung |
|---|---|
| `include/ui_s60.h` | Bảng màu + metric S60 duy nhất cho **cả firmware và simulator** (macro `S60_RGB`, `S60_PANE_TOP/BOT`, `S60_SEL_TOP/BOT`, `S60_SOFT_TOP/BOT`, `S60_ACCENT`, `S60_ROW_H`…) |
| `src/main.cpp` § *Symbian S60 theme engine* | `s60_text()` (chữ đậm), gradient 565, thanh chọn xanh, panel bo góc, keycap, icon glyph |
| `sim/s60_main.cpp` + `sim/s60_out/*.bmp` | Regression: splash, Speed Dial, list row, trang web, Options menu, keypad ảo, field |

Các macro `UI_*` cũ vẫn còn nhưng giờ là alias của palette S60, nên mọi điểm gọi
cũ không phải sửa.

## Những gì đã đổi trên màn hình

- **Application pane (22 px)**: một dải gradient xanh thép → navy dùng chung cho
  status pane và title bar, 1 px kẻ sáng dưới chân pane. Bên trái là 5 vạch sóng
  Nokia (lấy mức RSSI thật trên ESP32), bên phải là biểu tượng WiFi + pin, ở giữa
  là tiêu đề trang **in đậm**. Khi đang tải, 3 px cuối của pane thành progress bar
  xanh `S60_ACCENT`; dòng trạng thái `Connecting… / Sending request… /
  Receiving…` thay chỗ tiêu đề.
- **Thanh softkey (18 px)**: gradient đen, 1 px kẻ sáng, nhãn trái/phải **đậm**
  (`Menu`/`Back`, `OK`/`Cancel`, `Select`/`Cancel`) và đồng hồ NTP ở giữa.
- **Dòng danh sách S60** (`<folder>`/`<dir>`): thanh chọn xanh gradient có kẻ sáng,
  icon glyph 16 px (globe/settings/bookmark/history/search/wifi/info/help) chọn theo
  nhãn, nhãn **đậm**, mũi tên phải, 1 px kẻ chân dòng. Khoảng trắng giữa hai dòng
  danh sách được co từ 22 px xuống 2 px nên danh sách liền mạch như menu Nokia.
- **Ô nhập liệu** (`<input>`/`<field>`): bo góc, khi focus thì **tô nền xanh + chữ
  trắng** đúng kiểu text box S60.
- **Options menu**: panel bo góc có bóng, mục **đậm**, thanh chọn xanh full-width,
  mũi tên cho mục có menu con.
- **Bàn phím ảo**: keycap gradient bo góc, key đang chọn là gradient xanh S60.
- **Splash**: pane trên cùng + logo Qeafbrowser + nhãn `Symbian S60 interface` +
  progress bar.
- **Bản đồ nhỏ / scrollbar / overview**: tông xám-xanh `S60_SCROLL_*`, khung focus
  xanh `S60_ACCENT`.

Chữ đậm **không** thêm font bitmap thứ hai: `s60_text()` vẽ lại chuỗi lệch 1 px
sang phải (`S60_BOLD_PX`), nên không tốn flash/RAM, số đo wrap của `wml.cpp`
không đổi, và ESP32 với simulator cho ra **cùng một kết quả**.

## Lệnh kiểm tra mới

```
pio device monitor
> doc          # in layout da parse: dong / style / chieu cao / y (style 3 = hang danh sach)
```

`extern "C"` cho harness: `sim_draw_splash()` (chụp lại màn khởi động) và
`sim_doc_dump()` (in layout ra stdout).

## Sửa kèm trong bản này

- **Chuột ảo bị dính giữa các trang**: `mouse_on` trước đây không được tắt khi mở
  trang WML/`mtt:`, nên chuột ảo bật từ trang desktop trước đó vẫn "nuốt" D-Pad
  của UI nhỏ. Giờ trang WML luôn tắt chuột ảo.
- **Harness build được trên Windows**: biến `OUT` trong các `sim/*_main.cpp` bị
  `<windef.h>` định nghĩa thành macro rỗng → đổi tên thành `OUTDIR`.
- **Fixture `https://keypad.test/long.html`** (WML dài 24 block) để regression
  cuộn pixel/inertia có đủ quãng cuộn; `pixel_scroll_main` và `inertia_scroll_main`
  chờ animation settle lâu hơn (2 s).
- `sim/inspect_bmp.py`: đọc BMP 240×320 ra terminal (bands / ascii / zoom) để kiểm
  tra layout và màu khi không mở được cửa sổ ảnh.

Kiểm chứng: `pio run -e esp32-s3-st7789` → SUCCESS, RAM 64576 B (19.7%),
Flash 1096613 B (16.7%). Regression: `s60` 0 FAIL, `pixel_scroll`, `inertia_scroll`,
`overview_anim`, `headless` đều chạy hết. `qeafivels` chỉ còn `thumbnail_lru=FAIL`
trên Windows vì bản build Windows của simulator không có decoder JPEG/PNG
(`QB_HAS_TJPEG`/`QB_HAS_PNGDEC` = 0); trên Linux (`-lpng16 -ljpeg`) và trên ESP32
đường decode đầy đủ.

## Qeafbrowser v2.0 — light D-Pad inertia

- Main-page pixel scrolling now keeps a small fixed-point velocity while D-Pad is held.
- Releasing the D-Pad applies integer friction (`230/256` per animation tick), producing a short natural coast before settling.
- Direction reversal sheds old momentum quickly instead of producing a long overshoot.
- Autoscroll gets at most an 8-pixel coast margin, so the focused Java/Symbian content block remains visible.
- No secondary framebuffer, sprite, float math, or heap allocation was added to the inertia path.
- v1.9 and v2.0 host builds have identical BSS size (`277104` bytes); only code text increased slightly.

## Qeafbrowser v1.9 pixel-smooth content scrolling

- Main-page scrolling now animates in real pixels with fixed-point integer easing.
- D-Pad focus navigation updates a logical target line while the visual document position eases toward the target.
- Overview OK handoff uses the same easing rhythm so returning from Desktop Overview no longer jumps.
- Mini-map and scrollbar follow the visual pixel position during motion.
- No second framebuffer or full-screen sprite is allocated.


## Qeafbrowser v1.8 smooth overview

Overview navigation now uses low-RAM fixed-point easing inspired by Opera Mini 4 / Java-Symbian browsers. D-Pad pan and zoom update a target position immediately, while the visible viewport, scrollbar, mini-map, and outer page-tile cursor interpolate toward that target at ~17 ms ticks. The animation stores only integer state and never allocates a second framebuffer. In the Linux simulator build, BSS increased from 277104 to 277136 bytes (+32 bytes) compared with v1.7.

Controls in Overview remain: LEFT/RIGHT change zoom x1..x8, UP/DOWN pan by a fraction of the current viewport, OK enters the currently visible position, and BACK exits Overview.

# Qeafbrowser — Opera Mini 4 Style Browser v1.7

## v1.7 highlights

- Default website: `https://qeafivels.com/`.
- Real JPEG/PNG thumbnails on ESP32 using `TJpg_Decoder` + `PNGdec`.
- Two-tier thumbnail cache: **PSRAM LRU** + **LittleFS RGB565 cache with CRC32**.
- Opera Mini 4-inspired overview cursor with corner handles and center crosshair.
- `x1..x8` overview zoom; UP/DOWN pans by viewport chunks.
- Page overview is a grid of **preview page tiles** based on real rendered pixel heights, including cached image thumbnails.
- Smooth feature-phone scrollbar and mini-map remain visible on normal browsing screens.


This build extends the Qeafbrowser-style keypad browser with an Opera Mini 4 inspired browsing mode for 240×320 feature-phone layouts.

## v1.3 highlights
- Opera Mini 4.5-style HTTP User-Agent and mobile/WAP Accept headers.
- HTTPS redirect support kept intact (`qeafivels.com` -> `www.qeafivels.com`).
- Fit-to-width single-column HTML rendering for low-memory ESP32-S3 devices.
- Blue D-Pad focus rectangle around text/link blocks; center OK opens the selected link.
- Opera-style page overview and virtual mouse retained.
- HTTPS lock indicator in the red title bar.
- Qeafivels added to the Speed Dial start page.
- Parser additionally suppresses modern `template`, `iframe`, and `object` payloads.

# Qeafbrowser — trình duyệt web nhẹ cho E524546

Firmware Arduino/PlatformIO cho thiết bị cầm tay **E524546**
(ESP32-S3-WROOM-1 N16R8 + ST7789 240×320 dọc + keypad Symbian 3×3+SELECT + SD),
viết lại từ engine QQ Browser (Qeafbrowser) đã phân tích trong
`D:\MRE\Porting-code\legacy keypad browser\src_extract` (323 module C++ gốc → 1 firmware C++ gọn).


## Keypad Focus Navigation 1.2

- Focus rectangle màu xanh kiểu Java/Symbian: không tô kín nội dung, chỉ ôm sát text/link/block đang chọn.
- Paragraph/heading bị wrap nhiều dòng được giữ thành một focus block nhờ `block-id` từ parser.
- Link wrap nhiều dòng được focus như một khối duy nhất; nhấn phím giữa **OK** mở URL của link.
- D-Pad `UP/LEFT` sang block trước, `DOWN/RIGHT` sang block sau trong layout mobile một cột.
- Viewport tự cuộn để block đang focus luôn nằm trong vùng nhìn thấy.
- Sửa parser anchor để link trong `<p>` không bị mất `link-id` và sửa lỗi mất ký tự đầu sau nhiều block rỗng.
- Có headless simulator/CI harness (`sim/headless_main.cpp`) chạy trực tiếp cùng `src/*.cpp` và xuất framebuffer 240×320 thành BMP/PNG test.

## Nâng cấp browser thật trong 1.1

- HTTPS/TLS trên ESP32-S3 (`WiFiClientSecure`), HTTP/1.1 và `Host`/User-Agent/Accept hiện đại hơn.
- Redirect 301/302/303/307/308 và `Location` tương đối; URL base giữ đúng HTTP/HTTPS sau redirect.
- URL bar nhận `example.com`, tự chuyển thành `https://example.com`; Google Search có URL-encode query.
- Back stack 12 trang + Forward stack 12 trang; `Tools > Forward`, Refresh giữ đúng URL hiện tại.
- HTML parser lấy `<title>`, bỏ script/CSS, giữ khoảng trắng giữa từ, hỗ trợ `img alt`, entity cơ bản và attribute có khoảng trắng như `href = "..."`.
- Cookie session được lưu bền tại `/Qeafbrowser/cookies.txt`, cùng History/Bookmark/Config.
- Response binary/content-type không render được sẽ báo loại MIME thay vì in byte rác lên màn hình.


## Cơ chế port (theo docs/VXP_PORTING_SKILL.md + J2ME_to_MRE_Porting_Guide.md)

| Engine gốc (legacy keypad browser) | Bản port (Qeafbrowser) |
|---|---|
| `DOM/Parser/XML_Parser` + 41 `WML_*` + 33 `HTML_*` | `src/wml.cpp` — 1 vòng quét tag, pool tĩnh `Doc.lines[400]`, word-wrap theo `gfx_textWidth` |
| `HttpConnection/Request/Response/Cookie` | `src/http.cpp` — HTTP/HTTPS GET thật, TLS trên ESP32-S3, HTTP/1.1, chunked, redirect relative ≤6 hop, cookie theo domain |
| `Bookmark/History/DownloadManager` | `src/store.cpp` — file text trên SD (`/Qeafbrowser/*.txt`), fallback LittleFS |
| `UIMain/UILayer/UIMenu/QGDI` | `src/main.cpp` — state machine 5 màn + softkey bar, vẽ bằng LovyanGFX |
| `vm_create_timer` game loop | `loop()` + `keys_poll()` debounce 25 ms (pattern `E524546-OS/src/main.cpp`) |
| `vm_malloc` heap | buffer 2×48 KB cấp 1 lần từ **PSRAM** (`heap_caps_malloc`), không malloc trong loop |
| `mtt:` pseudo-URL | giữ nguyên: `mtt:start/history/bookmark/config/about/help` |
| Asset `.rodata` (14 blob carve) | start page + About/Help/Settings **nguyên văn** từ bản carve; splash vẽ `logo565.h` (RGB565 + color-key port từ `.vm_res`) |

## Điều khiển (keypad Symbian — GPIO theo docs/system_prompt_phan_cung.md)

```
[MENU 18] trang chu | [UP 7] cuoc len  | [A 15] lui
[LEFT 45] link truoc| [START 17] mo    | [RIGHT 6] link sau
[OPTION 8] bookmark | [DOWN 46] cuoc xuong | [B 5] history
        [SELECT 16] giu >600ms: doi Game/T9
```

## Cài đặt

### Cách 1 — wizard WiFi trên máy (khuyến nghị, như điện thoại)

1. Bấm **MENU** về trang chủ → chọn **Settings** → **>> Ket noi WiFi (quet mang)**.
2. Máy **tự động quét** WiFi xung quanh (tối đa 12, mạnh nhất trước) → UP/DOWN chọn mạng, OK mở.
3. **Nhập mật khẩu multi-tap** kiểu Nokia/Symbian ngay trên keypad:

   | Nhit | Ký tự |
   |---|---|
   | `UP` ×1/2/3/4 | a b c 2 |
   | `A` ×1..4 | d e f 3 |
   | `LEFT` ×1..4 | g h i 4 |
   | `START` ×1..4 | j k l 5 |
   | `RIGHT` ×1..4 | m n o 6 |
   | `OPTION` ×1..5 | p q r s 7 |
   | `DOWN` ×1..4 | t u v 8 |
   | `B` ×1..5 | w x y z 9 |
   | `MENU` | `. ? 1 , !` (mật khẩu) / `. : / @ ? !` (URL) |
   | `SELECT` | space (mật khẩu) / `-_&=#%+` (URL); **giu SELECT >600ms** = HOA/thuong |
   | `B` | xoa ky tu | `OK` | ket noi / mo URL | `A` | quay lai danh sach |

   Bam nhanh trong 900 ms de doi ky tu trong cung nhom; ky tu dang cho hien
   nghieng `_` tren man. Ở sim, gõ thẳng bàn phím PC (host bridge).
4. OK → màn hình **Dang ket noi...** (timeout 12 s) → thành công thì:
   - SSID + mật khẩu lưu ngay vào **bộ nhớ tạm** (`cfg_ssid/cfg_pass` trong RAM,
     `WiFi.persistent(true)` lưu cả credential vào flash ESP32),
   - và **ghi bền** xuống `/Qeafbrowser/config.ini` trên thẻ SD (nếu có SD/LittleFS),
   - lần boot sau **tự động kết nối lại** mạng đã lưu, không cần wizard.
5. Mạng open (không mật khẩu) → kết nối thẳng, bỏ qua bước nhập.
6. Ở màn danh sách, bấm **OPTION** = quên cấu hình (xóa config + quét lại).

### Cách 2 — file cấu hình sẵn (môi trường không có màn hình cảm ứng)

1. Chép `sd/Qeafbrowser/config.ini` vào thẻ SD, điền `wifi_ssid=...`, `wifi_pass=...`.
2. Build + nạp:

```powershell
cd D:\Program\arduino\legacy-32-classic-E524546\Qeafbrowser
pio run -t upload
pio device monitor     # 115200
```

Trạng thái WiFi luôn xem được ở **Settings** (CONNECTED/OFF + SSID + IP sau khi kết nối).

## Giới hạn (chấp nhận đc — đúng tinh thần WAP-era)

- **HTTP + HTTPS thật** trên ESP32-S3. URL không có scheme như `example.com` tự ưu tiên `https://`. Redirect và link tương đối (`/`, `../`, `?query`, `#anchor`) được resolve theo base URL.
- TLS hiện dùng `WiFiClientSecure::setInsecure()` để tăng tương thích với web công cộng trên thiết bị không có CA store/RTC đầy đủ. Kênh vẫn mã hóa nhưng **không xác thực chứng chỉ máy chủ**; với ứng dụng đăng nhập/nhạy cảm nên thay bằng CA bundle.
- Render là **lightweight HTML/WML text browser**, không phải Chromium: không chạy JavaScript, không CSS layout đầy đủ, không video/WebAssembly. `script/style/noscript/svg/canvas` bị bỏ khỏi nội dung hiển thị.
- Ảnh chưa decode/render trực tiếp; nếu có `alt`/`aria-label` thì browser hiển thị mô tả ảnh để trang vẫn đọc được.
- Server cố tình trả gzip/Brotli dù client yêu cầu `identity` sẽ báo lỗi thay vì render dữ liệu nén.
- 1 card mỗi trang (nhiều `<card>` WML: render card đầu).
- Nhập URL / mật khẩu: **bàn phím ảo D-pad** (kiểu E524546-OS / LegacyOS TextEditor):
  QWERTY 4 hàng + SHF/SYM/SPC/DEL/GO. OK=chọn ký tự, MENU=xong, A=huy, B=xóa,
  SELECT=SHIFT, OPTION=SYM. Sim: gõ bàn phím PC vẫn được (host bridge).

## Sim PC (Windows)

```powershell
cd sim
powershell -File build_sim.ps1          # MinGW g++, static
.\qeafbrowser_sim.exe --test             # wizard WiFi + OPTION menu + nhap URL (mock HTTP)
.\qeafbrowser_sim.exe --live             # cua so tuong tac, host TCP bridge (Winsock)
.\qeafbrowser_sim.exe --fetch http://example.com/   # GET HTTP thật qua host TCP bridge
```

| Cờ | Ý nghĩa |
|---|---|
| `--test` | kịch bản tự động, HTTP mock (ổn định) |
| `--live` | HTTP qua **host TCP bridge** (socket thật trên PC) |
| `--mock` | ép HTTP mock |
| `--fetch <url>` | boot + mở URL bằng hộp nhập, GET HTTP thật. HTTPS mock chạy được; `--live` Win32 vẫn là raw Winsock nên chưa có TLS/Schannel. HTTPS thật chạy trên ESP32 |

Phím sim: mũi tên = di chuyển, Enter=OK, Esc=A/Back, Backspace=B/Delete,
Home=MENU, Alt=OPTION, Space=SELECT. **Ban phim PC** gõ thẳng vào hộp nhập URL.

## Cấu trúc

```
Qeafbrowser/
├── platformio.ini          # espressif32 + LovyanGFX (board devkitc1-n16r8, PSRAM opi)
├── include/
│   ├── pins.h              # GPIO E524546 (bat kha xam pham)
│   ├── browser.h           # ABI Doc/Http/Store
│   ├── LGFX_ESP32S3_ST7789.h  # copy tu E524546-OS (dung chung cau hinh panel)
│   └── logo_dmax.h         # 45x45 RGB565 port tu .vm_res goc
├── src/  wml.cpp http.cpp store.cpp main.cpp
├── sim/  Windows sim + host TCP bridge + build_sim.ps1
└── sd/Qeafbrowser/config.ini
```


## v1.5 - Real Thumbnail + Overview Cursor + Multi Zoom
- Added real JPEG/PNG thumbnail decode path in Linux simulator for `<img>` blocks (used by qeafivels regression).
- `<img>` tags now become dedicated image blocks with cached thumbnail rendering and blue focus frame.
- Overview cursor refined to a tighter Opera Mini 4 style viewport box with mini-map at top-right.
- Zoom levels extended to x1..x5 in Desktop overview via D-Pad LEFT/RIGHT.
- Mini-map added on browse screen right edge to show total page and current viewport.
- Added simulator image assets and updated qeafivels regression screenshots.

## v2.1 — Real-time Internet clock

- Footer clock now synchronizes with NTP after Wi-Fi/Internet becomes available.
- Default timezone is Viet Nam UTC+7 via `timezone=ICT-7` in `/Qeafbrowser/config.ini`.
- NTP sources: `pool.ntp.org`, `time.google.com`, and `time.cloudflare.com`.
- Until the first successful synchronization the clock shows `--:--`. After synchronization, ESP32 system time keeps running even if the network temporarily drops.
- The center clock region alone is repainted once per second; no framebuffer, background task, or full-page redraw is added. The colon blinks once per second in the Java/Symbian feature-phone style.


## v2.1.2 — ESP32 build fix

- Compatible with PNGdec 1.1.6 callback API (`int PNGDraw(PNGDRAW*)`).
- PNG callback now returns `1` to continue scanline decoding.
- PNGdec is pinned to `1.1.6` in `platformio.ini`.
- Duplicate USB compile defines were removed; `board_build.arduino.cdc_on_boot = 1` remains.
- Default hardware target remains ESP32-S3 N16R8 (`qio_opi`, 16 MB flash, PSRAM enabled).
