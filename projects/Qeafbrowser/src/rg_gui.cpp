// rg_gui.cpp — cài đặt lớp vẽ cao (xem rg_gui.h).
// Port nguyên thuật toán retro-go/components/retro-go/rg_gui.c:
//   draw_text: padding 1px, line_height = font_height + 2, căn trái/giữa/phải,
//              MULTILINE tự xuống dòng tại ranh giới glyph, DUMMY_DRAW chỉ đo.
//   draw_rect: viền + tô nền; C_TRANSPARENT/C_NONE bỏ qua phần tô.
//   draw_image: resample nearest-neighbor khi đổi kích thước; magenta = trong suốt.
//   draw_dialog + rg_gui_dialog: hộp 0.82 màn, 2 cột "label: value", cuộn có mũi tên.
// Khác biệt duy nhất: vẽ vào framebuffer rg_display thay vì SPI trực tiếp.
#include "rg_gui.h"
#include "rg_display.h"
#include "rg_input.h"
#include "rg_theme.h"
#include "fonts/lc_font_vn12.h"
#include "fonts/lc_font_vn16.h"
#include <string.h>
#include <stdio.h>
#include <Arduino.h>

typedef LcFont rg_font_t;    // stream rg_font_t của Retro-Go (make_font.py)

// style dialog lấy từ theme (section "dialog")
static struct {
  rg_color_t box_background, box_foreground, box_border, box_header,
             scrollbar, shadow, item_standard, item_disabled, item_message;
} gui_style;

static const rg_font_t *pick_font(uint32_t flags) {
  return (flags & RG_TEXT_BIGGER) ? &lc_font_vn16 : &lc_font_vn12;
}

void rg_gui_apply_theme() {
  const rg_theme_dialog_t &d = rg_theme_current()->dialog;
  gui_style.box_background = d.background;
  gui_style.box_foreground = d.foreground;
  gui_style.box_border     = d.border;
  gui_style.box_header     = d.header;
  gui_style.scrollbar      = d.scrollbar;
  gui_style.shadow         = d.shadow;
  gui_style.item_standard  = d.item_standard;
  gui_style.item_disabled  = d.item_disabled;
  gui_style.item_message   = d.item_message;
}

void rg_gui_init() { rg_gui_apply_theme(); }

rg_color_t rg_gui_rgb888(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ------------------------------------------------- font engine (port lc_* của v1.7)
static int rg_utf8_decode(const char **ptr) {
  const unsigned char *p = (const unsigned char *)*ptr;
  if (!p || !*p) return 0;
  int c;
  if (*p < 0x80) { c = *p; *ptr += 1; }
  else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
    c = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *ptr += 2;
  } else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
    c = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *ptr += 3;
  } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
             (p[3] & 0xC0) == 0x80) {
    c = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    *ptr += 4;
  } else { *ptr += 1; return 0xFFFD; }
  return c;
}

static const uint8_t *find_glyph(const rg_font_t *font, int code, int *adv) {
  const uint8_t *ptr = font->data;
  for (;;) {
    uint16_t gc = (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
    if (gc == 0) return NULL;
    if (gc == (uint16_t)code) { if (adv) *adv = ptr[6]; return ptr; }
    int w = ptr[3], h = ptr[4];
    int blen = (w == 0 || h == 0) ? 0 : (((w * h) - 1) / 8 + 1);
    ptr += 7 + blen;
  }
}

// Decode glyph -> rows[y] (bit 0 = pixel trái nhất). Trả về advance width.
static int get_glyph_rows(uint32_t *rows, const rg_font_t *font, int code) {
  int adv = 0;
  const uint8_t *g = find_glyph(font, code, &adv);
  for (int y = 0; y < (int)font->height; y++) rows[y] = 0;
  if (!g) return font->width ? font->width : 5;   // glyph thiếu -> khoảng trống
  int ofs_y = g[2], w = g[3], h = g[4], ofs_x = g[5];
  const uint8_t *data = g + 7;
  int ch = 0, mask = 0x80;
  for (int y = 0; y < h; y++) {
    uint32_t row = 0;
    for (int x = 0; x < w; x++) {
      if (((x + y * w) % 8) == 0) { mask = 0x80; ch = *data++; }
      if (ch & mask) { int px = ofs_x + x; if (px >= 0 && px < 32) row |= (1u << px); }
      mask >>= 1;
    }
    int oy = ofs_y + y;
    if (oy >= 0 && oy < (int)font->height) rows[oy] |= row;
  }
  return adv ? adv : (ofs_x + w);
}

int rg_gui_font_height(int size) { return (size >= RG_FONT_BIG) ? 20 : 14; }

int rg_gui_text_width(const char *s, int size) {
  if (!s) return 0;
  const rg_font_t *font = (size >= RG_FONT_BIG) ? &lc_font_vn16 : &lc_font_vn12;
  int w = 0;
  const char *p = s;
  while (*p) {
    int c = rg_utf8_decode(&p);
    if (c == '\n' || c == '\r') break;
    int adv = 0; find_glyph(font, c, &adv);
    w += adv;
  }
  return w;
}

// ------------------------------------------------- tọa độ tương đối (rg_gui.c)
static int hpos(int x, int width) {
  if (x == (int)RG_GUI_CENTER) return (RG_SCREEN_W - width) / 2;
  if (x == (int)RG_GUI_LEFT)   return 0;
  if (x == (int)RG_GUI_RIGHT)  return RG_SCREEN_W - width;
  if (x < 0) return RG_SCREEN_W + x - width;
  return x;
}
static int vpos(int y, int height) {
  if (y == (int)RG_GUI_CENTER) return (RG_SCREEN_H - height) / 2;
  if (y == (int)RG_GUI_TOP)    return 0;
  if (y == (int)RG_GUI_BOTTOM) return RG_SCREEN_H - height;
  if (y < 0) return RG_SCREEN_H + y - height;
  return y;
}
// ------------------------------------------------- draw_text (port rg_gui.c 377-498)
rg_rect_t rg_gui_draw_text(int x_pos, int y_pos, int width, const char *text,
                           rg_color_t color_fg, rg_color_t color_bg, uint32_t flags) {
  const rg_font_t *font = pick_font(flags);
  int padding = (flags & RG_TEXT_NO_PADDING) ? 0 : 1;
  int font_height = (int)font->height;
  int line_height = font_height + padding * 2;
  int line_count = 0;

  if (!text || *text == 0) text = " ";

  if (width == 0) {
    int line_width = padding * 2;
    for (const char *ptr = text; *ptr;) {
      int chr = rg_utf8_decode(&ptr);
      int adv = 0; find_glyph(font, chr, &adv);
      line_width += adv;
      if (chr == '\n' || *ptr == 0) {
        if (line_width > width) width = line_width;
        line_width = padding * 2;
        line_count++;
      }
    }
  }

  x_pos = hpos(x_pos, width);
  y_pos = vpos(y_pos, line_height);
  if (x_pos >= RG_SCREEN_W || y_pos >= RG_SCREEN_H) return {x_pos, y_pos, 0, 0};
  int draw_width = width < (RG_SCREEN_W - x_pos) ? width : (RG_SCREEN_W - x_pos);
  if (draw_width <= 0) return {x_pos, y_pos, 0, 0};
  int y_offset = 0;

  for (const char *ptr = text; *ptr;) {
    int x_offset = padding;

    if (flags & (RG_TEXT_ALIGN_RIGHT | RG_TEXT_ALIGN_CENTER)) {
      const char *line = ptr;
      while (x_offset < draw_width && *line && *line != '\n') {
        int chr = rg_utf8_decode(&line);
        int adv = 0; find_glyph(font, chr, &adv);
        if (draw_width - x_offset < adv) break;   // không cắt nửa glyph
        x_offset += adv;
      }
      if (flags & RG_TEXT_ALIGN_CENTER)      x_offset = (draw_width - x_offset) / 2;
      else if (flags & RG_TEXT_ALIGN_RIGHT)  x_offset = draw_width - x_offset;
    }

    if (!(flags & RG_TEXT_DUMMY_DRAW)) {
      // tô nền dòng (trừ khi transparent/none) — Retro-Go get_draw_buffer(fill bg)
      if (color_bg != C_TRANSPARENT && color_bg != C_NONE)
        rg_display_fill_rect(x_pos, y_pos + y_offset, draw_width, line_height, color_bg);
    }

    while (x_offset < draw_width) {
      uint32_t rows[32];
      const char *prev_ptr = ptr;
      int chr = rg_utf8_decode(&ptr);
      int glyph_width = get_glyph_rows(rows, font, chr);
      if (draw_width - x_offset < glyph_width) {
        if (flags & RG_TEXT_MULTILINE) ptr = prev_ptr;
        break;
      }
      if (!(flags & RG_TEXT_DUMMY_DRAW) && color_fg != C_TRANSPARENT && color_fg != C_NONE) {
        uint16_t *fb = rg_display_buffer();
        for (int gy = 0; gy < font_height; gy++) {
          uint32_t row = rows[gy];
          if (!row) continue;
          int yy = y_pos + y_offset + padding + gy;
          if (yy < 0 || yy >= RG_SCREEN_H) continue;
          for (int gx = 0; gx < 32; gx++) {
            if (!(row & (1u << gx))) continue;
            int xx = x_pos + x_offset + gx;
            if (xx >= 0 && xx < RG_SCREEN_W) fb[yy * RG_SCREEN_W + xx] = color_fg;
          }
        }
      }
      x_offset += glyph_width;
      if (*ptr == 0 || *ptr == '\n') { if (*ptr == '\n') ptr++; break; }
    }

    y_offset += line_height;
    if (!(flags & RG_TEXT_MULTILINE)) break;
  }

  return {x_pos, y_pos, draw_width, y_offset};
}

// ------------------------------------------------- draw_rect (port rg_gui.c 500-532)
void rg_gui_draw_rect(int x_pos, int y_pos, int width, int height, int border_size,
                      rg_color_t border_color, rg_color_t fill_color) {
  if (width <= 0 || height <= 0) return;
  x_pos = hpos(x_pos, width);
  y_pos = vpos(y_pos, height);
  if (border_size > 0 && border_color != C_TRANSPARENT && border_color != C_NONE) {
    rg_display_fill_rect(x_pos, y_pos, width, border_size, border_color);
    rg_display_fill_rect(x_pos, y_pos + height - border_size, width, border_size, border_color);
    rg_display_fill_rect(x_pos, y_pos, border_size, height, border_color);
    rg_display_fill_rect(x_pos + width - border_size, y_pos, border_size, height, border_color);
    x_pos += border_size; y_pos += border_size;
    width -= border_size * 2; height -= border_size * 2;
  }
  if (width > 0 && height > 0 && fill_color != C_NONE && fill_color != C_TRANSPARENT)
    rg_display_fill_rect(x_pos, y_pos, width, height, fill_color);
}

// ------------------------------------------------- draw_image (port rg_gui.c 534-553)
// Resample nearest-neighbor khi w/h khác ảnh; pixel magenta (0xF81F) = trong suốt.
void rg_gui_draw_image(int x_pos, int y_pos, int width, int height, bool resample,
                       const rg_image_t *img) {
  if (!img || !img->data || img->width <= 0 || img->height <= 0) {
    rg_gui_draw_rect(x_pos, y_pos, width, height, 2, C_RED, C_BLACK);
    return;
  }
  int dw = width ? (width < img->width ? width : img->width) : img->width;
  int dh = height ? (height < img->height ? height : img->height) : img->height;
  uint16_t *fb = rg_display_buffer();
  if (resample && width && height && (width != img->width || height != img->height)) {
    for (int j = 0; j < height; j++) {
      int sy = (int)((int64_t)j * img->height / height);
      int yy = y_pos + j;
      if (yy < 0 || yy >= RG_SCREEN_H) continue;
      for (int i = 0; i < width; i++) {
        int sx = (int)((int64_t)i * img->width / width);
        uint16_t px = img->data[sy * img->width + sx];
        if (px == RG_MAGENTA) continue;
        int xx = x_pos + i;
        if (xx >= 0 && xx < RG_SCREEN_W) fb[yy * RG_SCREEN_W + xx] = px;
      }
    }
    return;
  }
  for (int j = 0; j < dh; j++) {
    int yy = y_pos + j;
    if (yy < 0 || yy >= RG_SCREEN_H) continue;
    for (int i = 0; i < dw; i++) {
      uint16_t px = img->data[j * img->width + i];
      if (px == RG_MAGENTA) continue;
      int xx = x_pos + i;
      if (xx >= 0 && xx < RG_SCREEN_W) fb[yy * RG_SCREEN_W + xx] = px;
    }
  }
}
// ------------------------------------------------- draw_dialog (port rg_gui.c 675-845)
#define TEXT_RECT(text, width) rg_gui_draw_text(-(width), 0, (width), (text), C_BLACK, C_BLACK, \
                                                RG_TEXT_DUMMY_DRAW | RG_TEXT_MULTILINE)
rg_rect_t rg_gui_draw_dialog(const char *title, const rg_gui_option_t *options,
                             int options_count, int sel) {
  const int font_height = (int)lc_font_vn12.height;
  const int sep_width = TEXT_RECT(": ", 0).w;
  const int max_box_width = (int)(0.82f * RG_SCREEN_W);
  const int max_box_height = (int)(0.82f * RG_SCREEN_H);
  const int box_padding = 6;
  const int row_padding_x = 8;
  const int max_inner_width = max_box_width - sep_width - (row_padding_x + box_padding) * 2;

  int box_width = box_padding * 2;
  int box_height = box_padding * 2 + (title ? font_height + 6 : 0);
  int inner_width = TEXT_RECT(title ? title : "", 0).w;
  int col1_width = -1, col2_width = -1;
  uint8_t row_height[RG_GUI_DIALOG_MAX_ITEMS];

  if (options_count > RG_GUI_DIALOG_MAX_ITEMS) options_count = RG_GUI_DIALOG_MAX_ITEMS;

  for (int i = 0; i < options_count; i++) {
    if (options[i].flags == RG_DIALOG_FLAG_HIDDEN) { row_height[i] = 0; continue; }
    rg_rect_t label = TEXT_RECT(options[i].label ? options[i].label : "", max_inner_width);
    rg_rect_t value = {0, 0, 0, 0};
    if (label.w > inner_width) inner_width = label.w;
    if (options[i].value) {
      value = TEXT_RECT(options[i].value, max_inner_width - label.w);
      if (label.w > col1_width) col1_width = label.w;
      if (value.w > col2_width) col2_width = value.w;
    }
    row_height[i] = (uint8_t)((label.h > value.h ? label.h : value.h));
    box_height += row_height[i];
  }
  if (col1_width > max_box_width) col1_width = max_box_width;
  if (col2_width > max_box_width) col2_width = max_box_width;
  if (col2_width >= 0 && inner_width < col1_width + col2_width + sep_width)
    inner_width = col1_width + col2_width + sep_width;
  if (inner_width > max_box_width) inner_width = max_box_width;
  col2_width = inner_width - col1_width - sep_width;
  box_width += inner_width + row_padding_x * 2;
  if (box_height > max_box_height) box_height = max_box_height;
  int box_x = (RG_SCREEN_W - box_width) / 2;
  int box_y = (RG_SCREEN_H - box_height) / 2;
  int x = box_x + box_padding;
  int y = box_y + box_padding;

  if (title) {
    int w = inner_width + row_padding_x * 2;
    rg_gui_draw_text(x, y, w, title, gui_style.box_header, gui_style.box_background,
                     RG_TEXT_ALIGN_CENTER);
    rg_gui_draw_rect(x, y + font_height, w, 6, 0, 0, gui_style.box_background);
    y += font_height + 6;
  }

  // Tìm đầu trang chứa selection (cuộn danh sách)
  int list_top_i = 0, list_end_i = 0;
  for (int yy = y, i = 0; i <= sel && i < options_count; i++) {
    yy += row_height[i];
    if (yy >= box_y + box_height) {
      if (sel < i) break;
      yy = y + row_height[i];
      list_top_i = i;
    }
  }

  for (int i = list_top_i; i < options_count; i++) {
    int xx = x + row_padding_x;
    int yy = y;
    int height = 8;
    if (y + row_height[i] >= box_y + box_height) break;
    list_end_i = i;
    if (options[i].flags == RG_DIALOG_FLAG_HIDDEN) continue;

    rg_color_t color =
      (options[i].flags == RG_DIALOG_FLAG_MESSAGE)  ? gui_style.item_message :
      (options[i].flags == RG_DIALOG_FLAG_NORMAL)   ? gui_style.item_standard :
                                                      gui_style.item_disabled;
    bool highlight = (options[i].flags & RG_DIALOG_FLAG_MODE_MASK) != RG_DIALOG_FLAG_SKIP &&
                     i == sel;
    rg_color_t fg = highlight ? gui_style.box_background : color;
    rg_color_t bg = highlight ? color : gui_style.box_background;

    if (options[i].value) {
      rg_gui_draw_text(xx, yy, col1_width, options[i].label, fg, bg, 0);
      rg_gui_draw_text(xx + col1_width, yy, sep_width, ": ", fg, bg, 0);
      height = rg_gui_draw_text(xx + col1_width + sep_width, yy, col2_width,
                                options[i].value, fg, bg, RG_TEXT_MULTILINE).h;
      if ((height / font_height) >= 2)
        rg_gui_draw_rect(xx, yy + font_height + 1, inner_width - col2_width,
                         height - font_height, 0, 0, bg);
    } else {
      height = rg_gui_draw_text(xx, yy, inner_width, options[i].label, fg, bg,
                                RG_TEXT_MULTILINE).h;
    }
    y += height;
  }

  if (y < box_y + box_height)
    rg_gui_draw_rect(box_x, y, box_width, (box_y + box_height) - y, 0, 0,
                     gui_style.box_background);

  // viền hộp: padding trong = màu nền, ngoài 1px = màu border
  rg_gui_draw_rect(box_x, box_y, box_width, box_height, box_padding,
                   gui_style.box_background, C_NONE);
  rg_gui_draw_rect(box_x - 1, box_y - 1, box_width + 2, box_height + 2, 1,
                   gui_style.box_border, C_NONE);

  // mũi tên cuộn (scrollbar color)
  if (list_top_i > 0) {
    int ax = box_x + box_width - 10, ay = box_y + box_padding + 2;
    rg_gui_draw_rect(ax + 0, ay - 0, 6, 2, 0, 0, gui_style.scrollbar);
    rg_gui_draw_rect(ax + 1, ay - 2, 4, 2, 0, 0, gui_style.scrollbar);
    rg_gui_draw_rect(ax + 2, ay - 4, 2, 2, 0, 0, gui_style.scrollbar);
  }
  if (list_end_i + 1 < options_count) {
    int ax = box_x + box_width - 10, ay = box_y + box_height - 6;
    rg_gui_draw_rect(ax + 0, ay - 4, 6, 2, 0, 0, gui_style.scrollbar);
    rg_gui_draw_rect(ax + 1, ay - 2, 4, 2, 0, 0, gui_style.scrollbar);
    rg_gui_draw_rect(ax + 2, ay - 0, 2, 2, 0, 0, gui_style.scrollbar);
  }
  return {box_x, box_y, box_width, box_height};
}
// ------------------------------------------------- draw_message + dialog loop
rg_rect_t rg_gui_draw_message(const char *format, ...) {
  char buffer[512];
  va_list va;
  va_start(va, format);
  vsnprintf(buffer, sizeof(buffer), format, va);
  va_end(va);
  rg_gui_option_t options[] = {
    {0, buffer, NULL, RG_DIALOG_FLAG_MESSAGE, NULL},
    RG_DIALOG_END,
  };
  return rg_gui_draw_dialog(NULL, options, 1, 0);
}

// Port rg_gui.c 864-1010: vòng lặp chặn, trả về options[sel].arg hoặc RG_DIALOG_CANCELLED.
// Phím: UP/DOWN chọn, LEFT/RIGHT đổi value (update_cb PREV/NEXT), OK = chọn,
// BACK/MENU = hủy. Option có update_cb nhận thêm ENTER (OK) và FOCUS events.
intptr_t rg_gui_dialog(const char *title, const rg_gui_option_t *options_const,
                       int selected_index) {
  rg_gui_option_t *options = (rg_gui_option_t *)options_const;
  int options_count = 0;
  if (options)
    while (options[options_count].label || options[options_count].value ||
           options[options_count].update_cb)
      options_count++;
  if (options_count == 0) return RG_DIALOG_CANCELLED;
  if (options_count > RG_GUI_DIALOG_MAX_ITEMS) options_count = RG_GUI_DIALOG_MAX_ITEMS;

  // Copy options khi có callback (Retro-Go: shadow options, value có thể bị mutate)
  static rg_gui_option_t shadow[RG_GUI_DIALOG_MAX_ITEMS + 1];
  static char text_buffer[RG_GUI_DIALOG_MAX_ITEMS * 40];
  bool has_cb = false;
  for (int i = 0; i < options_count; i++) if (options[i].update_cb) { has_cb = true; break; }
  if (has_cb) {
    memcpy(shadow, options, sizeof(rg_gui_option_t) * (options_count + 1));
    char *tb = text_buffer;
    for (int i = 0; i < options_count; i++) {
      if (!shadow[i].value || !shadow[i].update_cb) continue;
      strncpy(tb, shadow[i].value, 39); tb[39] = 0;
      shadow[i].value = tb;
      shadow[i].update_cb(&shadow[i], RG_DIALOG_INIT);
      tb += strlen(tb) + 1;
    }
    options = shadow;
  }

  if (selected_index < 0) selected_index += options_count;
  int sel = selected_index;
  if (sel < 0) sel = 0;
  if (sel >= options_count) sel = options_count - 1;
  int sel_old = -1;

  rg_gui_event_t event = RG_DIALOG_VOID;
  bool redraw = true;
  rg_input_wait_release(RG_KEY_ALL);

  while (event != RG_DIALOG_SELECT && event != RG_DIALOG_CANCEL) {
    uint32_t keys = rg_input_read();
    event = RG_DIALOG_VOID;
    if (keys) {
      bool active = options[sel].flags == RG_DIALOG_FLAG_NORMAL;
      rg_gui_callback_t cb = active ? options[sel].update_cb : NULL;
      if (keys & RG_KEY_UP)            { if (--sel < 0) sel = options_count - 1; }
      else if (keys & RG_KEY_DOWN)     { if (++sel >= options_count) sel = 0; }
      else if (keys & (RG_KEY_BACK | RG_KEY_MENU)) event = RG_DIALOG_CANCEL;
      else if ((keys & RG_KEY_LEFT) && cb)  { event = cb(&options[sel], RG_DIALOG_PREV); redraw = true; }
      else if ((keys & RG_KEY_RIGHT) && cb) { event = cb(&options[sel], RG_DIALOG_NEXT); redraw = true; }
      else if ((keys & RG_KEY_OK) && cb)    { event = cb(&options[sel], RG_DIALOG_ENTER); redraw = true; }
      else if ((keys & RG_KEY_OK) && active) event = RG_DIALOG_SELECT;
    }
    // bỏ qua các mục SKIP/SEPARATOR/HIDDEN khi di chuyển
    if (sel_old != sel) {
      for (int guard = 0; guard < options_count; guard++) {
        if (options[sel].flags == RG_DIALOG_FLAG_NORMAL ||
            options[sel].flags == RG_DIALOG_FLAG_DISABLED) break;
        sel += (keys & RG_KEY_UP) ? -1 : 1;
        if (sel < 0) sel = options_count - 1;
        if (sel >= options_count) sel = 0;
      }
      if (sel_old != -1 && options[sel_old].update_cb)
        options[sel_old].update_cb(&options[sel_old], RG_DIALOG_FOCUS_LOST);
      if (options[sel].update_cb)
        options[sel].update_cb(&options[sel], RG_DIALOG_FOCUS_GAINED);
      redraw = true;
      sel_old = sel;
    }
    if (event == RG_DIALOG_REDRAW || event == RG_DIALOG_UPDATE) {
      for (int i = 0; i < options_count; i++)
        if (options[i].update_cb) options[i].update_cb(&options[i], RG_DIALOG_UPDATE);
      if (event == RG_DIALOG_REDRAW) rg_display_force_redraw();
      redraw = true;
    }
    if (redraw) {
      rg_gui_draw_dialog(title, options, options_count, sel);
      rg_display_flush();
      redraw = false;
    }
    delay(20);
  }

  rg_input_wait_release(RG_KEY_ALL);
  rg_display_force_redraw();
  if (event == RG_DIALOG_SELECT) return options[sel].arg;
  return RG_DIALOG_CANCELLED;
}
