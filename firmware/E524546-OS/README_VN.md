# E524546-OS — Code Lua (lõi LuaS30) cho ESP32-S3 + ST7789 240x320 + 10 nút + SD

Lõi Lua **5.1.5 nguyên bản** copy từ `D:\MRE\LuaS30-IDE\vendor\lua-5.1.5` vào `src/lua/`.
API Lua giữ **y hệt LuaS30** (`doc/reference/API.md`): global `engine` + alias `mre`,
lifecycle `load/update(dt)/draw/keypressed/keyreleased/pause/resume/quit`.

## 1. Phần cứng (đúng BOM bạn đưa)

| TFT ST7789 2" 240x320 | ESP32-S3 |
|---|---|
| VCC | VCC (3V3) |
| GND | GND |
| LEDK | GPIO 39 |
| D/C | GPIO 47 |
| CS | GPIO 14 |
| SCL | GPIO 48 |
| SDA (MOSI) | GPIO 12 |
| RESET | GPIO 3 |

Nút bấm Symbian 3x3 + SELECT (nối GPIO–GND, `INPUT_PULLUP`, nhấn = LOW),
theo `docs/system_prompt_phan_cung.md`:

```
[MENU 18]="1" Home | [UP 7]="2" len | [A 15]="3" Back
[LEFT 45]="4" trai | [START 17]="5" OK | [RIGHT 6]="6" phai
[OPTION 8]="7" tuy chon | [DOWN 46]="8" xuong | [B 5]="9" xoa
             [SELECT 16]="0" (giu >600ms: doi Game/T9)
```

Tên phím trong Lua (chế độ Game): `menu/up/back/left/ok/right/option/down/
delete/mode`; chế độ T9: `"1".."9","0"`. Giữ SELECT > 600ms để đổi chế độ
(Serial in `[key] che do: T9/GAME`).

SD (SDMMC 1-bit): `CLK 13 | CMD 11 | DAT0 9` (CD/DAT3=10 không bắt buộc).

## 2. Build + nạp (PlatformIO)

```powershell
cd D:\Program\arduino\legacy-32-classic-E524546\E524546-OS
pio run -e esp32-s3-st7789            # biên dịch
pio run -e esp32-s3-st7789 -t upload  # nạp firmware
pio run -e esp32-s3-st7789 -t uploadfs  # nạp data/*.lua vào LittleFS
pio device monitor -b 115200
```

Board: `esp32-s3-devkitc1-n16r8` (đúng N16R8: Flash 16MB + PSRAM 8MB Octal).
Thư viện duy nhất: `lovyan03/LovyanGFX` (driver ST7789 + framebuffer RGB565 + decode PNG/JPG/BMP).

## 3. Code bằng Lua ở đâu?

## 3b. Doodle OS — menu + tính năng kiểu PochitaOS (mặc định trong `data/`)

Launcher giữ đúng 15 app theo `pochita/src/app_registry.h`: WiFi, Files,
Bluetooth, Terminal, Notes, LoRa, IR, Browser, Settings, Radio, Music, Paint,
Retro, Chat, VM — nhưng vẽ tay 100% bằng code phong cách vở học sinh
(`data/src/doodle.lua`: giấy kẻ ngang + lề đỏ, nét run, sao/mũi tên/xoắn ốc).

- **WiFi**: menu như `router.h` (Mang hien co / Mang da luu / Trang thai /
  Quet mang) + quét, cột sóng, ổ khóa, Chi tiết (BSSID/kênh/bảo mật), menu
  ngữ cảnh (Ket noi/Luu/Quen/Chi tiet), lưu `wifi.dat`. Mô phỏng (chưa gọi
  WiFi thật).
- **Files**: duyệt `main.lua/conf.lua/*.dat`, SELECT xem trước, OPTION xóa.
- **Terminal**: chạy lệnh `help/info/dem/xoa` bằng SELECT.
- **Notes/Paint**: 3 trang vẽ lưu `trangN.dat`, chia sẻ editor chung.
- **Retro**: game Rắn săn mồi, lưu điểm cao `diem.dat`.
- **Settings/Radio/Music/Chat/VM/LoRa/IR/Browser**: chỉnh fps/bút/BT
  (`settings.dat`), dò đài, phát nhạc giả (chưa có loa), chat bot, xem máy.
- Điều hướng: lên/xuống chọn, START (=OK, phím giữa) mở, A (=Back) hoặc
  MENU về menu, B (=Delete) xóa/lùi nét, OPTION mở tùy chọn.

Kiểm thử trên PC: `python tools/run_smoke.py` (stub engine + đi hết 15 app,
assert clear/text/flush/line > 0, chống màn hình đen kiểu LuaS30).

Chỉ cần sửa 2 file trong `data/`, rồi `uploadfs` — **không cần đụng C++**:

- `data/conf.lua` — `{fps=30}` (8..60)
- `data/main.lua` — app chính, dùng `engine.*`
- `data/src/*.lua` — module riêng, gọi bằng `require("src.ten_module")`
- Ảnh: `data/assets/*.png|jpg|bmp` → `E.image(x, y, "assets/a.png")`
- Ảnh trên SD: `E.image(x, y, "sd:anh.png")` (file ở gốc thẻ SD)
- Save: `E.file_write/read/exists/delete("save.dat")` → LittleFS; thêm tiền tố `sd:` để ghi ra thẻ.

Khung `main.lua` tối thiểu (copy từ `templates/basic` của LuaS30):

```lua
local E = engine
local x, y = 105, 140
local bg = E.color(18, 24, 38)
local white = E.color(255, 255, 255)
local keys = {}
function E.load() E.set_font(8) end
function E.update(dt)
  if keys.left then x = x - 90*dt end
  if keys.right then x = x + 90*dt end
  if keys.up then y = y - 90*dt end
  if keys.down then y = y + 90*dt end
end
function E.draw()
  E.clear(bg)
  E.rect(math.floor(x), math.floor(y), 16, 16, E.color(255,210,70))
  E.text(4, 4, "Hello S3", white)
  E.flush() -- bắt buộc 1 lần/frame
end
function E.keypressed(k)
  k = tostring(k):lower()
  keys[k] = true
end
function E.keyreleased(k)
  keys[tostring(k):lower()] = false
end
```

## 4. API engine.* khả dụng (port từ `engine/src/runtime_bridge.c`)

```
E.W, E.H, E.version
E.color(r,g,b) E.clear(c) E.rect E.frame E.line E.text E.set_font
E.text_width(s) E.font_height() E.image E.image_region E.flush
E.tick_ms() E.log(s) E.exit()
E.file_exists/write/read/delete
E.audio_* (stub, has_audio=false — chưa có chân I2S trong BOM)
E.capabilities() E.device_info() E.runtime_compat()
E.sd_ok()  -- riêng bản ESP32: true nếu thẻ SD mount OK
E.has_files=true E.has_images=true E.has_removable=true E.has_log=true
```

Quy tắc hiệu năng của LuaS30 vẫn giữ: cache `E.color(...)`, không tạo table
trong `draw()`, `flush()` 1 lần/frame, ảnh load 1 lần (firmware tự cache qua file).

## 5. Kiến trúc port

```
data/main.lua (Lua của bạn)
  -> engine.* (src/engine_esp32.cpp — port runtime_bridge.c)
  -> Lua 5.1.5 (src/lua/ — nguyên bản vendor)
  -> LovyanGFX sprite 240x320 RGB565 + LittleFS/SD_MMC
  -> ESP32-S3-WROOM-1 N16R8 + ST7789 + 10 nút
```

`runtime_lua.c` (lifecycle + error_screen + fps timer) được port thành
`engine_open_vm/draw_frame/key_event/error_screen` trong cùng file C++.
