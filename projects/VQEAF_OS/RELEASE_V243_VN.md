# VQEAF OS v2.4.3 — sửa lỗi gián đoạn khi mở `.qeapp` và `.vqeaf`

Đối chiếu video thiết bị thật IMG_3535.MOV (19–21 s và 27–30 s) với mã v2.4.2. Phát hiện một lỗi điều phối phím có thể tái hiện: START chọn nội dung ở sự kiện nhấn ngắn, sau đó nhấn dài 650 ms gây một lệnh **chuyển sang Music toàn cục**, cắt ngang quá trình đang mở bộ cài hoặc Themes.

Bản v2.4.3 sửa key gesture, tăng khả năng mở file ngoài danh mục, rút ngắn đường đi File Manager → Installer, làm rõ chẩn đoán khóa ký số và bổ sung Serial trace.

**Chưa chạy lại trên bo mạch người dùng**; kiểm thử với host giả GPIO+thời gian, giả FS và crypto là bước kiểm thử trước khi nạp firmware mới.

Xem `docs/DEVICE_INSTALL_THEME_FIX_V243_VN.md` và `build_reports/v243/report.md`.
