# VQEAF OS v2.4.4 — Chẩn đoán reset khi cài `.qeapp`

**Trạng thái:** mã nguồn candidate đã kiểm tra bằng bộ dựng C++ trên PC. **Chưa nạp ESP32-S3 thật**; chưa thể kết luận nguyên nhân reboot chỉ dựa vào dấu hiệu trên màn hình. Không đổi chân GPIO, kiểu chữ ký hoặc quyền cài gói QEAPP/2.

## Các thay đổi

1. Chuyển 2 bộ đệm manifest 2 KiB ra heap cấp phát có kiểm tra OOM. Trong `recoverTransactions`, chuyển 2 danh sách snapshot 1.6 KiB ra heap; giảm stack đỉnh các hàm gọi lồng nhau.
2. Loại bỏ kiểm tra chữ ký của **mọi file trong Inbox** mỗi lần mở lại App Manager. Khi chọn file, `openDetails()` vẫn kiểm tra chữ ký; `install()` **kiểm tra lại chữ ký nguồn**, xác minh hash khi sao chép và chữ ký receipt tại staging/final. Không bỏ qua bảo mật.
3. Bộ đệm icon 2048 byte chuyển từ biến cục bộ sang đối tượng màn hình tĩnh. Giảm stack tại luồng tải icon app đã ký.
4. Chặn cài khi free internal RAM < 32 KiB hoặc largest free internal block < 16 KiB: thông báo thay vì thử cấp phát rồi panic. Ngưỡng ban đầu cần đối chiếu số đo phần cứng.
5. Bổ sung Serial `[QEAPP][INSTALL][phase]` ghi free internal heap / largest block / stack high-water mark, cùng RTC marker giai đoạn đang cài (không phụ thuộc NVS ghi mỗi giai đoạn). Sau reboot, `[QEAPP][PREVIOUS_RESET]` hiển thị marker nếu RTC còn.

### Log mẫu và ý nghĩa

```text
[QEAPP][INSTALL][10] install_entry heap=... largest=... stack_hwm=...
[QEAPP][INSTALL][20] source_signature_verified ...
[QEAPP][INSTALL][25] catalog_recovery_begin ...
[QEAPP][INSTALL][30] catalog_recovered ...
[QEAPP][INSTALL][40] staging_directory_created ...
[QEAPP][INSTALL][50] copy_complete ...
[QEAPP][INSTALL][55] reverify_source_begin ...
[QEAPP][INSTALL][60] verify_staged_receipt_begin ...
[QEAPP][INSTALL][70] activation_rename_begin ...
[QEAPP][INSTALL][80] final_integrity_check_begin ...
[QEAPP][INSTALL][90] catalog_refresh_begin ...
[QEAPP][INSTALL][100] install_complete ...
```

Nếu xuất hiện `Brownout detector was triggered`, kiểm tra nguồn 3,3V **theo đúng đặc tả mạch**, đặc biệt khi SD/TFT hoạt động. Nếu `Task watchdog`, kiểm tra giai đoạn gần nhất và thời gian xử lý. `Guru Meditation`, `stack canary` hoặc `Stack smashing` chỉ ra loại lỗi khác; giữ nguyên backtrace. RTC mất dữ liệu khi sụt nguồn nên log không phải bằng chứng duy nhất. `stack_hwm` là số byte theo API của ESP-IDF mục tiêu; đo trực tiếp trên board để quyết định điều chỉnh loopTask.

## Cách xác minh trên Windows

```powershell
cd VQEAF-OS
py -3 tools/test_v244_install_reset.py
py -3 tools/verify_v243.py
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
py -3 -m pip install pyserial
py -3 tools/capture_install_reset.py --port COM5 --baud 115200
```

Sau khi mở trình ghi Serial, RESET board để chụp lý do reset boot. Chép `welcome.qeapp` (đúng khóa production) vào `/System/Apps/Inbox` thẻ SD; chọn từng file -> Install -> giữ nguyên nguồn -> quan sát. Test package Snake demo phải **bị từ chối** trên bản production; không thay khóa ký production để lách lỗi. Kiểm tra app có còn sau reboot và thử áp dụng theme độc lập.

**Nếu vẫn reset:** gửi toàn bộ log từ trước `[QEAPP][INSTALL][10]` đến sau `[QEAPP][PREVIOUS_RESET]`, kèm kết quả `pio run`, hình nguồn/SD và loại `.qeapp` được thử. Chỉ khi có log mới khoanh vùng watchdog/stack/SD/brownout thực tế.
