# SKILLS.md — Kỹ năng và checklist phát triển QEAPP Studio

Mọi agent bắt đầu bằng `PROMPT.md` và file này; kích hoạt thêm mục kỹ năng phù hợp. Nguồn chuẩn: **code hiện tại** của firmware có ưu tiên cao hơn draft API trong bộ tài liệu.

## Skill 1 — Project scaffolding

**Trigger:** New Project, template, app/game mới. **Input:** project ID, loại, phiên bản, target firmware, quyền dự kiến. **Các bước:**

1. Chọn mode: `CURRENT_QEAPP2`, `BUILTIN_NATIVE_HANDLER` hoặc `PROPOSED_SCRIPT_RUNTIME`.
2. Hiện tại dùng `qeapp.project.json` của Studio làm metadata *trên PC*, KHÔNG ghi JSON này vào QEAPP/2: signer chuyển sang ASCII manifest whitelist.
3. Tạo `assets/`, `src/` hoặc `content.txt`, `tests/`, `README.md`, `CHANGELOG.md`; giữ `dist/` và khóa bí mật ngoài Git.
4. Chạy `python tools/qstudio.py validate <project>` và test dự án. Mẫu `snake-lua-proposal` luôn báo chưa có runtime.

**PASS:** ID, tên, version đúng hợp đồng; file asset có thực; không có symlink/path traversal; phân biệt mode rõ ràng.

## Skill 2 — Signed QEAPP build & install

**Trigger:** build, export, ký, cài, update. **Các bước:**

1. Đọc format thực trong `docs/QEAPP_V15_SIGNING.md` và script gốc; `pip install cryptography Pillow` trên PC.
2. Tạo private key P-256 **bên ngoài** repo, ghi public key và key-id phù hợp vào firmware bằng `tools/qeapp_keys.py` rồi rebuild/reflash firmware. Backup key riêng.
3. Gọi `qstudio.py build ... --sign-key PATH --firmware-root PATH`; verify `qstudio.py inspect --public-key ... --key-id ...`.
4. Test sửa 1 byte → chữ ký không hợp lệ; test downgrade, same-version, mất SD và backup recovery. Copy gói tới `/System/Apps/Inbox/`, mở App Installer và xác nhận.

**FAIL bắt buộc:** khác key-id, private/public mismatch, manifest có key ngoài whitelist, payload >256KiB, icon không đúng 32x32, unsigned QEAPP/1. **Không bypass verifier** để "sửa" lỗi cài đặt.

## Skill 3 — Game loop, UI, phím cứng

**Trigger:** game 2D, app có tương tác, màn hình 240x320. Lưu API giả lập host và adapter ESP32 trong lớp platform riêng. Dự kiến hàm `init`, `update(dt_ms)`, `render`, `key`, `pause`, `resume`, `shutdown`. Giới hạn fixed-step, không block input, clamp dt. Đặt softkey/statusbar dưới quyền OS; test MENU/Home, A/Back, START/OK, SELECT long-press >600ms và D-Pad repeat. Render RGB565 và kiểm tra clipping tại biên 0..239, 0..319.

**PASS:** 240x320 golden image, 60s stress test trên host, input determinism, không mất phím/đóng băng launcher; thiết bị thật là gate riêng.

## Skill 4 — Asset pipeline & bộ nhớ

**Trigger:** spritesheet, fonts, tilemap, âm thanh. Dùng pipeline offline PNG → RGB565 hoặc palette/RLE theo asset, không decode ảnh lớn vào internal SRAM. Đo dung lượng `.qeapp` và bitmap giải nén; xếp tài nguyên trên SD theo thư mục sandbox tương lai. Đừng tự nới payload firmware mà không đổi format/version và test toàn bộ parser.

**PASS:** lossless RGB565 round-trip tại target, palette range/chunk bounds, không OOM trong test lâu; số đo trên PC không phải số đo ESP32.

## Skill 5 — Runtime sandbox (chỉ roadmap)

**Trigger:** yêu cầu "biên dịch game/app Lua thành `.qeapp` chạy độc lập". Tạo host interpreter + platform adapter + capability broker, không sửa manifest sang `type=lua` trên QEAPP/2. Thiết kế hợp đồng ABI mới/versioned, kiểm tra bytecode/source khi load, hạn mức instruction/memory, thư mục data theo ID và không dùng `loadfile/dofile/require` tự do từ SD.

**PASS:** host unit tests, fuzz malformed bytecode/resources, watchdog, host screenshots; **chưa PASS phát hành** cho đến khi firmware parser/loader/runtime/installer có E2E trên thiết bị thật.

## Skill 6 — IDE + debugger

**Trigger:** UI IDE tương tự LuaS30-IDE. Qt/PySide6 trên PC chỉ là giao diện; gọi cùng `qstudio` CLI cho Build/Inspect/Test, không giữ private key trong workspace JSON. Có Explorer, editor, asset preview, Build log, Error diagnostics, simulator controls, screenshots và device Serial Monitor. **Stop** phải hủy process thử nghiệm; các command đầy quyền cần xác nhận.

**PASS:** workflow tạo `text-app` → validate → signed build → verify, với mock signing key từ test tạm; nút Build trả đúng CLI exit code; key không bị ghi log.

## Skill 7 — Regression và báo cáo

- Bộ host: parser, hashes, P-256, icon/endian, malicious payload, signature mismatch, power-loss stage recovery, file sizes, game loop, render golden, resource leak; suite `verify_v242.py` trong firmware.
- Bộ ESP32 (cần board): cross-build PlatformIO, 115200 serial, SD FAT mounting, WiFi/NTP/TLS, install/update/reboot, theme interaction, display 240x320, D-Pad, 10-minute runtime soak, FPS/heap low-water, screenshots chụp từ máy hoặc camera.
- Báo cáo bắt buộc ghi `host simulated` hay `hardware verified`, SHA artifact, commit SHA, trust key-id (không có private key), tool versions và giới hạn.

## Skill 8 — M1 portable host game/app core (ĐÃ CÓ từ v0.2)

**Trigger:** tạo game/app C++ host demo, kiểm tra loop/input/renderer hoặc đổi API core.

1. Đọc `docs/07_M1_HOST_ENGINE.md`, `engine/include/qe/runtime.h`, `tests/golden/host_sha256.json`.
2. Dùng `qe::App` (`init/update/draw/onKey/pause/resume/shutdown`) và `qe::Platform`; không import Arduino/TFT_eSPI trong engine core.
3. Phím `Menu` / `SelectLong` chỉ đi đến `reservedSystemKey`; không giải phóng quyền này cho game.
4. Giữ ring input 32 event, fixed-step 50ms và capped catch-up; không cấp phát trong `Runtime::tick()`.
5. Chạy `python3 tools/qstudio.py simulate --demo snake --scenario playing -o screenshot.png` và `... --demo hello`; chạy `python3 -m unittest discover -s tests -v`.
6. Golden mismatch là **FAIL** cho đến khi ảnh được người phát triển xem và cập nhật golden có giải thích.

**Không được hứa:** preview host nghĩa là app `.qeapp` native được firmware cài và chạy; `QEAPP/2` chỉ hỗ trợ web/text có chữ ký.

## Skill 9 — M2 PySide6 IDE (ĐÃ CODE trong v0.3, kiểm thử GUI cần PySide6)

**Kích hoạt:** thiết kế Explorer/Editor/Build/Preview/Stop/New Project. Quy trình:

1. Đọc `docs/09_M2_DESKTOP_CORE.md`, `studio/core/{workspace,config,commands,jobs}.py`, `studio/gui/window.py`.
2. Tất cả file-access qua `Workspace`, chỉ nhận đường dẫn tương đối; không sửa symlink, file khóa, build artifacts; conflict khi nội dung trên đĩa đổi là lỗi bắt buộc.
3. Đảm bảo giao diện ghi rõ preview là **HOST replay**, không phải thiết bị/firmware; signed build chỉ `text`/`web`.
4. Async command luôn gọi `commands.*` → `JobRunner.execute(argv)`; khi nhấn Stop, kill process tree và báo lỗi/timeout rõ ràng; không lưu private key vào config hoặc log.
5. Test core trước: `python -m unittest discover -s studio/tests -v`; bắt buộc đọc `skipped` và KHÔNG coi test Qt bị skip là đã chạy.
6. Test Qt thật: cài `requirements-studio.txt`; chạy `QT_QPA_PLATFORM=offscreen python -m unittest discover -s studio/tests -v` và mở ứng dụng trên desktop Windows để thử New/Open/Edit/Build/Preview/Stop.
7. Test QEAPP/2 thực với firmware checkout và khóa tạm riêng: `QEAPP_FIRMWARE_ROOT=/path/to/VQEAF-OS python -m unittest discover -s tests -v`.

**Chưa hỗ trợ ở M2:** trình mô phỏng firmware thực, breakpoint/debug live, Game Engine Lua trên ESP32, `.qeapp` native thực thi tùy ý, tự động flash board. Giữ các nhãn này tách biệt trong GUI và tài liệu.

## v0.5: Pixel Sprite beta contract

- Đọc `docs/13_SPRITE_AND_LUA_WORKFLOW_V05.md` và `tests/test_sprite_v05.py` trước mọi thay đổi sprite.
- Mã `runtime/src/QeLuaRuntime.cpp` phải mirror đúng trong `firmware/VQEAF-OS/src/lua/`; sửa một bên thì chạy test đồng bộ.
- `engine.blit1` chỉ nhận mask 1-bit row-major/MSB-first, kích thước tối đa 32×32, color RGB565; clipping vùng game 240×270, budget 512 draw calls. KHÔNG tạo API chưa có hoặc mở đường đọc SD/file từ Lua.
- IDE PNG preview dùng `Workspace.read_png_preview()` chỉ với PNG nội bộ dự án, giới hạn 1 MiB/512×512, chặn symlink/traversal. PNG→Lua chuyển đổi CHỈ chạy trên PC.
- Với release beta: chạy `python tools/verify_v05.py`; ghi riêng GUI SKIPPED và PlatformIO/hardware NOT_RUN. Chữ ký hợp lệ không đồng nghĩa app chạy được trên stock firmware.
