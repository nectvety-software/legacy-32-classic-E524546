# VQEAF OS v2.3.5 — tối ưu icon Pixel Art RGB565, bảo toàn hình ảnh

## Mục tiêu và giới hạn

Cập nhật **nguyên bộ 12 icon hệ thống, mỗi icon có bản 24×24 và 36×36**
trong dự án v2.3.4. Giữ nguyên giá trị RGB565 sau bộ lượng tử màu v2.3.4:
**không thay đổi bảng màu, không scale lại, không dither, không giảm số màu, không
động tới source PNG.** UI Home/Menu, dấu chọn (focus), theme `.vqeaf` và ứng
dụng `.qeapp` vẫn dùng API `VqeafIcons::draw` hiện tại.

## Dung lượng dữ liệu tĩnh

| Đầu mục | v2.3.4 | v2.3.5 | Giảm |
|:--|--:|--:|--:|
| 12 icon × 24×24 | 2.892 B | 2.255 B | 637 B / 22,03% |
| 12 icon × 36×36 | 5.368 B | 4.188 B | 1.180 B / 21,98% |
| **Tổng stream + bảng màu RGB565** | **8.260 B** | **6.443 B** | **1.817 B / 22,00%** |
| Số descriptor | 24 | 24 | không đổi |
| Descriptor ESP32-S3, mỗi icon | 16 B | 16 B | không đổi |

Phép đo trên là dữ liệu mảng `uint8_t` và `uint16_t` do generator tạo,
**không** phải kích thước `firmware.bin`. Trình giải nén mới chứa thêm nhánh
nibble4 nhưng thay decoder RLE cũ; mã máy sau link phụ thuộc toolchain,
linker flags và ABI. Cần build PlatformIO target để đo mức giảm Flash cuối.

## Codec và ranh giới màn hình

1. Bộ tạo giữ nguyên **đúng** thứ tự palette RGB565 và pixel index của bản 2.3.4.
2. Tính bounding rectangle của pixel không trong suốt, chỉ lưu phần thực sự có
   hình; `x0/y0/w/h` được lưu trong descriptor. Kích thước API vẫn **24×24** hoặc
   **36×36**; vùng bị cắt vẫn trong suốt.
3. RLE hiện chạy **xuyên qua ranh giới dòng** trong vùng crop. 5 bit cao là
   palette role, 3 bit thấp là run dài 1–8. Token `0xF8` + 1 byte là lệnh
   transparent run dài 8–263 pixel. Role `31` được giữ cho escape, không cấp
   cho màu thực tế.
4. Với palette <=16, bộ tạo thử **nibble4** (2 pixel/byte); chỉ dùng khi
   lượng byte stream nhỏ hơn RLE. Hiện có hai biến thể Bluetooth dùng nibble4,
   22 biến thể còn lại dùng cropped cross-row RLE5.
5. C++ renderer giải mã trực tiếp các segment màu thành `drawFastHLine`,
   không cần cấp phát heap, framebuffer trung gian, PNG decoder hoặc đọc SD.
   Khi `clearBackground=true`, fill toàn bộ **ô 24/36** trước khi vẽ; khi false,
   vùng alpha-trong suốt bảo tồn pixel nền cũ.

Chú ý: pixel-perfect ở đây là **so với màu RGB565 đã lượng tử ở v2.3.4**, không
phải khôi phục giá trị RGB888 từ ảnh gốc. Nội dung mới có thể lệch với PNG 24-bit
mà mắt người không phân biệt, đúng như dữ liệu firmware cũ.

## Kiểm thử đã thực hiện

- Biên dịch **2 bản C++ renderer độc lập** (source cũ đã đóng băng và source mới).
- So sánh nguyên vẹn từng byte trong **192 kịch bản** (24 sprite × 4 màu nền ×
  bật/tắt clear); tổng **179.712 pixel RGB565**. Không chấp nhận sai khác.
- So sánh từng pixel của **24 icon** từ host C++ với PNG gốc qua **đúng hàm
  quantize RGB565 hiện tại** (**22.464 pixel**).
- Biên dịch GUI thật (`SymbianUI.cpp`) với màn hình mô phỏng TFT và kiểm tra
  3 shortcut Home, 12 ô Menu, 12 danh sách sử dụng icon 24px, focus cập nhật
  đúng vùng, theme tùy chọn.
- Render Home/Menu thật (host C++) và so sánh từng pixel với 4 ảnh v2.3.4:
  `home`, `menu`, `home_selected`, `menu_selected` đều giống 100% RGB.
- `tools/check_board_config.py`: cấu hình 240×320, GPIO và partition còn nguyên.
- Dry-run offline: PASS, **chỉ mô phỏng**, không thay cho build target.

## Tái tạo và thử trên Windows

```powershell
cd VQEAF-OS
py -3 -m pip install Pillow==12.3.0   # môi trường kiểm thử đã dùng
py -3 tools\verify_icon_optimization_v235.py
# Nếu có đủ PlatformIO/toolchain/cache mới build thật:
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Nếu generator cho ra kết quả khác (ví dụ Pillow đời khác thay đổi thuật toán
quantize), bộ test sẽ so với **source C++ v2.3.4 đóng băng** và báo lỗi thay vì
âm thầm đổi hình. Giữ thư mục `docs/verification/v234_baseline` chỉ làm nguồn
tham chiếu unit test, không đưa mã này vào `src/`.

## Đường dẫn code

- `src/core/VqeafIconData.h`: bảng palette, stream đã nén và descriptors.
- `src/core/VqeafIconRenderer.cpp`: hai codec; API giữ nguyên như 2.3.4.
- `tools/pixel_icon_assets/png/*`: 24 ảnh gốc, bất biến.
- `tools/pixel_icon_assets/compile_pixel_icons.py`: generator reproducible.
- `tools/test_lossless_icons_v235.py`: kiểm thử hai bản C++ độc lập.
- `tools/verify_icon_optimization_v235.py`: 1 lệnh chạy suite và xuất báo cáo.
- `build_reports/icons_v235/report.md` / `report.json`: kết quả hiện tại.

**Chưa xác nhận:** `.bin` target ESP32-S3, Flash thực sau link, FPS/SPI thực và
thử nghiệm màu trên ST7789; không kèm firmware flashable giả.

### Kiểm tra kích thước sau link bằng trình biên dịch PC (tham khảo)

Dùng cùng mã C++, `g++ -Os -flto -ffunction-sections -fdata-sections`
`-Wl,--gc-sections` và cùng host framebuffer harness trên máy thử nghiệm
x86-64: tổng `text+data+bss` từ **14.778 B xuống 13.408 B** (−1.370 B).
Đây chỉ là phép đo **native PC**, **không suy ra tỷ lệ giảm Flash ESP32-S3**.
Log kiểm tra tại `build_reports/icons_v235/native_linked_size.txt`.
