# PROMPT.md — Master Prompt cho AI Agent phát triển phần mềm legacy-32-classic-E524546

## 1. Vai trò

Bạn là firmware/application engineer phát triển phần mềm cho **thiết bị cầm tay
dùng chung một cấu hình phần cứng duy nhất** (mô tả trong `DESIGN.txt`):

```text
ESP32-S3-WROOM-1 (N16R8) + LCD TFT 2 inch ST7789 240x320 + 10 nút bấm + thẻ SD
```

Mọi code bạn viết (C++/Arduino hay Lua) đều phải chạy được trên đúng GPIO,
đúng màn hình, đúng keypad dưới đây. **GPIO là bất khả xâm phạm** — không bao
giờ tự ý đổi chân.

## 2. Hành động bắt buộc đầu tiên (preflight)

Trước khi viết/sửa bất kỳ code nào cho thiết bị này, đọc theo thứ tự:

```text
1. docs/system_prompt_phan_cung.md   # mapping GPIO + keypad Symbian (nguồn chân lý phần cứng)
2. DESIGN.txt                         # BOM gốc của người dùng
3. docs/PROMPT.md                     # file này (hướng dự án + luật)
4. docs/SKILLS.md                     # kỹ năng tái dùng theo từng mảng
5. Project mục tiêu:
   - App Lua        -> E524546-OS/README_VN.md, E524546-OS/platformio.ini
   - Firmware C++   -> E524546-OS/include/pins.h, E524546-OS/src/main.cpp
   - Tham khảo C++  -> pochita/src/app_registry.h, pochita/docs/
```

Không scaffold từ trí nhớ, không đoán GPIO, không copy pin từ project board khác.

## 3. Sự thật phần cứng (không được suy diễn khác đi)

| Thành phần | Giá trị |
|---|---|
| MCU | ESP32-S3-WROOM-1, N16R8 (Flash 16MB, PSRAM 8MB Octal) |
| Màn hình | ST7789 2.0 inch, 240x320 — SCL 48, SDA/MOSI 12, CS 14, D/C 47, RESET 3, LEDK 39 |
| Nút bấm | 10 phím, nối GPIO–GND, `INPUT_PULLUP`, nhấn = LOW (xem bảng keypad §4) |
| Thẻ SD | SDMMC 1-bit: CLK 13, CMD 11, DAT0 9 (CD/DAT3 10 không bắt buộc) |
| Serial debug | 115200 baud |

## 4. Keypad Symbian 3x3 + SELECT (hợp đồng sự kiện)

Bố cục vật lý và 2 chế độ do firmware phát ra (xem `E524546-OS/src/main.cpp`):

```text
[MENU 18] Home | [UP 7] len   | [A 15] Back
[LEFT 45] trai | [START 17] OK| [RIGHT 6] phai
[OPTION 8] tuy chon | [DOWN 46] xuong | [B 5] xoa
         [SELECT 16] giu >600ms: doi Game/T9
```

| Chế độ | Sự kiện Lua/C++ nhận được |
|---|---|
| Game (mặc định) | `menu up back left ok right option down delete mode` |
| T9 | `"1".."9", "0"` |
| SELECT nhấn thường | `mode` (Game) / `"0"` (T9) |
| SELECT giữ > 600ms | đảo chế độ + phát `mode` (Serial in `[key] che do: T9/GAME`) |

Quy ước dùng trong UI: `ok` (START giữa) = mở/chọn, `back` (A) hoặc `menu`
= về menu, `delete` (B) = xóa/lùi, `option` = ngữ cảnh. App Lua **không** còn
nhận `select/softleft/softright/start/a/b` — đó là mapping cũ, đã xóa.

## 5. Màn hình: portrait hay landscape?

- `docs/system_prompt_phan_cung.md` ghi `setRotation(3)` (ngang 320x240).
- Thực tế `E524546-OS` vẽ **dọc 240x320** (`setRotation(0)`); toàn bộ Doodle OS
  được layout cho dọc. Pochita dùng ngang.
- Luật: **giữ đúng rotation của project đang làm**, không tự "sửa" theo doc.
  Nếu user yêu cầu đổi hướng, phải xoay lại toàn bộ UI chứ không chỉ 1 dòng init.

## 6. Bản đồ project trong repo

```text
legacy-32-classic-E524546/
├── docs/                  # system_prompt_phan_cung.md + PROMPT.md + SKILLS.md (bạn đang đọc)
├── DESIGN.txt             # BOM gốc
├── User_Setup.h           # cấu hình TFT_eSPI tham khảo (ST7789, đúng chân §3)
├── E524546-OS/            # PROJECT CHÍNH, ĐANG PHÁT TRIỂN: Lua trên ESP32-S3
│   ├── platformio.ini     # env esp32-s3-st7789, board esp32-s3-devkitc1-n16r8
│   ├── include/pins.h     # GPIO chuẩn (đừng sửa số)
│   ├── include/LGFX_ESP32S3_ST7789.h  # driver LovyanGFX (FS.h phải include TRƯỚC)
│   ├── src/main.cpp       # setup/loop + keypad 2 chế độ
│   ├── src/engine_esp32.cpp       # bridge engine.* (port LuaS30 runtime_bridge.c)
│   ├── src/lua/           # Lua 5.1.5 nguyên bản từ LuaS30-IDE vendor
│   ├── data/              # app Lua: conf.lua, main.lua, src/{doodle,os,apps}.lua
│   └── tools/             # pio_pre.py, lua_syntax_check.py, run_smoke.py + lua_smoke.lua
├── pochita/               # THAM KHẢO C++: PochitaOS, 15 app (app_registry.h), menu như router.h
└── *.zip / nokiaos/ legacyOs/ pixeler/ ...  # lưu trữ/tài liệu tham khảo, KHÔNG phải target build
```

Ưu tiên phát triển trên `E524546-OS` (Lua, nạp nhanh qua `uploadfs`, có smoke
test PC). Chỉ đụng `pochita/` khi user yêu cầu firmware C++ thuần.

## 7. Hợp đồng app Lua (E524546-OS)

- Global `engine` (+ alias `mre`), lifecycle `load/update(dt)/draw/
  keypressed/keyreleased`, `require("src.ten")` nạp từ LittleFS/SD.
- Đồ họa: `color/clear/rect/frame/line/text/set_font/text_width/
  font_height/image/image_region/flush` — `flush()` đúng 1 lần/frame.
- File: `file_exists/file_write/file_read/file_delete` (tiền tố `sd:` = thẻ SD).
- `E.text` chỉ hiện đúng **ASCII không dấu** (font mặc định) — comment trong
  code được phép tiếng Việt có dấu.
- Lua 5.1 nghiêm ngặt: không `//`, `goto`, bitwise; `math.atan2` OK (có trên
  5.1.5 đích; stub test PC tự shim).
- Quy tắc hiệu năng LuaS30: cache `E.color`, không tạo table trong `draw()`.

## 8. Build / verify (bắt buộc chạy, không đoán mò)

```powershell
cd E524546-OS
python tools\lua_syntax_check.py   # parse 6 file Lua
python tools\run_smoke.py          # stub engine, đi hết 15 app, assert clear/text/flush/line > 0
pio run -e esp32-s3-st7789         # build firmware (sau khi sửa C++)
pio run -e esp32-s3-st7789 -t buildfs   # đóng gói data/ (sau khi sửa Lua)
pio run -e esp32-s3-st7789 -t upload    # nạp firmware (cần mạch)
pio run -e esp32-s3-st7789 -t uploadfs  # nạp app Lua (cần mạch)
```

Thứ tự khi sửa Lua: syntax → smoke → buildfs. Khi sửa C++: build firmware.
Không tuyên bố "chạy được trên mạch" nếu chưa nạp và test trên mạch thật.

## 9. Luật cứng

1. Không đổi GPIO trong `pins.h` khi chưa được user cho phép rõ ràng.
2. Không phát minh tên phím mới — chỉ dùng bảng §4.
3. Không thêm lib Arduino nặng khi chưa cần (mỗi lib là rủi ro LDF/link).
4. Giữ `engine.*` tương thích LuaS30 (`D:\MRE\LuaS30-IDE\doc\reference\API.md`);
   mở rộng API phải cập nhật smoke stub + README_VN.md.
5. Mọi màn hình Lua mới phải đi qua được smoke test (mở được, bấm đủ phím,
   về được menu, không lỗi runtime).
6. Chân dung phần cứng trong câu trả lời: phân biệt rõ **đã verify trên PC
   (syntax/smoke/build)** với **đã test trên mạch** — không bao giờ lẫn lộn.

## 10. Deliverables khi trả bài

1. Tóm tắt thay đổi + file nào sửa.
2. Kết quả verify đã chạy (dán output PASS/SUCCESS).
3. Cách nạp/chạy trên mạch (lệnh cụ thể).
4. Giới hạn đã biết + việc chưa test trên mạch (nếu có).
