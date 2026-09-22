# ESP Browser — Opera Mini 4 Mode v1.3

This build extends the DMAX-style keypad browser with an Opera Mini 4 inspired browsing mode for 240×320 feature-phone layouts.

## v1.3 highlights
- Opera Mini 4.5-style HTTP User-Agent and mobile/WAP Accept headers.
- HTTPS redirect support kept intact (`qeafivels.com` -> `www.qeafivels.com`).
- Fit-to-width single-column HTML rendering for low-memory ESP32-S3 devices.
- Blue D-Pad focus rectangle around text/link blocks; center OK opens the selected link.
- Opera-style page overview and virtual mouse retained.
- HTTPS lock indicator in the red title bar.
- Qeafivels added to the Speed Dial start page.
- Parser additionally suppresses modern `template`, `iframe`, and `object` payloads.

# ESP_Browser 1.2 — trình duyệt web nhẹ kiểu DMAX cho E524546 (UI name: **Browser**)

Firmware Arduino/PlatformIO cho thiết bị cầm tay **E524546**
(ESP32-S3-WROOM-1 N16R8 + ST7789 240×320 dọc + keypad Symbian 3×3+SELECT + SD),
viết lại từ engine QQ Browser (DMAX) đã phân tích trong
`D:\MRE\Porting-code\DMAX_QQ_Browser\src_extract` (323 module C++ gốc → 1 firmware C++ gọn).


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
- Cookie session được lưu bền tại `/ESPBrowser/cookies.txt`, cùng History/Bookmark/Config.
- Response binary/content-type không render được sẽ báo loại MIME thay vì in byte rác lên màn hình.


## Cơ chế port (theo docs/VXP_PORTING_SKILL.md + J2ME_to_MRE_Porting_Guide.md)

| Engine gốc (DMAX_QQ_Browser) | Bản port (ESP_Browser) |
|---|---|
| `DOM/Parser/XML_Parser` + 41 `WML_*` + 33 `HTML_*` | `src/wml.cpp` — 1 vòng quét tag, pool tĩnh `Doc.lines[400]`, word-wrap theo `gfx_textWidth` |
| `HttpConnection/Request/Response/Cookie` | `src/http.cpp` — HTTP/HTTPS GET thật, TLS trên ESP32-S3, HTTP/1.1, chunked, redirect relative ≤6 hop, cookie theo domain |
| `Bookmark/History/DownloadManager` | `src/store.cpp` — file text trên SD (`/ESPBrowser/*.txt`), fallback LittleFS |
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
   - và **ghi bền** xuống `/ESPBrowser/config.ini` trên thẻ SD (nếu có SD/LittleFS),
   - lần boot sau **tự động kết nối lại** mạng đã lưu, không cần wizard.
5. Mạng open (không mật khẩu) → kết nối thẳng, bỏ qua bước nhập.
6. Ở màn danh sách, bấm **OPTION** = quên cấu hình (xóa config + quét lại).

### Cách 2 — file cấu hình sẵn (môi trường không có màn hình cảm ứng)

1. Chép `sd/ESPBrowser/config.ini` vào thẻ SD, điền `wifi_ssid=...`, `wifi_pass=...`.
2. Build + nạp:

```powershell
cd D:\Program\arduino\legacy-32-classic-E524546\ESP_Browser
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
.\espbrowser_sim.exe --test             # wizard WiFi + OPTION menu + nhap URL (mock HTTP)
.\espbrowser_sim.exe --live             # cua so tuong tac, host TCP bridge (Winsock)
.\espbrowser_sim.exe --fetch http://example.com/   # GET HTTP thật qua host TCP bridge
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
ESP_Browser/
├── platformio.ini          # espressif32 + LovyanGFX (board devkitc1-n16r8, PSRAM opi)
├── include/
│   ├── pins.h              # GPIO E524546 (bat kha xam pham)
│   ├── browser.h           # ABI Doc/Http/Store
│   ├── LGFX_ESP32S3_ST7789.h  # copy tu E524546-OS (dung chung cau hinh panel)
│   └── logo_dmax.h         # 45x45 RGB565 port tu .vm_res goc
├── src/  wml.cpp http.cpp store.cpp main.cpp
├── sim/  Windows sim + host TCP bridge + build_sim.ps1
└── sd/ESPBrowser/config.ini
```
