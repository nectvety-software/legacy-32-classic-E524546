# Pixel Snake (.qeapp) — template game 240×320 cho VQEAF OS v2.4.1

Đây là template game Rắn Săn Mồi phong cách pixel. **Không phải runtime cho game tùy ý**:
firmware v2.4.0 chỉ chạy `.qeapp` loại `web` và `text`. Bản v2.4.1 bổ sung
**engine game native cố định `snake_pixel`** và chỉ nhận cấu hình text đã được
ký ECDSA P-256 bên trong QEAPP/2. Game logic/render là mã C++ được flash
trong firmware. Cài file `.qeapp` **một mình vào v2.4.0 sẽ mở Text Viewer**,
không chạy game; cần firmware v2.4.1 trong gói phát hành.

## Nội dung

- `snake.cfg`: file cấu hình thuần ký trong gói; thay speed, wrap, palette.
- `snake_icon_32.png`: icon 32×32 RGB565 được build_qeapp.py chuyển đổi.
- `make_icon.py`: dựng lại icon dạng pixel, không cần tải font/hình từ mạng.
- `build_package.py`: xác thực cấu hình rồi ký gói QEAPP/2 bằng khóa của bạn.
- `dist/snake_pixel_demo.qeapp`: **gói DEMO** được ký bằng khóa dùng một lần;
  public key tương ứng ở `src/services/SnakeDemoTrustKey.h` của firmware đi kèm.
  **Khóa private DEMO không phát hành** nên không thể ký bản cập nhật tương ứng.

## Cách chạy demo (không đè lên môi trường sản xuất)

1. Sao lưu firmware, dữ liệu cá nhân và app đang cài. Môi trường `vqeaf_snake_demo`
   dùng key ID `0x534e414b`; các app đang dùng trust key khác sẽ không còn xác minh
   được trong thời gian bạn flash profile demo.
2. Build/nạp firmware ESP32-S3 N16R8:

```powershell
cd VQEAF-OS
pio run -e vqeaf_snake_demo
pio run -e vqeaf_snake_demo -t upload
pio device monitor -b 115200
```

3. Chép `games/pixel_snake/dist/snake_pixel_demo.qeapp` lên microSD:
   `/System/Apps/Inbox/snake_pixel_demo.qeapp`.
4. Trên máy: Menu → Applications → App Installer → App inbox → Pixel Snake
   → Details → Install. Sau đó Applications → Pixel Snake → Open.

Nếu firmware hiện tại vẫn dùng profile `vqeaf_os`, demo `.qeapp` sẽ **bị từ chối
đúng thiết kế** do chữ ký không khớp. Không tắt xác minh chữ ký để cài game.

## Phát hành ứng dụng dùng key của bạn

```powershell
# Sinh key 1 lần vào thư mục bảo mật bên ngoài repository
py -3 tools/qeapp_keys.py --private C:\secure\publisher.pem `
  --header src/services/QeappTrustKey.h

# Kiểm tra nội dung snake.cfg, vẽ icon và đóng gói:
py -3 games/pixel_snake/make_icon.py
py -3 games/pixel_snake/build_package.py --sign-key C:\secure\publisher.pem `
  --version 1.0.0 -o snake_pixel.qeapp

# Firmware sản xuất pin key của bạn, không dùng profile demo:
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
```

**Không** đưa private key lên SD, GitHub, firmware, ZIP hoặc thư mục public.
Để update, giữ nguyên ID `snake_pixel`, tăng `--version` và ký bằng **cùng key**.

## Game/Phím

- `UP/DOWN/LEFT/RIGHT`: điều khiển rắn (không được quay đầu 180°).
- `START`: bắt đầu, tạm dừng/tiếp tục, chơi lại sau khi thua.
- `OPTION`: tạm dừng/tiếp tục.
- `A` hoặc `B`: thoát về Applications (lưu điểm cao).
- `MENU`: về launcher, hệ thống xử lý như các app khác.
- `SELECT giữ >600ms`: đổi Game/T9 theo firmware; không chiếm phím hệ thống.

Bàn chơi **16 × 18 ô**, mỗi ô 12×12 pixel, từ `(24,60)` đến `(215,275)`.
Header OS ở y=0..27, HUD ở 32..54, footer OS ở 298..319. Điểm +10
mỗi quả; kết thúc khi chạm tường/thân (hoặc bật `wrap=1` để xuyên tường).
Điểm cao được lưu qua `QeappDataService` trong `state.bin`, không sửa file tùy ý.

## Test

```powershell
py -3 tools/test_pixel_snake.py   # C++11 logic + renderer + QEAPP/2 installer thực qua host mock
py -3 tools/verify_v240.py       # Hồi quy firmware v2.4.0
pio run -e vqeaf_snake_demo     # Cần PlatformIO và toolchain thật; chưa xác nhận trong môi trường tạo kit
```

Ảnh `240×320` được render bằng cùng lớp renderer C++ trên PC nhưng phần header/
footer được mô phỏng; không phải ảnh chụp thiết bị thật. Theme `.vqeaf` vẫn dùng
cho UI/khung, còn màu sân game tùy `palette` trong `snake.cfg`.
