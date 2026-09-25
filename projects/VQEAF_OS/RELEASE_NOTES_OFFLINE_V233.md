# VQEAF OS v2.3.3 — Offline Build Tools Update

**Phạm vi**: Chỉ bổ sung công cụ build offline và tài liệu; không thay đổi file `.cpp`/`.h`, layout Home/Menu, asset icon, driver GPIO, schema `.vqeaf` hoặc trình xác thực signed `.qeapp`.

**Tạo mới**
- `tools/build_offline.py`: kiểm tra cấu hình, cache PlatformIO/board/ESP32-S3 toolchain/Arduino/esptool/scons, 4 lib pinned, clean build, SHA-256 firmware, log/báo cáo Markdown + JSON, chẩn đoán lỗi compiler/linker/library/network.
- `tools/build_offline.bat`, `tools/build_offline.ps1`, `tools/build_offline.sh`: launcher cho Windows CMD, PowerShell và POSIX.
- `tools/test_offline_build.py`: 10 kiểm thử với PlatformIO **giả**, chạy không có mạng hoặc toolchain thật.
- `docs/OFFLINE_PLATFORMIO_BUILD_V233.md`: quy trình tạo cache trên PC có mạng và build trên PC offline.

**Kết quả đã thực hiện tại môi trường tạo gói**
- `tools/check_board_config.py`: PASS.
- `tools/build_pio.py --host-only`: PASS (preflight, build guards, link 26 file C++, Home/Menu/icons host smoke). Không phải cross-build.
- `tools/test_offline_build.py`: **10/10 PASS** trên PlatformIO **giả**; các báo cáo lỗi và kịch bản cache được kiểm tra bằng fixture.
- `tools/build_offline.py` với môi trường thật hiện tại: **BLOCKED_PREFLIGHT** đúng như dự kiến (chưa có PlatformIO và ESP32-S3 toolchain/4 lib cache). Không có `firmware.bin` mới.
- Chưa chạy PlatformIO thật hoặc test trên mạch. Các kết quả PASS của test fake không chứng minh firmware có thể flash.
