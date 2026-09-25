// rg_theme.h — đọc theme.json ĐÚNG schema Retro-Go (retro-go/THEMING.md):
//   {
//     "dialog":     { background, foreground, border, header, scrollbar, shadow,
//                     item_standard, item_disabled, item_message },
//     "launcher_1..4": { background, foreground, list_standard_bg/fg,
//                        list_selected_bg/fg }
//   }
// Màu RGB565: số nguyên (4129) hoặc chuỗi hex ("0x0010"); "transparent" = RG_C_TRANSPARENT
// (magenta 0xF81F — không tô nền), "none" = RG_C_NONE (không vẽ gì).
// Thiếu key -> dùng giá trị của theme mặc định Retro-Go (themes/default/theme.json).
// File hỏng/thiếu SD -> rơi về mặc định, KHÔNG crash.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "rg_config.h"

typedef uint16_t rg_color_t;

typedef struct {
  rg_color_t background, foreground;
  rg_color_t list_standard_bg, list_standard_fg;
  rg_color_t list_selected_bg, list_selected_fg;
} rg_theme_launcher_t;   // section launcher_N (mỗi tab một tông)

typedef struct {
  rg_color_t background, foreground, border, header, scrollbar, shadow;
  rg_color_t item_standard, item_disabled, item_message;
} rg_theme_dialog_t;     // section dialog (dùng chung mọi hộp thoại)

typedef struct {
  rg_theme_dialog_t   dialog;
  rg_theme_launcher_t launcher[4];
  char name[RG_THEME_NAME_MAX];
  bool from_file;        // true = đọc từ file, false = mặc định build-in
} rg_theme_t;

void              rg_theme_defaults(rg_theme_t *t);   // theme mặc định Retro-Go
bool              rg_theme_load(const char *name);    // SD -> LittleFS -> mặc định
const rg_theme_t *rg_theme_current();
int               rg_theme_list(char names[][RG_THEME_NAME_MAX], int maxn); // liệt kê thư mục theme
bool              rg_theme_next(int dir);             // đổi theme kế (Settings dùng)
void              rg_theme_set_name(const char *name);// ghi nhớ tên theme hiện tại
const char       *rg_theme_name();
void              rg_theme_set_sd_mounted(bool mounted); // main báo SD đã mount

