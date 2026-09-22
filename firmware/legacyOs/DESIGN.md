# Tài Liệu Thiết Kế Hệ Điều Hành CyberOS v2.0
## Hướng dẫn triển khai giao diện người dùng trên màn hình ST7789 TFT (1.54" và 2.0") sử dụng ESP32-S3 và PlatformIO

Tài liệu này cung cấp hướng dẫn chi tiết về mặt kỹ thuật, sơ đồ phần cứng, bảng mã màu, cấu trúc giao diện và phương pháp tối ưu hóa đồ họa để người dùng tự triển khai hệ điều hành **CyberOS v2.0** trên các thiết bị phần cứng thật sử dụng chip **ESP32-S3** kết hợp màn hình TFT ST7789 (kích thước 1.54" hoặc 2.0").

---

## 1. Sơ Đồ Khối Phần Cứng & Bản Đồ GPIO 

Dưới đây là cấu hình GPIO chính thức được thiết kế để đảm bảo hiệu năng tối đa của bus SPI, I²C và các linh kiện ngoại vi khác mà không gây xung đột tài nguyên phần cứng trên ESP32S3.

```
                  +-----------------------------------+
                  |            ESP32-S3               |
                  +-----------------------------------+
                    |   |   |   |   |        |   |   
     +--------------+   |   |   |   +----+   |   +-------------+
     |                  |   |   |        |   |                 |
+----v----+             |   |   |    +---v---v---+           +-v------+
| ST7789  |             |   |   |    |  BME280   |           | Buzzer |
|  Video  |             |   |   |    | Ambient   |           |  Sound |
+---------+             |   |   |    +-----------+           +--------+
  BL: IO39              |   |   |      SDA: IO18              PWM: IO41
  DC: IO47              |   |   |      SCL: IO06
  CS: IO14              |   |   |
 SCLK: IO48             |   |   +-------------------+
 MOSI: IO12             |   |                       |
  RST: IO03             |   +-----------+           |
                        |               |           |
                  +-----v-----+   +-----v-----+   +-v---------+
                  |  microSD  |   | Joysticks |   |  Battery  |
                  |  Storage  |   | 6-Buttons |   | Det/ADC   |
                  +-----------+   +-----------+   +-----------+
                    MISO: IO09      UP:    IO40     BAT_ADC: IO1
                    MOSI: IO11      DOWN:  IO05
                    SCLK: IO13      LEFT:  IO04
                    CS:   IO10      RIGHT: IO45
                    DET:  IO38      OK/A:  IO37
                                    BACK/B:IO36
```

### Bảng Ánh Xạ GPIO Chi Tiết

| Ngoại vi / Linh Kiện | Định nghĩa Chân | Chức năng chi tiết | Mô tả Trạng thái & Điện áp |
| :--- | :--- | :--- | :--- |
| **ST7789 TFT** | `TFT_BL` = **GPIO 39** | Bật tắt đèn nền | OUTPUT, Kích hoạt ở mức điện áp **HIGH** |
| | `TFT_DC` = **GPIO 47** | Lựa chọn Lệnh / Dữ liệu | OUTPUT, Thao tác ghi trực tiếp từ SPI |
| | `TFT_CS` = **GPIO 14** | Chip Select | OUTPUT, Kéo xuống mức điện áp thấp khi giao tiếp |
| | `TFT_SCLK` = **GPIO 48** | Xung nhịp SPI chính | Chân xung tốc độ cao 40 MHz (SPI2_HOST) |
| | `TFT_MOSI` = **GPIO 12** | Đường truyền dữ liệu SPI | Master Out Slave In |
| | `TFT_RST` = **GPIO 3** | Khởi động lại màn hình | OUTPUT, Kéo xuống mức thấp để Reset mạch cứng |
| **Bàn Phím / Joystick**| `BTN_UP` = **GPIO 40** | Nút di chuyển lên | Cấu hình `INPUT_PULLUP` (Kích hoạt ở mức **LOW**) |
| (Điều khiển số) | `BTN_DOWN` = **GPIO 5** | Nút di chuyển xuống | Cấu hình `INPUT_PULLUP` (Kích hoạt ở mức **LOW**) |
| | `BTN_LEFT` = **GPIO 4** | Nút di chuyển trái | Cấu hình `INPUT_PULLUP` (Kích hoạt ở mức **LOW**) |
| | `BTN_RIGHT` = **GPIO 45**| Nút di chuyển phải | Cấu hình `INPUT_PULLUP` (Kích hoạt ở mức **LOW**) |
| | `BTN_A` / **OK** = **GPIO 37**| Phím Chọn / Xác nhận | Cấu hình `INPUT_PULLUP` (Kích hoạt ở mức **LOW**) |
| | `BTN_B` / **BACK** = **GPIO 36**| Phím Quay lại / Thoát | Cấu hình `INPUT_PULLUP` (Kích hoạt ở mức **LOW**) |
| **Mạch Thẻ nhớ SD** | `SD_CS` = **GPIO 10** | Chọn chip Thẻ nhớ | SPI CS Thẻ MicroSD |
| (Chuẩn giao tiếp SPI) | `SD_MOSI` = **GPIO 11** | Truyền dữ liệu SPI | Master Out Slave In cho SD |
| | `SD_SCLK` = **GPIO 13** | Xung thẻ nhớ SPI | Tối đa đạt 20 MHz |
| | `SD_MISO` = **GPIO 9** | Nhận dữ liệu từ SD | Master In Slave Out |
| | `SD_DET` = **GPIO 38** | Phát hiện thẻ nhớ | Đóng cắt mạch cơ học phát hiện cắm thẻ |
| **Cảm biến BME280** | `I2C_SDA` = **GPIO 18** | Đường truyền dữ liệu I²C | Cần điện trở kéo lên bên ngoài (Pull-up 4.7k) |
| (Khí tượng môi trường) | `I2C_SCL` = **GPIO 06** | Đường phát xung I²C | Tần số giao tiếp Fast Mode 400 KHz |
| **Bộ Phát Âm Thanh** | `BUZZ_PIN` = **GPIO 41**| Đầu ra còi chíp | OUTPUT, Sử dụng tín hiệu PWM để phát nốt nhạc |
| **Đo Dung Lượng Pin**| `PIN_BAT` = **GPIO 1** | Giám sát mức pin | ANALOG INPUT (Qua phân áp tỉ lệ 1:2) |

---

## 2. Đặc Tả Kỹ Thuật Hai Cỡ Màn Hình TFT ST7789

Hệ thống CyberOS hỗ trợ song song hai giao diện hiển thị nhằm tương tương thích hoàn hảo với hai loại tấm nền phổ biến nhất trên thị trường:

### 2.1 Cấu hình Tấm nền 1.54" (Tỉ lệ 1:1)
- **Độ phân giải**: 240 x 240 Pixels.
- **Tần số làm tươi tối đa**: 60Hz (ở mức xung SPI 40MHz).
- **Đặc trưng hiển thị**: Mọi điểm đồ họa phân bố hoàn toàn đối xứng. Bố cục dạng lưới bento 3x3 (Home Menu) được co dãn cân đối hoàn toàn ở cả chiều dọc lẫn chiều ngang.
- **Yêu cầu cân chỉnh offset**: Cấu hình offset `X = 0`, `Y = 0` do tấm nền vuông bám sát RAM hiển thị trong IC ST7789.

### 2.2 Cấu hình Tấm nền 2.0" (Tỉ lệ 3:4)
- **Độ phân giải**: 240 x 320 Pixels.
- **Tần số làm tươi tối đa**: 50Hz (SPI 40MHz).
- **Đặc trưng hiển thị**: Thêm khoảng trống 80 dòng ở chiều dọc.
- **Yêu cầu xử lý giao diện**:
  1. Tăng khoảng không hiển thị (Padding dọc) cho vùng lưới bento trên màn hình để không bị tụ lại một góc.
  2. Bổ sung biểu đồ khí tượng lớn hơn (mở rộng vùng hiển thị đồ thị trong Weather Console).
  3. Bổ sung thanh công cụ hiển thị phím chức năng nhanh ở đáy màn hình.

---

## 3. Cấu Hình Trình Điều Khiển Đồ Hoạ (Drivers) cực hạn

Để màn hình cập nhật khung hình tức thì (Refresh rate > 45 FPS), chúng ta cấu hình các tham số truyền thống trực tiếp dưới tầng điều khiển phần cứng của LovyanGFX hoặc TFT_eSPI.

### 3.1 Cấu hình qua thư viện LovyanGFX (Khuyên dùng)
Trình điều khiển **LovyanGFX** nổi bật nhờ viết trực tiếp bằng C++/ASM tối ưu thanh ghi cho dòng ESP32, hỗ trợ DMA siêu tốc. Hãy định nghĩa một lớp cấu hình tùy biến:

```cpp
#include <LovyanGFX.hpp>

class LGFX_CyberOS_Config : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;
  lgfx::Light_PWM    _light_instance;

public:
  LGFX_CyberOS_Config() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;     // Sử dụng cổng SPI thứ 2 của ESP32-S3
      cfg.spi_mode = 0;              // SPI Mode 0 chuẩn màn hình ST7789
      cfg.freq_write = 40000000;     // Tốc độ truyền tải 40 MHz (Cực kỳ ổn định)
      cfg.freq_read  = 16000000;     // Tốc độ đọc 16 MHz
      cfg.spi_3wire = true;          // Giao tiếp ba dây tiết kiệm chân
      cfg.use_lock = true;           // Chống xung đột giữa các luồng
      cfg.pin_sclk = 48;             // Cấu hình chân SCLK
      cfg.pin_mosi = 12;             // Cấu hình chân MOSI
      cfg.pin_miso = -1;             // Không dùng chân nhận
      cfg.pin_dc   = 47;             // Chân Data/Command
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 14;     // Chân chọn Chip
      cfg.pin_rst          = 3;      // Chân khởi động lại màn hình
      cfg.panel_width      = 240;    // Chiều ngang màn hình hiển thị
      cfg.panel_height     = 240;    // 1.54" thiét lập 240, 2.0" thiết lập 320
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.dummy_read_pixel = 8;
      cfg.readable         = false;
      cfg.invert           = true;   // Bật chế độ đảo ngược màu (bắt buộc cho ST7789)
      cfg.rgb_order        = false;  // Thứ tự sắp xếp màu BGR/RGB
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 39;               // Chân điều khiển đèn nền LED
      cfg.freq   = 12000;            // Tần số xung điều khiển PWM chống nhấp nháy
      cfg.pwm_channel = 1;           // Số kênh PWM ESP32
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX_CyberOS_Config tft;
```

### 3.2 Cấu hình qua thư viện TFT_eSPI
Trong môi trường PlatformIO, thay vì chỉnh sửa file header quốc tế `User_Setup.h` trong thư viện dùng chung gây mất dữ liệu khi cập nhật, hãy chèn thẳng cơ chế cấu hình thông qua cờ biên dịch (Build Flags) tại file `platformio.ini`:

```ini
build_flags =
    -D USER_SETUP_LOADED=1
    -D ST7789_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=240        ; Thiết lập thành 320 đối với bản màn hình 2.0 inch
    -D TFT_MISO=-1
    -D TFT_MOSI=12
    -D TFT_SCLK=48
    -D TFT_CS=14
    -D TFT_DC=47
    -D TFT_RST=3
    -D TFT_BL=39
    -D TFT_BACKLIGHT_ON=HIGH
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=16000000
```

---

## 4. Bảng Mã Màu Sắc RGB565 CyberOS UI Theme

Để tăng cường độ thẩm mỹ kỹ thuật số (Industrial Aesthetic), CyberOS v2.0 áp dụng hệ màu tối (Modern Dark Theme) độ tương phản cao, được tối ưu hóa cho màn hình ST7789 sử dụng hệ màu hiển thị 16-bit (RGB565).

```
[  SLATE_BLACK  ] 0x0000 | ■ Đen thẫm (Nền chính giảm mỏi mắt)
[  COSMIC_BLUE ] 0x1082 | ■ Xanh vũ trụ sẫm (Header và viền bảng điều khiển)
[ ACTIVE_AMBER ] 0xFDA0 | ■ Vàng hổ phách rực rỡ (Highlight mục được chọn)
[ ELECTRIC_GRN ] 0x07E0 | ■ Xanh hạt nhân (Giá trị tối ưu, trạng thái ổn định)
[ BRIGHT_CYAN  ] 0x07FF | ■ Xanh neon rực rỡ (Thông tin bổ trợ, đồ thị)
[ BORDER_GREY  ] 0x5AEB | ■ Xám kim loại (Đường phân chia và hộp biên thứ)
[ CRITICAL_RED ] 0xF800 | ■ Đỏ khẩn cấp (Trạng thái cảnh báo, ngắt kết nối)
```

### Bảng công thức convert để đổi màu HEX (24-bit) sang RGB565 (16-bit)
Để thuận tiện cho nhà phát triển thiết kế màu mới:
```cpp
uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
// Ví dụ: Màu hổ phách rực rỡ (255,180,0) -> 0xFDA0
```

---

## 5. Cấu trúc Giao Diện Thiết Kế Động (Responsive UI Model)

Đối với màn hình thiết bị nhúng nhỏ, thiết kế bố cục chặt chẽ là chìa khóa tạo sự tinh tế, thanh nhã.

```
+-----------------------------------+
| [X] CYBER OS CORE           98% |  <- Header (Chiều cao: 24 Pixels)
+-----------------------------------+
|  [BUS]       [WFI]       [DIR]    |
|  DEVS       Wi-Fi       Files     |
|                                   |
|  [SNS]       [TOL]       [SET]    |  <- Grid 3x3 (Khoảng không thay đổi linh động)
| Weather     Tools       Settings  |
|                                   |
|  [CPU]       [LOG]       [OSI]    |
| CPU Info    Sys Log     About     |
+-----------------------------------+
| JOYPAD Navigation       OK: SELECT|  <- Footer (Chiều cao: 22 Pixels)
+-----------------------------------+
```

### 5.1 Cấu trúc Header (StatusBar)
- **Chiều cao cố định**: 24 Pixels.
- **Tọa độ kết thúc**: Phân cách bằng một đường gạch kẻ mảnh chạy ngang màn hình tại `Y = 24`.
- **Thành phần hiển thị**: 
  - Trái: Tên Phân hệ (VD: `CYBER OS CORE` hoặc `WEATHER CONSOLE`)
  - Giữa: Biểu tượng sóng Wi-Fi (Vẽ bằng tam giác rỗng nếu ngắt kết nối, tam giác đặc rực rỡ `0x07FF` nếu kết nối thành công).
  - Phải: Mức phần trăm dung lượng pin và hiển thị hộp đồ họa cột pin phát triển trực quan.

### 5.2 Màn hình lưới bento Home (Grid 3x3)
Giao diện chính được chia thành 9 ô điều hướng nhanh. Để tương thích linh hoạt giữa màn hình vuông 1.54" (240x240) và màn hình dọc 2.0" (240x320), ta áp dụng thuật toán tính toán phân phối tọa độ các ô như sau:

```cpp
int headerY = 25;                       // Bắt đầu ngay sau vạch phân cách Header
int footerY = SCREEN_HEIGHT - 22;      // Điểm cắt của vạch phân cách Footer
int gridSpace = footerY - headerY;      // Tổng độ cao thực tế của lưới bento

int colWidth = SCREEN_WIDTH / 3;        // Độ rộng một ô (Bằng 80 Pixels)
int rowHeight = gridSpace / 3;          // Độ cao một ô (ST7789 1.54": ~64px | 2.0": ~91px)

for (int i = 0; i < 9; i++) {
  int col = i % 3;
  int row = i / 3;
  
  // Tạo biên tự do (padding) khoảng cách giữa các ô là 4px
  int cellX = col * colWidth + 4;
  int cellY = headerY + row * rowHeight + 4;
  int cellW = colWidth - 8;
  int cellH = rowHeight - 8;
  
  bool isSelected = (selectedIndex == i);
  uint16_t cardBg = isSelected ? 0xFDA0 : 0x18C3; // Hổ phách nếu chọn, xám sẫm nếu không
  uint16_t cardBorder = isSelected ? 0xFFFF : 0x5AEB;
  
  tft.fillRoundRect(cellX, cellY, cellW, cellH, 6, cardBg);
  tft.drawRoundRect(cellX, cellY, cellW, cellH, 6, cardBorder);
}
```

### 5.3 Cấu trúc Footer (Thanh Hướng dẫn Nhanh)
- **Chiều cao cố định**: 22 Pixels.
- **Điểm bắt đầu**: Phân cách bằng đường kẻ mảnh quét ngang tại tọa độ `Y = SCREEN_HEIGHT - 22`.
- **Thành phần hiển thị**:
  - Trái: Hành vi tương ứng phím bấm hiện thời (Ví dụ: `UP/DOWN Scroll`, `BACK: Exit`).
  - Phải: Chỉ dẫn nhanh nút chấp nhận (`OK: ENTER` hoặc `OK: ENABLE`).

---

## 6. Phác thảo Sơ đồ Hoạt động của 6 Phân Hệ Core

### 6.1 Màn hình khởi động (Splash Screen)
- **Sứ mệnh**: Trọng tải và dựng nền cơ bản hệ thống, đồng thời khởi động các cấu hình phần cứng nhạy cảm kéo dòng cao (như chip truyền phát Wi-Fi, tải thông số thẻ nhớ MicroSD).
- **Thiết kế**: Vẽ một hộp chữ nhật bo tròn viền xanh lục bảo `0x07E0` tạo tinh thần bọc bảo mật; hiển thị dòng chữ đậm giữa màn hình hành trình nạp dữ liệu từ 0% đến 100% cực trực quan; tạo trải nghiệm mượt mà không bị treo khi boot.

```cpp
// Thao tác vẽ thanh nạp dữ liệu tỷ lệ thực tế
int barW = SCREEN_WIDTH - 60;
int barX = 30;
int barY = SCREEN_HEIGHT / 2 + 40;
tft.drawRect(barX, barY, barW, 10, 0x4228); // Hộp bao ngoài
int actualPrg = ((millis() - splashStart) * (barW - 4)) / 2500; // Thời gian giả định tải hệ thống 2.5 giây
tft.fillRect(barX + 2, barY + 2, actualPrg, 6, 0x07E0); // Vẽ tiến trình nạp
```

### 6.2 Phân hệ Quét mạng (Wi-Fi Scanner)
- **Cơ chế**: Khởi động card thu phát sóng không dây, gọi phương thức quét `WiFi.scanNetworks()`.
- **Thiết kế**: Hiển thị danh sách 5 mạng không dây nằm trong tầm phủ có cường độ tín hiệu tốt nhất.
  - Sử dụng biểu tượng `[K]` ở cạnh phải biểu thị mạng được khóa an ninh bằng mật mã chuẩn WPA2.
  - Hiển thị cường độ RSSI bằng dạng số âm (Ví dụ: -45 dBm cho sóng cực mạnh, -88 dBm cho sóng chập chờn).
  - Chọn một điểm mạng và ấn `OK` để chuyển tiếp sang trạng thái bắt tay thực thi kết nối (được hiển thị hộp IP chi tiết sau khi thành công).

### 6.3 Phân hệ Trạm Khí Tượng (Weather Console)
- **Cơ chế**: Thăm dò dữ liệu liên tục từ cảm biến I²C BME280 qua địa chỉ phần cứng `0x76` hoặc `0x77`.
- **Thiết kế**: Phân vùng giao diện thành 3 widget hộp bo góc có màu sắc phân chia riêng biệt:
  - **Nhiệt độ (Nhiệt năng)**: Nền chữ Vàng Hổ Phách rực rỡ, hiển thị bằng đơn vị độ C (ºC) kèm 2 chữ số thập phân chuẩn xác.
  - **Độ ẩm không khí**: Nền chữ Xanh Neon dịu mắt (`0x07FF`), hiển thị theo phần trăm thực tế (%).
  - **Áp suất khí quyển**: Nền màu Xanh lá (`0x07E0`), biểu thị bằng đơn vị hPa.
- **Biểu đồ xu hướng (Temp Graph)**: Khi nhấn nút `OK` tại bảng khí tượng, màn hình chuyển sang đồ họa trực quan hóa chuỗi dữ liệu 15 mẫu lấy xung nhịp thời gian lưu trữ trong bộ đệm vòng:

```cpp
// Vẽ biểu đồ xu hướng nhiệt trị dạng gấp khúc mảnh
for (int i = 0; i < 14; i++) {
  int x1 = startX + (i * chartW) / 14;
  int y1 = startY + chartH - ((tempHistory[i] - minTemp) * chartH) / (maxTemp - minTemp);
  int x2 = startX + ((i + 1) * chartW) / 14;
  int y2 = startY + chartH - ((tempHistory[i+1] - minTemp) * chartH) / (maxTemp - minTemp);
  tft.drawLine(x1, y1, x2, y2, 0x07FF); // Đường kẻ đồ thị màu xanh cyan rực rỡ
  tft.fillCircle(x1, y1, 2, 0xFFFF);     // Tiêu điểm tròn trắng bạc
}
```

### 6.4 Phân thế Thẻ Nhớ & Nhật Ký (File Repository)
- **Cơ chế**: Khởi chạy bộ định tuyến đùm dữ liệu trên bộ nhớ thẻ qua giao thức `SD.begin(SD_CS)`.
- **Thiết kế**: Trường hợp thẻ cấu hình chuẩn FAT32 hoạt động đầy đủ, hệ sinh thái CyberOS sẽ đọc các chỉ mục thư mục chính.
  - Hiển thị đầy đủ tên từng tập tin cấu trúc, hậu tố định dạng (`.log`, `.json`, `.png`) và dung lượng quy chuẩn về đơn vị Kilobytes (KB).
  - Chọn một file Log bất kỳ và ấn `OK` để đọc trực tiếp chuyển tiếp lên phân hệ Bảng màn hình ảo dòng (Terminal Console).

### 6.5 Sơ đồ Bộ điều kiển chân (GPIO Hardware Debugger)
- **Cơ chế**: Cho phép giám sát trạng thái Logic vật lý của các chân vi điều khiển đang hoạt động trực tiếp.
- **Phạm vi hiển thị**: Cho phép giám sát các kênh định vị: IO39 (Đèn nền), SDA/SCL, các phím bấm Joystick và CS Thẻ nhớ.
- **Khả năng tương tác**: 
  - Tại ô dòng GPIO đầu ra (ví dụ: `IO39 - TFT_BL`), người dùng có thể nhấn phím `OK` để thay đổi đảo ngược trạng thái điện áp thực tế (kéo mức HIGH hay LOW ngay tại thời điểm thực tế). Điều này giúp kỹ thuật viên dễ dàng kiểm thử mạch phần cứng xem transistor đóng cắt sáng mờ đèn pin có làm việc hoàn hảo không.

### 6.6 Trình cấu hình hiển thị điện tử (Settings Cabinet)
- Cho phép người sử dụng thay đổi 3 thông số có tính tác động trực tiếp tới hiệu năng phần cứng:
  - **Độ sáng đèn nền TFT (TFT Backlight Dim)**: Chu chuyển độ sáng từ 20%, 40%, 60%, 80% đến 100%. LovyanGFX sẽ băm xung PWM thay đổi lượng dòng ra làm dịu độ sáng bóng màn ST7789.
  - **Hệ thống cảnh báo rảnh tay (Auto-Sleep Timeout)**: Điều khiển thời gian 15, 30, 45, 60 giây trước khi hệ thống ngắt điện áp chân IO39 bảo vệ tiết kiệm thời lượng pin.
  - **Cân vị trí góc hiển thị (Active Rotation)**: Lật cấu hình xoay màn hình `0º`, `90º`, `180º`, `270º` tương thích tư thế người sử dụng cầm tấm bo mạch.

---

## 7. Giải Pháp Tối Ưu Hóa Tránh Nhấp Nháy (Anti-Flickering & DMA)

Khi vẽ lại màn hình ở tần số cao, do sự chênh lệch thời gian xóa nền đen và vẽ lại các đường vector chi tiết, mắt người dùng sẽ dễ dàng nhận ra các vệt mờ quét sọc (được gọi là hiện tượng nhấp nháy - Flickering). CyberOS kiểm soát hiện tượng này triệt để bằng 3 cơ chế:

### 7.1 Kỹ thuật Vẽ Đệm Phân Vùng (Dirty Rectangles)
Tuyệt đối không sử dụng lệnh xóa toàn thể màn hình bằng `tft.fillScreen(0x0000)` trước khi cập nhật một thông số nhỏ. 
- **Cách làm đúng**: Hãy thiết lập cơ chế lưu giữ biến trạng thái của chu kỳ trước đó. Nếu giá trị không hề thay đổi, không vẽ lại. Nếu giá trị thay đổi, chỉ vẽ đè đè một hộp đen khớp kích thước của thông số cũ để làm sạch, rồi tiếp tục đè giá trị mới lên.

```cpp
// Cách vẽ tối ưu vùng thay đổi dung lượng pin
static int previousBat = -1;
if (currBat != previousBat) {
  tft.fillRect(SCREEN_WIDTH - 82, 8, 40, 10, 0x1082); // Xóa trắng duy nhất một ô nhỏ trong vùng Header
  tft.setCursor(SCREEN_WIDTH - 82, 8);
  tft.printf("BAT: %d%%", currBat);
  previousBat = currBat;
}
```

### 7.2 Sử dụng Bộ Đệm Khung Sóng Sprite (Double Buffering)
LovyanGFX cho phép tạo ra các vùng nhớ RAM nội mang tên Sprite đóng vai trò bộ đệm dựng hình tạm thời trước khi đẩy ra màn hiển thị vật lý:

```cpp
LGFX_Sprite canvas(&tft); // Khởi tạo vùng không gian đệm ảo bám sát màn hình chính

void setup() {
  canvas.createSprite(240, 240); // Khai báo kích thước đệm ảo khớp kích cỡ vuông hiển thị
}

void renderLoop() {
  canvas.clear(); // Xóa khung hình ảo nằm yên tĩnh trên bộ nhớ trong RAM
  canvas.fillRoundRect(10, 10, 100, 100, 8, 0xFDA0); // Vẽ các khối hộp
  canvas.drawString("CYBER OS V2", 20, 20); // Dựng ký tự
  // Khi mọi tác vụ tô vẽ phức tạp hoàn thành hoàn toàn trên RAM
  canvas.pushSprite(0, 0); // Đẩy toàn bộ khối dữ liệu nén ra ngoài màn qua DMA trong 1 xung nhịp !
}
```

---

## 8. Bản Đồ Tần Số Âm Thanh Buzzer & Thuật Toán Phím

### 8.1 Bộ tạo nhạc hiệu Buzzer
Sử dụng bộ tạo dao động nội sẵn có trong hàm `tone()` của Arduino để cấp phát năng lượng cho Còi Chíp (Passive Buzzer) tại chân `GPIO 41`:

- **Sự kiện nhấn nút điều hướng (Keyboard Click)**: Tần số `1600 Hz`, thời lượng nhấp nhẹ `15 ms`. Chạy dứt khoát không gây trễ phím.
- **Sự kiện xác nhận vào phân hệ (OK Handshake)**: Tần số `2400 Hz` sau đó nâng nhanh dốc lên `3200 Hz`, thời lượng tổng `100 ms`.
- **Sự kiện cảnh báo / Rời phân hệ (Back / Critical Notification)**: Tần số `600 Hz` giảm dốc sâu xuống `450 Hz`, âm lượng ngân nhẹ `150 ms`.

### 8.2 Thuật toán Chống rung Phím Bấm điều hướng (Debouncing)
Để ngăn ngừa tình trạng một lần ấn cơ học phát sinh liên tiếp 5-6 lệnh nhập (bởi độ bật cơ lò xo của phím nút vật lý), chúng ta áp dụng vòng trễ chốt bảo vệ khống khít thời gian:

```cpp
bool readKeyDebounced(int pin) {
  if (digitalRead(pin) == LOW) { // Phát hiện phím nhấn xuống đất
    delay(20);                   // Trì hoãn 20ms vượt qua vùng nhiễu sóng lò xo
    if (digitalRead(pin) == LOW) { // Xác nhận lại lần hai trạng thái thực tế
      while(digitalRead(pin) == LOW); // Trạng thái giữ phím chặt: chờ nhả hoàn toàn
      return true;               // Xác lập tín hiệu nhấn nút hợp lệ duy nhất !
    }
  }
  return false;
}
```

---

## 9. Hướng dẫn Biên dịch và Nạp Mạch với PlatformIO IDE

Để chạy thử mã nguồn đã thiết kế tự động, hãy làm theo quy trình 3 bước vàng sau:

1. **Chuẩn bị Thao tác**:
   - Sử dụng cáp USB Type-C chính hãng có hỗ trợ truyền nhận dữ liệu kết nối ESP32-S3 trực tiếp vào cổng USB máy tính của bạn.
2. **Cài đặt môi trường IDE**:
   - Tải phần mềm **VS Code** (Visual Studio Code).
   - Truy cập vào cửa hàng tiện ích (Extensions), tìm cài đặt extension mang tên **PlatformIO IDE**.
3. **Mở Dự án và Nạp cốt**:
   - Giải nén tệp `.ZIP` mã nguồn PlatformIO đã tải xuống từ ứng dụng.
   - Nhấp vào biểu tượng PlatformIO (hình đầu kiến) trên cột menu bên trái VS Code và chọn **Open Project** dẫn đến thư mục vừa giải nén.
   - Nhấp chọn Driver màn hình hiển thị phù hợp của bạn trong thiết kế cấu hình `platformio.ini` (Khuyên dùng **LovyanGFX**).
   - Nhấp nút **Upload & Monitor (biểu tượng hình mũi tên sang phải)** nằm ở thanh chân đế trạng thái đáy màn hình để bắt đầu quá trình biên dịch toàn phần sang file mã nhị phân `.bin` và nạp thẳng tự động vào ESP32-S3.
   - Trình giám sát Serial Terminal sẽ tự động hiển thị khởi động rực rỡ và bạn có thể theo dõi tiến độ hoạt động một cách chi tiết nhất!
