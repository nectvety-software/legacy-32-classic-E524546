# UI v0.3 — S60 3×3 Launcher

## Mục tiêu

Launcher dùng bố cục menu icon 3×3 giống dòng Nokia/S60 cổ điển, nhưng vẫn giữ renderer nhẹ của ESP32-S3: không PNG, không framebuffer toàn màn hình, không animation tốn RAM.

## Bố cục

- Title/status chrome: `y=0..29`.
- Grid: 3 cột × 3 hàng trong vùng `y=33..192`.
- Mỗi cell khoảng 97×51 px; icon procedural 24×24 đặt ở giữa cell.
- Focus dùng nền slate, border trắng + beige và hai corner pixel để giữ chất pixel/S60.
- Hint strip `y=195..213` hiển thị tên/mô tả app đang focus.
- Softkey bar bắt đầu tại `y=216`.

## Map keypad

```text
MENU      = Home/Menu
UP/DOWN   = đổi hàng
LEFT/RIGHT= đổi cột
START     = Open / OK
SELECT    = Open (launcher alternate OK)
OPTION    = Options popup
A         = Back / Cancel ở app con
```

D-pad wrap ở mép: LEFT từ cột đầu sang cột cuối cùng cùng hàng, RIGHT ngược lại; UP/DOWN wrap theo cùng cột. Giữ D-pad 380 ms bắt đầu auto-repeat mỗi 120 ms.

## 9 icon mặc định

1. WiFi
2. Bluetooth
3. Music
4. File mgr
5. Collection
6. Settings
7. Apps
8. Clock
9. System

About vẫn nằm trong Applications/Options để không phá bố cục 3×3.
