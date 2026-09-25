# QEAPP/2 — Cặp khóa ký ứng dụng mới (ECDSA P-256)

**Bộ khóa này MỚI ĐƯỢC SINH RA; KHÔNG phải khóa riêng hiện tại của bản VQEAF-OS Release v2.5.1 + Back r2.** Không thể tính private key từ public key đã ghim trong firmware. Để thiết bị chấp nhận ứng dụng do cặp khóa này ký, bạn phải ghim `QeappTrustKey.h` mới và **biên dịch/nạp lại firmware**. Các ứng dụng cũ được ký bằng public key khác có thể bị từ chối sau khi thay khóa.

## Tệp trong gói

- `qeapp_private.pem`: **KHÓA RIÊNG — BÍ MẬT** theo chuẩn PKCS#8 PEM; dùng cho trường `--sign-key` trong Studio/CLI.
- `qeapp_private.key`: bản **giống hệt** `qeapp_private.pem`, chỉ đổi đuôi để dùng với phần mềm yêu cầu `.key`. **ĐỪNG** coi đây là một cặp khóa thứ hai.
- `qeapp_public.pem`, `qeapp_public.key`: cùng một khóa **CÔNG KHAI** theo chuẩn SubjectPublicKeyInfo PEM; không dùng những tệp này để ký.
- `QeappTrustKey.h`: public key dạng điểm SEC1 không nén 65 byte, đúng bố cục mà firmware VQEAF-OS yêu cầu.
- `verify_keypair.py`: kiểm tra các tệp có cùng một cặp khóa, thử ký/xác minh ECDSA và so sánh header firmware.

**Key ID:** `0x31534351` (tương thích với mặc định của `tools/build_qeapp.py`); trùng ID **không có nghĩa** trùng khóa với bản Release đã nạp. Firmware xác minh **cả ID lẫn chữ ký bằng public key được ghim**.

## Cài cho firmware riêng của bạn (trên Windows)

**1. LƯU KHÓA RIÊNG AN TOÀN.** Không commit `.pem`/private `.key`, không chép lên thẻ SD, không gửi cho cộng tác viên, không đưa vào bug report, không chia sẻ ZIP công khai. Gói tải trong hội thoại không phải phương án quản lý khóa sản xuất. Để phát hành thương mại nên tạo một cặp **mới trên máy tin cậy của chính bạn** bằng `tools/qeapp_keys.py` và sao lưu mã hóa.

**2. Lấy đúng checkout firmware của bạn**, sao lưu tệp public header hiện có trước khi đổi:

```powershell
cd D:\Projects\VQEAF-OS
Copy-Item src\services\QeappTrustKey.h src\services\QeappTrustKey.original.h
Copy-Item D:\Private\QeappTrustKey.h src\services\QeappTrustKey.h
pio run -e vqeaf_os
```

Bản `QeappTrustKey.h` chứa **chỉ public key**, có thể đưa vào repository firmware **nếu chính bạn chủ động muốn**. Không commit `QeappTrustKey.original.h` nếu không cần. Hãy thử bộ cài riêng trên board thử nghiệm trước; cập nhật full `.img` từ Release gốc sẽ khôi phục public key cũ.

**3. Tạo và ký gói `.qeapp` kiểu text/web mặc định:**

```powershell
# Trong repository QEAPP-Studio; thay mọi đường dẫn tùy máy:
py -3 tools\qstudio.py build D:\Projects\MyApplication `
  --firmware-root D:\Projects\VQEAF-OS `
  --sign-key D:\Private\qeapp_private.pem `
  --key-id 0x31534351 `
  -o D:\Build\MyApplication.qeapp
```

Hoặc dùng trực tiếp firmware signer:

```powershell
cd D:\Projects\VQEAF-OS
py -3 tools\build_qeapp.py --id myapp --name MyApp --version 1.0.0 `
  --type text --text D:\Projects\MyApplication\content.txt `
  --sign-key D:\Private\qeapp_private.pem --key-id 0x31534351 `
  -o D:\Build\MyApplication.qeapp
```

`type=lua` tương tác **không hoạt động trên firmware stock** chỉ nhờ sở hữu khóa; yêu cầu firmware Lua beta tương ứng và cấu hình trust/ID của nó.

## Kiểm thử trước khi sử dụng

```powershell
py -3 -m pip install cryptography
py -3 verify_keypair.py
```

Lệnh phải báo `PASS`. Kiểm thử đó **chỉ kiểm cặp khóa**, không chứng minh firmware đang chạy đã chấp nhận khóa này. Việc ghim public key chỉ thực sự có tác dụng sau khi build và nạp firmware mới thành công.

## Tạo khóa sản xuất an toàn hơn trên máy của bạn

```powershell
# Trên checkout VQEAF-OS của bạn, cẩn thận đừng ghi đè header cũ ngoài ý muốn:
py -3 tools\qeapp_keys.py `
  --private D:\Private\qeapp_production.pem `
  --header D:\Private\QeappTrustKey.production.h `
  --key-id 0x31534351
```

Sau đó tự ghim public header vào bản firmware bạn phát hành. Đối với khóa sản xuất, ưu tiên tạo và lưu hoàn toàn trên máy do bạn kiểm soát.
