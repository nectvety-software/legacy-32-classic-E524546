# VQEAF OS v2.4.2 — Core recovery: QEAPP, Browser, Themes

- Quét package đã ký từ Apps Inbox, Downloads và gốc thẻ SD; báo SD mất/thiếu layout.
- Bổ sung `doctor_v242.py` đối chiếu chữ ký QEAPP/2, SHA256 và key ID mặc định với khóa Snake demo.
- Sửa vòng đời bộ đệm link Qeafbrowser (URL bị xóa trong reset); sửa Reload của URL lỗi và Back không xóa lịch sử khi mạng thất bại.
- Giải mã HTTP chunked giới hạn RAM cho HTML; tải `.qeapp`/`.vqeaf` chunked qua tệp tạm, chống tệp cài dở.
- Giám sát microSD, key ID, WiFi và đồng hồ TLS qua `corediag` + log boot 115200.
- Validate đầy đủ `.vqeaf` trước khi đưa vào danh sách Apply. Download theme chuyển thẳng tới Themes, không tự áp dụng.
- Giữ nguyên GPIO, giao diện 240×320, icon Pixel Art RGB565/RLE và chính sách chữ ký số.

**Đã thử trên PC:** bộ kiểm thử trong `tools/verify_v242.py`; kết quả chính thức xem `build_reports/v242/report.md`.
**Chưa thử:** PlatformIO target build, nạp ESP32-S3 thật, kết nối TLS/SD/LCD vật lý và các website có JavaScript.

Xem `docs/CORE_FIX_V242_VN.md` để cài, gỡ lỗi và thu thập log từ thiết bị.
