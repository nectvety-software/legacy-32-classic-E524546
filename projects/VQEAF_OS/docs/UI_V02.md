# UI v0.2 notes

Mục tiêu là mô phỏng bố cục S60/Symbian cổ điển từ ảnh tham khảo mà không dùng framebuffer lớn hoặc ảnh icon ngoài.

## Bố cục 320×240

- `0..29`: title/status chrome beige.
- `31..215`: content charcoal.
- `216..239`: softkey chrome beige.
- List row cao 36 px; tối đa 5 row toàn màn hình.
- Scrollbar nằm ở mép phải và chỉ render khi `total > visible`.

## Popup menu

Popup có chiều rộng 190 px, neo phía dưới bên trái ngay trên softkey bar. Tối đa 5 item hiện cùng lúc. Menu dài tự cuộn bằng `PopupState.offset` và có thumb riêng.

## Icon

Icon v0.2 không dùng PNG/BMP. `SymbianUI::drawIcon()` dựng icon bằng `fillRect`, `drawLine`, `drawCircle`, `fillTriangle`, giúp giảm IO từ SD và tránh phân mảnh heap/PSRAM. Các loại chính: WiFi, BLE, Music, Folder, File, Gallery, Settings, Apps, Brightness, Clock, System info, About.
