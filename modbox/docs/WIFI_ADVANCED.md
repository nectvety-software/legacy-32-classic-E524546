# WiFi Manager - Advanced Features

## Menu Structure

```
WiFi Manager
├── Quet WiFi
│   ├── Quet diem truy cap
│   ├── Quet AP truc tiep (Live)
│   ├── Quet tram (Stations)
│   ├── Tinh trang kenh
│   ├── Quet toan bo (Sweep)
│   └── Tro ve
├── Mang da luu
├── Thuoc tinh
├── AP Mode
└── Thoat
```

## Quet WiFi (Scanning)

### Quet diem truy cap
- Quét tất cả các điểm truy cập WiFi
- Hiển thị: SSID, RSSI, Channel, Encryption
- Màu sắc theo cường độ tín hiệu:
  - Xanh lá: >= -60 dBm
  - Vàng: >= -70 dBm
  - Đỏ: < -70 dBm

### Quet AP truc tiep (Live Scan)
- Quét trực tiếp trong thời gian thực
- Hiển thị mạng mới ngay khi được phát hiện
- Tự động cập nhật màn hình

### Quet tram (Stations)
- Quét các thiết bị kết nối với AP
- Hiển thị MAC, RSSI

### Tinh trang kenh
- Hiển thị mức độ tắc nghẽn của từng kênh (1-13)
- Thanh màu biểu thị số lượng AP trên kênh

### Quet toan bo (Sweep)
- Quét WiFi + Stations + BLE
- Lưu kết quả vào thẻ SD
- Định dạng CSV tương thích Kismet/Wigle

## Nham Muc Tieu (Targeting)

### Chon AP
- Chọn điểm truy cập làm mục tiêu
- Đánh dấu [X] khi đã chọn
- Hỗ trợ chọn nhiều mục tiêu

### Xem danh sach
- Xem danh sách các AP đã chọn
- Hiển thị Channel và Encryption

### Xoa muc tieu
- Xóa tất cả các mục tiêu đã chọn

### Bo chon tat ca
- Bỏ chọn tất cả các AP

## Tan Cong (Offense)

### Huy xac thuc (Deauth)
- Gửi khung Deauth đến thiết bị mục tiêu
- Chế độ Broadcast (FF:FF:FF:FF:FF:FF)
- Hiển thị thông tin AP đang tấn công

### Canh bao
⚠️ **Chỉ thực hiện trên mạng bạn sở hữu hoặc có quyền kiểm tra!**

## Mang (Network)

### Ket noi WiFi
- Kết nối với mạng đã lưu
- Hiển thị danh sách mạng

### Ngat ket noi
- Ngắt kết nối WiFi hiện tại

### Quet thiet bi LAN
- Quét thiết bị trong mạng LAN
- Hiển thị IP và dịch vụ

### Quet cong (Ports)
- Quét cổng trên thiết bị
- Kiểm tra: HTTP, SSH, FTP, DNS...

### Kiem tra SSH
- Kiểm tra cổng SSH trên thiết bị

## Dau Ra (Output)

### Che do USB Dongle
- Chế độ USB Dongle cho Wireshark
- Truyền dữ liệu WiFi real-time

### Luu vao SD
- Lưu kết quả vào thẻ SD
- Định dạng CSV

## Virtual Keyboard

```
┌──────────────────────────┐
│ Command Input            │
├──────────────────────────┤
│ [1234567890-_|          │
│  [QWERTYUIOP]           │
│   [ASDFGHJKL]           │
│    [ZXCVBNM]            │
│ [.!@#$%^&*()+=]        │
├──────────────────────────┤
│ [DEL][   SPACE   ][OK][CLR] │
└──────────────────────────┘
```

## Controls

| Button | Action |
|--------|--------|
| UP/DOWN | Navigate menu |
| LEFT/RIGHT | Change value |
| A/SELECT | Confirm |
| B/MENU | Back/Cancel |
