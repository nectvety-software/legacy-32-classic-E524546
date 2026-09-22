# Thay doi cho Legacy-32-Classic

## UI

- Tao Splash Screen moi cho 240x320.
- Tao Home Screen card-based, hien thi SD, PSRAM va uptime.
- Tao menu ung dung moi voi icon, focus, scrollbar va phim tat.
- Doi cac chuoi giao dien chinh sang tieng Viet ASCII.
- Tinh chinh bang mau RGB565 toi/cyan/mint.

## Hardware

- Giu nguyen tat ca GPIO TFT, 10 nut bam va SD card.
- KEY_SELECT duoc map thanh `BTN_OK`.
- KEY_B duoc map thanh `BTN_BACK`.
- SDMMC duoc sua de dung dung host slot 0 tren ESP32-S3.
- Tat ca duong SD khong dung trong che do 1-bit duoc dat `GPIO_NUM_NC`, tranh chiem GPIO0.
- `CD/DAT3 GPIO10` van duoc khai bao trong board map.
- I2S duoc dat disconnected va MP3 context da duoc xoa khoi ban tuy chinh de tranh trung pin.

## Packaging

- Xoa `.pio`, object, ELF va firmware cu.
- Vendor framework trong `lib/pixeler` de du an doc lap.
- Them PlatformIO config cho N16R8 va partition OTA 16 MB.
- Them script kiem tra GPIO va batch build/upload/monitor.
