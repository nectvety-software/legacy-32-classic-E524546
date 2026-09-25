# VQEAF OS v2.3.3 Offline Dry-run — kiểm thử bản add-on

Ngày: 24/09/2026. Phạm vi: `tools/build_offline.py`, CLI wrappers và `tools/test_offline_build.py`. Nguồn firmware C++ và GPIO không thay đổi.

| Kiểm tra | Kết quả thực tế | Ghi chú |
|---|---|---|
| `python3 -m py_compile` (builder + test + static board checker) | PASS | Python trong container |
| `python3 tools/check_board_config.py` | PASS | Board N16R8, 10 keypad, ST7789 dọc, CSV partitions |
| 9 bài kiểm thử bổ sung dry-run | PASS 9/9 | Fake PIO trace đảm bảo không hề được gọi; không phát sinh `.bin` |
| 10 bài kiểm thử hồi quy offline builder | PASS 10/10 | Dùng PIO test double và cache giả, **không phải firmware thật** |
| Mô phỏng `--dry-run --host-tests --buildfs` với `--pio-home` trống | DRY_RUN_COMPLETE (exit 0) | Thực thi local Python board check; giả lập 9 stage |
| Mô phỏng lỗi linker | DRY_RUN_SIMULATED_FAILURE (exit 1) | `LINKER`, không gắn nhầm thành lỗi compiler |
| `.sh` `--dry-run --check-only` | DRY_RUN_CHECK_ONLY (exit 0) | Không có `build.log` hoặc firmware |
| Windows `.bat` và PowerShell `.ps1` | CHƯA THỬ TRÊN WINDOWS | Đã bổ sung flag và kiểm tra tĩnh |
| ESP32-S3 PlatformIO target build | CHƯA THỬ | Không có PlatformIO/toolchain ESP32-S3 trong môi trường chạy thử |
| Thiết bị thật | CHƯA THỬ | Không có mạch trong môi trường |

**Kết luận kiểm thử:** Script mô phỏng được các giai đoạn và báo cáo trên PC không cần PlatformIO/Xtensa. Đây là kiểm thử trình build, không xác nhận firmware biên dịch được hoặc chạy được trên phần cứng.

Dữ liệu: `docs/verification/dry_run_9_tests.log`, `legacy_offline_10_tests.log`, ví dụ report trong gói release.
