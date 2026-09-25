// rg_gui.h — lớp vẽ cao, port retro-go/components/retro-go/rg_gui.c sang
// LovyanGFX + framebuffer rg_display. Giữ nguyên API/tham số/flags của Retro-Go
// (rg_gui_draw_text/draw_rect/draw_image/draw_dialog/rg_gui_dialog...).
// Font: stream rg_font_t của Retro-Go (tools/make_font.py), hỗ trợ tiếng Việt.
// Mọi thao tác ghi vào framebuffer; caller gọi rg_display_flush() sau khi vẽ.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "rg_config.h"
#include "rg_theme.h"   // rg_color_t

// ---- màu đặc biệt (rg_gui.h Retro-Go) ----
#define C_NONE        RG_C_NONE        // không vẽ gì
#define C_TRANSPARENT RG_C_TRANSPARENT // magenta: giữ nền hiện có
// vài màu hay dùng (RGB565)
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_GRAY        0x8410
#define C_SILVER      0xC618
#define C_RED         0xF800
#define C_GREEN       0x07E0
#define C_BLUE        0x001F
#define C_YELLOW      0xFFE0

// ---- tọa độ đặc biệt (rg_gui.h Retro-Go: vị trí tương đối màn hình) ----
// 32 bit: bit 23..22 = khoảng (0=abs), phần dưới = độ lệch. CENTER/TOP/BOTTOM/LEFT/RIGHT.
#define RG_GUI_CENTER         0x00C000
#define RG_GUI_TOP            0x400000
#define RG_GUI_BOTTOM         0x800000
#define RG_GUI_LEFT           0x400000
#define RG_GUI_RIGHT          0x800000

// ---- flags của draw_text (rg_gui.h Retro-Go) ----
#define RG_TEXT_ALIGN_LEFT    0x00000000
#define RG_TEXT_ALIGN_CENTER  0x01000000
#define RG_TEXT_ALIGN_RIGHT   0x02000000
#define RG_TEXT_NO_PADDING    0x04000000
#define RG_TEXT_DUMMY_DRAW    0x08000000   // chỉ đo, không vẽ (TEXT_RECT)
#define RG_TEXT_MULTILINE     0x10000000   // tự xuống dòng theo width
#define RG_TEXT_BIGGER        0x20000000   // font lớn (Tahoma 16)
#define RG_TEXT_MONOSPACE     0x40000000   // cách đều theo font->width (ít dùng)

typedef struct { int x, y, w, h; } rg_rect_t;

typedef struct { int width, height; const uint16_t *data; } rg_image_t;

// forward decl cho rg_text_rect
rg_rect_t rg_gui_draw_text(int x, int y, int width, const char *text,
                           rg_color_t color_fg, rg_color_t color_bg, uint32_t flags);

// Đo kích thước textbox (như TEXT_RECT của Retro-Go)
static inline rg_rect_t rg_text_rect(const char *text, int width, uint32_t flags) {
  return rg_gui_draw_text(-width, 0, width, text, C_BLACK, C_BLACK,
                          (flags | RG_TEXT_DUMMY_DRAW | RG_TEXT_MULTILINE));
}

// ---- dialog (rg_gui.h Retro-Go) ----
typedef enum {
  RG_DIALOG_FLAG_NORMAL = 0,
  RG_DIALOG_FLAG_HIDDEN,
  RG_DIALOG_FLAG_DISABLED,
  RG_DIALOG_FLAG_SKIP,
  RG_DIALOG_FLAG_SEPARATOR,
  RG_DIALOG_FLAG_MESSAGE,
} rg_gui_flag_t;
#define RG_DIALOG_FLAG_MODE_MASK 0x0F

typedef enum {
  RG_DIALOG_INIT = 0,
  RG_DIALOG_PREV,
  RG_DIALOG_NEXT,
  RG_DIALOG_ENTER,
  RG_DIALOG_SELECT,
  RG_DIALOG_CANCEL,
  RG_DIALOG_REDRAW,
  RG_DIALOG_UPDATE,
  RG_DIALOG_FOCUS_GAINED,
  RG_DIALOG_FOCUS_LOST,
  RG_DIALOG_VOID,
} rg_gui_event_t;

struct rg_gui_option_s;
typedef rg_gui_event_t (*rg_gui_callback_t)(struct rg_gui_option_s *, rg_gui_event_t event);

typedef struct rg_gui_option_s {
  intptr_t arg;
  const char *label;
  char *value;                 // có value -> hiển thị 2 cột "label: value", LEFT/RIGHT đổi
  int flags;                   // rg_gui_flag_t
  rg_gui_callback_t update_cb; // NULL = mục tĩnh
} rg_gui_option_t;

#define RG_DIALOG_CANCELLED ((intptr_t)-1)
#define RG_DIALOG_CHOICE_FIRST 0
#define RG_DIALOG_END {0, NULL, NULL, RG_DIALOG_FLAG_HIDDEN, NULL}

void       rg_gui_init();
int        rg_gui_font_height(int size);              // 14 (small) / 20 (big)
int        rg_gui_text_width(const char *s, int size);
void       rg_gui_draw_rect(int x, int y, int w, int h, int border_size,
                            rg_color_t border_color, rg_color_t fill_color);
void       rg_gui_draw_image(int x, int y, int w, int h, bool resample, const rg_image_t *img);
rg_rect_t  rg_gui_draw_dialog(const char *title, const rg_gui_option_t *options,
                              int options_count, int sel);
rg_rect_t  rg_gui_draw_message(const char *format, ...);
intptr_t   rg_gui_dialog(const char *title, const rg_gui_option_t *options, int selected_index);
rg_color_t rg_gui_rgb888(uint8_t r, uint8_t g, uint8_t b);

// Đồng bộ style dialog từ theme hiện hành (gọi lại sau khi đổi theme)
void       rg_gui_apply_theme();
