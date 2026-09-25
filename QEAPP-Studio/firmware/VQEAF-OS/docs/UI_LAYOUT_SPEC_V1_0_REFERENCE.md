# VQEAF OS — UI Layout Specification v1.0

**Đích:** firmware ESP32-S3-WROOM-1 N16R8 • ST7789 **240 × 320 portrait** • `.vqeaf` theme • signed `.qeapp` apps  
**Chuẩn hiển thị:** bản mô phỏng Home / Menu / Menu Navigated / WiFi / Bluetooth / Files / Gallery / Music / Shell / Settings / Themes / Applications / Installer / Recovery / Calculator / Stopwatch / Browser do người dùng cung cấp.  
**Định hướng thẩm mỹ:** BỐ CỤC CHÍNH XÁC theo ảnh kiểu điện thoại cổ điển; bảng màu, danh sách có trọng tâm, chuyển focus theo vùng cập nhật và modal lấy cảm hứng từ cách tổ chức UI Retro-Go; **không** sao chép nguồn, logo, ảnh nền, font hay theme Retro-Go.  
**Tài liệu này là SPEC để triển khai**, không phải xác nhận các chức năng chưa được lập trình hoặc đã chạy trên mạch.

---

## 0. Những quyết định không được làm sai

1. **Home** vẫn là dashboard đồng hồ + mạng + thông báo + 3 shortcut. Không biến Home thành danh sách tab Retro-Go.
2. **Menu chính** là **lưới 3 cột × 4 hàng** như ảnh. Điều hướng qua D-pad và có viền ô sáng; từ đây mở từng app thành màn hình riêng. Các app bổ sung nằm trong Applications/Library, không chen vào màn Menu mặc định.
3. Nội dung app có **thanh trên chung / vùng nội dung / ba softkey bên dưới**. Theme áp xuyên suốt cả Home, Menu và app, không chỉ launcher.
4. Retro-Go là **nguồn cảm hứng cách tương tác và render**: mục đang chọn rõ ràng, danh sách cuộn, status sạch, popup modal, cập nhật dirty region, preview tùy ngữ cảnh. **Không** áp cơ học layout 320×240 hay assets 272×24 lên màn hình này.
5. `.vqeaf` là DSL VQEAF Theme Studio `@vqeaf 1.0`, không phải JSON đổi đuôi. `.qeapp` của bản firmware gốc là **QEAPP/2 binary có ECDSA-P256**, không phải thư mục ZIP chứa Lua.
6. Giữ **TFT_eSPI + Arduino + PlatformIO** của ZIP firmware đã cung cấp. Không tự chuyển sang LVGL/LovyanGFX khi chưa lập kế hoạch kiểm thử. Không đổi các chân GPIO.
7. Không giả lập SIM/LTE hoặc mức pin thực: BOM hiện tại không có modem/ADC đo pin; nếu không có nguồn đo thật, chỉ vẽ biểu tượng pin dạng outline hoặc ẩn.

## 1. Baseline và phạm vi chuyển đổi

| Thành phần | Baseline được thấy trong source VQEAF OS v2.1 | Hợp đồng UI mục tiêu |
|---|---|---|
| Driver | Arduino + TFT_eSPI, ST7789, SPI 40 MHz | Giữ nguyên, 240×320, rotation 0 |
| Hiển thị cũ | `src/core/SymbianUI.*` có Home/grid/list/status/softkeys | Giữ layout Home/grid như ảnh, nâng styling theo `.vqeaf` |
| Launcher v2.1 | `src/launcher/LauncherView.*` có 6 tab Home/Internet/Applications/Media/System/Settings | Không thay màn Menu 3×4; dùng tab/list này **tùy chọn** ở Applications/Library hoặc chế độ Explorer |
| Theme | `ThemeFileService` đọc palette + optional `launcher {}` từ `.vqeaf` | Một theme thống nhất cho chrome, Home, grid, list, dialog, trình duyệt và launcher Explorer |
| Installer | QEAPP/2, chỉ `type=web` và `type=text`, icon signed 32×32 RGB565 | Không đổi container/bảo vệ/loại payload; chỉ đổi presentation |
| Màn phụ | Các app WiFi, BLE, File, Gallery, Music, Shell, Recovery, v.v. đã có | Giữ lõi nghiệp vụ; đồng bộ chrome/theme/focus/error |

**Yêu cầu tương thích dữ liệu:** không xóa NVS cũ, không đổi `receipt.bin`, không làm mất mạng WiFi đã lưu, lịch sử browser, nội dung SD, các `.qeapp` đã xác thực. Việc đổi tên `SymbianUI` thành `VqeafUI` có thể làm dần qua adapter; tên class nội bộ không được hiển thị trên UI.

## 2. Phần cứng & ánh xạ phím bất biến

**ESP32-S3-WROOM-1 N16R8**: flash 16 MB, PSRAM 8 MB. **ST7789**: SCL/SCK 48; MOSI 12; CS 14; DC 47; RESET 3; LEDK 39. **SDMMC 1-bit**: CLK 13; CMD 11; DAT0 9; DAT3/CD 10 tùy sử dụng. Debug serial 115200 baud.  
**Bàn phím nối GPIO–GND, INPUT_PULLUP, nhấn mức LOW:** MENU 18 · UP 7 · A 15 · LEFT 45 · START 17 · RIGHT 6 · OPTION 8 · DOWN 46 · B 5 · SELECT 16.

| Phím / tổ hợp | Home | Menu/grid | List/app | Popup/input |
|---|---|---|---|---|
| MENU (ngắn) | Menu chính | Trở Home | Home | Đóng popup và về Home chỉ khi không có thao tác nguy hiểm chưa xác nhận |
| MENU (giữ) | Task switcher | Task switcher | Task switcher nếu app cho phép | Theo ngữ cảnh |
| UP / DOWN | Chuyển lựa chọn thích hợp | Đổi hàng | Di chuyển item/scroll | Đổi option / vị trí con trỏ |
| LEFT / RIGHT | Đổi shortcut WiFi/Music/Files | Đổi cột | Đổi giá trị nếu control cho phép; không tự nhảy tab trong mọi app | Đổi lựa chọn / giá trị |
| START (OK) | Mở shortcut chọn | Mở app | Chọn/mở/áp dụng | Xác nhận |
| A (Back) | Không thoát OS | Trở Home | Trở màn trước | Đóng/hủy |
| B (Delete) | Tác vụ phụ nếu có | Tác vụ phụ (nếu không: Back) | Xóa chỉ khi ngữ cảnh cho phép | Backspace |
| OPTION | Quick menu | Menu tùy chọn mục | Context menu | Tùy chọn màn nhập |
| SELECT ngắn | Theo mode và app hiện hành | Giữ hành vi tương thích firmware | Giữ hành vi app hiện hành | `0` ở T9 nếu ứng dụng nhập số |
| SELECT giữ >600 ms | Đổi Game/T9 | Đổi Game/T9 | Đổi Game/T9 | Đổi Game/T9, không chèn `0` ngoài ý muốn |

**Debounce:** 25 ms; nhấn giữ UP/DOWN bắt đầu tự lặp sau khoảng 320 ms, 90–120 ms/lần (cấu hình, không tự tạo nhiều `DOWN` giả). Trả key-up sạch khi đóng màn hình; không mở app hai lần do key repeat START. Với SELECT, ranh giới **>600 ms** là hợp đồng chung; nếu baseline dùng 650 ms, cấu hình một nơi và kiểm tra lại regression, không chỉnh GPIO.

## 3. Hệ tọa độ 240×320 / quy tắc chia vùng

Tất cả hình chữ nhật dùng quy ước **[x, y, width, height]** và pixel phải nằm trong [0..239]×[0..319]. Không được phụ thuộc resolution của ảnh tổng hợp (contact sheet có khoảng đệm ngoài mỗi khung).

| Vùng | X | Y | W × H | Tác dụng |
|---|---:|---:|---:|---|
| Header/status chung | 0 | 0 | 240×27 | Tên màn hình bên trái; đồng hồ giữa; WiFi + battery bên phải |
| Nội dung có thể cuộn | 0 | 28 | 240×270 | Tùy màn hình; pixel hàng 27 là đường phân tách |
| Footer 3 softkey | 0 | 298 | 240×22 | Cột 0: x=0–79; cột 1: 80–159; cột 2: 160–239 |
| Grid menu | 1 | 28 | 3 × 4 ô danh nghĩa 78×66 | Lưới lấp nội dung tới y=291; 5px dưới dành chống va footer |
| Hàng danh sách thường | 2 | 29 | 233×41 | 6 hàng, bước 42px; thanh cuộn x≈236, không đè text |
| Popup option | 11 | căn giữa | ≤218×(30+n×28+8) | Tối đa 5 mục hiển thị, có scrollbar |

**Quy tắc phủ:** Header và footer được vẽ riêng, `clearContent()` không xóa chúng. Modal lưu hoặc dựng lại đúng nền phía sau; không gọi fillScreen(BLACK) để đóng. Giai đoạn này không yêu cầu framebuffer RGB565 toàn màn (~150 KiB); TFT_eSPI direct draw + dirty area được chấp nhận theo baseline. Nếu về sau thêm PSRAM framebuffer/DMA, đó là nhánh tối ưu riêng.

### 3.1 Header/status (ảnh mẫu)

- Nền phụ thuộc theme; mẫu xanh có `#296D18` gần màu ảnh; tiêu đề chữ sáng tại x=4, y=5.
- Ba vùng logic 80/80/80 px: trái tên màn hình tối đa khoảng 76px (cắt theo pixel chiều rộng, thêm `~`), giữa `--:--` nếu NTP chưa đồng bộ, phải WiFi + viền pin.
- Dấu chấm/thông báo chỉ có khi thực sự có unread; không giả pin %, mạng SIM. Khi thời gian đổi, chỉ vẽ lại khối đồng hồ; khi RSSI đổi, chỉ vẽ lại icon WiFi.
- Chuẩn tiêu đề UI: `VQEAF` Home; `Menu`, `WiFi`, `Bluetooth`, `File manager`, `Gallery`, `Music`, `Shell`, `Settings`, `Themes`, `Applications`, `App installer`, `Recovery`, `Calculator`, `Stopwatch`, `Qeafbrowser`.

### 3.2 Footer (mọi màn hình)

- Ba phần bằng nhau x=0–79/80–159/160–239, text căn trái/giữa/phải tùy cột, tối thiểu 4px padding; nếu dài, cắt theo pixel, không cắt byte UTF-8.
- Tên ngữ cảnh thay đổi theo app: `Menu | Open | Quick`, `Options | Open | Back`, `Options | Apply | Back`, `Options | Connect | Back`, `Options | Enter | Back`, v.v.
- Không được đồng nhất chữ trên footer với tín hiệu điện: `A` vẫn Back theo hợp đồng phím; footer trình bày chức năng theo màn hình và tương thích shortcut đang có.

### 3.3 Kiểu hiển thị đồng nhất

- **Icon**: chủ yếu vẽ procedural RGB565; Menu dùng hộp 36×36; ứng dụng cài đặt có icon đã ký 32×32 RGB565. Không lấy asset/logo từ Retro-Go.
- **Focus grid**: nền nhạt/viền trắng dày nhìn thấy được quanh đúng một ô; focus list: hàng nền khác màu + đường nhấn ở trái + text tương phản. Khi đổi focus chỉ redraw ô cũ + mới; khi cuộn thay offset thì redraw viewport con.
- **Font**: tiêu đề và item chính dùng phông tỷ lệ có độ cao ~14–16 px; status, metadata, mô tả dài dùng ~8–12px; ký tự VN phải qua UTF-8/glyph atlas thích hợp. Baseline TFT built-in font không đảm bảo tiếng Việt, đây là **tiêu chí cần triển khai**, không ghi là đã có.
- **Khoảng cách**: nội dung x padding 8–12 px, dòng chạm tối thiểu 28–42 px tùy danh sách; văn bản dài xuống dòng theo kích thước pixel, ký tự không tràn header/footer.
- **Ảnh**: gallery/cover load theo nhu cầu, giữ 1–2 ảnh preview trong PSRAM; khi SD lỗi hiển thị thông báo và không crash. Không ép 320×240 background Retro-Go lên 240×320.

## 4. Layout chi tiết THEO 17 khung tham chiếu (16 màn độc nhất)

### 4.1 Home — dashboard, không phải tab launcher

| Bộ phận | Vùng [x,y,w,h] | Chi tiết |
|---|---|---|
| Header | [0,0,240,27] | VQEAF, NTP, WiFi/battery, unread badge nhỏ |
| Đồng hồ + ngày | [12,43,216,78] | `HH:MM` hoặc `--:--`, dưới là ngày / `Date not set` |
| Tình trạng thiết bị | [12,132,216,46] | Dòng 1: WiFi trạng thái/SSID rút gọn; dòng 2: nhạc hoặc unread/device ready |
| Shortcut WiFi | [8,191,72,67] | Icon + nhãn, focus luân phiên |
| Shortcut Music | [84,191,72,67] | Icon + nhãn |
| Shortcut Files | [160,191,72,67] | Icon + nhãn |
| Hint | [8,266,224,20] | `Hold MENU: tasks; Hold OPT: settings`, bật theo cấu hình accessibility |
| Footer | [0,298,240,22] | `Menu | Open | Quick` |

D-pad LEFT/RIGHT di chuyển trên 3 shortcut; START mở; MENU đi đến Menu 3×4. Status WiFi update từng vùng, giờ update vùng đồng hồ; **không** refresh toàn bộ màn hình mỗi giây. Khi chưa NTP hiển thị `--:--`, không dùng uptime giả ngày giờ.

### 4.2 Menu / Menu Navigated — 3×4 grid giữ nguyên ảnh

Hàng 1: **WiFi | Bluetooth | Music**. Hàng 2: **File mgr | Gallery | Internet**. Hàng 3: **Shell | Recovery | Settings**. Hàng 4: **Themes | Apps | Library**.  
Vị trí: `col 0/1/2 → x≈1/79/157`; `row 0/1/2/3 → y=28/94/160/226`; ô danh nghĩa 78×66; icon 36×36 căn ngang, nhãn dưới icon. Có thể dùng dải sáng tối rất nhẹ theo hàng; **không đổi thứ tự app để tạo tab**. Cấu hình chọn giữ cột khi di chuyển UP/DOWN, xử lý focus góc phải cuối row không tràn. Viền selected trắng + nền sáng, giống trạng thái `Menu Navigated`.

Footer mặc định: `Options | Open | Exit` (về Home); OPTION menu nhanh gồm các chức năng có thật; không hiện menu rỗng. Các mục chỉ xuất hiện theo khả năng phần cứng và trạng thái app (ví dụ Bluetooth có BLE, không giả chức năng điện thoại).

### 4.3 WiFi

Màn `WiFi` độc lập, khi chưa tìm thấy mạng: nhãn `WIFI` + `No networks found` + `Options > Rescan` như ảnh. Khi có mạng, list có SSID, biểu tượng cường độ, bảo mật, nhãn saved/current; không hiện mật khẩu. Nhấn START chọn, mở wizard cho WPA; OPTION: scan lại / mạng đã lưu / xem trạng thái / quên mạng / nhập SSID ẩn nếu firmware hỗ trợ. Footer: `Options | Connect | Back`. Scan và connect bất đồng bộ hoặc có progress; không chặn UI. Khi browser đang kết nối, scan không tự ngắt phiên dùng mạng.

**Nguồn dữ liệu:** dùng `WiFiProfileStore` hiện có; bản v2.x lưu tối đa 5 profile NVS (theo source hiện hành), **không tự chuyển sang wifi.json 4 mạng** nếu không có migration và xác nhận yêu cầu.

### 4.4 Bluetooth

Như ảnh: `Bluetooth` / `No BLE devices found` / `Options > Rescan` khi trống. Khi scan thấy thiết bị, mỗi hàng hiện tên, RSSI/địa chỉ rút gọn; START = Details nếu scan-only (không hứa kết nối A2DP/BT Classic nếu phần cứng/code không hỗ trợ). Footer `Options | Details | Back`; giải phóng resource BLE khi đóng scan.

### 4.5 File manager

Giữ khung `File manager`, vùng content thông báo `microSD is not mounted` khi không có SD. Khi mount: dòng breadcrumb/path, list thư mục/file, folder trước file, mỗi hàng hiển thị icon/nhãn/kích thước hoặc ngày nếu biết; B thao tác theo context; xóa bắt buộc confirm. Footer `Options | Open | Back`; không truy cập ngoài root được phép khi dùng cổng `.qeapp`.

### 4.6 Gallery

Trống SD: `microSD is not mounted`. Khi có hình: danh sách ảnh/thumb theo 6 hàng hoặc grid thumbnail nhỏ có lựa chọn, START xem full-frame fit 240×270, OPTION slideshow/chi tiết nếu có, A trở về vị trí đã chọn; kích thước lớn giải mã theo dòng hoặc buffer bounded.

### 4.7 Music

Trạng thái trình phát ở đáy nội dung **không đè footer**: file đang phát, % volume, shuffle/repeat, codec/trạng thái nếu thực sự biết. Khi không có file: thông báo rõ. Hàng list chọn track và vùng mini-player theo ảnh, footer `Options | Play/Pause | Back`. Không ghi 'Audio ready' nếu DAC chưa init/không có file thực.

### 4.8 Shell

Vùng terminal nền rất tối/đen, mực sáng và accent, monospace đủ đọc trên 240 px. Dòng output cuộn nội dung (bounded ring buffer); prompt cố định sát đáy `s3:/$` hoặc `vqeaf:/$` mới nhưng cần giữ alias lệnh cũ, START nhập lệnh, UP history, OPTION menu. Footer `Options | Command | Back`. Không mở quyền shell tùy ý từ `.qeapp`.

### 4.9 Settings

Danh sách icon/trạng thái theo ảnh, 6 hàng ×42px, có scrollbar. Thứ tự: Theme → Backlight → Audio volume → Clock format → Auto WiFi strongest → Auto keypad lock. Khi màn nhỏ, nhãn phụ dài hiển thị dòng 2 và ellipsis theo glyph. START mở chỉnh, LEFT/RIGHT thay đổi value đối với slider/cycle có thể; backlight phải cập nhật LEDK thật nếu code đã hỗ trợ. Cài đặt lưu NVS; không để tác vụ mạng chạy bên trong hàm draw.

### 4.10 Themes

Mục gồm `S60 Green (Built-in)`, `AMOLED Red`, `Black`, `Classic beige` và theme `.vqeaf` đã scan. Dòng selected gồm biểu tượng palette 24×24, tên, thông tin Built-in/SD/Applied. START/Apply: validate đầy đủ trước khi thay palette và NVS; lỗi/mất SD phải giữ theme hiện tại trong phiên và đưa về built-in khi lần boot sau không đọc được. Footer `Options | Apply | Back`. **Theme Studio 1.0** hỗ trợ palette trên LCD thật; `phoneShell/keypad` mô phỏng bên ngoài thiết bị không biến thành lớp ảnh trên ST7789 vật lý.

### 4.11 Applications

Danh sách như ảnh: `Shell`, `Open apps`, `Notes`, `Notifications`, `Text viewer`, `Recovery` + danh sách signed `.qeapp` đã xác thực (nếu có). Mỗi item có icon nhỏ x≈10, văn bản chính x≈47, dòng phụ nhỏ x≈47. Item có cover optional khi người dùng bấm Details, không hy sinh 6 dòng danh sách. START mở; OPTION: Info/Pin/Uninstall nếu ứng dụng cài và quyền phù hợp. Không hiển thị app không hợp lệ trong catalog verified.

### 4.12 App installer

Header `App installer`, nội dung `App inbox`; khi SD không mount hiện `microSD is not available`. Khi có thẻ quét `/System/Apps/Inbox/*.qeapp`, danh sách file + trạng thái xác thực. Chọn mở `Package details` có publisher key ID, `type`, kích thước, chữ ký verified/rejected. START cài khi verified, xác nhận rõ tên app; không đổi định dạng QEAPP/2; B/Back không làm biến mất transaction cài đang ghi dở.

### 4.13 Recovery

Danh sách đúng thứ tự: Boot status (Power on/crash count), Start normal mode, Enable Safe Mode, Clear recovery flags, Restart device. Các thao tác nguy hiểm (Clear/Restart) cần xác nhận. Giữ DOWN khi boot: vào Recovery/Safe Mode theo hành vi firmware xác nhận; nếu bảng baseline cũng cho A thì nêu trong tài liệu thiết bị. Nền theme tối giản fallback khi theme file gây boot-loop.

### 4.14 Calculator

Cấu trúc theo ảnh: vùng biểu thức/kết quả [12,40,216,48] và keypad 4×4 trong vùng ~[8,100,224,180]. Bố trí nút `7 8 9 /`, `4 5 6 *`, `1 2 3 -`, `C 0 = +`. D-pad di chuyển ô, START nhấn nút, B=backspace; phép tính giới hạn đầu vào/bảo vệ chia 0. Footer `Options | Enter | Back`. Focus có tương phản rõ khi theme sáng/tối.

### 4.15 Stopwatch

Khối giờ [10,52,220,74] `00:00.00` to rõ; nút ngang `Start/Pause | Lap | Reset` trong [8,135,224,36]. D-pad chọn nút, START kích hoạt, không reset khi quay về nếu stopwatch service còn chạy. Chỉ vùng chữ số timer refresh theo nhịp; không vẽ toàn màn hình liên tục. Footer `Options | Select | Back`.

### 4.16 Qeafbrowser

Màn `Qeafbrowser` như ảnh lúc không mạng: `WiFi is not connected`, gợi ý `Options > Home can open cache`. Khi có mạng: phần địa chỉ/tiêu đề nằm dưới header, nội dung HTML/WML trong viewport không đè softkeys, link focus rõ, URL/T9 editor kế thừa phím phần cứng, history/bookmarks/download giữ nguyên. OPTIONS: Home, Back, Forward, Reload, Bookmark, Address, Zoom/Display mode (nếu có thật). HTTP/HTTPS xử lý riêng task; kết nối TLS không xác thực CA **không được trình bày như verified secure**; trang nhạy cảm hiển thị cảnh báo nếu còn đường `setInsecure()`.

### 4.17 Library / About / Lock / Task switcher / Notifications / Quick Panel

Những màn có sẵn nhưng không nằm trong ảnh 17 khung vẫn dùng chung header/content/footer, một mẫu list + popup. Library: duyệt nội dung SD theo thể loại, có thể mở Explorer tab/list kiểu Retro-Go **bên trong** để xem preview/cover nếu người dùng chọn. About: tên riêng VQEAF OS, phiên bản firmware/build ID, nguồn tham khảo ở mục Licenses; không hiện OS/brand cũ trong giao diện. Lock: đồng hồ + trạng thái thực, START để unlock; Tasks/Quick Panel là overlay/screen phụ, không thay Menu chính.

## 5. Theme `.vqeaf`: hợp đồng giao diện chính thức

### 5.1 Cấu trúc chuẩn, tương thích Theme Studio

```vqeaf
@vqeaf 1.0
<theme id="vqeaf_reference_lime" name="VQEAF Reference Lime">
  metadata {
    author: "VQEAF OS"
    version: "1.0.0"
  }
  studio { orientation: "portrait" autoId: false }
  palette {
    shellTop: "#296D18"
    shellBottom: "#232B25"
    shellBorder: "#4A7D29"
    screen: "#8BCA20"
    key: "#B4DE73"
    keyPressed: "#DEF29C"
    keyBorder: "#FFFFFF"
    keyText: "#111B0D"
    subText: "#184C18"
    accent: "#4A7D29"
    glow: "#00000000"
  }
  launcher {
    background: "#8BCA20"
    foreground: "#111B0D"
    headerBg: "#296D18"
    headerFg: "#FFFFFF"
    tabAccent: "#4A7D29"
    listBg: "#8BCA20"
    listFg: "#111B0D"
    selectedBg: "#DEF29C"
    selectedFg: "#111B0D"
    previewBg: "#B4DE73"
    previewFg: "#111B0D"
    scrollbar: "#4A7D29"
    footerBg: "#B4DE73"
    footerFg: "#111B0D"
    border: "#FFFFFF"
  }
  metrics { shellRadius: 8dp keyRadius: 4dp keyBorder: 1dp }
  <component id="screen" type="panel">
    shape { radius: 0dp fill: $palette.screen }
  </component>
</theme>
```

**Đây là ví dụ hợp lệ với tập token chuẩn + `launcher{}` của firmware hiện hành; file đầy đủ ở `examples/vqeaf_reference_lime.vqeaf`.** Các mã màu xanh được lấy mẫu từ ảnh tổng hợp (gần đúng vì ảnh mô phỏng có thể có scale). Bản mặc định có thể là **VQEAF Night** tối/retro; theme **Reference Lime** là preset để hồi quy ảnh và để người dùng chọn, không ép mọi máy đổi sang xanh khi nâng cấp.

### 5.2 Ánh xạ màu màn hình vật lý

| Khối UI | Token chuẩn ưu tiên | Override nếu loader có | Lưu ý |
|---|---|---|---|
| Header/status | `shellTop` + chữ tương phản | `launcher.headerBg/headerFg` | Đối với Home/Menu chrome có thể dùng `ThemeColors.chrome` |
| Nền content | `screen` | `launcher.background/listBg` | App body khác nhau nhưng nền chung |
| Card/mini-player | `key` | `launcher.previewBg/previewFg` | Không ép màu khung máy ngoài LCD |
| Focus selected | `keyPressed` | `launcher.selectedBg/selectedFg` | Phải đảm bảo tương phản văn bản |
| Viền | `keyBorder/shellBorder` | `launcher.border` | Nét selected nổi |
| Accent | `accent` | `launcher.tabAccent/scrollbar` | Thanh progress/focus |
| Footer | `key` và text tương phản | `launcher.footerBg/footerFg` | 3 softkey |

**Quan trọng:** loader baseline hiện tại đọc `palette` và các khóa của `launcher{}` **cho launcher tab**, nhưng **chưa bảo đảm tất cả màn Home/Menu/app kế thừa launcher override**. Mục tiêu triển khai: tạo `UiTokens`/adapter thống nhất và chuyển toàn bộ draw path sang cùng palette. Block tương lai `os{}` nếu thêm thì cần cập nhật cả VQEAF Theme Studio parser/serializer, validator và firmware; **không tự khai báo rồi giả định tương thích**.

### 5.3 Validation & giới hạn

- Header `@vqeaf 1.0`, theme id/name, UTF-8; `palette.screen/keyText/accent` là base bắt buộc của importer hiện hành; màu `#RGB`, `#ARGB`, `#RRGGBB`, `#AARRGGBB` sau khi loại alpha/convert RGB565. Unknown field không crash; không có `eval`.
- Nạp từ SD `/System/Themes/` ưu tiên, `/Themes/` tương thích. Bản hiện tại giới hạn **16 themes**, file ≤**512 KiB**, chỉ đọc tối đa **16 KiB** phần đầu để kiểm tra palette/launcher; data URI lớn được bỏ qua an toàn.
- Theme Studio có thể bỏ `launcher{}` khi re-export; vì vậy fallback chuẩn **luôn phải dùng `palette` trước**, rồi mới override. Firmware không render PhoneShell/virtual keypad textures, SVG-like vectors, alpha animation WebP, blur/glow thực trên LCD vật lý nếu chưa bổ sung engine riêng.
- Theme đổi lúc app đang chạy: invalidate toàn bộ chrome/softkey và vùng nội dung; render lại không rò rỉ cache ảnh. Invalid theme: giữ theme đã áp nếu đang chạy, boot fallback builtin Night/Reference Lime tùy cài đặt, tuyệt đối không treo.

## 6. App `.qeapp`: không phá định dạng có chữ ký

**Container QEAPP/2** theo firmware kèm ZIP: magic `QEAPP2\r\n`, header 116 byte, manifest tối đa 2048 byte dạng `key=value`, icon tùy chọn 32×32 RGB565 little-endian =2048 byte, payload tối đa 256 KiB; cuối là trailer 76 byte `QSIGP256` + 4 byte key ID + r(32) + s(32), ký trên SHA-256 của header/manifest/icon/payload. Chỉ một khóa publisher ECDSA P-256 công khai được ghim tại build. **Màn UI không được bỏ qua kiểm tra ở install, catalog hoặc use-time**.

| App type đang hỗ trợ | Khởi chạy bằng | Màn áp theme |
|---|---|---|
| `web` | URL HTTPS trong browser hiện hữu | Header / progress / cảnh báo / popup Qeafbrowser |
| `text` | Payload text vào Text Viewer | Header / text list / reader options |

`.qeapp` **không chạy Lua/ELF/VXP trong bản hiện tại**. Tính năng app script chỉ thực hiện khi có runtime + quyền/sandbox + định dạng/phiên bản mới riêng. `manifest.ini` bên trong thư mục đã cài không đồng nghĩa cho phép thư mục `.qeapp` chưa ký. **Đường dẫn:** inbox `/System/Apps/Inbox/*.qeapp`; thư mục đã cài `/System/Apps/Installed/<id>/`; Themes nằm riêng `/System/Themes/*.vqeaf`.

## 7. Routing + vòng đời màn hình / trạng thái

- `BOOT → Recovery` khi nhấn giữ DOWN đúng khoảng lấy mẫu boot của board; `BOOT → HOME` nếu bình thường. Crash-loop: vào safe layout built-in, không tự chạy theme/app có thể gây lỗi.
- `HOME → MENU (MENU)`; `HOME → SHORTCUT (START)`; `MENU → SYSTEM_APP (START)`; `SYSTEM_APP → MENU hoặc màn gọi trước (A)`; `MENU → HOME (A)`. Không pop Home ra khỏi stack.
- Applications/Installer giữ focus và offset sau khi trở về; Browser giữ URL/history/current scroll; Music phát nền theo service, không bị app khác vô tình stop.
- Modal có **một chủ**: OPTION mở context; A huỷ; START xác nhận; với destructive action hiện confirmation riêng. Event đầu tiên sau popup đóng không lọt xuống màn bên dưới (prevent accidental double action).
- Theme mới làm invalidation UI tại thời điểm áp, không chạy trong redraw interrupt; dùng một queue sự kiện GUI đơn giản. Tác vụ WiFi/BLE/network/decode ở worker/async khi cần, đổi cờ trạng thái và yêu cầu repaint; không malloc vô hạn trong `draw()`.

## 8. Hệ thành phần C++ đề xuất / mapping vào firmware thật

```
VQEAF-OS/
├── include/BoardConfig.h                # giữ nguyên pinout; rotation=0
├── src/main.cpp                       # boot/router/event pump, tên brand VQEAF OS
├── src/core/
│   ├── SymbianUI.h/.cpp                # adapter đang chạy; di trú nội bộ sang VqeafUI
│   ├── VqeafLayout.h                   # MỚI: 240×320 rectangles + safe clipping
│   ├── VqeafUiTokens.h                 # MỚI: palette thống nhất toàn hệ thống
│   ├── VqeafUiRenderer.h/.cpp          # MỚI: header/footer/grid/list/mini-card/dialog
│   ├── InputManager.h/.cpp             # GIỮ: GPIO/debounce/SELECT T9
│   └── TextKeyboard.h/.cpp             # GIỮ: VQEAF palette + UTF-8 nếu hỗ trợ
├── src/apps/Apps.h/.cpp                # giữ nghiệp vụ, thay callsite vẽ dần
├── src/launcher/LauncherView.*         # giữ optional Explorer tab/list, không làm Menu mặc định
├── src/services/ThemeFileService.*     # nạp .vqeaf; bổ sung map UiTokens tập trung
├── src/services/AppInstallerService.*  # giữ bảo mật QEAPP/2, chỉ nâng giao diện
├── themes/vqeaf_night.vqeaf            # theme mặc định tối
├── sd/System/Themes/*.vqeaf            # SD themes, có Reference Lime
├── sd/System/Apps/Inbox/*.qeapp        # app unsigned phải bị từ chối
└── docs/UI_LAYOUT_SPEC.md              # copy tài liệu này
```

**Hiện trạng so với mục tiêu:** VQEAF v2.1 source có Home/grid cũ và Launcher tab mới; tài liệu yêu cầu **đặt Home/grid thành primary navigation**, còn render theme và dialog thống nhất trong cả các app là công việc phải làm. G3 trước đó không được xem là đã hoàn thành chỉ vì đã có `LauncherView`.

## 9. Quy tắc render / tốc độ / bộ nhớ

1. Thay focus trong grid: redraw đúng **hai ô 78×66**; thay focus list: redraw hai hàng **233×41**; status clock chỉ repaint vùng 80×25; không `fillScreen()` cho mỗi phím.
2. Cuộn list có mục tiêu >30 focus changes/giây trong host sim và đo lại trên thiết bị; không đặt timer 30 FPS toàn UI. Stopwatch và animation chủ động chỉ đánh dấu dirty khi giá trị hiển thị đổi.
3. Vùng vẽ phải clip x≥0,y≥0,x+w≤240,y+h≤320; khung modal chiều cao phải tính trước, không overflow khi label 2 dòng.
4. Ảnh/icon cache bounded; nếu thiếu SD không đọc quá file size cho phép, lỗi decoder được báo nhỏ gọn; PSRAM image buffers không đặt vào SRAM DMA sai loại.
5. Áp theme không hủy NVS cũ/cookie/browser cache; không dùng RAM cho toàn bộ `.vqeaf` có base64. Khi đang decode/theme/app install, UI hiển thị progress có thể huỷ an toàn khi khả thi.
6. Crash/boot-loop: log nguyên nhân nếu có storage, tránh tự format LittleFS/SD khi lỗi theme. Recovery dùng fallback không phụ thuộc thẻ SD hay ảnh.

## 10. Hồi quy/acceptance — **chưa đánh dấu PASS nếu chưa chạy**

| ID | Kịch bản | Điều kiện đạt |
|---|---|---|
| UI-01 | Boot màn hình dọc | Đúng 240×320, pin/GPIO không đổi, Home hiển thị |
| UI-02 | Home lúc không có NTP/SD/mạng | `--:--`, trạng thái thật; WiFi/Music/Files đúng vị trí |
| UI-03 | Menu grid 12 mục | Đúng thứ tự 3×4, không còn Menu tab-list là entry mặc định |
| UI-04 | Menu Navigated | Viền focus đúng ô; redraw chỉ 2 ô khi đổi hướng |
| UI-05 | Header/footer tất cả app | 27px/22px, không tràn chữ hay giẫm icon |
| UI-06 | 16 màn hình mẫu | Từng app có tiêu đề, empty-state, footer thích hợp |
| UI-07 | Scroll Settings/Apps/Themes | Chọn 6 rows ×42; offset, focus, scrollbar ổn định |
| UI-08 | Áp theme .vqeaf Night/Lime | Toàn bộ Home/Menu/app đổi màu, restart vẫn đúng |
| UI-09 | Theme Studio có data URI lớn | Nạp palette bounded, không OOM và không giả render hình |
| UI-10 | Theme lỗi / không có SD | Không crash; fallback theme built-in; không reset tùy ý |
| UI-11 | QEAPP/2 hợp lệ | Signature verified, icon 32×32, install → Applications → launch |
| UI-12 | QEAPP không ký/đổi payload/khóa lạ | Bị chặn, không lên catalog, không thể chạy |
| UI-13 | WiFi reconnect, browser | Không block redraw; URL/scroll/back giữ nguyên |
| UI-14 | SELECT giữ >600 ms | Chuyển Game/T9 đúng một lần mỗi lần giữ; debounce không double |
| UI-15 | No-SD Gallery/Files/Installer | Empty-state như ảnh, không crash/format tự động |
| UI-16 | Recovery boot DOWN | Vào Recovery với palette built-in, restart không loop |
| UI-17 | Stopwatch/clock | Chỉ cập nhật vùng chữ số/status; không nháy toàn màn |
| UI-18 | Tiếng Việt dài | Wrap theo glyph và ellipsis; không bị cắt UTF-8 |
| UI-19 | 10 phút duyệt Menu/apps | Không leak tăng liên tục, không tràn SD cache/heap |
| UI-20 | Ảnh so sánh host/device | Bố cục đúng từng khung; màu có thể khác khi đổi theme, vùng không đổi |

### Quy trình verification đề nghị

```powershell
cd VQEAF-OS
# Các test đã có trong source v2.1 (chạy sau mỗi thay đổi tương ứng):
python tools/test_v21.py
python tools/test_v15_signature.py
python tools/test_v201_boarddiag.py
# Build thật (không thay bằng host mock):
pio run -e vqeaf_os
pio run -e vqeaf_os -t upload
pio device monitor -b 115200
```

Ảnh hồi quy nên có **17 frame** giống contact sheet (Home, Menu, Menu Navigated và 14 màn app) ở độ phân giải gốc 240×320, thêm capture riêng Theme Night/Reference Lime, một frame no-SD/theme-invalid và một frame QEAPP verified/rejected. Phân biệt rõ mock ảnh render PC với ảnh từ LCD thật, lưu thời gian, git commit, mã build và số heap/PSRAM.

## 11. Giai đoạn thực hiện và điểm dừng

| Stage | Sửa gì | Tiêu chí dừng |
|---|---|---|
| L1 — Layout contract | `VqeafLayout` + no-overlap/clipping + 17 reference screenshots | Host geometry test PASS |
| L2 — UI tokens / theme | `UiTokens` map `.vqeaf` + Home/Menu theme thống nhất | Night & Lime cả 2 màn + header/footer đổi đồng bộ |
| L3 — Home/Menu phục hồi | Route boot → Home → Menu 3×4; không xoá tab Explorer optional | D-pad/START/A/MENU, auto-repeat regression PASS |
| L4 — System app chrome | WiFi/BLE/Files/Gallery/Music/Shell/Settings/Themes/Apps/Installer/Recovery/Calc/Stopwatch/Browser | 16 screen captures đủ và content không đè footer |
| L5 — QEAPP/theme hardening | signed install và theme fallback, app icon, recovery | Test tamper/no-SD/boot-loop PASS |
| L6 — Thiết bị & tối ưu | PlatformIO + test mạch thật + screenshot màu/text/RSSI + soak | Không kết luận đạt nếu không có log và ảnh thực tế |

### Cần thống nhất trước khi sửa firmware diện rộng

- Bố cục Home/Menu trong ảnh **là mặc định**; tab Retro-Go `LauncherView` được đưa vào **Explorer trong Applications/Library** (có thể bật chế độ tùy chọn), không xóa mã ngay.
- Khi nâng cấp giữ theme đang chọn trong NVS và hỗ trợ `vqeaf_night.vqeaf` / `vqeaf_reference_lime.vqeaf` bằng cách chọn thủ công; không cưỡng bức đổi palette người dùng.
- Footer mềm trái/giữa/phải hiển thị như ảnh nhưng phần cứng có **MENU/START/A/OPTION** riêng; giải thích shortcut trong Help để tránh nghĩ LCD có ba phím cảm ứng.
- Tên hệ điều hành công khai **VQEAF OS**, website `qeafivels.com`, dòng `© VXPstore. All rights reserved.` chỉ dùng cho nội dung do người dùng sở hữu, và có Licenses riêng cho thư viện GPL/MIT/Apache nếu có.

---

**Ghi chú nguồn nội bộ:** yêu cầu + ảnh reference do người dùng cung cấp; số đo Home/Menu/font/chrome và các tính năng baseline đối chiếu từ firmware ZIP gốc cùng source VQEAF OS v2.1 đã tạo trong phiên làm việc; format `.vqeaf` theo repo VQEAF Theme Studio, QEAPP/2 theo `docs/QEAPP_V15_SIGNING.md` trong ZIP. Không tự suy diễn rằng baseline đã hỗ trợ mọi yêu cầu tương lai của spec.
