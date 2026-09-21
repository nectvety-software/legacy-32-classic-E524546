# SKILLS.md — Kỹ năng tái dùng cho thiết bị legacy-32-classic-E524546

Mỗi skill là một công thức độc lập: đọc phần cần thiết, làm theo các bước,
verify bằng lệnh chuẩn. Nguồn chân lý phần cứng duy nhất:
`docs/system_prompt_phan_cung.md` + `DESIGN.txt`.

## Skill 1 — Keypad Symbian (`keypad-symbian`)

**Khi dùng:** mọi việc liên quan nút bấm, mapping phím, thêm hành động phím.

**Bảng sự kiện (firmware `E524546-OS/src/main.cpp` phát ra):**

```text
Game mode: menu | up | back | left | ok | right | option | down | delete | mode
T9 mode  : "1" "2" "3" "4" "5" "6" "7" "8" "9" "0"
```

- GPIO: MENU 18, UP 7, A 15, LEFT 45, START 17, RIGHT 6, OPTION 8, DOWN 46,
  B 5, SELECT 16. Tất cả `INPUT_PULLUP`, nhấn = LOW, debounce 25ms.
- Vai trò OS: START(giữa)=`ok` mở/chọn; A=`back` hủy/về; MENU=`menu` home;
  B=`delete` xóa; OPTION=`option` ngữ cảnh; SELECT ngắn=`mode`, giữ >600ms
  đảo Game/T9.
- App Lua bắt phím bằng `keypressed(k)` với `k` đã lowercase; về menu khi
  `k == "back" or k == "menu"`; tuyệt đối không dùng tên cũ
  (`select/softleft/softright/start/a/b`).
- Sau khi sửa mapping: chạy `python tools\run_smoke.py` (scenario bấm đủ
  13 tên phím qua hết 15 app) rồi `pio run -t buildfs` (Lua) /
  `pio run` (C++).

## Skill 2 — Màn hình ST7789 (`display-st7789`)

**Khi dùng:** driver màn hình, xoay màn hình, màu sắc, vẽ cơ bản.

- Chân: SCL 48, SDA 12, CS 14, D/C 47, RESET 3, LEDK 39 (backlight PWM).
- `E524546-OS` dùng **LovyanGFX** (`include/LGFX_ESP32S3_ST7789.h`), SPI2,
  40MHz write, panel invert, **dọc 240x320** (`setRotation(0)`).
- Bẫy include-order của LovyanGFX: `FS.h`/`LittleFS.h`/`SD_MMC.h`/`SPI.h`
  phải include **TRƯỚC** `LovyanGFX.hpp`, nếu không overload
  `drawPngFile/drawJpgFile` cho filesystem biên dịch thiếu (`DataWrapperT`
  abstract). File header trong repo đã làm đúng — giữ nguyên thứ tự.
- Tham khảo TFT_eSPI: `User_Setup.h` ở root (đúng chân, SPI 80MHz).
- Màu Lua: luôn qua `E.color(r,g,b)` (RGB565), cache lại, không convert mỗi frame.

## Skill 3 — Lưu trữ SD + LittleFS (`storage-sd`)

**Khi dùng:** đọc/ghi file, thêm asset, nạp data.

- SD SDMMC 1-bit: CLK 13, CMD 11, DAT0 9 (`SD_MMC.setPins` rồi `begin`).
  Trong Lua: tiền tố `sd:` (vd `E.image(x, y, "sd:anh.png")`).
- App Lua sống ở LittleFS (`data/`): `conf.lua`, `main.lua`, `src/*.lua`.
  Nạp bằng `pio run -t uploadfs`; kiểm tra đóng gói bằng
  `pio run -t buildfs`.
- File API Lua: `file_exists/file_write/file_read/file_delete`; file save
  của OS (`dem.dat`, `wifi.dat`, `settings.dat`, `trangN.dat`, `diem.dat`).
- Bẫy build pioarduino: lib framework FS/LittleFS/SD_MMC/SPI thiếu
  `library.json` gây lỗi `FS.h: No such file` — đã fix bằng
  `tools/pio_pre.py` (thêm CPPPATH toàn cục) + `#include <FS.h>` trực tiếp
  trong source. Đừng xóa 2 thứ này.

## Skill 4 — App Lua engine.* (`lua-app`)

**Khi dùng:** viết/sửa màn hình, thêm app vào Doodle OS.

- Khung: `data/main.lua` (dispatch `screen`: boot/menu/<app-id>),
  `data/src/os.lua` (registry 15 app + launcher + settings),
  `data/src/apps.lua` (`enter/update/draw/key` cho từng app),
  `data/src/doodle.lua` (thư viện vẽ).
- Thêm app mới: (1) thêm `{id, name}` vào `S.apps` trong `os.lua`,
  (2) thêm icon vào `D.icon` trong `doodle.lua`, (3) thêm 4 hàm
  `enter/update/draw/key` + case trong dispatch cuối `apps.lua`,
  (4) `A.key` phải trả `"menu"` khi back, (5) footer ghi đúng phím mới.
- `require("src.x")` nạp `data/src/x.lua`; module `return` table.
- Verify: `lua_syntax_check.py` → `run_smoke.py` (đi hết app, assert
  clear/text/flush/line > 0) → `buildfs`.

## Skill 5 — UI Doodle vở học sinh (`doodle-ui`)

**Khi dùng:** vẽ màn hình mới đúng phong cách hiện tại.

- Nền `D.paper()` (giấy vàng + kẻ ngang + lề đỏ + lỗ đục), tiêu đề
  `D.title`, chân trang `D.footer`, hộp thoại `D.dialog`,
  tiến trình `D.progress`, công tắc `D.toggle`, sóng `D.bars`, ổ khóa `D.lock`.
- Nét run tay: `D.wline/skrect/skcircle`, trang trí `D.star/D.arrow/D.spiral`.
- Chữ hiển thị **ASCII không dấu**; số dòng vừa khung 240x320
  (nội dung từ y≈76 đến y≈300, footer y=306).
- Tham khảo menu 15 app kiểu PochitaOS: `pochita/src/app_registry.h`
  (danh sách), `pochita/src/router.h` (cấu trúc menu WiFi).

## Skill 6 — Build PlatformIO (`pio-build`)

**Khi dùng:** biên dịch, nạp, gỡ lỗi build.

- `E524546-OS`: env `esp32-s3-st7789`, board `esp32-s3-devkitc1-n16r8`,
  Arduino, lib duy nhất `lovyan03/LovyanGFX`. Monitor 115200.
- `pochita` (tham khảo): env `pochita_esp32s3`, board `esp32-s3-devkitc-1`,
  platform `espressif32@6.10.0`, nhiều lib (audio/NES/BLE) — build nặng,
  chỉ đụng khi user yêu cầu.
- Lỗi thường gặp: thiếu `SPI.h` trong LDF graph (link lỗi `undefined
  reference to SPI`) → thêm `#include <SPI.h>`; LovyanGFX thiếu filesystem
  overload → kiểm tra thứ tự include (Skill 2); `FS.h` not found → kiểm tra
  `tools/pio_pre.py` còn được gọi trong `platformio.ini`.
- Không commit `.pio/`, `dist/`, file `.zip` build lên git khi chưa được yêu cầu.

## Chỉ mục file tham khảo (đọc khi cần chi tiết)

```text
docs/system_prompt_phan_cung.md  GPIO + keypad + luật firmware/display
DESIGN.txt                       BOM gốc
E524546-OS/README_VN.md          hướng dẫn Lua + keymap + lệnh nạp
E524546-OS/include/pins.h        GPIO chuẩn (đừng đổi số)
E524546-OS/src/main.cpp          keypad 2 chế độ + SD + LittleFS init
E524546-OS/src/engine_esp32.cpp  toàn bộ engine.* (port LuaS30 runtime_bridge.c)
E524546-OS/tools/lua_smoke.lua   scenario smoke test (stub engine + đi 15 app)
pochita/docs/                    tài liệu SD/file/app của PochitaOS
D:\MRE\LuaS30-IDE\doc\reference\API.md   gốc API engine.* (LuaS30)
```
