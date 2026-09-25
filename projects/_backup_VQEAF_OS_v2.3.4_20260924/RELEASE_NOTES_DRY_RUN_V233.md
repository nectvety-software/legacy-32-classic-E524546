# VQEAF OS v2.3.3 — Offline Build Dry-run Add-on

* Firmware, GPIO, Home/Menu, icon, `.vqeaf` và `.qeapp`: **không chỉnh sửa**.
* Bổ sung `--dry-run` cho `tools/build_offline.py`; hỗ trợ `.bat`, `.ps1`, `.sh`.
* Chỉ chạy kiểm tra tĩnh board thực bằng Python; giả lập preflight cache, PIO CLI, host tests, clean, compile, link, artifact verification, buildfs.
* `--simulate-failure` thử 10 loại sự cố có phân loại và lời khuyên tiếng Việt.
* Xuất `build_reports/offline/dry_run/report.json`, `report.md`, từng log giả lập và `dry_run_plan.log`.
* Đầu ra được đánh dấu `DRY_RUN_*` và `SIMULATED_*`: **không tạo/không xác nhận firmware.bin**.
* Mở rộng `tools/test_offline_build.py` kiểm thử cả chế độ thật với PIO test double và chế độ dry-run không thực thi PIO.

Chạy: `tools\\build_offline.bat --dry-run --host-tests --buildfs`.
Xem: `docs/DRY_RUN_PLATFORMIO_V233.md`.
