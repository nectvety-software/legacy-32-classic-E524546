# Arduino Firmware Simulator (Qeafbrowser)

Giả lập **firmware Arduino** của `Qeafbrowser` trên Windows — chạy **thật** `src/*.cpp` + `include/` thông qua shim (`sim/shims/`, `sim_arduino.*`).

Không phải mock UI: `setup()` / `loop()` / keypad / WML / HTTP / store đều là code firmware.

## Build & chạy

```powershell
cd sim
powershell -File build_sim.ps1
.\qeafbrowser_sim.exe              # GUI: cửa sổ 240×320 + menu File/Help
.\qeafbrowser_sim.exe --test       # self-test wizard WiFi + browse (mock HTTP)
.\qeafbrowser_sim.exe --live       # HTTP qua host TCP bridge (Winsock)
.\qeafbrowser_sim.exe --fetch URL  # mở URL thật để thử firmware
```

## Build harness headless trên Windows (MinGW/MSYS2)

Các harness `*_main.cpp` biên dịch trực tiếp `src/*.cpp` (không phải mock UI) và
xuất framebuffer 240×320 ra BMP. Trên Windows cần thêm `-D_POSIX_THREAD_SAFE_FUNCTIONS=1`
và cả `C:\msys64\usr\bin` trong `PATH` (collect2 cần nó):

```bash
export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
COMMON="-O2 -std=gnu++17 -static -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -Isim/shims -Iinclude"
SRC="src/main.cpp src/wml.cpp src/http.cpp src/store.cpp src/launcher.cpp"

# theme Symbian S60 (xem README chính, mục “Symbian S60 interface”)
g++ $COMMON -o sim/qb_s60.exe sim/s60_main.cpp sim/sim_arduino.cpp $SRC -lws2_32
./sim/qb_s60.exe            # -> sim/s60_out/*.bmp

# launcher Retro-Go (theme JSON + tabs + art)
g++ $COMMON -o sim/qb_launcher.exe sim/launcher_main.cpp sim/sim_arduino.cpp $SRC -lws2_32
./sim/qb_launcher.exe       # -> sim/launcher_out/*.bmp

# các regression cũ (chạy từ thư mục gốc dự án)
g++ $COMMON -o sim/qb_headless.exe sim/headless_main.cpp sim/sim_arduino.cpp $SRC -lws2_32
./sim/qb_headless.exe
```

`sim/inspect_bmp.py` in layout/màu của một BMP ra terminal (bands/ascii/zoom) khi
không mở được cửa sổ ảnh:

```bash
python sim/inspect_bmp.py sim/s60_out/02_speed_dial_list.bmp bands
```

## Menu (chọn nguồn chạy thử)

| File | Ý nghĩa |
|---|---|
| **Open URL** | nạp URL vào firmware (`go_url`) |
| **Open Local Page…** | chọn file HTML → `file://` |
| **Source: Mock HTTP** | mock server (test ổn định) |
| **Source: Live TCP** | socket thật trên PC (host bridge) |
| **Exit** | thoát |

| Help | Ý nghĩa |
|---|---|
| **Settings** | UA / encoding / HTTPS / nguồn |
| **About** | Qeafbrowser 1.7 |

## Shim giả lập phần cứng

| Thiết bị thật | Shim |
|---|---|
| ESP32-S3 + Arduino | `sim_arduino.h/cpp`, `shims/Arduino.h` |
| ST7789 240×320 | `LGFX` framebuffer → BMP + cửa sổ Win32 |
| Keypad 10 nút | phím PC (mũi tên/Enter/Esc/Home/Alt/…) |
| WiFi | mock SSID + host TCP bridge |
| SD / LittleFS | `sim_sd/`, `sim_lfs/` |
| Serial | stdout + `sim_log.txt` |

## Input

- **Bàn phím ảo D-pad** (URL / Search / WiFi password)
- **PC gõ thẳng** vào ô nhập (host bridge)
- OPTIONS popup: Bmrk / SpDial / Zoom / Mouse / Sett / Help
