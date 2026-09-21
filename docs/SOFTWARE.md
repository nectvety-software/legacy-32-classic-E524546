# SOFTWARE.md — Tồn kho phần mềm legacy-32-classic-E524546

Tất cả phần mềm dưới đây dùng chung một cấu hình phần cứng duy nhất
(`DESIGN.txt` + `docs/system_prompt_phan_cung.md`):
ESP32-S3-WROOM-1 (N16R8) + ST7789 240x320 + 10 nút keypad Symbian + thẻ SD.

Chuẩn keypad mới: **START(giữa)=OK, A=Back, MENU=Home, B=Delete,
OPTION=Context, SELECT=Game/T9 (giữ >600ms)**. GPIO không đổi ở mọi project.

## Project chính (đang phát triển)

| Project | Loại | Build | Keypad mới | Ghi chú |
|---|---|---|---|---|
| `E524546-OS/` | Lua OS (engine.*) trên ESP32-S3 | `pio run -e esp32-s3-st7789` | ✅ gốc chuẩn (Game/T9 + hold SELECT) | App Doodle 15 màn hình kiểu Pochita; smoke test `tools/run_smoke.py` |
| `pochita/` | C++ PochitaOS (15 app) | `pio run` (env `pochita_esp32s3`) | ✅ đã migrate toàn bộ call-site | Build SUCCESS 2026; `retro-go-port` sửa 6/10 GPIO |

## Project C++ đã migrate keypad

| Project | File sửa chính | Trạng thái verify |
|---|---|---|
| `nokiaos/` | `drivers/InputManager.*` (sửa bug tràn bit `1<<46`, thêm `isOK/isBack/isHome/isDelete/isContext`, hold SELECT), `core/AppManager.cpp`, 7 app `isStart()` | Brace-balance OK; chưa build (không có `platformio.ini`, dùng Arduino IDE) |
| `LegacyOS_E524546/` | `InputManager.h` (long-press 600ms + accessor), `LegacyOS.cpp`, `AppImplementations.cpp`, `TextEditor.h`, `MusicPlayer.h`, footer gợi ý phím | Brace-balance OK; chưa build |
| `Legacy32-Pixeler-UI/` | `src/config/input_config.h` (`BTN_OK/BACK`) | Config OK |
| `pixeler/examples/ESP32S3/Legacy-32-Classic/` | `src/config/input_config.h` (như trên) | Config OK |
| `pochitta_os_platformio/` | `board_config.h` (comment), `src/os.cpp` (START=activate, A=goBack) | Brace-balance OK |
| `bleai2/` | `bleai2.ino` (OK/back chuẩn, remote giữ vật lý) | Brace-balance OK |
| `Blender/` | `Blender.ino` (confirm→START, Back→A) | Brace-balance OK |
| `ESP32bluetothSpeaker/` | `bluetoothSpeaker.ino` (confirm→START, Back→A) | Brace-balance OK |
| `ESP32S3_NokiaOS/` | gộp 4 handler (OK→START, Back→A, Delete→B giữ trống, SELECT→mode stub) | Brace-balance OK |
| `filemanager/` | editor/main-loop/menu/scan/quét (scan→OPTION, save→START, exit→A) | Brace-balance OK |
| `gensing/` | confirm/attack → START | Brace-balance OK |
| `ModBoxOS/` | confirm → START (A/B giữ chức năng nhập liệu vật lý) | Brace-balance OK |
| `modbox/` | 8 file (OK→START, Back→A, xóa-chữ về đúng B, phím game/chuột giữ vật lý) | Brace-balance OK |
| `nokiaos/nokiaos.ino` | SELECT→START, back→A, zoom+→START | Brace-balance OK |
| `nokiaos/nokiaos_simple.ino` | thêm phím thiếu, OK→START, Back/Home | Brace-balance OK |
| `Remote/` | OK ra phím giữa, SELECT thành MODE (logic HID theo index giữ nguyên) | Brace-balance OK |
| `WaveshareRadioStream-main/` | OK=START, pause→START, back→A + **dời I2S_MCLK 8→38** (trùng KEY_OPTION) | Brace-balance OK |

## Thư mục lưu trữ / tham khảo (không build)

`*.zip`, `*.rar`, `bitmap/`, `legacyOs/`, `ModBoxOS/` (bản rar), `nokiaos/` cũ,
`pixeler/` gốc, `ESP32S3_NokiaOS/` zip, `LegacyOS_E524546.zip`, `filemanager.zip`,
`Blender/`, `gensing/` — giữ nguyên để tham khảo, không phải target build.

## Quy ước cho AI Agent

1. Đọc `docs/PROMPT.md` + `docs/SKILLS.md` trước khi đụng bất kỳ project nào.
2. Project nào thêm/sửa phím phải tuân bảng keypad chuẩn (xem PROMPT.md §4).
3. Thứ tự verify: review diff → brace-balance → build (`pio run` nếu có
   `platformio.ini`) → nạp mạch test tay. Không tuyên bố chạy được khi chưa test.
