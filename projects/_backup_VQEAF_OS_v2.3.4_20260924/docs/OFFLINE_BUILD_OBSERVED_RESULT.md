# Kết quả triển khai công cụ build offline (môi trường tạo gói)

- **Kiểm tra mã nguồn cấu hình board:** `python tools/check_board_config.py` → PASS.
- **Hồi quy firmware trên host:** `python tools/build_pio.py --host-only` → PASS (board, build guards, link 26 `.cpp`, Home/Menu + icon host smoke). Không phải build chip.
- **Đơn vị script offline:** `python tools/test_offline_build.py` → **10/10 PASS** với **PIO giả + cache giả**; kiểm tra mã thoát, file báo cáo, lỗi thiếu package/lib, sai version, lỗi compiler, không sinh firmware, buildfs.
- **Chạy thực `python tools/build_offline.py` trên môi trường tạo gói:** `BLOCKED_PREFLIGHT` (exit 2) vì **chưa có PlatformIO CLI/toolchain/packages/libdeps cài cục bộ**; script dừng trước `pio run`, không tải gói, không có firmware.bin.
- **Chưa được kiểm chứng:** `pio run` với ESP32-S3 toolchain thật, nạp firmware, tính ổn định trên mạch.

Không nhầm PASS của trình giả lập test script với PASS của build target ESP32-S3. Trên PC đã cài cache, kết quả build thực được lưu vào `build_reports/offline/report.md` và `report.json` theo từng lần chạy.
