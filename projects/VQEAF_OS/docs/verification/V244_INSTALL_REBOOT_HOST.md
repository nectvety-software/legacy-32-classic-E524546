# VQEAF OS v2.4.4 — Báo cáo ứng viên sửa reboot khi cài ứng dụng

## Điều chắc chắn và chưa chắc chắn

Người dùng báo board ESP32-S3 **khởi động lại trong lúc cài `.qeapp`**. Chưa có UART 115200 của **lần reset tương ứng** nên không thể khẳng định đó là stack overflow, watchdog hay brownout. Các thay đổi bên dưới là phòng ngừa và bổ sung chẩn đoán.

- Vật liệu: bản source v2.4.3 DeepTest candidate (bao gồm sửa BrowserService từ phiên kiểm thử trước).
- Vị trí chạy: Linux PC (g++, Python, OpenSSL, Arduino/SD shims); **không có PlatformIO hoặc ESP32-S3 thật**.
- Cấu hình board, chân ST7789, microSD 1-bit, icon RGB565/RLE, signature QEAPP/2, theme `.vqeaf`: không thay đổi.

## Kết quả quan sát

| Kiểm thử | Kết quả | Giới hạn |
|---|---|---|
| `tools/test_v244_install_reset.py` | **PASS** | Biên dịch mã nhánh ESP32 của logger bằng header shim, kiểm tra lazy-Inbox/stack allocation; suite app-manager thật trên host |
| `tools/test_v24_app_manager.py` | **PASS** | Signed update/rollback/staging và quota trên host |
| `tools/verify_v243.py` | **PASS 5/5** | Bao gồm hồi quy v2.4.2 **17/17** và bản C++ host firmware-link, chỉ trên PC |
| `tools/check_board_config.py` | **PASS** | Chỉ preflight cấu hình, không có toolchain |
| `tools/verify_v244.py` full rerun | **CHƯA HOÀN TẤT** | Lượt thứ hai bị timeout môi trường sau khi bước targeted PASS; không báo PASS full gate |
| PlatformIO + flash board + reset thật | **CHƯA CHẠY** | Cần log thiết bị để xác nhận |

## So sánh khung stack trên g++ x86_64 `-O1 -fstack-usage`

| Hàm | Trước (byte) | Sau (byte) | Giảm (byte) |
|---|---:|---:|---:|
| `scanPackage` | 3,280 | 1,232 | 2,048 |
| `verifyDirectory` | 3,456 | 1,408 | 2,048 |
| `recoverTransactions` | 2,768 | 1,184 | 1,584 |
| `install` | 1,824 | 1,824 | 0 |

Các số liệu x86_64 là **dấu hiệu giảm stack ở những hàm trên**; không phải số liệu Xtensa hoặc đỉnh toàn bộ call chain ESP32, cũng không tự chứng minh fix reboot.

## Hướng dẫn tái lập và thu log

Xem `docs/INSTALLER_REBOOT_DIAGNOSTIC_V244_VN.md`. Thu Serial 115200 trước khi tái hiện lỗi, ưu tiên cài `welcome.qeapp` đúng khóa trong SD. Không tắt chữ ký để che lỗi. Nếu bản firmware candidate còn reset, gửi nhật ký `[QEAPP][INSTALL]`, `[QEAPP][PREVIOUS_RESET]`, `Guru Meditation` hoặc `Brownout` để xác định chính xác nguyên nhân.
