// launcher.cpp — Launcher phong cach Retro-Go cho E524546 (ESP32-S3 + ST7789).
//
// Thiet ke:
//   * 2 lop: dev_display_* (thap, dirty-rect + push qu LovyanGFX DMA) va gui_*
//     (cao: text proportional, image, list, dialog, statusbar).
//   * Immediate-mode: chi ve khi co su kien phim / ket qua art. Khong timer redraw.
//   * Theme .vqeaf (VQEAF Theme Studio @vqeaf 1.x) tu LittleFS
//     /launcher/themes/<ten>.vqeaf — palette RGB565, khong chay code tu file.
//     Fallback theme.json neu khong co .vqeaf.
//   * Art PNG decode bang PNGdec (PSRAM) trong FreeRTOS task rieng -> khong chan
//     vong lap chinh; ket qua ve o launcher_tick().
//   * SD_MMC 1-bit (chan trong pins.h) duoc mount tai day, that bai thi fallback
//     LittleFS (fs_sel cua store.cpp) hoac chi dung icon mac dinh.
//   * Tien Viet co dau: string dua qua _() — hien tai tra ve nguyen ban (tab en),
//     moi cho mo rong bang bang dich sau.
#include "launcher.h"
#include "launcher_config.h"
#include "pins.h"
#include "LGFX_ESP32S3_ST7789.h"
#include "browser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(ARDUINO) && __has_include(<PNGdec.h>)
#include <PNGdec.h>
#define LC_HAS_PNGDEC 1
#else
#define LC_HAS_PNGDEC 0
#endif

extern LGFX_ESP32S3_ST7789 disp;          // src/main.cpp

// Redirect low-level drawing (cli_shot dumps frame_buf, not the panel).
// nullptr = draw straight to panel (normal path).
static void *s_target = nullptr;
void launcher_set_target(void *g) { s_target = g; }
#if defined(ARDUINO)
#define LCD() (s_target ? *(lgfx::LovyanGFX *)s_target : (lgfx::LovyanGFX &)disp)
#else
#define LCD() disp
#endif

// wifi_up va clock_cache la static trong main.cpp (khong export duoc) nen duoc
// truyen vao launcher qua 2 setter. Mac dinh: offline + "--:--".
static bool s_wifi_up = false;
static const char *s_clock = "--:--";
void launcher_set_net(bool up) { s_wifi_up = up; }
void launcher_set_clock(const char *hhmm) { s_clock = hhmm; }
#define wifi_up s_wifi_up
#define clock_cache s_clock

// ------------- dich vu FS dung chung (store.cpp) -------------
extern bool store_fs_read(const char *path, char *buf, size_t cap, size_t *len);
extern bool store_fs_write(const char *path, const char *data, size_t n);
extern bool store_fs_exists(const char *path);
extern bool store_fs_mount_sd();
extern bool store_theme_read(const char *path, char *buf, size_t cap, size_t *len); // LittleFS only
extern int  store_theme_list(char names[][24], int maxn);

// translate hook: giai doan dau chi tab en. Thay bang bang dich de co Viet dau.
static const char *tr(const char *s) { return s; }
#define _(s) tr(s)

// ============================================================ dev_display (lop thap)
// Dirty rect don gian: luu vung bi anh huong, flush() day tung vung bang
// pushImage (LovyanGFX da DMA). Full screen chi la truong hop dac biet.
static struct { int x, y, w, h; bool dirty; } dev_dirty;
static bool dev_active = false;            // launcher dang giu man hinh

void dev_display_init() {
  // panel da duoc init boi setup() cua src/main.cpp (g_panel->init()).
  dev_dirty.dirty = false;
}

void dev_display_dirty(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > LC_W) w = LC_W - x;
  if (y + h > LC_H) h = LC_H - y;
  if (w <= 0 || h <= 0) return;
  if (!dev_dirty.dirty) { dev_dirty = { x, y, w, h, true }; return; }
  int x1 = dev_dirty.x + dev_dirty.w, y1 = dev_dirty.y + dev_dirty.h;
  int nx0 = x < dev_dirty.x ? x : dev_dirty.x;
  int ny0 = y < dev_dirty.y ? y : dev_dirty.y;
  int nx1 = (x + w) > x1 ? (x + w) : x1;
  int ny1 = (y + h) > y1 ? (y + h) : y1;
  dev_dirty = { nx0, ny0, nx1 - nx0, ny1 - ny0, true };
}

void dev_display_write_rect(int x, int y, int w, int h, const uint16_t *pix) {
  if (!pix || w <= 0 || h <= 0) return;
  LCD().startWrite();
  LCD().pushImage(x, y, w, h, pix);
  LCD().endWrite();
  dev_display_dirty(x, y, w, h);
}

void dev_display_fill_rect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  LCD().startWrite();
  LCD().fillRect(x, y, w, h, color);
  LCD().endWrite();
  dev_display_dirty(x, y, w, h);
}

void dev_display_flush() {
  if (!dev_dirty.dirty) return;
  // pushImage trong write/fill da day truc tiep; flush hien tai chi clear dirty.
  // (Giu ham de mo rong sang double-buffer sau nay ma khong doi API.)
  dev_dirty.dirty = false;
}

// ============================================================ gui (lop cao)
// Font proportional RGB bitmap (Retro-Go rg_font_t) + tieng Viet co dau.
// size=1 -> Tahoma 12 (cao 14); size=2 -> Tahoma 16 (cao 20). Sinh bang
// tools/make_font.py, khong dung font LovyanGFX cho UI launcher.
#include "fonts/lc_font_vn12.h"
#include "fonts/lc_font_vn16.h"

static const LcFont *lc_pick_font(int size) {
  return (size >= 2) ? &lc_font_vn16 : &lc_font_vn12;
}

int gui_font_height(int size) {
  return (int)lc_pick_font(size)->height;
}

// UTF-8 -> codepoint; tro toi byte tiep theo sau codepoint.
static int lc_utf8_decode(const char **ptr) {
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

// Tim glyph; tra ve tro stream tai glyph do (hoac NULL). *adv = xDelta (advance).
static const uint8_t *lc_find_glyph(const LcFont *font, int code, int *adv) {
  const uint8_t *ptr = font->data;
  for (;;) {
    uint16_t gc = (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
    if (gc == 0) return NULL;
    if (gc == (uint16_t)code) {
      if (adv) *adv = ptr[6];
      return ptr;
    }
    int w = ptr[3], h = ptr[4];
    // Empty glyph (w==0 or h==0) has no bitmap bytes. C: ((0)-1)/8+1 == 1 would desync.
    int blen = (w == 0 || h == 0) ? 0 : (((w * h) - 1) / 8 + 1);
    ptr += 7 + blen;
  }
}

// Decode 1 glyph -> rows[y] (bit 0 = pixel trai nhat). Tra ve advance width.
static int lc_get_glyph(uint32_t *rows, const LcFont *font, int code) {
  int adv = 0;
  const uint8_t *g = lc_find_glyph(font, code, &adv);
  int box_w = font->width ? font->width : 8;
  if (!g) {
    // khong co glyph -> hop rong mo (khong ve thanh dac nhu truoc)
    for (int y = 0; y < (int)font->height; y++) rows[y] = 0;
    int bw = (box_w >= 3 && box_w <= 30) ? box_w : 6;
    rows[2] = rows[3] = rows[4] = rows[5] = rows[6] = rows[7] =
        (uint32_t)((1u << bw) - 2u);
    rows[2] |= 1u;
    rows[7] |= 1u;
    for (int y = 2; y <= 7; y++) rows[y] |= (1u << (bw - 1));
    return bw;
  }
  int ofs_y = g[2];
  int w = g[3], h = g[4];
  int ofs_x = g[5];
  const uint8_t *data = g + 7;
  for (int y = 0; y < (int)font->height; y++) rows[y] = 0;
  int ch = 0, mask = 0x80;
  for (int y = 0; y < h; y++) {
    uint32_t row = 0;
    for (int x = 0; x < w; x++) {
      if (((x + y * w) % 8) == 0) { mask = 0x80; ch = *data++; }
      if (ch & mask) {
        int px = ofs_x + x;
        if (px >= 0 && px < 32) row |= (1u << px);
      }
      mask >>= 1;
    }
    int oy = ofs_y + y;
    if (oy >= 0 && oy < (int)font->height) rows[oy] |= row;
  }
  return adv ? adv : (ofs_x + w);
}

void gui_clear_rect(int x, int y, int w, int h, uint16_t color) {
  dev_display_fill_rect(x, y, w, h, color);
}

int gui_text_width(const char *s, int size) {
  if (!s) return 0;
  const LcFont *font = lc_pick_font(size);
  int w = 0;
  const char *p = s;
  while (*p) {
    int c = lc_utf8_decode(&p);
    if (c == '\n' || c == '\r') break;
    int adv = 0;
    lc_find_glyph(font, c, &adv);
    w += adv;
  }
  return w;
}

void gui_draw_text(int x, int y, uint16_t fg, uint16_t bg, const char *s, int size, bool transparent_bg) {
  if (!s) return;
  const LcFont *font = lc_pick_font(size);
  const int fh = (int)font->height;
  uint32_t rows[32];
  if (fh > 32) return;
  LCD().startWrite();
  int cx = x;
  const char *p = s;
  while (*p) {
    int c = lc_utf8_decode(&p);
    if (c == '\n') break;
    if (c == '\r') continue;
    int adv = lc_get_glyph(rows, font, c);
    int gw = 0;
    const uint8_t *g = lc_find_glyph(font, c, nullptr);
    if (g) gw = g[3];
    // background box per glyph (neu khong transparent)
    if (!transparent_bg && gw > 0) {
      int ofs_x = g ? g[5] : 0;
      int ofs_y = g ? g[2] : 0;
      int gh = g ? g[4] : fh;
      if (ofs_x + gw > 0)
        LCD().fillRect(cx + (ofs_x > 0 ? ofs_x : 0), y + ofs_y,
                      (ofs_x > 0 ? gw : gw + ofs_x), gh, bg);
    }
    for (int gy = 0; gy < fh; gy++) {
      uint32_t row = rows[gy];
      if (!row) continue;
      int run_x = -1;
      for (int gx = 0; gx <= 32; gx++) {
        bool on = (gx < 32) && (row & (1u << gx));
        if (on && run_x < 0) run_x = gx;
        else if (!on && run_x >= 0) {
          LCD().fillRect(cx + run_x, y + gy, gx - run_x, 1, fg);
          run_x = -1;
        }
      }
    }
    cx += adv;
  }
  LCD().endWrite();
  int w = cx - x;
  if (w < 0) w = 0;
  dev_display_dirty(x, y, w + 1, fh);
}

// ve anh RGB565 co mau magenta lam trong suot (nhu Retro-Go)
void gui_draw_image(int x, int y, int w, int h, const uint16_t *pix) {
  if (!pix || w <= 0 || h <= 0) return;
  LCD().startWrite();
  for (int j = 0; j < h; j++) {
    int run = -1;
    for (int i = 0; i <= w; i++) {
      bool key = (i < w) && (pix[j * w + i] == LC_ART_KEY_MAGENTA);
      if (!key && run >= 0) {
        if (i - run > 0) LCD().pushImage(x + run, y + j, i - run, 1, (uint16_t *)(pix + j * w + run));
        run = -1;
      } else if (key && run < 0) {
        run = i;
      }
    }
  }
  LCD().endWrite();
  dev_display_dirty(x, y, w, h);
}

// ---------------- danh sach ----------------
void gui_draw_list(int x, int y, int w, int h, const LcItem *items, int n,
                   int selected, int top, const LcTheme::LcTab &t) {
  if (!items) n = 0;
  int rows = h / LC_ROW_H;
  if (rows < 1) rows = 1;
  gui_clear_rect(x, y, w, rows * LC_ROW_H, t.background);
  for (int r = 0; r < rows; r++) {
    int idx = top + r;
    if (idx >= n) break;
    int ry = y + r * LC_ROW_H;
    bool sel = (idx == selected);
    if (sel) dev_display_fill_rect(x, ry, w, LC_ROW_H - 1, t.list_selected_bg);
    uint16_t fg = sel ? t.list_selected_fg : t.list_standard_fg;
    if (items[idx].kind == 1) fg = LC_DIM_DEFAULT;                 // muc thong tin: xam
    else if (!sel && items[idx].favorite) fg = 0xFFE0;             // yeu thich: vang nhat
    char star[2] = { 0, 0 };
    if (items[idx].favorite) { star[0] = '*'; }
    char line[LC_TITLE_MAX + 4];
    snprintf(line, sizeof line, "%s%s", star, items[idx].title);
    // cat be rong danh cho proportional font
    int tw = gui_text_width(line, 1);
    while (tw > w - 8 && line[0]) {
      int L = (int)strlen(line);
      L--; while (L > 0 && ((unsigned char)line[L] & 0xC0) == 0x80) L--;
      line[L] = 0;
      tw = gui_text_width(line, 1);
    }
    int fh1 = gui_font_height(1);
    gui_draw_text(x + 4, ry + (LC_ROW_H - fh1) / 2, fg,
                  sel ? t.list_selected_bg : t.background, line, 1, true);
    if (!sel) dev_display_fill_rect(x, ry + LC_ROW_H - 1, w, 1, 0x2104); // ke chan dong toi
  }
  // thanh cuon
  if (n > rows) {
    int sx = x + w - 3;
    dev_display_fill_rect(sx, y, 2, rows * LC_ROW_H, 0x1082);
    int gh = (rows * LC_ROW_H) * rows / n; if (gh < 6) gh = 6;
    int gy = y + (top * (rows * LC_ROW_H)) / n;
    if (gy + gh > y + rows * LC_ROW_H) gh = y + rows * LC_ROW_H - gy;
    dev_display_fill_rect(sx, gy, 2, gh, 0x7BEF);
  }
  dev_display_flush();
}

// ---------------- hop thoai modal ----------------
static uint16_t dialog_border_color()  { return theme_current().dlg_border; }
static uint16_t dialog_shadow_color()  { return theme_current().dlg_shadow; }

bool gui_dialog(const char *title, const char *const *lines, int nlines,
                const char *const *options, int noptions, int *sel) {
  if (!options || noptions < 1 || !sel) return false;
  if (*sel < 0) *sel = 0;
  if (*sel >= noptions) *sel = noptions - 1;

  const int dw = 200, pad = 8, oh = 16;
  int fh1 = gui_font_height(1);
  int dh = 24 + (nlines > 0 ? nlines * (fh1 + 2) : 0) + noptions * oh + pad;
  if (dh > LC_H - 40) dh = LC_H - 40;
  const int dx = (LC_W - dw) / 2, dy = (LC_H - dh) / 2 - 10;
  const LcTheme &th = theme_current();

  // bong do (neu mau shadow khac mau nen)
  if (th.dlg_shadow != th.dlg_background)
    dev_display_fill_rect(dx + 3, dy + 3, dw, dh, th.dlg_shadow);
  dev_display_fill_rect(dx, dy, dw, dh, th.dlg_background);
  // vien
  for (int i = 0; i < 2; i++) {
    dev_display_fill_rect(dx + i, dy + i, dw - 2 * i, 1, th.dlg_border);
    dev_display_fill_rect(dx + i, dy + dh - 1 - i, dw - 2 * i, 1, th.dlg_border);
    dev_display_fill_rect(dx + i, dy + i, 1, dh - 2 * i, th.dlg_border);
    dev_display_fill_rect(dx + dw - 1 - i, dy + i, 1, dh - 2 * i, th.dlg_border);
  }
  // header
  dev_display_fill_rect(dx + 2, dy + 2, dw - 4, fh1 + 4, th.dlg_header);
  gui_draw_text(dx + 6, dy + 4, th.dlg_foreground, th.dlg_header, title, 1, true);

  int ly = dy + fh1 + 8;
  for (int i = 0; i < nlines && ly + fh1 < dy + dh - noptions * oh - 4; i++, ly += fh1 + 2)
    gui_draw_text(dx + pad, ly, th.dlg_item_message, th.dlg_background, lines[i], 1, true);

  // options (muc disabled = xam)
  int oy = dy + dh - noptions * oh - 2;
  for (int i = 0; i < noptions; i++, oy += oh) {
    bool disabled = (options[i][0] == '~');          // quy uoc: "~Ten" = disabled
    const char *lab = disabled ? options[i] + 1 : options[i];
    if (i == *sel) dev_display_fill_rect(dx + 2, oy, dw - 4, oh - 1, th.dlg_item_standard);
    gui_draw_text(dx + pad, oy + (oh - fh1) / 2, disabled ? LC_DIM_DEFAULT : th.dlg_foreground,
                  (i == *sel) ? th.dlg_item_standard : th.dlg_background, lab, 1, true);
  }
  dev_display_flush();
  return true;
}

// ---------------- status bar (dung lai dong ho NTP cua browser) ----------------
// (gui_statusbar dung wifi_up/clock_cache qua macro o tren)
void gui_statusbar(uint16_t fg, uint16_t bg) {
  char buf[48];
  int n = snprintf(buf, sizeof buf, "%s", wifi_up ? "WiFi" : "--");
  n += snprintf(buf + n, sizeof buf - n, "  %s", clock_cache);
  snprintf(buf + n, sizeof buf - n, "  PWR");
  int w = gui_text_width(buf, 1);
  gui_draw_text(LC_W - w - 4, (LC_STATUS_H - gui_font_height(1)) / 2, fg, bg, buf, 1, true);
  // cot tinh hieu trai ( S60 status pane )
  const int sy = (LC_STATUS_H - 8) / 2;
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    dev_display_fill_rect(4 + i * 4, sy + 8 - bh, 3, bh, wifi_up ? UC_STATUS_FG : UC_ITEM_DIM);
  }
}

// ============================================================ theme JSON
// Parser JSON nho: chi doc "key": value theo section. Chap nhan:
//   - so nguyen RGB565:            "background": 16
//   - chuoi hex:                   "background": "0x0010"
//   - "transparent":               coi nhu khong ghi de (giu mac dinh)
#define LC_JSON_MAX 4096
static LcTheme s_theme;
static char s_theme_name[32] = LC_THEME_DEFAULT;
static char s_theme_display[40] = LC_THEME_DEFAULT;  // ten tu <theme name="...">
static char s_theme_buf[LC_JSON_MAX];

static uint16_t parse_color_value(const char *v, uint16_t fallback) {
  while (*v == ' ' || *v == '\t') v++;
  if (!*v || !strncmp(v, "\"transparent\"", 13)) return fallback;
  if (*v == '"') {
    unsigned long x = strtoul(v + 1, nullptr, 16);
    return (uint16_t)x;
  }
  return (uint16_t)strtoul(v, nullptr, 10);
}

static void json_color(const char *json, const char *key, uint16_t &out, uint16_t fallback) {
  char pat[32];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char *p = strstr(json, pat);
  if (!p) return;
  p += strlen(pat);
  p = strchr(p, ':');
  if (!p) return;
  p++;
  out = parse_color_value(p, fallback);
}

// ---------------- VQEAF (@vqeaf 1.x) — giong SymbianS3 ThemeFileService ----------
// Chi doc palette mau RGB hex #RRGGBB; bo metadata/system/layer khac.
static bool vqeaf_hex(const char *s, uint16_t &rgb) {
  if (!s || s[0] != '#' || strlen(s) != 7) return false;
  uint32_t code = 0;
  for (int i = 1; i <= 6; ++i) {
    unsigned char c = (unsigned char)s[i];
    int d = (c >= '0' && c <= '9') ? c - '0' :
            (c >= 'A' && c <= 'F') ? c - 'A' + 10 :
            (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
    if (d < 0) return false;
    code = (code << 4) | (uint32_t)d;
  }
  uint16_t r = (uint16_t)((code >> 16) & 255);
  uint16_t g = (uint16_t)((code >> 8) & 255);
  uint16_t b = (uint16_t)(code & 255);
  rgb = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
  return true;
}

static char *vqeaf_trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r') s++;
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
  return s;
}

static bool vqeaf_quoted(const char *s, char *out, size_t cap) {
  if (!s || !out || cap < 2) return false;
  const char *a = strchr(s, '"');
  if (!a) return false;
  const char *b = strchr(a + 1, '"');
  if (!b || b == a + 1 || (size_t)(b - (a + 1)) >= cap) return false;
  size_t n = (size_t)(b - a - 1);
  memcpy(out, a + 1, n);
  out[n] = 0;
  return true;
}

// Parse .vqeaf -> LcTheme (ap dung palette vao 4 tab + dialog). false = file hong.
static bool theme_parse_vqeaf(char *text, LcTheme &out, char *disp, size_t disp_cap) {
  if (!text) return false;
  theme_defaults(out);
  bool has_magic = false, in_pal = false, pending = false, pal = false;
  bool ok_screen = false, ok_text = false, ok_accent = false, closed = false;
  int lines = 0;
  char *save = nullptr;
  for (char *ln = strtok_r(text, "\r\n", &save); ln && lines < 400;
       ln = strtok_r(nullptr, "\r\n", &save), lines++) {
    char *s = vqeaf_trim(ln);
    if (!has_magic && (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
    if (!*s || s[0] == '#' || (s[0] == '/' && s[1] == '/')) continue;
    if (!has_magic) {
      if (strncmp(s, "@vqeaf 1.", 9) != 0) return false;
      has_magic = true;
      continue;
    }
    if (strncmp(s, "<theme ", 7) == 0) {
      const char *p = strstr(s, "name=");
      char label[40];
      if (p && vqeaf_quoted(p + 5, label, sizeof label) && disp && disp_cap)
        snprintf(disp, disp_cap, "%s", label);
      continue;
    }
    if (!in_pal) {
      if (pal && strstr(s, "</theme>")) { closed = true; break; }
      if (pending && *s == '{') {
        in_pal = true; pal = true; pending = false;
      } else if (!pal && strncmp(s, "palette", 7) == 0 &&
                 (s[7] == 0 || s[7] == ' ' || s[7] == '\t' || s[7] == '{')) {
        if (strchr(s, '{')) { in_pal = true; pal = true; }
        else pending = true;
      } else if (pending) {
        return false;
      }
      continue;
    }
    if (strchr(s, '}')) { in_pal = false; continue; }
    char *colon = strchr(s, ':');
    if (!colon) continue;
    *colon++ = 0;
    const char *key = vqeaf_trim(s);
    const char *known[] = {
      "screen", "key", "panel", "keyPressed", "selected", "keyBorder",
      "border", "keyText", "subText", "shellTop", "titlebar",
      "chromeText", "shellBottom", "shellBorder", "accent", "glow"
    };
    bool rec = false;
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++)
      if (!strcmp(key, known[i])) { rec = true; break; }
    if (!rec) continue;
    char val[24];
    if (!vqeaf_quoted(colon, val, sizeof val)) return false;
    uint16_t c = 0;
    if (!vqeaf_hex(val, c)) return false;
    // --- map palette -> LcTheme (giong SymbianS3 ThemeFileService) ---
    if (!strcmp(key, "screen")) {
      out.tab[0].background = out.tab[1].background =
      out.tab[2].background = out.tab[3].background = c;
      out.tab[0].list_standard_bg = out.tab[1].list_standard_bg =
      out.tab[2].list_standard_bg = out.tab[3].list_standard_bg = c;
      out.dlg_background = c;
      ok_screen = true;
    } else if (!strcmp(key, "key") || !strcmp(key, "panel")) {
      out.dlg_item_standard = c;
      out.tab[3].background = c;  // Settings: panel
    } else if (!strcmp(key, "keyPressed") || !strcmp(key, "selected")) {
      out.tab[0].list_selected_bg = out.tab[1].list_selected_bg =
      out.tab[2].list_selected_bg = out.tab[3].list_selected_bg = c;
      out.dlg_item_standard = c;
    } else if (!strcmp(key, "keyBorder") || !strcmp(key, "border")) {
      out.dlg_border = c;
    } else if (!strcmp(key, "keyText")) {
      out.tab[0].foreground = out.tab[1].foreground =
      out.tab[2].foreground = out.tab[3].foreground = c;
      out.tab[0].list_selected_fg = out.tab[1].list_selected_fg =
      out.tab[2].list_selected_fg = out.tab[3].list_selected_fg = c;
      out.dlg_foreground = c;
      out.dlg_item_message = c;
      ok_text = true;
    } else if (!strcmp(key, "subText")) {
      out.tab[0].list_standard_fg = out.tab[1].list_standard_fg =
      out.tab[2].list_standard_fg = out.tab[3].list_standard_fg = c;
      out.dlg_item_disabled = c;
      out.dlg_item_message = c;
    } else if (!strcmp(key, "shellTop") || !strcmp(key, "titlebar")) {
      out.dlg_header = c;
    } else if (!strcmp(key, "chromeText")) {
      // foreground header — su dung nhu list_selected_fg neu can
    } else if (!strcmp(key, "shellBottom")) {
      out.dlg_background = c;
    } else if (!strcmp(key, "accent")) {
      out.dlg_header = c;
      out.dlg_scrollbar = c;
      ok_accent = true;
    } else if (!strcmp(key, "glow")) {
      out.dlg_shadow = c;
    }
  }
  if (!has_magic || !pal || !ok_screen || !ok_text || !ok_accent || !closed || in_pal)
    return false;
  return true;
}

void theme_defaults(LcTheme &t) {
  snprintf(t.name, sizeof t.name, "%s", "(default)");
  t.dlg_background   = LC_DIALOG_BG_DEFAULT;
  t.dlg_foreground   = LC_FG_DEFAULT;
  t.dlg_border       = LC_BORDER_DEFAULT;
  t.dlg_header       = 0x39E7;          // vang dam
  t.dlg_scrollbar    = 0x7BEF;
  t.dlg_shadow       = 0x0000;
  t.dlg_item_standard= LC_SEL_BG_DEFAULT;
  t.dlg_item_disabled= LC_DIM_DEFAULT;
  t.dlg_item_message = LC_DIM_DEFAULT;
  for (int i = 0; i < 4; i++) {
    t.tab[i].background        = LC_BG_DEFAULT;
    t.tab[i].foreground        = LC_FG_DEFAULT;
    t.tab[i].list_standard_bg  = LC_BG_DEFAULT;
    t.tab[i].list_standard_fg  = 0xC618; // xam sang (muc thuong nhu Retro-Go)
    t.tab[i].list_selected_bg  = LC_SEL_BG_DEFAULT;
    t.tab[i].list_selected_fg  = LC_SEL_FG_DEFAULT;
  }
  // launcher_2 (Games): nen toi hon
  t.tab[1].background = 0x0000;
  // launcher_3 (Favorites): nen den
  t.tab[2].background = 0x0000;
  // launcher_4 (Settings): nen xam toi
  t.tab[3].background = 0x1042;
}

const LcTheme &theme_current() { return s_theme; }
const char *cfg_theme_name()   { return s_theme_name; }
const char *theme_display_name() { return s_theme_display; }

bool theme_load(const char *name) {
  if (!name || !name[0]) name = LC_THEME_DEFAULT;
  char path[96];
  LcTheme t;
  theme_defaults(t);
  size_t len = 0;
  snprintf(s_theme_display, sizeof s_theme_display, "%s", name);

  // 1) .vqeaf tren LittleFS (uu tien)
  snprintf(path, sizeof path, "%s/%s" LC_THEME_EXT, LC_DIR_THEMES, name);
  if (store_theme_read(path, s_theme_buf, sizeof s_theme_buf - 1, &len) && len > 0) {
    s_theme_buf[len] = 0;
    char disp[40];
    snprintf(disp, sizeof disp, "%s", name);
    if (theme_parse_vqeaf(s_theme_buf, t, disp, sizeof disp)) {
      s_theme = t;
      snprintf(s_theme.name, sizeof s_theme.name, "%s", name);
      snprintf(s_theme_name, sizeof s_theme_name, "%s", name);
      snprintf(s_theme_display, sizeof s_theme_display, "%s", disp);
      Serial.printf("[lc] theme vqeaf: %s (%s)\n", name, disp);
      return true;
    }
    Serial.printf("[lc] theme %s: vqeaf hong -> fallback\n", path);
  }

  // 2) theme.json cu (van ho tro)
  snprintf(path, sizeof path, "%s/%s/theme.json", LC_DIR_THEMES, name);
  if (!store_fs_read(path, s_theme_buf, sizeof s_theme_buf - 1, &len)) {
    Serial.printf("[lc] theme %s: khong doc duoc -> mac dinh\n", path);
    s_theme = t;
    snprintf(s_theme.name, sizeof s_theme.name, "%s", name);
    snprintf(s_theme_name, sizeof s_theme_name, "%s", name);
    return false;
  }
  s_theme_buf[len] = 0;
  json_color(s_theme_buf, "background",     t.dlg_background,    t.dlg_background);
  json_color(s_theme_buf, "foreground",     t.dlg_foreground,    t.dlg_foreground);
  json_color(s_theme_buf, "border",         t.dlg_border,        t.dlg_border);
  json_color(s_theme_buf, "header",         t.dlg_header,        t.dlg_header);
  json_color(s_theme_buf, "scrollbar",      t.dlg_scrollbar,     t.dlg_scrollbar);
  json_color(s_theme_buf, "shadow",         t.dlg_shadow,        t.dlg_shadow);
  json_color(s_theme_buf, "item_standard",  t.dlg_item_standard, t.dlg_item_standard);
  json_color(s_theme_buf, "item_disabled",  t.dlg_item_disabled, t.dlg_item_disabled);
  json_color(s_theme_buf, "item_message",   t.dlg_item_message,  t.dlg_item_message);
  for (int i = 0; i < 4; i++) {
    char sec[16]; snprintf(sec, sizeof sec, "launcher_%d", i + 1);
    const char *s = strstr(s_theme_buf, sec);
    const char *end = s ? strchr(s, '}') : nullptr;
    if (s && end) {
      size_t span = (size_t)(end - s);
      static char secbuf[512];
      size_t cp = span < sizeof secbuf - 1 ? span : sizeof secbuf - 1;
      memcpy(secbuf, s, cp); secbuf[cp] = 0;
      json_color(secbuf, "background",       t.tab[i].background,       t.tab[i].background);
      json_color(secbuf, "foreground",       t.tab[i].foreground,       t.tab[i].foreground);
      json_color(secbuf, "list_standard_bg", t.tab[i].list_standard_bg, t.tab[i].list_standard_bg);
      json_color(secbuf, "list_standard_fg", t.tab[i].list_standard_fg, t.tab[i].list_standard_fg);
      json_color(secbuf, "list_selected_bg", t.tab[i].list_selected_bg, t.tab[i].list_selected_bg);
      json_color(secbuf, "list_selected_fg", t.tab[i].list_selected_fg, t.tab[i].list_selected_fg);
    }
  }
  s_theme = t;
  snprintf(s_theme.name, sizeof s_theme.name, "%s", name);
  snprintf(s_theme_name, sizeof s_theme_name, "%s", name);
  snprintf(s_theme_display, sizeof s_theme_display, "%s", name);
  Serial.printf("[lc] theme json: %s\n", name);
  return true;
}

bool theme_next(int dir) {
  // Quet /launcher/themes/*.vqeaf tren LittleFS (toi da 8).
  static char names[8][24];
  int n = store_theme_list(names, 8);
  if (n <= 0) return false;
  int cur = 0;
  for (int i = 0; i < n; i++)
    if (!strcmp(names[i], s_theme_name)) { cur = i; break; }
  int nxt = (cur + (dir > 0 ? 1 : -1) + n) % n;
  bool ok = theme_load(names[nxt]);
  if (ok) { cfg_save(); launcher_redraw(); }
  return ok;
}

// ============================================================ config launcher.ini
static int  s_brightness = 200;
static char s_cfg_theme[32] = LC_THEME_DEFAULT;

int cfg_brightness() { return s_brightness; }

bool cfg_load() {
  char buf[LC_CFG_LINE_MAX * 4];
  size_t len = 0;
  if (!store_fs_read(LC_CFG_INI, buf, sizeof buf - 1, &len)) return false;
  buf[len] = 0;
  char *p = strtok(buf, "\r\n");
  while (p) {
    char *eq = strchr(p, '=');
    if (eq) {
      *eq = 0;
      if (!strcmp(p, "theme")) snprintf(s_cfg_theme, sizeof s_cfg_theme, "%s", eq + 1);
      else if (!strcmp(p, "brightness")) s_brightness = atoi(eq + 1);
    }
    p = strtok(nullptr, "\r\n");
  }
  snprintf(s_theme_name, sizeof s_theme_name, "%s", s_cfg_theme);
  return true;
}

bool cfg_save() {
  char buf[LC_CFG_LINE_MAX];
  int n = snprintf(buf, sizeof buf,
                   "; Qeafbrowser launcher config\ntheme=%s\nbrightness=%d\n",
                   s_theme_name, s_brightness);
  bool ok = store_fs_write(LC_CFG_INI, buf, (size_t)n);
  Serial.printf("[lc] cfg_save %s\n", ok ? "ok" : "FAIL(no FS)");
  return ok;
}

// ============================================================ art PNG (task rieng)
#if LC_HAS_PNGDEC
static PNG *s_png = nullptr;
static uint16_t *s_art_pix = nullptr;      // PSRAM
static int  s_art_w = 0, s_art_h = 0;
static volatile bool s_art_req = false;
static volatile bool s_art_done = false;
static char s_art_path[96];
static uint8_t *s_art_file = nullptr;      // PSRAM

static void *lc_psram_malloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  if (!p) p = malloc(n);
  return p;
}

struct ArtCtx { int w, h, out_w, out_h, dx, dy; bool ok; };
static ArtCtx s_actx;

static int lc_png_draw(PNGDRAW *pDraw) {
  static uint16_t line[LC_ART_MAX_W];
  if (!s_png || !pDraw || s_actx.w < 1) return 1;
  s_png->getLineAsRGB565(pDraw, line, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
  int dy = s_actx.dy + (pDraw->y * s_actx.out_h) / s_actx.h;
  if (dy < 0 || dy >= s_art_h) return 1;
  for (int dx = 0; dx < s_actx.out_w; dx++) {
    int sx = (dx * s_actx.w) / s_actx.out_w;
    int tx = s_actx.dx + dx;
    if (tx >= 0 && tx < s_art_w) s_art_pix[dy * s_art_w + tx] = line[sx];
  }
  s_actx.ok = true;
  return 1;
}

static bool art_decode_ram(uint8_t *buf, size_t len) {
  if (!s_art_pix) return false;
  PNG *png = s_png;
  if (!png) return false;
  if (len > 0x7fffffffUL) return false;
  if (png->openRAM(buf, (int)len, lc_png_draw) != PNG_SUCCESS) return false;
  memset(s_art_pix, 0, (size_t)s_art_w * s_art_h * sizeof(uint16_t));
  // scale: hien thi theo dung kich thuoc neu vua, nguoc lai bo qua (dung icon)
  int sw = png->getWidth(), sh = png->getHeight();
  png->close();
  if (sw <= 0 || sh <= 0 || sw > LC_ART_MAX_W || sh > LC_ART_MAX_H) return false;
  s_actx = { sw, sh, sw, sh, 0, 0, false };
  if (png->openRAM(buf, (int)len, lc_png_draw) != PNG_SUCCESS) return false;
  png->decode(nullptr, 0);
  png->close();
  return s_actx.ok;
}

static void art_task(void *arg) {
  (void)arg;
  for (;;) {
    while (!s_art_req) vTaskDelay(pdMS_TO_TICKS(20));
    s_art_req = false;
    s_art_done = false;
    s_art_w = s_art_h = 0;
    if (!s_art_path[0]) { s_art_done = true; continue; }   // ve icon mac dinh
    char full[128];
    snprintf(full, sizeof full, "%s/%s", LC_DIR_ART, s_art_path);
    size_t len = 0;
    if (!store_fs_read(full, (char *)s_art_file, LC_ART_MAX_FILE, &len) || len == 0) {
      s_art_done = true; continue;                          // thieu file -> icon
    }
    if (art_decode_ram(s_art_file, len)) {
      // kich thuot art = s_art_w x s_art_h da decode (voi 240xN)
    } else {
      s_art_w = s_art_h = 0;
    }
    s_art_done = true;
  }
}

void art_task_start() {
  s_art_pix  = (uint16_t *)lc_psram_malloc(LC_ART_BYTES);
  s_art_file = (uint8_t  *)lc_psram_malloc(LC_ART_MAX_FILE);
  if (s_art_pix) memset(s_art_pix, 0, LC_ART_BYTES);
#if defined(ARDUINO)
  if (!s_png) {
    void *p = lc_psram_malloc(sizeof(PNG));
    if (p) s_png = new (p) PNG;
  }
  xTaskCreatePinnedToCore(art_task, "lc_art", LC_ART_TASK_STACK, nullptr,
                          tskIDLE_PRIORITY + 1, nullptr, 0);
#else
  // sim: khong co FreeRTOS — decode dong bo trong art_request (xem art_ready)
  (void)art_task;
#endif
}

void art_request(const char *rel_path) {
  snprintf(s_art_path, sizeof s_art_path, "%s", rel_path ? rel_path : "");
#if !defined(ARDUINO)
  if (s_art_path[0]) {
    char full[128];
    snprintf(full, sizeof full, "%s/%s", LC_DIR_ART, s_art_path);
    size_t len = 0;
    s_art_w = s_art_h = 0;
    if (s_art_pix && s_art_file &&
        store_fs_read(full, (char *)s_art_file, LC_ART_MAX_FILE, &len) && len > 0 &&
        art_decode_ram(s_art_file, len)) {
      // ok
    } else { s_art_w = s_art_h = 0; }
    s_art_done = true;
  } else { s_art_w = s_art_h = 0; s_art_done = true; }
#else
  s_art_req = true;
  s_art_done = false;
#endif
}

bool art_ready(uint16_t **pix, int *w, int *h) {
  if (!s_art_done) return false;
  if (pix) *pix = (s_art_w > 0) ? s_art_pix : nullptr;
  if (w) *w = s_art_w;
  if (h) *h = s_art_h;
  return true;
}
#else  // !LC_HAS_PNGDEC (build Windows khong co PNGdec)
static uint16_t *s_art_pix = nullptr;
void art_task_start() { (void)s_art_pix; }
void art_request(const char *rel_path) { (void)rel_path; }
bool art_ready(uint16_t **pix, int *w, int *h) { if (pix) *pix = nullptr; if (w) *w = 0; if (h) *h = 0; return true; }
#endif

// ============================================================ du lieu tab
// Nguon: tinh trong code (MVP). Sau nay co the doc /launcher/apps.txt tu SD.
struct LcItem s_items[LC_TAB_N][LC_ITEMS_MAX];
static int s_item_n[LC_TAB_N];

static void add_item(int tab, const char *title, const char *art, uint8_t kind, bool fav) {
  if (tab < 0 || tab >= LC_TAB_N || s_item_n[tab] >= LC_ITEMS_MAX) return;
  LcItem &it = s_items[tab][s_item_n[tab]++];
  snprintf(it.title, sizeof it.title, "%s", title);
  snprintf(it.art, sizeof it.art, "%s", art ? art : "");
  it.kind = kind;
  it.favorite = fav;
}

// loi yeu thich (luu vao /launcher/favorites.txt: moi dong "art\ttitle")
static char s_fav[LC_FAV_MAX][96]; static int s_fav_n;
static void rebuild_tabs();
static void item_set_fav_flags(int tab);

static void fav_load() {
  char buf[1024]; size_t len = 0;
  if (!store_fs_read(LC_DIR_ROOT "/favorites.txt", buf, sizeof buf - 1, &len)) return;
  buf[len] = 0;
  char *p = strtok(buf, "\r\n");
  while (p && s_fav_n < LC_FAV_MAX) {
    char *tab = strchr(p, '\t');
    if (tab) { *tab = 0; snprintf(s_fav[s_fav_n], 96, "%s", p); s_fav_n++; }
    p = strtok(nullptr, "\r\n");
  }
}
static bool fav_has(const char *art) {
  for (int i = 0; i < s_fav_n; i++) if (!strcmp(s_fav[i], art)) return true;
  return false;
}
static void fav_toggle(const char *art) {
  for (int i = 0; i < s_fav_n; i++) if (!strcmp(s_fav[i], art)) {
    for (int j = i; j + 1 < s_fav_n; j++) strcpy(s_fav[j], s_fav[j + 1]);
    s_fav_n--; goto save;
  }
  if (s_fav_n < LC_FAV_MAX) snprintf(s_fav[s_fav_n++], 96, "%s", art);
save: {
    char buf[1024]; int o = 0;
    for (int i = 0; i < s_fav_n; i++) o += snprintf(buf + o, sizeof buf - o, "%s\t%s\n", s_fav[i], s_fav[i]);
    store_fs_write(LC_DIR_ROOT "/favorites.txt", buf, (size_t)o);
    rebuild_tabs();
  }
}

// doi thu muc art thanh path tuong doi cho tab apps (vd "apps/browser.png")

// doi thu muc art thanh path tuong doi cho tab apps (vd "apps/browser.png")
static void item_set_fav_flags(int tab) {
  for (int i = 0; i < s_item_n[tab]; i++)
    s_items[tab][i].favorite = fav_has(s_items[tab][i].art);
}

// cap nhat lai tab Favorites va cac co
static void rebuild_tabs();

// ============================================================ launcher state
static bool s_active = false;
static int  s_tab = LC_TAB_APPS;
static int  s_sel[LC_TAB_N] = { 0, 0, 0, 0 };
static int  s_top[LC_TAB_N] = { 0, 0, 0, 0 };
static bool s_dialog_open = false;
static int  s_dialog_sel = 0;
static int  s_dlg_kind = 0;            // 0 = settings, 1 = context menu, 2 = info
static char s_ctx_title[LC_TITLE_MAX];

static const char *TAB_NAMES[LC_TAB_N] = { "Apps", "Games", "Favorites", "Settings" };
static const char *TAB_ART[LC_TAB_N]   = { "banner_apps.png", "banner_games.png",
                                           "banner_favorites.png", "banner_settings.png" };

// ---- cac tab noi dung ----
static void build_tabs() {
  s_item_n[LC_TAB_APPS] = s_item_n[LC_TAB_GAMES] = 0;
  add_item(LC_TAB_APPS, _("Browser"), "apps/browser.png", 0, false);
  add_item(LC_TAB_APPS, _("About"), "", 1, false);
  add_item(LC_TAB_GAMES, _("(No games yet)"), "", 1, false);
  // Settings tab: cac muc cau hinh
  s_item_n[LC_TAB_SETTINGS] = 0;
  char b[LC_TITLE_MAX];
  snprintf(b, sizeof b, _("Theme: %s"), theme_display_name()); add_item(LC_TAB_SETTINGS, b, "", 0, false);
  snprintf(b, sizeof b, _("Brightness: %d"), cfg_brightness()); add_item(LC_TAB_SETTINGS, b, "", 0, false);
  add_item(LC_TAB_SETTINGS, _("Language: English"), "", 0, false);
  snprintf(b, sizeof b, "WiFi: %s", cfg_ssid.length() ? cfg_ssid.c_str() : "-"); add_item(LC_TAB_SETTINGS, b, "", 1, false);
  snprintf(b, sizeof b, "Pass: %s", cfg_pass.length() ? cfg_pass.c_str() : "-"); add_item(LC_TAB_SETTINGS, b, "", 1, false);
  add_item(LC_TAB_SETTINGS, _("Back to browser"), "", 0, false);
  // Favorites: mirror tu flag
  s_item_n[LC_TAB_FAVORITES] = 0;
  for (int t = 0; t < LC_TAB_N; t++) {
    if (t == LC_TAB_FAVORITES) continue;
    for (int i = 0; i < s_item_n[t]; i++)
      if (s_items[t][i].favorite)
        add_item(LC_TAB_FAVORITES, s_items[t][i].title, s_items[t][i].art, s_items[t][i].kind, true);
  }
  if (!s_item_n[LC_TAB_FAVORITES])
    add_item(LC_TAB_FAVORITES, _("(Empty - press 9)"), "", 1, false);
}

static void rebuild_tabs() {
  item_set_fav_flags(LC_TAB_APPS);
  item_set_fav_flags(LC_TAB_GAMES);
  build_tabs();
}

// ============================================================ render (Qeafbrowser / S60 chrome)
static uint16_t lerp565(uint16_t a, uint16_t b, int num, int den) {
  if (den <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  int r = ar + ((br - ar) * num) / den;
  int g = ag + ((bg - ag) * num) / den;
  int bl = ab + ((bb - ab) * num) / den;
  if (r < 0) r = 0; if (r > 31) r = 31;
  if (g < 0) g = 0; if (g > 63) g = 63;
  if (bl < 0) bl = 0; if (bl > 31) bl = 31;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void fill_gradient_v(int x, int y, int w, int h, uint16_t c0, uint16_t c1) {
  int den = (h > 1) ? (h - 1) : 1;
  for (int i = 0; i < h; i++)
    dev_display_fill_rect(x, y + i, w, 1, lerp565(c0, c1, i, den));
}

// ============================================================ icon S60 pixel-art
enum LcTileIcon {
  TI_BROWSER = 0, TI_INFO, TI_EMPTY, TI_THEME, TI_BRIGHT,
  TI_LANG, TI_WIFI, TI_PASS, TI_BACK, TI_STAR, TI_FOLDER
};

// Tim icon type theo tab + title (khong can truong kind them).
static int tile_icon_kind(int tab, const char *title) {
  if (strstr(title, "Browser") || strstr(title, "Trình duyệt")) return TI_BROWSER;
  if (strstr(title, "About") || strstr(title, "Giới thiệu")) return TI_INFO;
  if (strstr(title, "No games") || strstr(title, "Empty") || strstr(title, "chưa có") || strstr(title, "trống")) return TI_EMPTY;
  if (strstr(title, "Theme") || strstr(title, "Giao diện")) return TI_THEME;
  if (strstr(title, "Brightness") || strstr(title, "Độ sáng")) return TI_BRIGHT;
  if (strstr(title, "Language") || strstr(title, "Ngôn ngữ")) return TI_LANG;
  if (strstr(title, "WiFi")) return TI_WIFI;
  if (strstr(title, "Pass")) return TI_PASS;
  if (strstr(title, "Back to") || strstr(title, "Quay lại")) return TI_BACK;
  if (tab == LC_TAB_GAMES) return TI_FOLDER;
  if (tab == LC_TAB_FAVORITES) return TI_STAR;
  if (tab == LC_TAB_SETTINGS) return TI_FOLDER;
  return TI_FOLDER;
}

// Ve pixel-art 24x24 trong o icon w x h. La = mau cho phep trong suot.
static void draw_tile_glyph(int x, int y, int w, int h, int kind) {
  const int ox = x + (w - 24) / 2;
  const int oy = y + (h - 24) / 2;
  // mau S60-ish (folder vang, globe xanh, ...)
  const uint16_t YEL = S60_RGB(0xF4, 0xC4, 0x30);
  const uint16_t YEL_D = S60_RGB(0xB8, 0x8A, 0x10);
  const uint16_t BLU = S60_RGB(0x2A, 0x6E, 0xC8);
  const uint16_t BLU_L = S60_RGB(0x7A, 0xB4, 0xF0);
  const uint16_t GRN = S60_RGB(0x3A, 0xA0, 0x40);
  const uint16_t GRY = S60_RGB(0x80, 0x88, 0x90);
  const uint16_t WHT = 0xFFFF;
  const uint16_t BLK = 0x0000;
  auto R = [&](int rx, int ry, int rw, int rh, uint16_t c) {
    if (rw <= 0 || rh <= 0) return;
    dev_display_fill_rect(ox + rx, oy + ry, rw, rh, c);
  };
  switch (kind) {
    case TI_FOLDER:
      R(2, 6, 8, 3, YEL_D);          // tab folder
      R(2, 8, 20, 14, YEL);          // body
      R(2, 8, 20, 1, YEL_D);
      R(2, 21, 20, 1, YEL_D);
      R(4, 11, 16, 8, S60_RGB(0xFF, 0xE0, 0x70));
      break;
    case TI_BROWSER:
      R(4, 4, 16, 16, BLU);          // globe
      R(4, 4, 16, 1, BLU_L);
      R(11, 4, 2, 16, BLU_L);        // meridian
      R(4, 11, 16, 2, BLU_L);        // equator
      R(6, 6, 4, 4, BLU_L);
      R(14, 14, 4, 4, BLU_L);
      break;
    case TI_INFO:
      R(6, 4, 12, 16, BLU);
      R(6, 4, 12, 1, BLU_L);
      R(11, 7, 2, 6, WHT);
      R(11, 15, 2, 2, WHT);
      break;
    case TI_EMPTY:
      R(5, 6, 14, 12, GRY);
      R(7, 8, 10, 8, WHT);
      R(9, 10, 6, 4, GRY);
      break;
    case TI_THEME:
      R(4, 6, 16, 12, WHT);          // palette
      R(4, 6, 16, 1, GRY);
      R(7, 10, 3, 3, S60_RGB(0xE0, 0x30, 0x30));
      R(12, 10, 3, 3, S60_RGB(0x30, 0x80, 0xE0));
      R(9, 14, 3, 3, S60_RGB(0x40, 0xA0, 0x40));
      R(14, 14, 3, 3, YEL);
      break;
    case TI_BRIGHT:
      R(10, 4, 4, 4, YEL);           // sun core
      R(4, 10, 4, 4, YEL); R(16, 10, 4, 4, YEL);
      R(10, 16, 4, 4, YEL);
      R(6, 6, 3, 3, YEL); R(15, 6, 3, 3, YEL);
      R(6, 15, 3, 3, YEL); R(15, 15, 3, 3, YEL);
      R(9, 9, 6, 6, S60_RGB(0xFF, 0xE8, 0x60));
      break;
    case TI_LANG:
      R(4, 6, 7, 12, BLU);           // A
      R(6, 8, 3, 3, WHT);
      R(4, 13, 7, 2, WHT);
      R(13, 6, 7, 12, GRN);          // B-ish / native
      R(15, 9, 3, 2, WHT);
      R(15, 13, 3, 2, WHT);
      break;
    case TI_WIFI:
      R(10, 16, 4, 4, BLK);          // dot
      R(7, 12, 10, 3, BLU);          // arc mid
      R(4, 8, 16, 3, BLU);
      R(2, 4, 20, 3, BLU_L);
      break;
    case TI_PASS:
      R(7, 10, 10, 10, YEL);         // key head
      R(9, 12, 6, 6, WHT);
      R(11, 14, 2, 2, YEL_D);
      R(16, 13, 6, 3, YEL);          // shaft
      R(19, 16, 2, 3, YEL);
      break;
    case TI_BACK:
      R(14, 5, 3, 14, BLU);          // bar
      R(6, 9, 8, 6, BLU);
      R(3, 11, 4, 2, BLU);
      break;
    case TI_STAR:
      R(11, 3, 3, 18, YEL);
      R(4, 9, 17, 3, YEL);
      R(6, 6, 13, 9, YEL);
      R(5, 15, 5, 4, YEL); R(15, 15, 5, 4, YEL);
      R(9, 8, 6, 5, S60_RGB(0xFF, 0xE8, 0x70));
      break;
    default:
      R(6, 6, 12, 12, GRY);
      break;
  }
}

// o icon 30x30: nen sang + bien + glyph S60 (luon sang de doi chieu voi sel blue)
static void draw_tile_icon(int x, int y, int w, int h, int tab, const char *title,
                           uint16_t bg, bool selected) {
  dev_display_fill_rect(x, y, w, h, bg);
  // bien 1px + highlight tren (kieu key S60)
  dev_display_fill_rect(x, y, w, 1, UC_TILE_LINE);
  dev_display_fill_rect(x, y + h - 1, w, 1, UC_TILE_LINE);
  dev_display_fill_rect(x, y, 1, h, UC_TILE_LINE);
  dev_display_fill_rect(x + w - 1, y, 1, h, UC_TILE_LINE);
  if (selected) {
    dev_display_fill_rect(x + 1, y + 1, w - 2, 1, 0xFFFF);
    dev_display_fill_rect(x + 1, y + 1, 1, h - 2, 0xFFFF);
  }
  draw_tile_glyph(x, y, w, h, tile_icon_kind(tab, title));
}

// chu nhan grid: bold S60 (ve 2 lan offset 1px nhu s60_bold)
void gui_draw_text_bold(int x, int y, uint16_t fg, uint16_t bg,
                        const char *s, int size) {
  gui_draw_text(x, y, fg, bg, s, size, true);
  gui_draw_text(x + 1, y, fg, bg, s, size, true);
}

// icon toolbar don gian (12x12 pixel art) — home / game / star / gear
static void draw_mini_icon(int kind, int cx, int cy, uint16_t c) {
  switch (kind) {
    case 0: // home
      dev_display_fill_rect(cx - 5, cy, 11, 2, c);
      dev_display_fill_rect(cx - 4, cy + 2, 9, 2, c);
      dev_display_fill_rect(cx - 3, cy + 4, 7, 6, c);
      dev_display_fill_rect(cx - 1, cy + 6, 3, 4, UC_URL_BG);
      break;
    case 1: // gamepad
      dev_display_fill_rect(cx - 6, cy + 2, 13, 7, c);
      dev_display_fill_rect(cx - 7, cy + 4, 3, 4, c);
      dev_display_fill_rect(cx + 5, cy + 4, 3, 4, c);
      dev_display_fill_rect(cx - 3, cy + 4, 2, 1, UC_URL_BG);
      dev_display_fill_rect(cx - 2, cy + 3, 1, 3, UC_URL_BG);
      dev_display_fill_rect(cx + 2, cy + 4, 2, 2, UC_URL_BG);
      break;
    case 2: // star
      dev_display_fill_rect(cx - 1, cy - 5, 3, 11, c);
      dev_display_fill_rect(cx - 5, cy - 1, 11, 3, c);
      dev_display_fill_rect(cx - 3, cy - 3, 7, 7, c);
      dev_display_fill_rect(cx - 4, cy + 2, 3, 3, c);
      dev_display_fill_rect(cx + 2, cy + 2, 3, 3, c);
      break;
    default: // gear
      dev_display_fill_rect(cx - 4, cy - 4, 9, 9, c);
      dev_display_fill_rect(cx - 5, cy - 1, 11, 3, c);
      dev_display_fill_rect(cx - 1, cy - 5, 3, 11, c);
      dev_display_fill_rect(cx - 2, cy - 2, 5, 5, UC_URL_BG);
      break;
  }
}

static const char *SECT_NAMES[LC_TAB_N] = {
  "Top Sites", "Games", "Favorites", "Settings"
};

static void draw_status_bar() {
  gui_clear_rect(0, 0, LC_W, LC_STATUS_H, UC_STATUS_BG);
  gui_statusbar(UC_STATUS_FG, UC_STATUS_BG);
}

static void draw_title_bar() {
  const int y = LC_STATUS_H, h = LC_TITLE_H;
  fill_gradient_v(0, y, LC_W, h, UC_TITLE_TOP, UC_TITLE_BOT);
  int fh = gui_font_height(1);
  gui_draw_text(6, y + (h - fh) / 2, UC_TITLE_FG, UC_TITLE_BOT, "Qeafbrowser", 1, true);
  const char *help = "Help";
  int hw = gui_text_width(help, 1);
  gui_draw_text(LC_W - hw - 8, y + (h - fh) / 2, UC_TITLE_FG, UC_TITLE_BOT, help, 1, true);
}

static void draw_url_bar() {
  const int y = LC_STATUS_H + LC_TITLE_H, h = LC_URL_H;
  gui_clear_rect(0, y, LC_W, h, UC_URL_BG);
  dev_display_fill_rect(0, y, LC_W, 1, UC_URL_BORDER);
  dev_display_fill_rect(0, y + h - 1, LC_W, 1, UC_URL_BORDER);
  // tab trai: globe + ten tab
  const int gy = y + (h - 12) / 2;
  dev_display_fill_rect(6, gy, 12, 12, UC_URL_BORDER);
  dev_display_fill_rect(7, gy + 1, 10, 10, UC_TITLE_TOP);
  dev_display_fill_rect(11, gy + 1, 2, 10, UC_URL_BORDER);
  dev_display_fill_rect(7, gy + 5, 10, 2, UC_URL_BORDER);
  int fh = gui_font_height(1);
  gui_draw_text(22, y + (h - fh) / 2, UC_URL_FG, UC_URL_BG, TAB_NAMES[s_tab], 1, true);
  // o search phai
  int sx = LC_W - 78;
  dev_display_fill_rect(sx, y + 4, 74, h - 8, 0xFFFF);
  dev_display_fill_rect(sx, y + 4, 1, h - 8, UC_URL_BORDER);
  dev_display_fill_rect(sx + 73, y + 4, 1, h - 8, UC_URL_BORDER);
  // magnifier
  const int my = y + h / 2;
  dev_display_fill_rect(sx + 6, my - 5, 7, 7, UC_URL_BORDER);
  dev_display_fill_rect(sx + 7, my - 4, 5, 5, 0xFFFF);
  dev_display_fill_rect(sx + 12, my + 2, 4, 2, UC_URL_BORDER);
  gui_draw_text(sx + 18, y + (h - fh) / 2, UC_ITEM_DIM, 0xFFFF, "Search", 1, true);
}

static void draw_icon_toolbar() {
  const int y = LC_STATUS_H + LC_TITLE_H + LC_URL_H, h = LC_ICON_H;
  fill_gradient_v(0, y, LC_W, h, UC_ICON_TOP, UC_ICON_BOT);
  // 4 icon = 4 tab; vi tri deu tren 240
  const int icons = LC_TAB_N;
  const int slot = LC_W / icons;
  for (int i = 0; i < icons; i++) {
    int cx = slot * i + slot / 2;
    int cy = y + h / 2;
    if (i == s_tab) {
      dev_display_fill_rect(cx - 14, y + 2, 28, h - 4, UC_ICON_SEL_BG);
      dev_display_fill_rect(cx - 14, y + 2, 28, 1, UC_TITLE_BOT);
    }
    draw_mini_icon(i, cx, cy, i == s_tab ? 0xFFFF : UC_TITLE_BOT);
  }
}

static void draw_section_header() {
  const int y = LC_STATUS_H + LC_TITLE_H + LC_URL_H + LC_ICON_H, h = LC_SECT_H;
  gui_clear_rect(0, y, LC_W, h, UC_SECT_BG);
  int fh = gui_font_height(1);
  gui_draw_text(6, y + (h - fh) / 2, UC_SECT_FG, UC_SECT_BG, SECT_NAMES[s_tab], 1, true);
  // chevron xuong phai
  const int ax = LC_W - 14, ay = y + h / 2;
  for (int i = 0; i < 4; i++)
    dev_display_fill_rect(ax - i, ay - 1 + i, 1 + 2 * i, 1, UC_SECT_FG);
}

// Grid 2 cot Top Sites (thay cho banner art + list doc cu)
static void draw_art_area() {
  // UC layout khong co vung art banner — giu hook de launcher_tick khong loi
}

static void draw_list_area() {
  const int gy = LC_HDR_H;
  const int gh = LC_H - LC_FTR_H - gy;
  gui_clear_rect(0, gy, LC_W, gh, UC_CONTENT_BG);
  const int n = s_item_n[s_tab];
  const int cols = 2;
  const int rows_vis = LC_ROWS;
  const int cells = cols * rows_vis;
  const int cw = LC_W / cols;
  int &sel = s_sel[s_tab], &top = s_top[s_tab];
  if (sel < 0) sel = 0;
  if (n > 0 && sel >= n) sel = n - 1;
  if (sel < 0) sel = 0;
  if (n <= cells) top = 0;
  else {
    if (sel < top) top = sel;
    if (sel >= top + cells) top = sel - cells + 1;
    if (top > n - cells) top = n - cells;
    if (top < 0) top = 0;
  }
  for (int i = 0; i < cells; i++) {
    int idx = top + i;
    if (idx >= n) break;
    int col = i % cols, row = i / cols;
    int cx = col * cw;
    int cy = gy + row * LC_GRID_ITEM_H;
    bool is_sel = (idx == sel);
    uint16_t lab_fg = is_sel ? UC_SEL_FG : (s_items[s_tab][idx].kind == 1 ? UC_ITEM_DIM : UC_ITEM_FG);
    // Focus block S60: khung 2px S60_SEL_LINE + gradient ben trong (gioi S60)
    if (is_sel) {
      fill_gradient_v(cx, cy, cw, LC_GRID_ITEM_H, S60_SEL_TOP, S60_SEL_BOT);
      // khung ngoai 2px — khong che het gradient (giu S60_SEL_TOP o hang trong)
      dev_display_fill_rect(cx, cy, cw, 1, S60_SEL_LINE);
      dev_display_fill_rect(cx, cy + LC_GRID_ITEM_H - 1, cw, 1, S60_SEL_LINE);
      dev_display_fill_rect(cx, cy, 1, LC_GRID_ITEM_H, S60_SEL_LINE);
      dev_display_fill_rect(cx + cw - 1, cy, 1, LC_GRID_ITEM_H, S60_SEL_LINE);
      dev_display_fill_rect(cx + 1, cy + 1, cw - 2, 1, S60_SEL_LINE);
      dev_display_fill_rect(cx + 1, cy + LC_GRID_ITEM_H - 2, cw - 2, 1, S60_SEL_LINE);
      dev_display_fill_rect(cx + 1, cy + 1, 1, LC_GRID_ITEM_H - 2, S60_SEL_LINE);
      dev_display_fill_rect(cx + cw - 2, cy + 1, 1, LC_GRID_ITEM_H - 2, S60_SEL_LINE);
      // vien trong 1px dam o 4 canh — tap trung vao hang 2.. de hang gradient TOP con mau
      dev_display_fill_rect(cx + 2, cy + 2, cw - 4, 1, S60_SEL_BOT);
      dev_display_fill_rect(cx + 2, cy + LC_GRID_ITEM_H - 3, cw - 4, 1, S60_SEL_BOT);
      dev_display_fill_rect(cx + 2, cy + 2, 1, LC_GRID_ITEM_H - 4, S60_SEL_BOT);
      dev_display_fill_rect(cx + cw - 3, cy + 2, 1, LC_GRID_ITEM_H - 4, S60_SEL_BOT);
    }
    // icon 30x30 — luon nen sang de glyph S60 do ro tren blue/white
    int icon_w = 30, icon_h = 30;
    int icon_x = cx + (cw - icon_w) / 2;
    int icon_y = cy + 3;
    draw_tile_icon(icon_x, icon_y, icon_w, icon_h, s_tab, s_items[s_tab][idx].title,
                   is_sel ? 0xFFFF : UC_TILE_BG, is_sel);
    // nhan duoi icon — font size 2 (Tahoma 16) de doc tren 240x320
    char line[LC_TITLE_MAX + 2];
    snprintf(line, sizeof line, "%s%s", s_items[s_tab][idx].favorite ? "*" : "",
             s_items[s_tab][idx].title);
    int maxw = cw - 8;
    int tw = gui_text_width(line, 2);
    while (tw > maxw && line[0]) {
      int L = (int)strlen(line);
      L--; while (L > 0 && ((unsigned char)line[L] & 0xC0) == 0x80) L--;
      line[L] = 0;
      tw = gui_text_width(line, 2);
    }
    int fh2 = gui_font_height(2);
    int lab_y = cy + icon_h + 4;
    if (lab_y + fh2 > cy + LC_GRID_ITEM_H) lab_y = cy + LC_GRID_ITEM_H - fh2;
    gui_draw_text_bold(cx + (cw - tw) / 2, lab_y, lab_fg,
                       is_sel ? S60_SEL_TOP : UC_CONTENT_BG, line, 2);
    // ke cot giua 2 cot
    if (col == 0)
      dev_display_fill_rect(cx + cw - 1, cy + 2, 1, LC_GRID_ITEM_H - 4, UC_RULE);
  }
  // thanh cuon neu vuot qua 1 man
  if (n > cells) {
    int sx = LC_W - 3;
    dev_display_fill_rect(sx, gy, 2, gh, S60_SCROLL_BG);
    int bar_h = (gh * cells) / n; if (bar_h < 8) bar_h = 8;
    int bar_y = gy + (top * (gh - bar_h)) / (n - cells > 0 ? (n - cells) : 1);
    dev_display_fill_rect(sx, bar_y, 2, bar_h, S60_SCROLL_FG);
  }
}

static void draw_footer() {
  const int y = LC_H - LC_FTR_H, h = LC_FTR_H;
  gui_clear_rect(0, y, LC_W, h, UC_SOFT_BG);
  int fh = gui_font_height(1);
  int ty = y + (h - fh) / 2;
  gui_draw_text(4, ty, UC_SOFT_FG, UC_SOFT_BG, "Menu", 1, true);
  const char *clk = clock_cache;
  int cw = gui_text_width(clk, 1);
  gui_draw_text((LC_W - cw) / 2, ty, UC_SOFT_FG, UC_SOFT_BG, clk, 1, true);
  const char *sw = "Switch";
  int sw_w = gui_text_width(sw, 1);
  gui_draw_text(LC_W - sw_w - 4, ty, UC_SOFT_FG, UC_SOFT_BG, sw, 1, true);
}

static void redraw_open_dialog();

void launcher_redraw() {
  if (!s_active) return;
  draw_status_bar();
  draw_title_bar();
  draw_url_bar();
  draw_icon_toolbar();
  draw_section_header();
  draw_art_area();
  draw_list_area();
  draw_footer();
  redraw_open_dialog();
  dev_display_flush();
}

// ============================================================ xu ly phim
static void open_selected() {
  LcItem &it = s_items[s_tab][s_sel[s_tab]];
  if (s_tab == LC_TAB_SETTINGS) {
    if (s_sel[s_tab] == 0) { theme_next(1); build_tabs(); launcher_redraw(); return; }
    if (s_sel[s_tab] == 1) {
      int b = cfg_brightness() + 40; if (b > 255) b = 40;
      s_brightness = b;
#if defined(ARDUINO)
      disp.setBrightness(b);
#endif
      cfg_save(); build_tabs(); launcher_redraw(); return;
    }
    if (s_sel[s_tab] == 2) {
      static const char *LANG_OPTS[] = { "~English", "Tiếng Việt" };
      s_dialog_sel = 0;  // mac dinh English
      gui_dialog("Language", nullptr, 0, LANG_OPTS, 2, &s_dialog_sel);
      return;
    }
    if (s_sel[s_tab] == 3 || s_sel[s_tab] == 4) {   // WiFi / Pass: chi hien thi
      s_dlg_kind = 2; s_dialog_open = true; s_dialog_sel = 0;
      snprintf(s_ctx_title, sizeof s_ctx_title, "%s", it.title);
      launcher_redraw(); return;
    }
    if (s_sel[s_tab] == 5) { launcher_exit_to_browser(); return; }
    return;
  }
  if (it.kind == 1) { s_dlg_kind = 2; s_dialog_open = true; s_dialog_sel = 0;
    snprintf(s_ctx_title, sizeof s_ctx_title, "%s", it.title);
    launcher_redraw(); return; }
  if (s_tab == LC_TAB_APPS && !strcmp(it.title, _("Browser"))) {
    launcher_exit_to_browser(); return;
  }
  // game chua co
  s_dlg_kind = 2; s_dialog_open = true; s_dialog_sel = 0;
  snprintf(s_ctx_title, sizeof s_ctx_title, "%s", it.title);
  launcher_redraw();
}

static void draw_context_dialog() {
  LcItem &it = s_items[s_tab][s_sel[s_tab]];
  const char *lines[2] = { it.title, it.art[0] ? it.art : "(no image)" };
  const char *opts[4] = { "Open", "~Properties", "Delete", "Cancel" };
  gui_dialog("Options", lines, 2, opts, 4, &s_dialog_sel);
}

// Ve lai dialog dang mo theo loai (goi sau moi su kien phim / redraw).
static void redraw_open_dialog() {
  if (!s_dialog_open) return;
  if (s_dlg_kind == 1) { draw_context_dialog(); return; }
  // kind 2: info / game chua co
  const char *lines[2] = { s_ctx_title, "(not supported yet)" };
  const char *opts[2] = { "Close", nullptr };
  gui_dialog("Information", lines, 2, opts, 1, &s_dialog_sel);
}

static void handle_key(const char *k) {
  if (s_dialog_open) {
    if (!strcmp(k, LK_BACK)) { s_dialog_open = false; launcher_redraw(); return; }
    if (!strcmp(k, LK_UP))   { s_dialog_sel--; redraw_open_dialog(); return; }
    if (!strcmp(k, LK_DOWN)) { s_dialog_sel++; redraw_open_dialog(); return; }
    if (!strcmp(k, LK_OK))   {
      // Context menu: chi "Huy"/"Dong" dong; muc khac cung dong (MVP).
      s_dialog_open = false; launcher_redraw(); return;
    }
    return;
  }
  int n = s_item_n[s_tab];
  if (!strcmp(k, LK_LEFT))  { s_tab = (s_tab + LC_TAB_N - 1) % LC_TAB_N; art_request(s_items[s_tab][s_sel[s_tab]].art[0] ? s_items[s_tab][s_sel[s_tab]].art : ""); launcher_redraw(); return; }
  if (!strcmp(k, LK_RIGHT)) { s_tab = (s_tab + 1) % LC_TAB_N; art_request(s_items[s_tab][s_sel[s_tab]].art[0] ? s_items[s_tab][s_sel[s_tab]].art : ""); launcher_redraw(); return; }
  if (!strcmp(k, LK_UP))    { s_sel[s_tab] = (s_sel[s_tab] + n - 1) % n; art_request(s_items[s_tab][s_sel[s_tab]].art); launcher_redraw(); return; }
  if (!strcmp(k, LK_DOWN))  { s_sel[s_tab] = (s_sel[s_tab] + 1) % n; art_request(s_items[s_tab][s_sel[s_tab]].art); launcher_redraw(); return; }
  if (!strcmp(k, LK_OK))    { open_selected(); return; }
  if (!strcmp(k, LK_FAV))   {
    LcItem &it = s_items[s_tab][s_sel[s_tab]];
    if (it.art[0] && it.kind == 0) { fav_toggle(it.art); launcher_redraw(); }
    return;
  }
  if (!strcmp(k, LK_CONTEXT)) { s_dlg_kind = 1; s_dialog_open = true; s_dialog_sel = 0; draw_context_dialog(); return; }
  if (!strcmp(k, LK_MENU))    { s_tab = LC_TAB_APPS; s_sel[s_tab] = 0; launcher_redraw(); return; }
}

void launcher_key(const char *game_key) {
  if (!s_active || !game_key) return;
  handle_key(game_key);
}

void launcher_tick() {
  // Ket qua art task -> ve lai vung art khi co anh moi (khong full redraw).
  static int last_art_w = -1;
  uint16_t *pix; int w, h;
  if (art_ready(&pix, &w, &h) && pix && w != last_art_w) {
    last_art_w = w;
    draw_art_area();
    dev_display_flush();
  }
}

// ============================================================ vao/ra launcher
void launcher_set_active(bool active) {
  if (s_active == active) return;
  s_active = active;
  if (s_active) { launcher_redraw(); }
}

bool launcher_active() { return s_active; }

// main.cpp cung cap (C linkage, dinh nghia o cuoi main.cpp)
extern "C" void sim_key_launch_go_home(void);

void launcher_exit_to_browser() {
  s_active = false;
  // Browser can hien lai man hinh ngay: ve trang chu (Speed Dial) de khong bi
  // man hinh launcher 'dung' sau khi launcher dong.
  sim_key_launch_go_home();
}

void launcher_begin(bool start_now) {
  theme_defaults(s_theme);
  store_fs_mount_sd();   // MOUNT SD TRUOC — sau do moi doc duoc theme/config
  cfg_load();
  theme_load(s_theme_name);
  fav_load();
  build_tabs();
  art_task_start();
  dev_display_init();
#if defined(ARDUINO)
  disp.setBrightness(cfg_brightness());
#endif
  s_active = false;
  if (start_now) launcher_set_active(true);
}

int  launcher_tab_index()  { return s_tab; }
int  launcher_item_index() { return s_sel[s_tab]; }
const char *launcher_item_title(int idx) {
  if (idx < 0 || idx >= s_item_n[s_tab]) return "";
  return s_items[s_tab][idx].title;
}
