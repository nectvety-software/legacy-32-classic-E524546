# VQEAF OS v2.4.0 — Lõi quản lý/cài đặt ứng dụng

**Nguồn:** nâng cấp trực tiếp từ v2.3.6, không thay GPIO, theme `.vqeaf`,
Home/Menu 240x320 dọc hoặc thuật toán RGB565/RLE của 12 icon đã tối ưu.

## Đã làm

1. Cài mới ứng dụng `.qeapp` **QEAPP/2 có chữ ký ECDSA P-256** trên microSD.
   Tệp không ký, tệp hỏng, loại app không hỗ trợ tiếp tục bị từ chối.
2. **Cập nhật tại chỗ** khi tệp mới cùng `id`, phiên bản cao hơn và cùng khóa
   nhà phát hành firmware tin cậy. Từ chối cài lại/giảm phiên bản, không yêu cầu
   người dùng gỡ v1 trước khi cập nhật v2.
3. Bản cũ được giữ ở `.backup-<id>` trong lúc kích hoạt bản mới; xác minh
   đầy đủ staged receipt/trailer/hashes, tự khôi phục bản cũ khi thất bại.
4. Quét khôi phục sau khi bật máy/cắm lại thẻ SD: ưu tiên bản cuối hợp lệ;
   không tự kích hoạt thư mục staging chưa được người dùng xác nhận.
5. Bộ nhớ dữ liệu riêng theo ứng dụng dưới `/System/Apps/Data/<id>/` với
   3 slot, 16 KiB/slot, 32 KiB/app; **dữ liệu không bị xóa ngầm khi update
   hoặc uninstall**. Trong App installer có lệnh Reset app data được xác nhận.
6. UI cài đặt trên màn hình 240×320: chi tiết chữ ký, Install/Update,
   tiến trình ghi, thông báo lỗi đầy đủ, tùy chọn Recover installs.
7. `yield()` trên ESP32 ở các vòng hash/copy dài, tránh chặn FreeRTOS idle
   không cần thiết; installer vẫn là thao tác đồng bộ theo sự kiện.

## Kiểm thử thực sự đã chạy

- `tools/verify_v240.py`: **7/7 bài PASS** (board cấu hình, C++ host 29/29
  translation units + link, chữ ký cũ, cập nhật/rollback/data mới, icon, grid).
- `tools/build_offline.py --dry-run --check-only`: **PASS** kiểm tra tĩnh
  board/GPIO và xuất báo cáo mô phỏng.
- Test nâng cấp mô phỏng hỏng thao tác SD rename, reset giữa update, stage
  dang dở, tệp lạ không được xóa, data preserved và progress callback.

**Chưa chạy:** `pio run -e vqeaf_os` bằng toolchain Xtensa, nạp trên ESP32-S3
và kiểm tra thực tế SD power loss. **Không có firmware.bin**; chỉ có nguồn.

## Cài đặt/chạy thử trên Windows

```powershell
cd VQEAF-OS
py -3 tools/verify_v240.py
.\tools\build_offline.bat --check-only
.\tools\build_offline.bat

# Khi PlatformIO và mọi dependency đã nằm trong cache:
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Khi có kết quả từ mạch, đọc log `[QEAPP][RECOVERY]`, kết quả trong
`build_reports/offline/`, và gửi `build.log` nếu build không thành công.

## Giới hạn kỹ thuật và giấy phép

- Đây là **trình cài đặt lấy cảm hứng từ trải nghiệm S60**, không phải bản
  sao Symbian/S60 hay môi trường chạy SIS/SISX. Không phân phối mã/asset S60.
- `.qeapp` hiện mới chạy hai loại ứng dụng an toàn `web` và `text`; các
  ứng dụng Lua/native độc lập cần một runtime mới được sandbox, chưa có.
- Signature xác nhận đúng khóa công khai nhúng trong firmware, không thể
  ngăn việc tráo thẻ SD bằng phần cứng; FAT rename không có đảm bảo hoàn
  toàn khi mất điện.

Xem `docs/QEAPP_S60_APP_MANAGER_V240.md` để đọc toàn bộ thiết kế và cách tạo
khóa ký riêng. Trước khi phát hành thương mại hãy thay khóa public dùng cho
sample/dev, giữ private key ở máy ký riêng, rồi rebuild/re-sign toàn bộ app.
