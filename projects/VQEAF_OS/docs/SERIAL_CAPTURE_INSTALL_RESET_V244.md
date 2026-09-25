# VQEAF OS — Thu thập Serial 115200 và đánh dấu reboot trong lúc cài `.qeapp`

**Công cụ:** `tools/capture_install_reset.py` (v1). **Firmware mục tiêu:** VQEAF OS v2.4.4. Không thay đổi firmware, mã ký số hoặc chức năng installer. Có thể dùng công cụ với firmware cũ hơn nhưng có thể không có marker `[QEAPP][INSTALL][...]`.

## 1. Chuẩn bị

- PC Windows 10/11 với Python 3.10+, cáp USB *truyền dữ liệu*, driver USB-UART hoặc USB CDC hoạt động.
- Dùng **một** chương trình đọc COM: đóng `pio device monitor`, Arduino Serial Monitor và các trình giám sát khác trước khi mở công cụ này.
- Công cụ kết nối baud **115200**; có thể tìm cổng bằng `--list-ports`.
- Công cụ không nạp firmware, không chủ động điều khiển RESET và mặc định không bật DTR/RTS. Với firmware dùng native USB CDC mà không thấy dữ liệu, thử `--dtr` sau khi kiểm tra đặc tính mạch; bật DTR trên một số board có thể làm reset. Ngay cả việc mở cổng serial cũng có thể reset trên vài loại adapter.

Windows (giải nén **gói công cụ độc lập** hoặc **full source**):

```powershell
cd VQEAF-OS
py -3 -m pip install -r tools\serial_requirements.txt
py -3 tools\capture_install_reset.py --list-ports
# Cách dễ nhất: nhấp đúp tools\capture_install_reset.bat
py -3 tools\capture_install_reset.py --port COM5 --baud 115200 --out build_reports\device_serial
```

Linux/macOS (nếu có):

```bash
python3 -m pip install -r tools/serial_requirements.txt
python3 tools/capture_install_reset.py --list-ports
python3 -u tools/capture_install_reset.py --port /dev/ttyACM0 --baud 115200
```

Nếu dùng USB CDC và nhận **0 byte** (đã xác minh firmware phát log qua USB Serial), thử `--dtr` (không bật `--rts` trừ khi sơ đồ board yêu cầu):

```powershell
py -3 tools\capture_install_reset.py --port COM5 --dtr
```

Công cụ có thể tự tìm cổng khi chỉ một cổng tồn tại hoặc tìm thấy duy nhất USB VID `303A`: `--port auto`. Nếu có nhiều cổng, chỉ định `--port COMx`. Khi USB tái nhận diện, chế độ reconnect mặc định thử mở lại liên tục; nếu tên COM đổi, chạy với `--port auto` và kiểm tra thứ tự kết nối.

## 2. Quy trình tái hiện lỗi cài app

1. Mở trình ghi log **trước** thao tác cài, xác nhận trên cửa sổ đã in `PORT_CONNECTED`.
2. Nhấn RESET **một lần trước phép thử** để ghi ROM boot baseline (giúp xác định chân lý reset; nếu đánh dấu chủ động hãy nhập `r` + Enter ngay trước khi bấm RESET).
3. Trong cửa sổ thu thập, nhập `i` + Enter **ngay trước khi nhấn Install** trên VQEAF OS. `USER_INSTALL_START` là dấu thời gian từ phía PC, không phải firmware.
4. Chọn `welcome.qeapp` khớp khóa production và bắt đầu Install. Ghi nhận marker firmware `[QEAPP][INSTALL][10]`, `[20]`, `[50]`, ...
5. Nếu thiết bị reset, **không tắt logger**, chờ nó tự tìm lại COM, ghi `ESP-ROM:esp32s3`, `rst:0x...`, `[QEAPP][PREVIOUS_RESET]` và cả `Guru Meditation`/`Backtrace` nếu có.
6. Tiếp tục vài giây, nhập `q` + Enter hoặc Ctrl+C để kết thúc. Gửi `report.md`, `events.jsonl` và `serial_timestamped.log` khi cần phân tích. Nếu file văn bản bị cắt/lỗi encoding, giữ `raw_uart.bin` để phục hồi từ byte gốc.

Có thể nhập `t` ngay trước khi **Apply theme**, `r` trước một reset cố ý, `m <ghi chú>` để đánh dấu tùy chỉnh, hoặc Enter để đặt dấu mốc nhanh. Bạn có thể dùng `--duration 240 --no-keyboard` nếu cần ghi tự động trong 4 phút.

**Bảo mật:** Log có thể chứa SSID, tên tệp, URL, khóa token hoặc dữ liệu nhạy cảm. Không gửi `raw_uart.bin` hay toàn bộ log lên issue công khai nếu chưa đọc và loại bỏ thông tin riêng tư.

## 3. Các tệp thu được

Mỗi lượt chạy tạo một thư mục **riêng**: `build_reports/device_serial/vqeaf_uart_YYYYMMDD_HHMMSS_mmmmmm/`.

```text
raw_uart.bin            Byte nhận được nguyên trạng, không giải mã, không tự xóa dữ liệu
serial_timestamped.log  Mọi dòng UART đã nhận + dấu thời gian địa phương đến ms
events.jsonl            Sự kiện host / giai đoạn QEAPP / panic / boot theo thời gian
 report.md               Báo cáo tóm tắt dễ đọc
 summary.json            Thống kê có thể dùng để kiểm thử tự động
```

Thời gian là **đồng hồ PC tại thời điểm nhận**, không phải thời gian thực thi chính xác bên trong ESP32. Khi UART ngắt, các byte xảy ra trước khi cổng mở lại **không thể thu hồi**; công cụ không thể đảm bảo “toàn bộ” dữ liệu ngoài khoảng đã kết nối.

### Giải thích nhãn

| Nhãn | Ý nghĩa |
|---|---|
| `INSTALL_STAGE` | Firmware phát marker giai đoạn đã gặp; xem trường `stage` trong `events.jsonl` |
| `BOOT_DURING_INSTALL_SUSPECTED` | Có **chữ ký boot ROM** sau khi ghi nhận Install mà chưa nhận completion, không kết luận nguyên nhân |
| `BOOT_AFTER_MANUAL_RESET` | Boot có dấu `r` của người dùng trước đó; không tự gán là lỗi cài |
| `BOOT_OBSERVED` | Đã nhận dữ liệu boot; có thể là boot ban đầu sau khi mở COM |
| `PORT_DISCONNECTED` / `PORT_RECONNECTED` | USB/COM ngắt và kết nối lại; **không** tự chứng minh ESP32 reset |
| `PANIC_OR_ABORT_TEXT` / `BACKTRACE_TEXT` | Thiết bị thực sự phát đoạn thông báo panic / backtrace |
| `BROWNOUT_TEXT`, `TASK_WATCHDOG_TEXT`, `STACK_FAILURE_TEXT` | Từ khóa hiển thị trong log; phải đối chiếu reset reason và cấu hình phần cứng |
| `PREVIOUS_RESET_MARKER` | VQEAF OS phát bản ghi giai đoạn cài đặt từ RTC sau reboot (nếu RTC còn dữ liệu); cũng có thể làm dấu boot dự phòng khi native USB CDC không nhận được ESP-ROM |

Giai đoạn cuối cùng được báo trong `summary.json`. Giai đoạn in ra không chứng minh thao tác đó là nguyên nhân panic; lỗi có thể bắt nguồn từ nơi khác (nguồn, SD, bộ nhớ).

## 4. Tự kiểm tra tool khi chưa có ESP32

```powershell
# Tạo dữ liệu giả để xem bố cục báo cáo; KHÔNG dùng nó làm chứng cứ phần cứng
py -3 tools\capture_install_reset.py --demo --out build_reports\serial_demo
py -3 tools\test_serial_install_capture.py
```

Có **17 phép thử mô phỏng** cho dữ liệu bị chia mảnh, byte lỗi UTF-8, giữ byte gốc, phát hiện giai đoạn, boot lặp nhanh, USB disconnect/reconnect, bàn giao báo cáo và bản demo. Bản phát hành này **chưa chạy với ESP32-S3 thực**; khi có log thiết bị mới có thể xác định nguyên nhân reset.

## 5. Câu lệnh bổ sung

```text
--list-ports              Liệt kê cổng hiện có
--port auto               Tự chọn duy nhất ESP VID=303A, hoặc cổng duy nhất
--baud 115200             Tốc độ mặc định
--duration 300            Thu tối đa 300 giây; mặc định 0 (đến khi dừng)
--no-reconnect            Ngừng khi mất USB thay vì đợi COM quay lại
--retry-seconds 1.5       Nhịp mở lại cổng
--dtr                     Bật DTR khi native USB CDC yêu cầu (có thể reset board)
--quiet                   Không in mỗi dòng UART ra console (vẫn ghi nguyên vẹn)
--output old_path.log     Legacy: tạo thêm bản sao log với tên cũ sau phiên ghi
```

**Mẫu dữ liệu trong gói được sinh bởi `--demo` hoàn toàn giả lập.** Tránh nhầm với bằng chứng phần cứng.
