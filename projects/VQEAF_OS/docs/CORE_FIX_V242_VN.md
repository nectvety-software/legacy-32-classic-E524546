# VQEAF OS v2.4.2 — Sửa lõi QEAPP, Qeafbrowser và VQEAF Theme

**Thiết bị:** ESP32-S3 N16R8 (Flash 16 MB/PSRAM 8 MB), ST7789 240×320 dọc, SD_MMC 1-bit; **không sửa sơ đồ GPIO**. Giữ giao diện Home/Menu, icon RGB565/RLE 24/36px, `.qeapp` QEAPP/2 có chữ ký và `.vqeaf` đúng cú pháp Theme Studio.

Đây là bản sửa ở **mã nguồn** dựa trên v2.4.1. Đã biên dịch/liên kết firmware bằng ESP32 host mock và chạy regression trên PC. Không có kết quả nạp ESP32-S3 hay thử HTTPS/microSD trên thiết bị thật trong bản phát hành này. Lỗi riêng của thẻ SD, WiFi, router hoặc chứng chỉ còn cần log thực tế.

## 1. Những lỗi đã đối chiếu từ mã nguồn và thay đổi

| Lõi | Vấn đề mã nguồn trước đây | Biện pháp v2.4.2 |
|---|---|---|
| App Installer | Chỉ xem `System/Apps/Inbox`, khiến file `.qeapp` chép vào Downloads hoặc gốc SD không hiện | Quét tối đa 12 package từ `/System/Apps/Inbox`, `/System/Downloads` và `/`; kiểm tra `QEAPP/2` trước khi cho phép cài; tạo layout SD và báo lỗi mount/đường dẫn. |
| App trust | Firmware mặc định và demo Pixel Snake ghim hai **public key khác nhau** | `tools/doctor_v242.py` xác minh hash từng section và ECDSA-P256 từ đúng trust header, báo **key mismatch** rõ ràng. **Không** tắt xác minh chữ ký. |
| Browser | HTTP `Transfer-Encoding: chunked` bị đưa nguyên framing vào HTML hoặc từ chối tải app/theme không có `Content-Length` | Bộ giải mã HTTP chunked **không dùng heap**; trang tối đa 32 KiB; tải `.qeapp`/`.vqeaf` chunked tối đa 4 MiB, ghi `.part` lên thẻ SD và chỉ đổi tên sau khi kiểm tra kích thước. |
| Browser | `openLink()` giữ URL là con trỏ vào bộ link pool, sau đó `resetPage()` xóa pool, khiến URL được mở rỗng/sai | Sao chép URL đích vào bộ đệm cục bộ **trước** `resetPage()`, có regression C++ mở liên kết. `Reload` ghi nhớ địa chỉ thất bại để thử lại. |
| Theme | `.vqeaf` chỉ có magic header vẫn xuất hiện rồi mới báo lỗi lúc Apply | Quét bằng parser giới hạn RAM, kiểm tra cấu trúc `<theme>`, `palette`, `screen`, `keyText`, `accent` và `</theme>` trước khi hiện; apply lỗi giữ nguyên theme trước. |
| Browser → hệ thống | Download theme dừng ở File Manager | File `.vqeaf` tải thành công đi tới Themes và chờ **Apply** thủ công; `.qeapp` đi tới App Installer và chờ xác nhận. |
| Quan sát lỗi | Dễ nhầm chưa mount microSD, thiếu thư mục, khác key hoặc đồng hồ HTTPS chưa đồng bộ | Lệnh Shell `corediag` và log boot `[VQEAF][CORE]` ở 115200 baud để phân biệt. |

## 2. Kiểm tra thực tế trên máy và SD

1. Sao lưu firmware hiện tại và dữ liệu thẻ SD. Thẻ SD cần FAT, lắp ổn định; firmware sẽ cố tạo layout `System/Apps/{Inbox,Installed}`, `System/Themes` và `System/Cache/Web`.
2. Để ứng dụng `.qeapp` đã được **ký bằng khóa mà firmware tin cậy** ở `/System/Apps/Inbox/` (hoặc `/System/Downloads/` hay gốc thẻ), mở **Apps > App Installer > Inbox**, xem chi tiết chữ ký, xác nhận Install. File ZIP/VXP/SIS/SISX/Lua hoặc `.qeapp` ký bằng khóa khác sẽ không thành ứng dụng chạy được.
3. Để theme hợp lệ `.vqeaf` tại `/System/Themes/` hoặc `/Themes/`. Mở **Themes > Rescan microSD > Apply**. Theme được tạo bởi Theme Studio phải có palette gồm `screen`, `keyText`, `accent`. Trình đọc firmware chỉ áp dụng màu và `launcher{}` được hỗ trợ; 3D mockup, animation hoặc mã nhúng trong theme **không được thực thi**.
4. Cho thiết bị kết nối WiFi, chờ đồng bộ thời gian NTP, thử một trang HTML nhỏ qua **Qeafbrowser**. Browser chỉ xử lý HTML/text đơn giản, không hỗ trợ JavaScript/CSS phức tạp, không bảo đảm chạy mọi website hiện đại. HTTPS chỉ chấp nhận chuỗi CA nằm trong trust store được nhúng.
5. Mở **Shell**, lần lượt chạy `corediag`, `sddiag rw`, `tlsdiag valid` và xem **Serial Monitor 115200**. Trong khi kiểm thử `sddiag rw` sẽ tạo/xóa một tệp thử nghiệm nhỏ trên SD; nên sao lưu SD trước khi thao tác.

**Chú ý khóa demo:** `vqeaf_os` dùng `src/services/QeappTrustKey.h` với ID `0x31534351`; game `snake_pixel_demo.qeapp` hiện dùng khóa DEMO khác `0x534E414B` ở `src/services/SnakeDemoTrustKey.h`. Vì thế **không cài được file Snake demo lên firmware production**. Có thể chọn môi trường `vqeaf_snake_demo` chỉ để thử nghiệm, hoặc ký lại bằng khóa sản xuất mà bạn sở hữu; không đưa khóa demo vào bản firmware thương mại.

**Công cụ doctor trên PC (chỉ đọc, không sửa package):**

```powershell
py -3 -m pip install cryptography
py -3 tools/doctor_v242.py
py -3 tools/doctor_v242.py --package games/pixel_snake/dist/snake_pixel_demo.qeapp --key-header src/services/SnakeDemoTrustKey.h
py -3 tools/doctor_v242.py --sd-root "E:\"
```

## 3. Build, kiểm thử và nạp (Windows / PlatformIO)

```powershell
cd VQEAF-OS
# Bộ regression C++ host (cần g++ trong PATH) + Python cryptography:
py -3 tools/verify_v242.py
# Bản firmware dùng trust key sản xuất:
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
# Riêng khi kiểm thử ứng dụng demo Snake có chữ ký demo:
pio run -e vqeaf_snake_demo
```

`build_reports/v242/report.md` chứa bảng PASS/FAIL cho kiểm thử trên PC; đừng hiểu kết quả PC là bằng chứng phần cứng. Nếu gặp lỗi thiết bị, gửi **log từ boot đến khi bấm Install/Open/Apply**, đầu ra `corediag`, `sddiag rw`, `tlsdiag valid`, file `.qeapp` (hoặc ID key, không cần private key) và 10–20 dòng đầu file `.vqeaf` để sửa đúng nguyên nhân.
