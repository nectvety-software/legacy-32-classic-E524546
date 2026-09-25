# VQEAF OS v2.3.3 — PlatformIO offline build

Công cụ: `tools/build_offline.py` (Python 3.8+, **chỉ dùng thư viện chuẩn**). Chạy từ thư mục nào cũng được. Không gọi `pip`, `pio pkg install`, update hoặc tải dependency. Giữ nguyên toàn bộ firmware/UI/GPIO/.vqeaf/.qeapp từ bản v2.3.3.

> **Offline không có nghĩa PlatformIO tự có toolchain.** Script chỉ biên dịch được khi máy đã có Python + PlatformIO CLI + cache packages/platforms và `.pio/libdeps/vqeaf_os` đúng phiên bản. Nếu thiếu, script dừng **trước `pio run`**, ghi rõ gói thiếu. Để chắc chắn không có kết nối mạng (bao gồm tool tự mở socket), hãy ngắt mạng hoặc dùng tường lửa/cô lập mạng cấp OS. Script cấu hình HTTP(S) proxy hỏng `127.0.0.1:9` như một lớp chặn truy cập vô tình, **không phải firewall**.

## 1. Chuẩn bị trên máy Windows có Internet (CÙNG hệ điều hành/kiến trúc với máy offline)

1. Cài Python 3, PlatformIO CLI, mở thư mục `VQEAF-OS`.
2. Cài/đóng gói toàn bộ dependency **một lần khi còn có mạng**:

```powershell
py -3 -m pip install platformio
pio pkg install -e vqeaf_os
pio run -e vqeaf_os
```

3. Khi `pio run` thành công, sao chép cả **`%USERPROFILE%\.platformio\`** (gồm `platforms/espressif32`, `packages/`), và **`VQEAF-OS\.pio\libdeps\vqeaf_os\`** lên máy offline tương ứng. Chép cả `VQEAF-OS` source; **không lấy `.pio/build` làm bằng chứng build offline**. Bộ Python + PlatformIO CLI phải được cài sẵn trên máy offline hoặc cài bằng wheel đã tải sẵn.

**Nếu PlatformIO CLI chưa có trên máy offline:** Trên máy online cùng Python/OS, tải bánh xe (wheel) của PlatformIO và dependencies: `py -3 -m pip download --dest offline_wheels platformio`. Chuyển `offline_wheels` sang máy offline và chạy `py -3 -m pip install --no-index --find-links offline_wheels platformio`. Bước cài wheel này làm **trước** khi chạy `build_offline.py`; script build tự nó không cài đặt gì. Nếu dependency không có wheel cho hệ/phiên bản Python offline, chuẩn bị lại trên máy online tương ứng.

> **Lưu ý:** Nếu chưa có máy online nào build dự án với các thư viện pin đúng phiên bản, script không thể tự chế tạo cache. Đừng trộn cache từ Linux/Windows hay ARM/x86-64.

## 2. Build trên Windows không kết nối Internet

```bat
cd VQEAF-OS
call tools\build_offline.bat --check-only
call tools\build_offline.bat
```

PowerShell (thay đường dẫn theo máy):

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_offline.ps1 -PioHome D:\PlatformIO\.platformio
```

Hoặc dùng Python trực tiếp (Windows/Linux/macOS):

```powershell
py -3 tools\build_offline.py --pio-home "D:\PlatformIO\.platformio" --verbose
```

Linux/macOS: `./tools/build_offline.sh --check-only` rồi `./tools/build_offline.sh`. Nếu PlatformIO ở chỗ đặc biệt, thêm `--pio "C:\...\pio.exe"` hoặc `--pio "/.../pio"`.

**Tùy chọn**: `--check-only` chỉ kiểm cache (không tuyên bố đã build); `--host-tests` chạy thêm unit test g++ nếu có; `--incremental` bỏ lệnh clean (mặc định **clean** rồi build); `--buildfs` chỉ dùng khi bạn **đã tạo thư mục `data/`** và cache `tool-mklittlefs`; `--report-dir <path>` đổi nơi lưu báo cáo; `--verbose` hiển thị toàn bộ output.

### Đầu ra

- `build_reports/offline/preflight.log`: phát hiện gói thiếu, phiên bản và lời khuyên.
- `board_preflight.log`: kiểm tra 10 phím, màn ST7789, board N16R8, flash/PSRAM, phân vùng.
- `platformio_version.log`, `clean.log`, `build.log`: lưu stdout/stderr và mã thoát riêng biệt.
- `buildfs.log` (nếu yêu cầu), `host_cpp.log` (nếu yêu cầu), `steps.log`.
- `report.json`: trạng thái cho CI/tooling, SHA-256/bộ nhớ và mã lỗi chuẩn hóa.
- `report.md`: báo cáo tiếng Việt đọc được, hướng dẫn xử lý.
- `.pio/build/vqeaf_os/firmware.bin`: **chỉ sau khi `target_build=PASS` và xác nhận file tồn tại, không rỗng, mới hơn mã nguồn**.

Báo cáo `READY_NO_BUILD` nghĩa là cache đã đủ theo preflight, **không phải đã build thành công**. Báo cáo `BLOCKED_PREFLIGHT` (exit 2) nghĩa là thiếu môi trường, tuyệt đối không coi là lỗi C++.

| Exit | Nghĩa |
|---:|---|
| 0 | `SUCCESS` sau biên dịch target thực hoặc `READY_NO_BUILD` nếu dùng `--check-only` |
| 1 | Clean / compiler / linker / buildfs lỗi, thiếu file `.bin`, hoặc host test lỗi |
| 2 | Preflight lỗi: board, pin, PIO CLI, cache platform/toolchain/libdeps |
| 3 | Lỗi nội bộ script (mở thêm `internal_error.log`) |
| 130 | Build bị người dùng hủy |

## 3. Cách đọc lỗi

- `OFFLINE_CACHE_INCOMPLETE`: xem `report.json.missing`. Chuẩn bị thư mục `.platformio` và `.pio/libdeps/vqeaf_os` đúng phiên bản trên máy online, rồi chép sang.
- `BOARD_CONFIG`: kiểm tra GPIO/board/partition trong `board_preflight.log`. Không tự đổi chân phần cứng.
- `MISSING_LIBRARY`: tìm `fatal error: XYZ.h` trong `build.log`; xác minh thư viện ghim đúng phiên bản.
- `LINKER`: tìm `undefined reference to` trong `build.log`; báo lại tên symbol và các dòng đầu tiên có lỗi.
- `FLASH_OVERFLOW`: xem bản đồ linker/partition `app0=0x640000` và kích thước image.
- `NETWORK_ACCESS`: PlatformIO vẫn muốn tải metadata/gói; cache chưa sẵn hoặc bản Core cố kiểm tra kết nối. Script không tự khôi phục bằng mạng.
- `NO_FIRMWARE_BIN` hoặc `STALE_FIRMWARE`: không sử dụng `.bin` cũ; chạy lại clean mặc định.
- `SCRIPT_ERROR`: xem `internal_error.log`; gửi `report.json` và log liên quan.

Để gửi báo cáo an toàn, nên **kiểm tra log trước** nếu tên thư mục hoặc thông tin cá nhân xuất hiện trong output toolchain. Không gửi private QEAPP signing key; script không cần khóa ký.

## 4. Kiểm thử riêng script (không phải firmware)

```powershell
py -3 tools\test_offline_build.py
```

Bài test tự tạo một cache và **PIO giả** trong thư mục tạm; xác nhận: thiếu cache, phiên bản sai, kiểm tra-only, build mô phỏng thành công/thất bại, không có `.bin`, phân loại lỗi compiler, kiểm tra `buildfs`. PASS của test này **không có nghĩa đã biên dịch được cho ESP32-S3**.

Kiểm tra firmware thực: phải chạy lại `tools/build_offline.py` với **PlatformIO thật**, có đủ cache, rồi nạp thiết bị (không tự động flash trong script build).

## 5. Mô phỏng toàn bộ build mà không có PlatformIO / Xtensa toolchain

Đã bổ sung `--dry-run`, xuất log tách biệt tại `build_reports/offline/dry_run`.

```bat
call tools\build_offline.bat --dry-run --host-tests --buildfs
call tools\build_offline.bat --dry-run --simulate-failure linker
```

Dry-run chỉ kiểm tra thật cấu hình board bằng Python; tất cả thao tác PlatformIO/
compiler/linker/firmware hash đều **mô phỏng**, không tạo `firmware.bin`.
Chi tiết, tất cả case inject lỗi, tùy chọn PowerShell và cách đọc báo cáo:
[`DRY_RUN_PLATFORMIO_V233.md`](DRY_RUN_PLATFORMIO_V233.md).
