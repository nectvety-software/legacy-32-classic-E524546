// rg_config.h — MỌI hằng số của Qeafbrowser (Retro-Go UI) trong MỘT file duy nhất.
// Không hard-code chân/kích thước/màu/phím ở nơi khác. Đổi phần cứng -> sửa file này.
// Tham chiếu: retro-go/launcher/main/gui.c + components/retro-go/rg_gui.c (layout),
// retro-go/THEMING.md (schema theme + kích thước asset), pins.h (GPIO bất khả xâm phạm).
#pragma once
#include <Arduino.h>
#include "pins.h"

// ------------------------------------------------- màn hình: DỌC 240x320 (xác nhận của user)
// Retro-Go gốc là 320x240 ngang; thiết bị này giữ dọc, layout Retro-Go được thích ứng
// nguyên tỷ lệ và thứ tự vùng (header 50px, logo trái, banner phải, status, list, preview).
#define RG_SCREEN_W            240
#define RG_SCREEN_H            320
#define RG_SCREEN_ROTATION     0        // LovyanGFX setRotation(0) = dọc

// ------------------------------------------------- layout launcher (port gui.c, dọc)
#define RG_HEADER_HEIGHT       50       // HEADER_HEIGHT của gui.c
#define RG_LOGO_WIDTH          46       // LOGO_WIDTH của gui.c (logo tab 46x50)
#define RG_LOGO_HEIGHT         50       // logo_<tab>.png chuẩn Retro-Go 46x50
#define RG_BANNER_X            (RG_LOGO_WIDTH + 1)          // banner bắt đầu sau logo
#define RG_BANNER_Y            8                            // offset + 8 trong gui.c
#define RG_BANNER_H            (RG_HEADER_HEIGHT - 8)       // 42
#define RG_BANNER_W            (RG_SCREEN_W - RG_BANNER_X)  // 193 (asset gốc 272x24, crop giữa)
#define RG_BANNER_SRC_W        272      // kích thước asset banner chuẩn Retro-Go
#define RG_BANNER_SRC_H        24
#define RG_STATUS_X            (RG_LOGO_WIDTH + 12)         // status_x trong gui.c
#define RG_STATUS_Y            (RG_HEADER_HEIGHT - 16)      // 34
#define RG_LIST_TOP            (RG_HEADER_HEIGHT + 6)       // list bắt đầu, gui.c: top = 50 + 6
#define RG_PREVIEW_W           (RG_SCREEN_W / 2)            // PREVIEW_WIDTH = 50% chiều rộng
#define RG_PREVIEW_H           ((int)(RG_SCREEN_H * 0.70f)) // PREVIEW_HEIGHT = 70% chiều cao
#define RG_COVER_W             160      // ảnh bìa chuẩn Retro-Go 160x168
#define RG_COVER_H             168
#define RG_THEME_PREVIEW_W     160      // preview.png của theme 160x120
#define RG_THEME_PREVIEW_H     120
#define RG_GUI_DIALOG_MAX_ITEMS 32      // tối đa mục trong 1 hộp thoại

// ------------------------------------------------- màu đặc biệt (rg_gui.h Retro-Go)
#define RG_MAGENTA             0xF81F   // màu trong suốt của ảnh PNG
#define RG_C_TRANSPARENT       0xF81F   // "transparent" trong theme.json
#define RG_C_NONE              0xFFFE   // "none" trong theme.json (không vẽ)

// ------------------------------------------------- phím (keypad 3x3 + SELECT, pins.h)
// Bitmask sự kiện. Tên giữ đúng chuẩn repo: menu/up/back/left/ok/right/option/down/delete/mode.
#define RG_KEY_MENU            (1u << 0)   // GPIO 18 — Home / menu chính
#define RG_KEY_UP              (1u << 1)   // GPIO 7
#define RG_KEY_BACK            (1u << 2)   // GPIO 15 (A) — quay lại/đóng
#define RG_KEY_LEFT            (1u << 3)   // GPIO 45 — đổi tab / đổi giá trị
#define RG_KEY_OK              (1u << 4)   // GPIO 17 (START) — mở/chấp nhận
#define RG_KEY_RIGHT           (1u << 5)   // GPIO 6
#define RG_KEY_OPTION          (1u << 6)   // GPIO 8 — hộp thoại tùy chọn/thuộc tính
#define RG_KEY_DOWN            (1u << 7)   // GPIO 46
#define RG_KEY_DELETE          (1u << 8)   // GPIO 5 (B) — xóa / thao tác phụ
#define RG_KEY_MODE            (1u << 9)   // GPIO 16 (SELECT) — nhấn thường; giữ >600ms đổi Game/T9
#define RG_KEY_ALL             0x3FFu
// Nhóm phím được auto-repeat khi giữ (cuộn danh sách)
#define RG_KEY_REPEAT_MASK     (RG_KEY_UP | RG_KEY_DOWN | RG_KEY_LEFT | RG_KEY_RIGHT)

#define RG_DEBOUNCE_MS         25       // chuẩn repo (docs/SKILLS.md)
#define RG_REPEAT_DELAY_MS     450      // giữ lâu bắt đầu lặp
#define RG_REPEAT_RATE_MS      110      // tốc độ lặp
#define RG_SELECT_HOLD_MS      600      // giữ SELECT đổi Game/T9 (chuẩn repo)

// ------------------------------------------------- đường dẫn (SD ưu tiên, LittleFS dự phòng)
// Theme Retro-Go chép nguyên thư mục themes/ vào SD là dùng được (THEMING.md).
#define RG_DIR_SD_THEMES       "/retro-go/themes"          // trên SD_MMC
#define RG_DIR_LFS_THEMES      "/retro-go/themes"          // trên LittleFS (uploadfs từ data/)
#define RG_THEME_DEFAULT       "default"
#define RG_THEME_JSON_MAX      4096     // giới hạn đọc theme.json
#define RG_THEME_NAME_MAX      24
#define RG_THEMES_MAX          16
// Dữ liệu trình duyệt (G4/G5 dùng; khai báo sẵn ở file config duy nhất)
#define RG_DIR_BROWSER         "/Qeafbrowser"
#define RG_FILE_WIFI_JSON      "/Qeafbrowser/wifi.json"    // tối đa 4 mạng
#define RG_FILE_CRASH_LOG      "/crash.log"                // log crash ra SD
#define RG_FILE_SETTINGS       "/Qeafbrowser/settings.json"

// ------------------------------------------------- bộ nhớ / hiệu năng
#define RG_FB_PIXELS           (RG_SCREEN_W * RG_SCREEN_H)
#define RG_FB_BYTES            (RG_FB_PIXELS * 2)          // framebuffer RGB565 trong PSRAM

// ------------------------------------------------- font (Retro-Go rg_font_t stream, tools/make_font.py)
// size 1 = Tahoma 12 (cao 14px, UI chính); size 2 = Tahoma 16 (cao 20px, banner/BIGGER).
#define RG_FONT_SMALL          1
#define RG_FONT_BIG            2

// ------------------------------------------------- độ sáng backlight (PWM LEDK GPIO 39)
#define RG_BRIGHTNESS_DEFAULT  200
