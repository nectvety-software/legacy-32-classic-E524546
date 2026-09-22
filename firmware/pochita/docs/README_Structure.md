# PochitaOS - ESP32 Handheld Console

## Cấu trúc Project / Project Structure

```
pochita/
├── component/          # Các thành phần chính
│   ├── Config.h        # Cấu hình chân, pins
│   ├── FileManager.cpp # Quản lý file trên SD
│   ├── Keyboard.cpp    # Bàn phím ảo
│   ├── ButtonManager  # Quản lý nút bấm
│   ├── Themes.cpp     # Giao diện theme
│   └── UIManager.cpp  # Quản lý UI
│
├── src/                # Ứng dụng / Apps
│   ├── notes.h         # App ghi chú
│   ├── lua_interpreter.h # Lua interpreter
│   ├── retro_go.h      # Retro game support
│   ├── router.h       # App WiFi
│   ├── blu.h          # App Bluetooth
│   ├── shell.h        # Terminal
│   └── settings.h     # Cài đặt
│
├── asset/              # Icons, bitmaps
│
├── docs/               # Tài liệu / Documentation
│
└── pochita.ino         # Main file
```

## Các tính năng / Features

### 📝 Notes App
- Soạn thảo văn bản
- Menu: New, Open, Save, Save As, Close
- Keyboard hỗ trợ ký tự đặc biệt
- Lưu vào SD card `/notes/`

### 🎮 Retro Gaming
Hỗ trợ các hệ máy:
- Nintendo: NES, SNES, GB, GBC, GBA
- Sega: SG-1000, Master System, Mega Drive, Game Gear
- ColecoVision, PC Engine, Atari Lynx, DOOM

### 📁 File Manager
- Duyệt file trên SD card
- Menu tùy chọn: Open, Run, Copy, Cut, Rename, Delete
- Hỗ trợ nhiều loại file

### 🔥 Lua Interpreter
- Chạy script .lua
- Hỗ trợ: biến, toán tử, print, vòng lặp

## SD Card Pinout

| Chức năng | Chân ESP32 |
|-----------|------------|
| SD_CS      | GPIO 10    |
| SD_MOSI    | GPIO 11    |
| SD_SCLK    | GPIO 13    |
| SD_MISO    | GPIO 9     |

## Buttons / Nút bấm

| Nút | Chân | Chức năng |
|-----|------|-----------|
| UP | GPIO 7 | Lên |
| DOWN | GPIO 46 | Xuống |
| MENU | GPIO 18 | Menu |
| SELECT | GPIO 16 | Chọn |
| A | GPIO 15 | Xác nhận A |
| B | GPIO 5 | Quay lại B |
