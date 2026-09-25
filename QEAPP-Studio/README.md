# QEAPP Studio v0.5 — PySide6 IDE + Lua 5.4 beta + Pixel Sprite

Môi trường viết game/app Lua cho **VQEAF OS**: editor PySide6, công cụ build/inspect signed `.qeapp`, trình thử game Lua trên PC, và firmware **beta** ESP32-S3 N16R8 dùng chung mã runtime C++. Bản v0.5 giữ recorder/input guards của v0.4.1 và thêm `engine.blit1` (mask 1-bit), PNG→Lua converter, template sprite game và PNG preview an toàn trong Explorer.

**Phân biệt mức độ:** `type=web`/`type=text` được firmware VQEAF OS v2.4.2 hỗ trợ. `type=lua` **chỉ** dành cho firmware thử nghiệm `vqeaf_lua_beta` dùng khóa ký độc lập; chưa build PlatformIO, nạp hay thử nghiệm runtime Lua trên thiết bị thật. Ảnh mô phỏng PC không phải ảnh chụp ST7789. Đây không phải giả lập Symbian/MRE.

## Cấu trúc

```text
QEAPP_Studio_v0.4.1/
├── PROMPT.md / SKILLS.md                     # Hợp đồng cho AI Agent
├── docs/{01..12}*.md                         # ABI, bảo mật, build/kiểm thử
├── run_studio.py / run_studio.bat             # Desktop IDE PySide6
├── studio/
│   ├── core/{workspace,jobs,commands,config,replays}.py
│   ├── gui/window.py                         # Explorer, Lua editor, recorder, Build/Run
│   └── tests/                                # Core + tùy chọn GUI offscreen
├── engine/                                   # M1 native C++ host renderer
├── runtime/
│   ├── include/QeLuaRuntime.h                # API portable Lua 5.4
│   ├── src/QeLuaRuntime.cpp                  # MỘT source dùng chung host & firmware
│   └── host/{qe_lua_host.cpp,compat/,tests/}
├── firmware/VQEAF-OS/                        # v2.4.2 + opt-in Lua beta overlay
│   ├── src/lua/                              # Mirror của runtime/src (feature-gated)
│   ├── lib/VqeafLua54/                       # Cài upstream Lua bằng bootstrap_lua.py
│   └── platformio.ini                        # env:vqeaf_os và env:vqeaf_lua_beta
├── projects/{lua-hello,lua-snake,text-notes,web-bookmark}/
├── tools/{qstudio,bootstrap_lua,build_lua_host,lua_preview,...}.py
└── tests/                                    # Regression/signature/security
```

## Chạy trên Windows

Yêu cầu Python 3.11+, Windows 10/11 x64, PySide6 cho GUI, GCC/MinGW/Clang cho host nếu muốn tự biên dịch VM, và `cryptography` để ký gói.

```powershell
py -3 -m pip install -r requirements-studio.txt
py -3 run_studio.py
```

Trong IDE: `File → New Lua App (Beta)` hoặc `New Pixel Snake Lua Game (Beta)`. Chỉnh `main.lua`; dùng khu **GAMEPAD REPLAY** để thêm sự kiện `start/up/down/left/right/option` tại frame chỉ định, nhấn **Save Replay JSON**, sau đó **F8**. Preview 240×320 là giả lập PC. **F6** validate; **F7** build signed QEAPP/2 sau khi đã tự tạo khóa ký beta và chọn đường dẫn firmware.

## Xây Lua host runtime

```powershell
py -3 tools/bootstrap_lua.py
py -3 tools/build_lua_host.py
py -3 tools/qstudio.py lua-preview projects/lua-snake --frames 8 --replay tests/input_replay.json -o build/snake-lua.png
```

`bootstrap_lua.py` tải Lua 5.4.8 chính thức, xác minh **SHA-256 ghim cứng** trước khi chép mã nguồn và LICENSE vào `firmware/VQEAF-OS/lib/VqeafLua54`. Offline: `--archive C:\Downloads\lua-5.4.8.tar.gz`. Chưa vendor Lua nguồn vào ZIP: cần bootstrap thành công trên máy phát triển. Trên Linux có `liblua5.4.so.0`, `build_lua_host.py --system-lua` là đường thử nghiệm PC, **không** thay cho Lua upstream khi release firmware.

## Tạo khóa và build beta firmware (chưa xác nhận trên board)

```powershell
py -3 tools/provision_lua_beta_key.py --private C:\SecureKeys\vqeaf-lua-beta-private.pem
pio run -d firmware\VQEAF-OS -e vqeaf_lua_beta
```

Chỉ public key được thêm vào firmware beta; **không bao giờ đưa private PEM vào repository, thẻ SD hoặc ảnh chụp/log**. Stock firmware không được đổi trust key. Trong beta profile, publisher beta là trust anchor duy nhất: những gói `text/web` cũ ký bằng key production có thể bị từ chối. Sao lưu khóa beta nếu còn cần cài lại gói đã phát hành.

Đóng gói `.qeapp` Lua sau khi đã provision key:

```powershell
py -3 tools/qstudio.py build projects/lua-snake --experimental-lua --firmware-root firmware/VQEAF-OS --sign-key C:\SecureKeys\vqeaf-lua-beta-private.pem --key-id 0x544c5541 -o dist/snake-lua.qeapp
py -3 tools/qstudio.py inspect dist/snake-lua.qeapp --public-key C:\SecureKeys\vqeaf-lua-beta-private_public.pem --key-id 0x544c5541
```

Chỉ cài trên firmware **beta đã nạp đúng public key**: chép vào microSD `/System/Apps/Inbox/`, mở App Installer; không tắt signature check để cài. `type=lua` chạy như text source giới hạn 64KiB, không được cấp quyền truy cập tuỳ ý file/network/SD. Firmware `vqeaf_os` bình thường vẫn từ chối ứng dụng Lua.

## Kiểm thử

```powershell
py -3 -m unittest discover -s studio/tests -v
py -3 -m unittest discover -s tests -v
py -3 tools/test_lua_beta_host.py
py -3 firmware/VQEAF-OS/tools/verify_v242.py
```

Đặt `QEAPP_FIRMWARE_ROOT=firmware/VQEAF-OS` (theo cú pháp shell đang dùng) nếu muốn suite chạy kiểm thử signer thực. Test GUI offscreen chỉ chạy khi máy đã cài PySide6; báo SKIPPED **không** được gọi PASS. Kết quả trong bản phát hành: `docs/11_VERIFICATION_V041.md`.

**Đọc tiếp:** `docs/10_LUA_BETA_ARCHITECTURE.md`, `docs/12_INPUT_REPLAY.md`, `PROMPT.md`, `SKILLS.md`.

## Tính năng v0.5: pixel sprite đơn sắc

- `File → New Sprite Lua Game (Beta)` hoặc `python tools/qstudio.py init --template lua-sprite --id new_game --name "New Game" -o new-game`.
- PNG 1..32 pixel/chiều → `tools/pixel_sprite.py` → Lua source; Lua VM có `engine.blit1`. Double-click PNG trong Explorer để preview (không mở binary bằng editor UTF-8).
- Tài liệu: `docs/13_SPRITE_AND_LUA_WORKFLOW_V05.md`.
- Host gate một lệnh: `python tools/verify_v05.py --system-lua` (Linux chẩn đoán) hoặc bootstrap upstream Lua rồi `python tools/verify_v05.py --require-qt`. Kết quả trong `build/reports/v05/`.
- Đường firmware beta vẫn là bản thử nghiệm; *không* cam kết chạy trên ESP32-S3 khi chưa có build PlatformIO và log thật.
