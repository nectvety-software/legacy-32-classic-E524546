# legacy-32-classic-E524546

Tuyển tập firmware và hệ điều hành cho **thiết bị cầm tay tự chế dùng chung một
cấu hình phần cứng**: ESP32-S3-WROOM-1 (N16R8, Flash 16MB + PSRAM 8MB),
màn hình TFT 2" ST7789 240x320, **10 nút keypad Symbian 3x3 + SELECT**, thẻ SD.

## Chuẩn keypad (thống nhất toàn repo)

```text
[MENU] Home | [UP] len   | [A] Back
[LEFT] trai | [START] OK | [RIGHT] phai
[OPTION] tuy chon | [DOWN] xuong | [B] xoa
         [SELECT] giu >600ms: doi Game/T9
```

Chi tiết chân GPIO: `DESIGN.txt`, `docs/system_prompt_phan_cung.md`.

## Các phần mềm

| Project | Mô tả |
|---|---|
| `E524546-OS/` | **Chính**: HĐH Lua (lõi LuaS30) — Doodle OS 15 app, nạp app qua LittleFS |
| `pochita/` | PochitaOS C++: WiFi, Files, Bluetooth, Terminal, game, NES... |
| `nokiaos/` | NokiaOS + `nokiaos_simple.ino` test màn hình/phím |
| `LegacyOS_E524546/` | LegacyOS: drawer, editor, music, snake, radio |
| `Legacy32-Pixeler-UI/`, `pixeler/` | UI framework Pixeler + ví dụ Legacy-32-Classic |
| `pochitta_os_platformio/` | PochittaOS bản PlatformIO |
| `ModBoxOS/`, `modbox/` | ModBox OS và module BLE/WiFi/web/cmd |
| `filemanager/`, `Blender/`, `bleai2/`, `gensing/` | App độc lập: quản lý file, vẽ 3D, BLE AI, game |
| `Remote/`, `WaveshareRadioStream-main/` | Điều khiển BLE HID, radio WiFi |
| `ESP32S3_NokiaOS/`, `ESP32bluetothSpeaker/` | OS/app thử nghiệm khác |

Trạng thái migrate keypad + lệnh build từng project: `docs/SOFTWARE.md`.

## Tài liệu cho AI Agent / dev mới

- `docs/system_prompt_phan_cung.md` — GPIO + luật firmware (nguồn chân lý)
- `docs/PROMPT.md` — master prompt: preflight, luật cứng, lệnh verify
- `docs/SKILLS.md` — 6 kỹ năng: keypad, màn hình, SD, app Lua, UI doodle, build
- `docs/SOFTWARE.md` — tồn kho phần mềm

## Build nhanh

```powershell
# E524546-OS (Lua): build firmware + filesystem
cd E524546-OS
pio run -e esp32-s3-st7789
pio run -e esp32-s3-st7789 -t uploadfs   # nap app Lua
python tools\run_smoke.py                # smoke test PC

# PochitaOS (C++)
cd pochita
pio run
```
