# VQEAF OS v2.4.3 — Sửa thao tác cài `.qeapp` và áp dụng `.vqeaf`

**Trạng thái:** Có mã nguồn và bộ kiểm thử host. Chưa có bằng chứng `pio run`/nạp ESP32-S3 thực tế hoặc video sau khi nạp; không đánh dấu lỗi thực tế là đã được giải quyết cho tới khi chạy lại trên thiết bị.

## 1. Phân tích video IMG_3535.MOV

| Thời điểm xấp xỉ | Quan sát trong video | Đối chiếu mã nguồn v2.4.2 |
|---|---|---|
| 16–18 giây | File Manager chọn file `VQEAF_Pixel_Snake_demo.qeapp` trên thẻ SD và bấm START | `FilesApp::handle` định tuyến đến `AppInstallerApp`, đợi quét/xác minh chữ ký |
| 19–21 giây | Màn “Opening” rồi bất ngờ chuyển sang Music | `InputManager` đã phát START ngắn khi nhấn; v2.4.2 còn phát START dài sau 650 ms và `main.cpp` ép chuyển sang Music ở *mọi* màn. Đây là một nguyên nhân có thể tái hiện trên PC và phù hợp với video; cần Serial để khẳng định chính là biến cố trên thiết bị của bạn. |
| 27–30 giây | Nhấn Themes từ Menu, màn “Opening”/Themes chưa kịp vẽ rồi hiển thị Music | Cũng khớp lỗi chuyển màn do nhấn START dài trong quá trình quét theme. |
| 37–39 giây | Qeafbrowser hiển thị “WiFi is not connected” | Là trạng thái mạng chưa kết nối trong video, chưa có bằng chứng lỗi HTTP riêng. |

## 2. Các thay đổi

1. **Phân biệt click/hold:** MENU được phát click sau khi thả hoặc hold trên 650 ms để mở Tasks. SELECT giữ trên 650 ms để đổi T9. START, OPTION, A và B là thao tác ngắn ngay khi nhấn, **không còn mở Music/Settings/Recovery/Lock do sự kiện nhấn dài thứ hai**. Có thể mở các ứng dụng này qua Home/Menu.
2. **Bộ cài đường dẫn trực tiếp:** Bấm mở `.qeapp` trong File Manager đi thẳng đến chẩn đoán gói vừa chọn mà không quét và xác minh lại toàn bộ thư mục Inbox trước khi phản hồi; khi bấm cài, vẫn dùng kiểm tra chữ ký và chép theo transaction/rollback như trước.
3. **Theme ngoài danh mục:** `.vqeaf` hợp lệ ở Downloads, Documents, thư mục lồng sâu hoặc ngoài giới hạn 16 theme giờ hiện thành một dòng *Opened file* có thể bấm Apply. Chỉ cho phép áp dụng sau khi parser kiểm tra lại; nếu theme bị lỗi giữ nguyên theme/NVS cũ.
4. **Thông báo và log:** Chia lỗi dài thành hai hàng vừa màn 240×320, đưa ra thông báo riêng khi `snake_pixel_demo.qeapp` sai khóa. In log `[VQEAF][FILE]`, `[VQEAF][QEAPP]`, `[VQEAF][THEME]`, `[VQEAF][KEY]` ở Serial 115200.
5. **Đường dẫn `.qeapp`:** Yêu cầu đường dẫn tuyệt đối, không `..` hoặc dấu gạch ngược, không quá 220 ký tự, và vẫn phải xác minh QEAPP/2 ECDSA P-256 hợp lệ.

### Giới hạn quan trọng: Pixel Snake DEMO

Gói demo đã ký key `0x534E414B` nhưng firmware `vqeaf_os` mặc định tin key `0x31534351`. **Vì vậy gói demo trong video không thể được cài trên firmware mặc định**, ngay cả khi lỗi nhấn dài đã sửa. Việc từ chối chữ ký này là chủ ý, không nên tắt kiểm tra để cài.

- Muốn thử **cài thành công trên firmware mặc định**, dùng `sd/System/Apps/Inbox/welcome.qeapp` hoặc `help_site.qeapp` từ bản phát hành tương ứng (được ký theo khóa firmware mặc định).
- Nếu phải thử Snake demo, sao lưu firmware và dữ liệu, sau đó build/nạp riêng `pio run -e vqeaf_snake_demo`; **các gói ký bằng khóa mặc định sẽ không được xác minh trong profile demo**.
- Với bản thương mại, tạo khóa publisher *riêng* bên ngoài repository, ký lại các gói ứng dụng và cấu hình firmware dùng public key cùng cặp. Không có quyền dùng lại khóa bí mật bản demo đã phát hành.

### Giới hạn theme

Firmware hiện tại hỗ trợ một tập con có kiểm soát của VQEAF Theme Studio: `@vqeaf 1.x`, envelope `<theme>`, palette RGB/ARGB và launcher overrides. Không diễn giải mọi vector/animation/ảnh của Theme Studio. Tệp phải <= 512 KiB và palette nằm trong 16 KiB đầu. Một theme vượt giới hạn vẫn bị từ chối có lý do.

## 3. Kiểm thử trên máy tính

```powershell
cd VQEAF-OS
py -3 tools/verify_v243.py
```

- Chạy lại 17/17 bài v2.4.2 (theme parser, browser HTTP, signed installer, toàn bộ firmware host link 33 đơn vị C++).
- Tái hiện logic nút của chính `InputManager` bằng GPIO/thời gian giả: giữ START >1.2 s **không** phát lệnh Music; MENU/SELECT click/hold loại trừ nhau.
- Kiểm thử direct load theme nằm sâu ngoài catalog, đường dẫn package lạ, chữ ký demo/production khác nhau.
- **Không thay thế kiểm thử trên bo mạch thật**, PSRAM, SDMMC và pin GPIO chưa được xác minh bằng video mới.

## 4. Quy trình test trên ESP32-S3 thật

1. **Sao lưu dữ liệu, không flash đè demo vào firmware sản xuất**. Kiểm tra đúng N16R8, chân ST7789 + SD và tốc độ Serial theo repo. Cài PlatformIO nếu chưa có.
2. Build/nạp firmware **mặc định** để thử gói production:

```powershell
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload --upload-port COM5
py -3 -m pip install pyserial
py -3 tools/capture_v243_serial.py --port COM5 --seconds 150
```

3. Mở Shell > `corediag`, `sddiag rw`. Đảm bảo hệ điều hành nhận SD và có quyền tạo `/System/Apps/Installed` + `/System/Themes`.
4. File Manager → `/System/Apps/Inbox/welcome.qeapp` → START **giữ gần 1 giây** để tái hiện tình huống video. Kỳ vọng vẫn ở **App Installer**, thấy “Signature verified”, rồi START lần nữa → hộp xác nhận → chọn Install → cài thành công; vào Applications → Welcome. **Không chuyển sang Music**.
5. File Manager → `/System/Themes/s60_green.vqeaf` → START giữ >1 giây; kỳ vọng Theme Manager hiện theme vừa mở, không chuyển sang Music. START/Apply, chuyển về Home để quan sát theme, reboot để xác nhận lưu NVS.
6. Thêm theme hợp lệ vào `/Documents/Custom/Nested/Palette/deep.vqeaf` (ngoài catalog depth2), mở qua File Manager; mong đợi dòng `Opened file | press Apply` rồi áp dụng. Test theme sai (không palette) phải báo lỗi mà không đổi theme hiện tại.
7. Thử `snake_pixel_demo.qeapp` trên firmware production: **kỳ vọng từ chối** với hướng dẫn khóa khác; không chạy vì khác trust anchor.
8. Xem log các nhãn `[VQEAF]`; gửi kèm video + Serial mới nếu một bước vẫn lỗi. Log Serial được thu thụ động; không tự gửi bất cứ dữ liệu nào qua mạng.

> Lưu ý: menu Tasks mở bằng giữ MENU; chọn Menu thông thường cần *thả* MENU. Bản sửa không đổi sơ đồ GPIO hay định dạng file QEAPP/VQEAF.
