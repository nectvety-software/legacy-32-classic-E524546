# QEAPP Studio UI + Core

M2 alpha: `python run_studio.py` (cần PySide6). `studio/core/` được kiểm thử riêng không phụ thuộc Qt. `studio/gui/window.py` xây dựng Explorer, UTF-8 editor, log build, host simulator preview, chọn firmware checkout và ký signed web/text QEAPP/2 bằng CLI gốc. Nút Stop hủy cây process. **Không có Lua/native `.qeapp` runtime độc lập trên firmware hiện tại**.

Xem `docs/09_M2_DESKTOP_CORE.md` và `run_tests_studio.sh`.
