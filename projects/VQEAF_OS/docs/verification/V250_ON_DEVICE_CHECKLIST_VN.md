# ESP32-S3 / ST7789 — kiểm thử chấp nhận v2.5.0 trên thiết bị thật

1. Sao lưu firmware và NVS/thẻ microSD; dùng nguồn USB ổn định. Không đổi GPIO trong mã nguồn.
2. Chạy `pio run -e vqeaf_render_compat -t upload` (lõi icon theo bản cũ) và chụp ảnh Home, Menu, danh sách app, Themes với chủ đề mặc định và chủ đề `.vqeaf` bên ngoài. Ghi Serial và màn hình sau 10 lần đổi menu.
3. Nạp `vqeaf_os`; chụp ảnh cùng từng màn, **so màu đỏ/xanh**, hình pixel/icon 24×24 và 36×36, vùng nền trong suốt và chữ Việt. Không chấp nhận đốm màu dù giảm thời gian vẽ.
4. Lặp lại điều hướng 100 lần UP/DOWN/LEFT/RIGHT (Menu và Explorer), vào/ra Home 20 lần, phát nhạc nền nếu có WAV phù hợp, mở Gallery JPG/PNG/BMP, vào web 10 URL, áp dụng theme từ thẻ 10 lần. Nếu xuất hiện reset chụp trọn log 115200 bằng Serial Reset Logger đi kèm.
5. Nạp `vqeaf_perf_diag`; đo có kiểm soát 60 giây với mỗi màn hình, phân loại `max_loop_us` và số lần >16/33/100 ms; so cùng mã nguồn `vqeaf_render_compat` nếu muốn đo riêng icon. Dữ liệu có in Serial không trực tiếp tương đương FPS; muốn đo FPS thật phải có frame counter theo từng app và timestamp trên phần cứng.
6. Cài `welcome.qeapp` bằng **khóa production**, xác nhận app vẫn có sau khi khởi động lại. Không nạp demo Snake key đè vào production rồi kết luận signature có lỗi. Lặp lại thử theme qua SD và kiểm tra hồi phục sau restart.
7. Đo heap/PSRAM, điện áp khi phát nhạc+cài ứng dụng và trễ khi cuộn Explorer. Nếu còn brownout/WDT/panic ghi trạng thái đầy đủ trước khi thay đổi tiếp.

**Tiêu chí phát hành:** không đảo màu, không cắt mất pixel/text, không hồi quy cài `.qeapp` và theme, không reset ngoài ý muốn trong kiểm thử, và số đo latency trên phần cứng không tệ đi khi chạy tác vụ giống nhau. Bài kiểm thử hiện tại trên PC **không thay thế** checklist này.
