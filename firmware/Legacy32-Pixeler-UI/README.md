# Legacy-32-Classic Pixeler UI

Firmware UI dua tren kien truc **Pixeler**, da tuy chinh rieng cho:

- Device: `legacy-32-classic E524546`
- MCU: `ESP32-S3-WROOM-1 (N16R8)`
- Flash: 16 MB QIO
- PSRAM: 8 MB OPI
- TFT: ST7789 IPS 2 inch, 240 x 320
- Dieu khien: 10 nut bam active-low
- SD card: SDMMC 1-bit

Du an da vendor san framework Pixeler trong `lib/pixeler`, vi vay chi can mo thu muc nay bang VS Code + PlatformIO va build.

## Nguon framework

Du an dua tren **Pixeler** cua Kolodieiev, phat hanh theo giay phep Apache-2.0. Ma framework duoc giu trong `lib/pixeler`; xem `LICENSE` de biet dieu khoan su dung.

## GPIO duoc giu nguyen

### TFT ST7789

| Tin hieu | GPIO |
|---|---:|
| LEDK / Backlight | 39 |
| D/C | 47 |
| CS | 14 |
| SCL / SCK | 48 |
| SDA / MOSI | 12 |
| RESET | 3 |

### Nut bam

| Nut | GPIO | Chuc nang UI |
|---|---:|---|
| KEY_UP | 7 | Len |
| KEY_DOWN | 46 | Xuong |
| KEY_LEFT | 45 | Trang truoc |
| KEY_RIGHT | 6 | Trang sau |
| KEY_MENU | 18 | Mo menu / quay ve Home |
| KEY_OPTION | 8 | Mo Cai dat nhanh |
| KEY_SELECT | 16 | Xac nhan |
| KEY_START | 17 | Mo Tep nhanh |
| KEY_A | 15 | Xac nhan / mo Tro choi nhanh |
| KEY_B | 5 | Quay lai |

Tat ca nut bam noi GPIO xuong GND khi nhan. Firmware dung `INPUT_PULLUP`.

### SD card

| Tin hieu | GPIO |
|---|---:|
| CD / DAT3 | 10 |
| CMD | 11 |
| CLK | 13 |
| DAT0 | 9 |

Firmware dung SDMMC 1-bit, nen duong truyen hoat dong la `CLK + CMD + DAT0`. `CD/DAT3 GPIO10` van duoc giu trong `board_config.h`, nhung khong bi driver dieu khien trong che do 1-bit.

## Giao dien da them

- Splash screen hien thi trang thai TFT, SD va PSRAM.
- Home Screen dang card toi uu cho man hinh doc 240 x 320.
- Menu ung dung co icon, focus cyan, scrollbar va bo dem vi tri.
- Giao dien chinh va cac menu phu da chuyen sang chuoi ASCII tieng Viet de font bitmap hien thi on dinh.
- Phim tat tren Home:
  - `SELECT` hoac `MENU`: mo menu.
  - `A`: mo Tro choi.
  - `START`: mo Tep.
  - `OPTION`: mo Cai dat.
- Da vo hieu hoa I2S/MP3 de khong chiem trung GPIO cua TFT, nut bam va SD.

## Build tren Windows

1. Cai VS Code.
2. Cai extension PlatformIO IDE.
3. Mo thu muc `Legacy32-Pixeler-UI`.
4. Chon environment `legacy32`.
5. Build hoac Upload bang thanh cong cu PlatformIO.

Lenh tu Terminal:

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

Co the dung cac tep:

- `build_windows.bat`
- `upload_windows.bat`
- `monitor_windows.bat`
- `clean_windows.bat`

Neu PlatformIO khong tu tim thay cong COM, them vao `[env:legacy32]`:

```ini
upload_port = COM5
monitor_port = COM5
```

Thay `COM5` bang cong cua bo mach trong Windows Device Manager.

## Cau truc

```text
Legacy32-Pixeler-UI/
├─ platformio.ini
├─ partitions-s3.csv
├─ src/
│  ├─ config/                 # GPIO va cau hinh board
│  ├─ context/                # Splash, Home, Menu, Files, WiFi, Games...
│  └─ main.cpp
├─ lib/
│  └─ pixeler/                # Framework Pixeler da vendor
├─ tools/
│  └─ check_gpio.py
└─ docs/
   └─ CHANGES.md
```

## Kiem tra GPIO

```powershell
python tools\check_gpio.py
```

Script se bao loi neu pin bi thay doi, trung nhau, hoac thieu dinh nghia bat buoc.

## Luu y

- The SD nen format FAT32.
- Ban dau tien co the build lau do PlatformIO tai toolchain va framework ESP32.
- Goi nay la ma nguon da sua. Khong su dung `firmware.bin` cu trong tep dau vao, vi no khong chua giao dien moi.
