# VQEAF OS v2.2 — Pixel Atlas 240 × 320 (dọc)

**Đơn vị:** px vật lý, gốc `(0,0)` ở trái-trên, hình chữ nhật `[x, y, w, h]`, mép phải/dưới không gồm pixel đích.

**Phạm vi:** hình tham chiếu người dùng có 17 khung (Menu và Menu Navigated là một màn có 2 trạng thái), 16 màn độc nhất. Đây là **tọa độ thiết kế chuẩn**, không phải chứng nhận từng màn đã render chính xác trên mạch.

**Hệ theme:** `.vqeaf` Theme Studio 1.0 với `palette` và `launcher{}`; firmware chỉ áp màu lên LCD thật. **Ứng dụng:** giữ QEAPP/2 nhị phân, ECDSA-P256, không biến thành ZIP/Lua.

## A. Khung dùng chung

| Vùng | X | Y | W | H | Token | Chức năng |
|---|---:|---:|---:|---:|---|---|
| `header` | 0 | 0 | 240 | 27 | `chrome` | Tiêu đề x4..79, NTP giữa, WiFi/pin outline phải |
| `header_rule` | 0 | 27 | 240 | 1 | `border` | Đường ngăn |
| `content` | 0 | 28 | 240 | 270 | `background` | Nội dung |
| `soft_left` | 0 | 298 | 80 | 22 | `footer` | Options/Menu |
| `soft_center` | 80 | 298 | 80 | 22 | `footer` | Open/Select |
| `soft_right` | 160 | 298 | 80 | 22 | `footer` | Back/Exit |

### Quy tắc hiển thị & input

- Header trái 80 px, giữa 80 px (giờ `--:--` trước NTP), phải 80 px (WiFi và pin **outline**), 1 px separator ở y=27.
- Content không đè lên footer; footer 3 cột 80 px, y=298..319. Không giải phóng/đọc NVS, SD, RF ở trong `draw()`.
- Chỉ vẽ lại vùng dirty khi focus, clock, WiFi thay đổi. Popup đóng phải dựng lại nền bên dưới, không gọi `fillScreen(BLACK)`.
- `MENU` từ Home mở grid; MENU trong grid về Home; OPTION trong grid có Retro Explorer tùy chọn; SELECT giữ >600 ms đổi Game/T9.

## B. Tọa độ từng màn

### 1. Home / General (`home`)

**Điều hướng:** MENU→Menu; LEFT/RIGHT chọn shortcut; START mở; OPTION→Quick  
**Hiện trạng:** integrated from existing Idle Home; geometry cross-checked with SymbianUI.cpp  
**Lưu ý:** Đồng hồ NTP không được giả giờ; battery chỉ outline

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `clock_card` | 12 | 43 | 216 | 78 | `panel` | Card đồng hồ + ngày |
| `clock_text` | 18 | 50 | 204 | 43 | `text` | HH:MM hoặc --:--, căn giữa |
| `date_line` | 18 | 91 | 204 | 22 | `text` | Ngày chưa đồng bộ hiển thị Date not set |
| `network_card` | 12 | 132 | 216 | 46 | `panel` | Trạng thái WiFi và số thông báo |
| `network_line` | 18 | 137 | 205 | 15 | `text` | SSID/trạng thái cắt theo pixel |
| `alerts_line` | 18 | 153 | 205 | 20 | `text` | Thông báo chưa đọc hoặc trạng thái nhạc |
| `quick_wifi` | 8 | 191 | 72 | 67 | `selected` | WiFi shortcut |
| `quick_music` | 84 | 191 | 72 | 67 | `panel` | Music shortcut |
| `quick_files` | 160 | 191 | 72 | 67 | `panel` | Files shortcut |
| `key_hint` | 8 | 266 | 224 | 20 | `status` | Phím giữ MENU / OPTION |

### 2. Menu 3 x 4 (`menu`)

**Điều hướng:** D-Pad di chuyển; START mở; OPTION popup; A/B về Home; Menu→Home  
**Hiện trạng:** integrated original firmware v2.0.1 grid; external vqeaf palette mapped  
**Lưu ý:** Giữ thứ tự 12 biểu tượng y hệt ảnh; Retro Explorer là tùy chọn trong Options.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `cell_00` | 1 | 28 | 76 | 66 | `selected` | Ô 1: WiFi |
| `icon_00` | 21 | 31 | 36 | 36 | `icon` | Icon của WiFi |
| `caption_00` | 3 | 72 | 72 | 16 | `text` | Nhãn WiFi |
| `cell_01` | 79 | 28 | 76 | 66 | `list` | Ô 2: Bluetooth |
| `icon_01` | 99 | 31 | 36 | 36 | `icon` | Icon của Bluetooth |
| `caption_01` | 81 | 72 | 72 | 16 | `text` | Nhãn Bluetooth |
| `cell_02` | 157 | 28 | 76 | 66 | `list` | Ô 3: Music |
| `icon_02` | 177 | 31 | 36 | 36 | `icon` | Icon của Music |
| `caption_02` | 159 | 72 | 72 | 16 | `text` | Nhãn Music |
| `cell_03` | 1 | 94 | 76 | 66 | `list` | Ô 4: File mgr |
| `icon_03` | 21 | 97 | 36 | 36 | `icon` | Icon của File mgr |
| `caption_03` | 3 | 138 | 72 | 16 | `text` | Nhãn File mgr |
| `cell_04` | 79 | 94 | 76 | 66 | `list` | Ô 5: Gallery |
| `icon_04` | 99 | 97 | 36 | 36 | `icon` | Icon của Gallery |
| `caption_04` | 81 | 138 | 72 | 16 | `text` | Nhãn Gallery |
| `cell_05` | 157 | 94 | 76 | 66 | `list` | Ô 6: Internet |
| `icon_05` | 177 | 97 | 36 | 36 | `icon` | Icon của Internet |
| `caption_05` | 159 | 138 | 72 | 16 | `text` | Nhãn Internet |
| `cell_06` | 1 | 160 | 76 | 66 | `list` | Ô 7: Shell |
| `icon_06` | 21 | 163 | 36 | 36 | `icon` | Icon của Shell |
| `caption_06` | 3 | 204 | 72 | 16 | `text` | Nhãn Shell |
| `cell_07` | 79 | 160 | 76 | 66 | `list` | Ô 8: Recovery |
| `icon_07` | 99 | 163 | 36 | 36 | `icon` | Icon của Recovery |
| `caption_07` | 81 | 204 | 72 | 16 | `text` | Nhãn Recovery |
| `cell_08` | 157 | 160 | 76 | 66 | `list` | Ô 9: Settings |
| `icon_08` | 177 | 163 | 36 | 36 | `icon` | Icon của Settings |
| `caption_08` | 159 | 204 | 72 | 16 | `text` | Nhãn Settings |
| `cell_09` | 1 | 226 | 76 | 66 | `list` | Ô 10: Themes |
| `icon_09` | 21 | 229 | 36 | 36 | `icon` | Icon của Themes |
| `caption_09` | 3 | 270 | 72 | 16 | `text` | Nhãn Themes |
| `cell_10` | 79 | 226 | 76 | 66 | `list` | Ô 11: Apps |
| `icon_10` | 99 | 229 | 36 | 36 | `icon` | Icon của Apps |
| `caption_10` | 81 | 270 | 72 | 16 | `text` | Nhãn Apps |
| `cell_11` | 157 | 226 | 76 | 66 | `list` | Ô 12: Library |
| `icon_11` | 177 | 229 | 36 | 36 | `icon` | Icon của Library |
| `caption_11` | 159 | 270 | 72 | 16 | `text` | Nhãn Library |
| `grid_scroll` | 236 | 34 | 2 | 252 | `scrollbar` | Thanh cuộn/menu rail |

### 3. WiFi (`wifi`)

**Điều hướng:** OPTION quét; UP/DOWN chọn; START kết nối; A lùi  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Trạng thái trống và list là 2 trạng thái loại trừ. Không hiển thị mật khẩu; scan không chặn UI.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `row_00` | 2 | 29 | 233 | 41 | `selected` | Danh sách dòng 1 |
| `row_01` | 2 | 71 | 233 | 41 | `list` | Danh sách dòng 2 |
| `row_02` | 2 | 113 | 233 | 41 | `list` | Danh sách dòng 3 |
| `row_03` | 2 | 155 | 233 | 41 | `list` | Danh sách dòng 4 |
| `row_04` | 2 | 197 | 233 | 41 | `list` | Danh sách dòng 5 |
| `row_05` | 2 | 239 | 233 | 41 | `list` | Danh sách dòng 6 |
| `page_label` | 12 | 55 | 216 | 24 | `text` | WIFI |
| `empty_notice` | 12 | 101 | 216 | 20 | `dim` | Trạng thái trống |
| `empty_hint` | 12 | 126 | 216 | 20 | `dim` | Hướng dẫn thao tác tiếp theo |
| `signal_icons` | 8 | 32 | 28 | 248 | `icon` | Cột biểu tượng mạng nếu quét thấy |

### 4. Bluetooth (`bluetooth`)

**Điều hướng:** OPTION quét; START chi tiết; A lùi  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** BLE scanner; không quảng cáo A2DP/Classic nếu chưa hỗ trợ.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `row_00` | 2 | 29 | 233 | 41 | `selected` | Danh sách dòng 1 |
| `row_01` | 2 | 71 | 233 | 41 | `list` | Danh sách dòng 2 |
| `row_02` | 2 | 113 | 233 | 41 | `list` | Danh sách dòng 3 |
| `row_03` | 2 | 155 | 233 | 41 | `list` | Danh sách dòng 4 |
| `row_04` | 2 | 197 | 233 | 41 | `list` | Danh sách dòng 5 |
| `row_05` | 2 | 239 | 233 | 41 | `list` | Danh sách dòng 6 |
| `page_label` | 12 | 55 | 216 | 24 | `text` | Bluetooth |
| `empty_notice` | 12 | 101 | 216 | 20 | `dim` | Trạng thái trống |
| `empty_hint` | 12 | 126 | 216 | 20 | `dim` | Hướng dẫn thao tác tiếp theo |

### 5. File manager (`files`)

**Điều hướng:** D-Pad di chuyển; START Open; OPTION actions; A lên một thư mục  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Bất kỳ thao tác xóa file nào cũng phải xác nhận; path bị sandbox.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `path_bar` | 5 | 32 | 226 | 21 | `panel` | Đường dẫn hiện tại |
| `row_00` | 2 | 56 | 233 | 41 | `selected` | Danh sách dòng 1 |
| `row_01` | 2 | 98 | 233 | 41 | `list` | Danh sách dòng 2 |
| `row_02` | 2 | 140 | 233 | 41 | `list` | Danh sách dòng 3 |
| `row_03` | 2 | 182 | 233 | 41 | `list` | Danh sách dòng 4 |
| `row_04` | 2 | 224 | 233 | 41 | `list` | Danh sách dòng 5 |
| `scrollbar` | 236 | 56 | 2 | 211 | `scrollbar` | Thumb danh sách file |
| `empty_notice` | 12 | 103 | 216 | 25 | `dim` | microSD is not mounted |

### 6. Gallery (`gallery`)

**Điều hướng:** UP/DOWN/LEFT/RIGHT thumbnails; START View; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Thumbnail view là layout đích; baseline hiện có thể dùng danh sách.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `page_title` | 12 | 53 | 216 | 22 | `text` | Gallery |
| `thumb0` | 7 | 81 | 72 | 73 | `panel` | Thumbnail trái |
| `thumb1` | 84 | 81 | 72 | 73 | `panel` | Thumbnail giữa |
| `thumb2` | 161 | 81 | 72 | 73 | `panel` | Thumbnail phải |
| `thumb3` | 7 | 158 | 72 | 73 | `panel` | Hàng thumbnail 2 |
| `thumb4` | 84 | 158 | 72 | 73 | `panel` | Hàng thumbnail 2 |
| `thumb5` | 161 | 158 | 72 | 73 | `panel` | Hàng thumbnail 2 |
| `missing_sd` | 12 | 103 | 216 | 24 | `dim` | microSD is not mounted |

### 7. Music (`music`)

**Điều hướng:** START Play/Pause; OPTION playlist; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Không báo audio ready khi phần cứng DAC chưa sẵn sàng; list & empty states loại trừ.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `title_line` | 12 | 51 | 216 | 27 | `text` | Tên media / trạng thái |
| `row_00` | 2 | 36 | 233 | 41 | `selected` | Danh sách dòng 1 |
| `row_01` | 2 | 78 | 233 | 41 | `list` | Danh sách dòng 2 |
| `row_02` | 2 | 120 | 233 | 41 | `list` | Danh sách dòng 3 |
| `row_03` | 2 | 162 | 233 | 41 | `list` | Danh sách dòng 4 |
| `row_04` | 2 | 204 | 233 | 41 | `list` | Danh sách dòng 5 |
| `mini_player` | 6 | 251 | 228 | 40 | `panel` | Bộ điều khiển nhỏ không chồng footer |
| `mini_line1` | 11 | 255 | 218 | 14 | `text` | Audio ready hoặc trạng thái thực |
| `mini_line2` | 11 | 273 | 218 | 14 | `dim` | Track / shuffle / repeat / volume |

### 8. Shell (`shell`)

**Điều hướng:** START Command; UP history; OPTION actions; A Back  
**Hiện trạng:** integrated existing shell; coordinates specified for next reflow  
**Lưu ý:** Lệnh được allowlist/sandbox; không trao arbitrary shell cho .qeapp.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `terminal` | 2 | 28 | 236 | 228 | `terminal` | Ring buffer 10-12 dòng đầu ra |
| `terminal_info` | 3 | 31 | 234 | 16 | `text` | Shell version/đường dẫn |
| `prompt` | 3 | 256 | 234 | 38 | `terminal` | Lệnh nhập + tối đa 1 dòng gợi ý |
| `prompt_text` | 7 | 264 | 226 | 16 | `accent` | vqeaf:/$ |

### 9. Settings (`settings`)

**Điều hướng:** UP/DOWN chọn; LEFT/RIGHT thay đổi nếu hợp lệ; START nhập; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Lưu NVS; trình render tuyệt đối không chạy WiFi scan.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `setting_0` | 2 | 29 | 233 | 41 | `selected` | Theme |
| `setting_1` | 2 | 71 | 233 | 41 | `list` | Backlight |
| `setting_2` | 2 | 113 | 233 | 41 | `list` | Audio volume |
| `setting_3` | 2 | 155 | 233 | 41 | `list` | Clock format |
| `setting_4` | 2 | 197 | 233 | 41 | `list` | Auto WiFi strongest |
| `setting_5` | 2 | 239 | 233 | 41 | `list` | Auto keypad lock |
| `scrollbar` | 236 | 30 | 2 | 264 | `scrollbar` | Theo số mục |

### 10. Themes (`themes`)

**Điều hướng:** START Apply; OPTION Import/Info; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Theme Studio DSL @vqeaf 1.x: palette và launcher overrides; tài nguyên phoneShell nằm ngoài LCD.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `theme_0` | 2 | 29 | 233 | 41 | `selected` | Theme row 1 |
| `theme_1` | 2 | 71 | 233 | 41 | `list` | Theme row 2 |
| `theme_2` | 2 | 113 | 233 | 41 | `list` | Theme row 3 |
| `theme_3` | 2 | 155 | 233 | 41 | `list` | Theme row 4 |
| `theme_4` | 2 | 197 | 233 | 41 | `list` | Theme row 5 |
| `theme_5` | 2 | 239 | 233 | 41 | `list` | Theme row 6 |
| `theme_icon` | 11 | 36 | 25 | 25 | `icon` | Preview mini palette |
| `scrollbar` | 236 | 30 | 2 | 264 | `scrollbar` | Có khi số theme >6 |

### 11. Applications (`applications`)

**Điều hướng:** START Open; OPTION Info/Uninstall; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Chỉ nhận installed QEAPP/2 signed + built-in; không chạy Lua native mới.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `app_0` | 2 | 29 | 233 | 41 | `selected` | Installed/system app row |
| `app_1` | 2 | 71 | 233 | 41 | `list` | Installed/system app row |
| `app_2` | 2 | 113 | 233 | 41 | `list` | Installed/system app row |
| `app_3` | 2 | 155 | 233 | 41 | `list` | Installed/system app row |
| `app_4` | 2 | 197 | 233 | 41 | `list` | Installed/system app row |
| `app_5` | 2 | 239 | 233 | 41 | `list` | Installed/system app row |
| `app_icon` | 10 | 36 | 27 | 27 | `icon` | Icon của app |
| `scrollbar` | 236 | 30 | 2 | 264 | `scrollbar` | Danh sách app |

### 12. App installer (`installer`)

**Điều hướng:** UP/DOWN chọn; START xem/xác minh; OPTION Install; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Không đổi QEAPP/2 định dạng ECDSA-P256; dialog loại trừ empty state.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `inbox_label` | 12 | 42 | 216 | 28 | `text` | App inbox |
| `row_00` | 2 | 76 | 233 | 41 | `selected` | Danh sách dòng 1 |
| `row_01` | 2 | 118 | 233 | 41 | `list` | Danh sách dòng 2 |
| `row_02` | 2 | 160 | 233 | 41 | `list` | Danh sách dòng 3 |
| `row_03` | 2 | 202 | 233 | 41 | `list` | Danh sách dòng 4 |
| `row_04` | 2 | 244 | 233 | 41 | `list` | Danh sách dòng 5 |
| `details_modal` | 11 | 77 | 218 | 166 | `modal` | Manifest/publisher/signature trước khi cài |
| `missing_sd` | 12 | 106 | 216 | 23 | `dim` | microSD is not available |

### 13. Recovery (`recovery`)

**Điều hướng:** UP/DOWN chọn; START Select; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Giữ DOWN lúc khởi động; không tải theme từ SD trước khi chọn Safe Mode.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `recovery_0` | 2 | 29 | 233 | 41 | `selected` | Boot status |
| `recovery_1` | 2 | 71 | 233 | 41 | `list` | Start normal mode |
| `recovery_2` | 2 | 113 | 233 | 41 | `list` | Enable Safe Mode |
| `recovery_3` | 2 | 155 | 233 | 41 | `list` | Clear recovery flags |
| `recovery_4` | 2 | 197 | 233 | 41 | `list` | Restart device |
| `confirmation_modal` | 14 | 88 | 212 | 132 | `modal` | Cần xác nhận Clear/Restart |

### 14. Calculator (`calculator`)

**Điều hướng:** D-pad move 4×4; START Enter; B xóa; OPTION reset; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Khu vực 4×4 không vượt vào footer; phép chia 0 được chặn.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `result` | 12 | 40 | 216 | 48 | `panel` | Biểu thức/kết quả |
| `result_text` | 17 | 52 | 205 | 30 | `text` | Số được căn phải, ellipsis nếu quá dài |
| `key_0` | 8 | 100 | 51 | 41 | `selected` | Phím 7 |
| `key_1` | 64 | 100 | 51 | 41 | `panel` | Phím 8 |
| `key_2` | 120 | 100 | 51 | 41 | `panel` | Phím 9 |
| `key_3` | 176 | 100 | 51 | 41 | `panel` | Phím / |
| `key_4` | 8 | 145 | 51 | 41 | `panel` | Phím 4 |
| `key_5` | 64 | 145 | 51 | 41 | `panel` | Phím 5 |
| `key_6` | 120 | 145 | 51 | 41 | `panel` | Phím 6 |
| `key_7` | 176 | 145 | 51 | 41 | `panel` | Phím * |
| `key_8` | 8 | 190 | 51 | 41 | `panel` | Phím 1 |
| `key_9` | 64 | 190 | 51 | 41 | `panel` | Phím 2 |
| `key_10` | 120 | 190 | 51 | 41 | `panel` | Phím 3 |
| `key_11` | 176 | 190 | 51 | 41 | `panel` | Phím - |
| `key_12` | 8 | 235 | 51 | 41 | `panel` | Phím C |
| `key_13` | 64 | 235 | 51 | 41 | `panel` | Phím 0 |
| `key_14` | 120 | 235 | 51 | 41 | `panel` | Phím = |
| `key_15` | 176 | 235 | 51 | 41 | `panel` | Phím + |
| `instruction` | 8 | 284 | 224 | 11 | `dim` | Options / Backspace / Clear |

### 15. Stopwatch (`stopwatch`)

**Điều hướng:** LEFT/RIGHT chọn; START thao tác; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Chỉ dirty vùng đồng hồ theo tick; thời gian chạy ở service không phụ thuộc màn hình.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `face` | 10 | 52 | 220 | 74 | `panel` | 00:00.00 hoặc elapsed |
| `digits` | 22 | 79 | 196 | 34 | `text` | Thời gian căn giữa |
| `start` | 8 | 135 | 72 | 36 | `selected` | Start/Pause |
| `lap` | 84 | 135 | 72 | 36 | `panel` | Lap |
| `reset` | 160 | 135 | 72 | 36 | `panel` | Reset |
| `lap_list` | 8 | 178 | 224 | 113 | `list` | Các vòng đã ghi (nếu có) |

### 16. Qeafbrowser (`browser`)

**Điều hướng:** OPTION browser menu; START open link; D-pad scroll/focus; SELECT hold T9; A Back  
**Hiện trạng:** target; retained baseline screen behavior  
**Lưu ý:** Offline và loaded là 2 trạng thái loại trừ; HTTPS phải kiểm CA khi có.

| Element | X | Y | W | H | Token | Mô tả |
|---|---:|---:|---:|---:|---|---|
| `address` | 4 | 29 | 232 | 26 | `panel` | Thanh địa chỉ nếu ở browser loaded mode |
| `page_view` | 4 | 59 | 232 | 234 | `list` | Viewport HTML/WML và scrollbar |
| `offline_header` | 12 | 57 | 216 | 22 | `text` | Qeafbrowser khi WiFi offline |
| `offline_reason` | 12 | 100 | 216 | 20 | `dim` | WiFi is not connected |
| `offline_hint` | 12 | 121 | 216 | 22 | `dim` | Options > Home can open cache |

## C. Trạng thái và điều kiện kiểm tra pixel

1. Tối thiểu 16 màn/17 trạng thái contact sheet, cả focus và no-SD/no-WiFi. Ảnh baseline của người dùng là nguồn bố cục; các tọa độ như đồng hồ, shortcut và grid dựa vào mã `SymbianUI.cpp`.
2. Mọi vùng trong atlas phải nằm trong màn 240×320; mọi body không tràn lên footer. Trùng nhau **có chủ đích** đối với vùng container và các trạng thái loại trừ.
3. Grid menu gồm 12 cell, 3 cột × 4 hàng. Biên giới cuối: cột 3 kết thúc x=233, hàng 4 kết thúc y=292; rail x=236; footer từ y=298.
4. RTL/fallback font tiếng Việt: baseline TFT_eSPI font bitmap có thể thiếu dấu; nâng cấp font cần bổ sung glyph/UTF-8 riêng, KHÔNG tuyên bố đã đầy đủ.
5. Giao diện `.vqeaf` với background/base64 của lớp **vỏ điện thoại ảo** không hiển thị trên màn ST7789 vật lý; chỉ các màu LCD `palette`/`launcher` được sử dụng.

## D. Từ atlas tới firmware

`tools/gen_pixel_atlas.py` xuất `docs/pixel_atlas.json`, tài liệu này và `src/core/UiScreenAtlas.h/.cpp` để đồng bộ màn hình. `src/core/UiLayoutGeometry.h` chứa tọa độ chrome/grid/list chính đang được `SymbianUI.h` dùng. Chạy `python tools/test_ui_v22.py` để kiểm tra bounds và build/link host.
