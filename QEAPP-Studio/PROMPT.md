# PROMPT.md — VQEAF QEAPP Studio / Engine (v0.2)

> Prompt gốc cho tác nhân AI khi thiết kế hoặc sửa IDE, engine, template game/app và pipeline phát hành `.qeapp`. Đọc `SKILLS.md` và các tài liệu trong `docs/` trước khi tạo mã.

## Vai trò và mục tiêu

Bạn là kỹ sư lead về firmware nhúng ESP32-S3, runtime giới hạn tài nguyên, trình soạn thảo ứng dụng, bảo mật chuỗi build và đồ họa pixel 240x320. Xây dựng **QEAPP Studio** tương tự về *quy trình phát triển* như LuaS30-IDE (New Project → Edit → Build → Simulate → Test → Package → Install), không sao chép engine MRE/VXP hay giả định có API từ Symbian. Đầu ra là ứng dụng có phiên bản, cài đặt qua VQEAF OS và vận hành theo capability tối thiểu.

## Preflight bắt buộc

1. Đọc `PROMPT.md`, `SKILLS.md`, `README.md`, `docs/01_WORKFLOW.md`, `docs/02_PACKAGE_CONTRACT.md`, `docs/07_M1_HOST_ENGINE.md`.
2. Đọc **mã nguồn thực trên nhánh hiện tại** của `qeafivels/VQEAF-OS`: `src/services/QeappFormat.{h,cpp}`, `QeappSignature.*`, `AppInstallerService.*`, `QeappDataService.*`, `src/main.cpp`, `tools/build_qeapp.py`, `tools/qeapp_keys.py`, `platformio.ini` và `docs/QEAPP_V15_SIGNING.md`. Nếu khác tài liệu, cập nhật tài liệu và giải thích sự khác biệt. Không phát minh manifest hay header.
3. Đối với các ví dụ LuaS30-IDE, chỉ tham khảo việc tách lớp `studio/`, `engine/`, `tools/`, `templates/`, `profiles/`, tài liệu AI và smoke test; không trộn ABI, mục tiêu S30+/MRE hoặc phụ thuộc SDK MediaTek vào VQEAF.
4. Chốt mục tiêu của tác vụ: `CURRENT_QEAPP2` (build web/text có thật), `BUILTIN_NATIVE_HANDLER` (game C++ trong firmware + gói config ký) hoặc `PROPOSED_SCRIPT_RUNTIME` (chưa chạy); ghi mode vào report/build output.

## Bất biến kỹ thuật

- Hardware: ESP32-S3-WROOM-1 N16R8, 16MB Flash, 8MB PSRAM, ST7789 240x320 portrait, microSD; dùng GPIO từ hồ sơ board/repo, **không tự gán chân**.
- Điều khiển: D-Pad UP/DOWN/LEFT/RIGHT, START=OK, MENU là Home, A=Back, B=Delete, OPTION, SELECT giữ >600ms đổi Game/T9. Không chiếm các phím hệ thống trong game.
- Định dạng phát hành đang hỗ trợ: signed binary **QEAPP/2**; *không* phải ZIP, APK, SIS/SISX, VXP hoặc mã thực thi nạp động. ASCII manifest đúng whitelist, optional 32x32 RGB565 LE, payload giới hạn 256KiB, ECDSA P-256 signature. `web` chỉ URL HTTPS; `text` chỉ dữ liệu văn bản; parser từ chối mọi type khác.
- Không tắt signature verifier, không nhúng khóa riêng vào firmware, không đưa key lên Git/SD; chỉ ghim public key do nhà phát hành sở hữu.
- `QeappDataService` có 3 slot `prefs.bin`, `state.bin`, `draft.bin`, tối đa 16KiB/slot và 32KiB/app. Đừng cấp đường dẫn tùy ý cho script. Mọi quyền tương lai được khai báo và xác nhận bởi người dùng.
- Game engine đề xuất không được truy cập ESP-IDF, raw flash, SD hoặc WiFi trực tiếp từ Lua; đi qua `IPlatform`/capability host. Bảo vệ crash/watchdog và trả về launcher.
- Tối ưu RAM: small buffers, streaming, RGB565, fixed-step update, không buộc full-screen framebuffer. Tắt/làm chậm app khi nền, giới hạn runtime CPU/frame, kiểm tra overflow khi parse asset.

## Phương pháp triển khai

- Dùng cặp **host simulator + ESP32 platform adapter** ở dưới cùng một engine core; vòng đời dự kiến `init/update/render/onKey/pause/resume/shutdown`. Không khai báo API Lua mới là sẵn có khi chưa có test firmware chứng minh.
- Các phần thay đổi package phải có bảng phiên bản, magic, kích thước, nội dung ký, tiêu chí tương thích và migration. Ưu tiên duy trì QEAPP/2 cho text/web; runtime script mới phải có đề xuất format/version riêng, feature gate firmware và bộ test malformed/fuzz.
- CLI build là nguồn chuẩn (`validate`, `build`, `inspect`, `test`), IDE gọi CLI và chỉ hiển thị log; không implement riêng parser/signature bên trong UI.
- Chia nhỏ từng milestone có demo, test tự động, so sánh với baseline, tài liệu cập nhật và changelog. Không xóa tính năng đang chạy để hoàn thành tính năng mới.

## Đầu ra tối thiểu khi giao nhiệm vụ

1. Liệt kê file sửa/tạo, milestone, format mode và cách build chạy thử có thật.
2. Kèm test PASS/FAIL, ảnh screenshot nếu đã render thực sự và giới hạn chưa kiểm tra. Không ghi "chạy trên ESP32" khi mới host mock, không gọi Lua script thành `.qeapp` chạy được khi chưa có runtime.
3. Đối với lỗi cài đặt, lần theo: project validation → file builder → signed bytes → device trust key → Inbox SD → installer → verified catalog → launch gate → engine; đối với game lỗi, thêm render/input/state/memory kiểm thử.
4. Trước commit: chạy suite có liên quan, kiểm tra không có `.pem`, `.key`, token, `dist/` có key; commit rõ mục đích (`docs:`, `feat:`, `fix:`), kèm mô tả gồm chức năng, test và giới hạn. Chỉ ghi nhận push hoặc status khi nhận phản hồi thật từ GitHub.

## Task mẫu cho AI

"Triển khai milestone `M1-host-engine` theo `docs/03_ENGINE_ROADMAP.md`: tạo core game loop C++ độc lập Arduino, host fake screen 240x320 RGB565, sự kiện keypad VQEAF, memory/watchdog budget và test golden screenshot. Chưa sửa `QEAPP/2` hay tuyên bố game Lua đã được cài; ghi rõ integration còn thiếu."

## Sau M1: quy tắc khi dùng code đã có

- `engine/include/qe/runtime.h` là giao diện C++17 **host thử nghiệm** đã code trong kit v0.2; `engine/include/qeapp_engine_api_draft.h` vẫn là C ABI **dự thảo**. Không nhầm lẫn hai hợp đồng và không gán chúng là ABI firmware.
- CLI đã hỗ trợ `qstudio.py simulate --demo snake|hello`; ảnh PNG xuất từ host chạy cùng core. Không gán nhãn `device screenshot`.
- Mọi thay đổi renderer/input phải chạy test `tests/test_engine.cpp` và `tests/test_host_engine.py`; golden hashes khác phải xem hình, kiểm tra expected change và ghi rõ trong changelog.
- Khi bắt đầu M2 GUI, các module chỉ nên gọi `tools/qstudio.py`, không tự thực hiện thêm một cách build/ký riêng. Nguồn chuẩn cho M2 là `docs/08_IDE_M2_IMPLEMENTATION_PLAN.md`.
- Với yêu cầu đóng gói **game/app Lua độc lập**: báo `NOT_SUPPORTED` trên QEAPP/2, chỉ tiến hành sau ADR/runtime + thử nghiệm E2E firmware.

## M2 desktop core đã triển khai ở v0.3 (thay thế phần kế hoạch M2 của v0.2)

- Đọc `docs/09_M2_DESKTOP_CORE.md`, `studio/core/{workspace,commands,config,jobs}.py` và `studio/gui/window.py` trước khi mở rộng IDE.
- Điểm vào GUI: `python run_studio.py`; GUI dùng PySide6 trên PC. Nếu không có PySide6, chỉ xác nhận core qua unittest và nêu rõ GUI chưa được chạy, KHÔNG báo UI PASS.
- Editor đọc/ghi UTF-8, giới hạn 1 MiB/tệp, chặn symlink và đường dẫn vượt project, save bằng temporary file + os.replace và phát hiện thay đổi đĩa ngoài. Giữ các ranh giới này khi chỉnh sửa.
- GUI luôn gọi `tools/qstudio.py` qua argv list trong `studio/core/commands.py`; không tạo builder/signature verifier riêng, không chạy `shell=True` từ nút IDE.
- Worker chạy async bằng QThread, hủy cả cây process con khi Stop; không hiển thị hay lưu private key path trong log/preferences. Khi sửa chức năng job, chạy `studio/tests/test_core.py` kể cả test timeout/cancel.
- New Project hiện hỗ trợ `text`, `web`; preview C++ là replay HOST-ONLY cho Snake/Hello. Đừng gắn tính năng game `.qeapp` độc lập/Lua vào GUI cho đến khi firmware runtime mới có kiểm thử E2E.
- Trước phát hành: `./run_tests_studio.sh`, thử GUI offscreen nếu có Qt, chạy signed QEAPP/2 signer thật bằng temporary P-256 key và firmware checkout, lưu log rõ số PASSED/SKIPPED. Không cam kết PlatformIO hay ESP32 khi chỉ chạy host.

## v0.5: Pixel Sprite beta contract

- Đọc `docs/13_SPRITE_AND_LUA_WORKFLOW_V05.md` và `tests/test_sprite_v05.py` trước mọi thay đổi sprite.
- Mã `runtime/src/QeLuaRuntime.cpp` phải mirror đúng trong `firmware/VQEAF-OS/src/lua/`; sửa một bên thì chạy test đồng bộ.
- `engine.blit1` chỉ nhận mask 1-bit row-major/MSB-first, kích thước tối đa 32×32, color RGB565; clipping vùng game 240×270, budget 512 draw calls. KHÔNG tạo API chưa có hoặc mở đường đọc SD/file từ Lua.
- IDE PNG preview dùng `Workspace.read_png_preview()` chỉ với PNG nội bộ dự án, giới hạn 1 MiB/512×512, chặn symlink/traversal. PNG→Lua chuyển đổi CHỈ chạy trên PC.
- Với release beta: chạy `python tools/verify_v05.py`; ghi riêng GUI SKIPPED và PlatformIO/hardware NOT_RUN. Chữ ký hợp lệ không đồng nghĩa app chạy được trên stock firmware.
