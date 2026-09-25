# VQEAF OS v2.3.6 — kiểm thử icon trên ESP32-S3 và đo Flash toàn firmware

**Phần cứng:** ESP32-S3-WROOM-1 N16R8 (Flash 16 MB, PSRAM 8 MB), ST7789
240×320 **dọc**, màn hình và keypad theo `BoardConfig.h`. **Không đổi GPIO.**
Giữ nguyên `.vqeaf`, `.qeapp`, 12 icon × 2 cỡ và bố cục Home/Menu.

## 1. Phương pháp kiểm thử không mất chi tiết

- `docs/verification/v234_baseline/*`: **bản render v2.3.4 đóng băng**; nguồn
  tham chiếu độc lập. `tools/generate_icon_device_golden.py` build trực tiếp C++
  v2.3.4 để tạo **192 CRC32** RGB565 theo byte little-endian: 12 icon × 2 cỡ ×
  4 màu nền × 2 chế độ giữ/xóa nền.
- Renderer sản xuất v2.3.6 và self-test ESP32 gọi **chính xác cùng hàm decode**
  (`src/core/VqeafIconRenderer.cpp`). Test dùng callback span ghi vào buffer
  36×36 trong SRAM, CRC và so với C++ v2.3.4. Không phụ thuộc đọc ngược LCD:
  ST7789 hiện tại nối MOSI một chiều, không có MISO.
- Mỗi sprite 24×24/36×36 được render cả nền trong suốt và nền xóa trước khi
  vẽ. Có thử `Id` sai, cỡ sai, callback nullptr để tránh sửa bộ nhớ khi dữ
  liệu đầu vào không hợp lệ.
- Firmware chẩn đoán sau đó gọi **đường vẽ TFT thật** 72 lần (24 icon × 3 lần)
  và ghi `micros()` thời gian truyền/vẽ tổng. Bộ đo chỉ dùng để tham khảo;
  không khẳng định màn hình vật lý pixel-perfect nếu chưa quan sát/chụp thực tế.
- Bản chẩn đoán dành riêng cho mạch: `VQEAF_ICON_SELFTEST=1`. Bản sản xuất
  **không** biên dịch golden checksums, buffer kiểm thử hay Serial test.

### A. Chạy kiểm thử không cần mạch (PC)

```powershell
cd VQEAF-OS
py -3 tools\verify_icon_device_v236.py
py -3 tools\measure_native_icon_link_v236.py
```

Lệnh đầu cần `g++`; đối chiếu nguồn v2.3.4, chạy nguyên bộ kiểm thử
mới bằng shim Arduino/TFT, kiểm tra board và 192 CRC. Báo cáo tại
`build_reports/icon_device_v236/host_verification.md`.
Lệnh thứ hai đo **chương trình icon liên kết cho x86-64**, không phải firmware.
Số đo host trong bộ thử này: **14.778 B → 13.444 B = −1.334 B**
(`size` text+data+bss). Kết quả có thể khác theo phiên bản g++.

### B. Biên dịch và chạy trên mạch ESP32-S3

```powershell
cd VQEAF-OS
pio run -e vqeaf_icon_selftest
pio run -e vqeaf_icon_selftest -t upload
py -3 -m pip install pyserial   # nếu chưa có
py -3 tools\capture_icon_selftest.py --port COM5 --timeout 90
```

Sửa `COM5` thành cổng thật. **Mở capture rồi nhấn RESET một lần trên thiết bị**,
để nhận cả preamble bộ nhớ và 24 bản báo cáo. Capture tự ngừng khi thấy SUMMARY.
Nếu không cài pyserial, dùng `pio device monitor -b 115200`, lưu nguyên log
ra file và gọi `py -3 tools\capture_icon_selftest.py --parse-log your_log.txt`.

Đầu ra cần thấy:

```text
[VQEAF][BUILD] target=ESP32-S3 N16R8 display=240x320 portrait
[VQEAF][MEM] flash=... psram=... free_heap=...
[ICONTEST] START v2.3.6 cases=192 ref=v2.3.4 format=RGB565-LE
[ICONTEST] VARIANT WiFi size=24 cases=8/8
...
[ICONTEST] TIMING decode_192_cases_us=... spi_draw_72_calls_us=...
[ICONTEST] SUMMARY passed=192 expected=192 errors=0 result=PASS
```

**Để được công nhận là thử trên mạch:** cần capture cả `[VQEAF][BUILD]`,
`[VQEAF][MEM]`, đủ 24 VARIANT và SUMMARY; kiểm tra flash >= 16.777.216 B,
PSRAM >= 8.388.608 B, màu thực trên ST7789, thiết bị không boot-loop.
Báo cáo JSON/Markdown + raw UART được ghi vào `build_reports/icon_device_v236/`.
Mã kiểm thử chỉ phát thông báo, không ghi/định dạng SD/NVS hay đổi chế độ cứu hộ.

## 2. Đo Flash **toàn bộ** firmware, không lấy số giảm mảng icon làm kết luận

`platformio.ini` có 3 env riêng:

| Env | Ý nghĩa |
|---|---|
| `vqeaf_size_baseline` | Cả OS hiện tại với renderer + RGB565/RLE **v2.3.4 đã đóng băng** |
| `vqeaf_size_optimized` | Cả OS hiện tại với renderer/crop/nibble tối ưu **v2.3.6** |
| `vqeaf_icon_selftest` | Chẩn đoán 192 CRC + timing trực tiếp TFT; **không dùng để so kích thước sản xuất** |

Cả 2 bản kích thước dùng cùng PlatformIO 6.10.0, cùng board, `build_flags`,
framework/libraries và toàn bộ các module OS; chỉ bản baseline có
`-D VQEAF_ICON_BASELINE=1`. Không thêm self-test vào hai bản đo Flash.

```powershell
cd VQEAF-OS
py -3 tools\measure_firmware_icon_impact.py
```

Script build **clean** lần lượt 2 env, yêu cầu **cả `firmware.bin` và
`firmware.elf` tồn tại**, đọc kích thước toàn bộ firmware.bin sau link,
SHA-256, `size -A` (nếu tìm thấy toolchain), ghi raw log riêng.
Công thức chênh lệch thật:

```text
saved_flash_image_bytes = baseline/firmware.bin.size - optimized/firmware.bin.size
saved_percent = saved_flash_image_bytes / baseline/firmware.bin.size * 100
```

Kích thước slot OTA `app0`/`app1`: 0x640000 = 6.553.600 B/slot.
Nếu `saved_flash_image_bytes` âm thì codec mới **làm tăng** firmware.bin;
script báo đúng số âm, không cố ép thành tiết kiệm. Do căn chỉnh các đoạn
trong file `.bin` và tối ưu LTO, khác biệt Flash tổng có thể khác −1.817 B
đến từ dữ liệu icon riêng.

Nếu máy đã có cache PlatformIO, có thể chạy `--offline` (chặn proxy thông
thường). Chế độ này **không đảm bảo cô lập mạng tuyệt đối**: muốn chắc chắn,
ngắt kết nối Internet và dùng cache đủ. Chạy `--report-only` chỉ xuất tình
trạng không đo, tuyệt đối không tạo số đo giả.

**Trong môi trường tạo bản phát hành này chưa có PlatformIO/toolchain**:
`build_reports/firmware_icon_size_v236/report.md` ghi **UNAVAILABLE**, không
có kích thước firmware thực. Dữ liệu mảng icon đã xác nhận:
8.260 → 6.443 B, tiết kiệm **1.817 B / 22,00% chỉ trong dữ liệu icon**.

## 3. Diễn giải kết quả, lỗi thường gặp

- `ModuleNotFoundError: platformio`: cài PlatformIO và bộ package ESP32-S3,
  hoặc chạy script trên máy có sẵn cache offline.
- `toolchain-xtensa-esp32s3` / `framework-arduinoespressif32` missing:
  cài đúng các package trong `platformio.ini` trước khi ngắt mạng.
- `undefined reference`: kiểm tra output `*_build.log` trước tiên, tránh
  đo hai firmware build không cùng cấu hình.
- Mất UART/không có SUMMARY: reset sau khi kết nối cổng; nếu USB CDC đổi cổng
  lúc reset, dùng UART serial ngoài ở tốc độ 115200.
- `VARIANT ... cases=7/8`: so CRC dự kiến và CRC thực của bản 24/36 cụ thể;
  **không** chấp nhận lỗi RGB565 sai 1 pixel.
- `psram`/`flash` nhỏ hơn N16R8: kiểm tra board thực tế và bootlog trước
  khi dựa vào tốc độ hoặc số liệu RAM.
- Kết quả CRC chỉ kiểm thử hàm giải nén chạy trên chip; để xác nhận ST7789,
  chụp màn hình/quan sát màu và viền trên thiết bị thật.

## 4. Các file mới/đã cập nhật

- `src/core/VqeafIconRenderer.cpp`, `.h`: callback span chung + lựa chọn
  frozen baseline chỉ cho phép đo.
- `src/core/VqeafIconSelfTest.cpp`, `.h`: test trên mạch có giới hạn bộ nhớ.
- `src/main.cpp`: chạy test sau khi khởi tạo ST7789, trước splash, riêng env test.
- `docs/verification/device_icons/golden_crc_v234.h`: 192 CRC tham chiếu.
- `tools/icon_device_host/*`, `tools/generate_icon_device_golden.py`:
  tạo CRC từ renderer v2.3.4, đối chiếu chính runner ESP trên PC.
- `tools/verify_icon_device_v236.py`, `tools/capture_icon_selftest.py`,
  `tools/measure_firmware_icon_impact.py`, `tools/measure_native_icon_link_v236.py`.
- `platformio.ini`: thêm 3 env, mặc định vẫn `vqeaf_os` tối ưu.

**Các tính năng VQEAF OS giữ nguyên:** giao diện Home/Menu 240×320 dọc,
GPIO, theme `.vqeaf`, app `.qeapp` và bộ kiểm tra chữ ký QEAPP/2.
