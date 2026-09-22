# Arduino Firmware Simulator (ESP_Browser)

Giả lập **firmware Arduino** của `ESP_Browser` trên Windows — chạy **thật** `src/*.cpp` + `include/` thông qua shim (`sim/shims/`, `sim_arduino.*`).

Không phải mock UI: `setup()` / `loop()` / keypad / WML / HTTP / store đều là code firmware.

## Build & chạy

```powershell
cd sim
powershell -File build_sim.ps1
.\espbrowser_sim.exe              # GUI: cửa sổ 240×320 + menu File/Help
.\espbrowser_sim.exe --test       # self-test wizard WiFi + browse (mock HTTP)
.\espbrowser_sim.exe --live       # HTTP qua host TCP bridge (Winsock)
.\espbrowser_sim.exe --fetch URL  # mở URL thật để thử firmware
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
| **About** | Browser V1 (DMAX port) |

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
