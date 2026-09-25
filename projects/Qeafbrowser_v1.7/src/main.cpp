// Qeafbrowser — main app cho E524546 (ESP32-S3 + ST7789 240x320 doc + keypad Symbian).
// Port tu legacy keypad browser (MRE VXP): UI WML card-style, mtt: pseudo-URL,
// start page + About/Help/Settings noi dung lay nguyen van tu ban carve .rodata.
// Lua firmware: docs/PROMPT.md — GPIO bat kha xam pham, doc 240x320 (rotation 0).
#include <Arduino.h>
#include <WiFi.h>
#include <stdarg.h>
#include <time.h>
#include <esp_heap_caps.h>
#include <new>                          // placement new (doi tuong PNG trong PSRAM)
#if defined(ARDUINO) && __has_include(<TJpg_Decoder.h>)
#include <TJpg_Decoder.h>
#define QB_HAS_TJPEG 1
#else
#define QB_HAS_TJPEG 0
#endif
#if defined(ARDUINO) && __has_include(<PNGdec.h>)
#include <PNGdec.h>
#define QB_HAS_PNGDEC 1
#else
#define QB_HAS_PNGDEC 0
#endif
#if !defined(ARDUINO) && defined(__linux__)
#include <png.h>
extern "C" {
#include <jpeglib.h>
}
#endif
#include "pins.h"
#include "browser.h"
#include "LGFX_ESP32S3_ST7789.h"
#include "launcher.h"

// loopTask stack: go_url -> http_get -> TLS -> doc_parse -> thumb_prefetch (http_get lai)
// tran 8KB mac dinh -> Stack canary. SET_LOOP_TASK_STACK_SIZE override weak getArduinoLoopTaskStackSize.
#if defined(ARDUINO)
SET_LOOP_TASK_STACK_SIZE(32768);
#endif

// ---------------- UI palette — Symbian S60 (Nokia feature phone) ----------------
// Toan bo mau nam trong include/ui_s60.h de firmware va simulator dung chung
// MOT dinh nghia theme duy nhat. Cac ten UI_* cu duoc giu lai lam alias nen moi
// diem goi cu van hoat dong, chi doi gia tri.
#include "ui_s60.h"

#define UI_TITLE  S60_PANE_MID
#define UI_SOFT   S60_SOFT_BOT
#define UI_BG     S60_BG
#define UI_FG     S60_FG
#define UI_LINK   S60_LINK
#define UI_HOT    S60_RED
#define UI_SEL    S60_SEL_BOT
#define UI_SELFG  S60_SEL_TEXT
#define UI_FIELD  S60_FIELD
#define UI_BORDER S60_RULE
#define UI_DIM    S60_DIM
#define UI_WHITE  S60_WHITE
#define UI_ROW    S60_SCROLL_BG
#define UI_SHADOW S60_SHADOW
#define UI_ARROW  S60_KEY_BOT

LGFX_ESP32S3_ST7789 disp;
// Kieu cu the cho init()/setBrightness() — dung duoc ca tren device lan sim.
static LGFX_ESP32S3_ST7789 *const g_panel = &disp;

#if defined(ARDUINO)
// ---- backbuffer RGB565 240x320 trong PSRAM (chong nhap nhay man hinh) ----
// Toan bo UI ve vao frame_buf roi push MOT len panel. Man hinh khong con
// bi "fillScreen xoa toan bo -> ve lai" tung phim/animation nen khong nhap nhay.
// Neu khong cap duoc PSRAM thi frame_ok = false -> ve truc tiep nhu truoc day.
static LGFX_Sprite frame_buf(&disp);
static lgfx::LovyanGFX *gfx_dst = &disp;   // "disp." duong dan den day
static bool frame_ok = false;
#define disp (*gfx_dst)                    // tu day xuong: disp = target hien tai
#endif

// Chon font noi dung theo style WML — font mac dinh LovyanGFX (Font4/Font0/Font2).
static void set_text_font(int style);
static void wifi_rssi_poll();
static void draw_icon_wifi(int x, int y);
static void draw_title_bar(bool loading_bar);
static void draw_status_pane();
// Khi != null: dong trang thai ("Receiving...", "Connecting...") hien thay title.
static const char *g_pane_status = nullptr;

static void set_text_font_impl(int style) {
  if (style == 2)      disp.setTextFont(4);   // h1/h2
  else if (style == 5) disp.setTextFont(1);   // small/meta
  else                 disp.setTextFont(2);   // body
}
static void set_text_font(int style) { set_text_font_impl(style); }

// ======================= Symbian S60 theme engine =======================
// Nokia 2700 / S60 dung font sans DAM. Thay vi nhung them mot bo bitmap font
// thu hai (ton flash + lech so do wrap cua wml.cpp), theme nay lam day net chu
// bang cach ve lai chuoi glyph lech 1 px sang phai. O co 5x7/6x8 thi net doc
// thanh 2 px that -> dung chat "dam net" cua Nokia, khong ton them RAM, va cho
// ra KET QUA GIONG NHAU tren ESP32 va tren simulator.
static bool s60_bold = true;

static void s60_text(const char *s, int x, int y) {
  if (!s) return;
  disp.drawString(s, x, y);
  if (s60_bold) disp.drawString(s, x + S60_BOLD_PX, y);
}
static int s60_text_w(const char *s) {
  return disp.textWidth(s) + (s60_bold ? S60_BOLD_PX : 0);
}
static void s60_text_center(const char *s, int cx, int y) { s60_text(s, cx - s60_text_w(s) / 2, y); }
static void s60_text_right(const char *s, int right_x, int y) { s60_text(s, right_x - s60_text_w(s), y); }

// Cat chuoi theo be rong do duoc, khong cat giua mot codepoint UTF-8.
static void s60_fit(const char *s, char *out, int cap, int maxw) {
  int n = s ? (int)strlen(s) : 0;
  if (n > cap - 1) n = cap - 1;
  if (n > 0) memcpy(out, s, (size_t)n);
  out[n] = 0;
  while (n > 0 && s60_text_w(out) > maxw) {
    n--;
    while (n > 0 && ((unsigned char)out[n] & 0xC0) == 0x80) n--;
    out[n] = 0;
  }
}

// ---- RGB565 gradient helpers (no sprite, no framebuffer) ----
static inline uint16_t s60_mix565(uint16_t a, uint16_t b, int t) {
  int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  int r = ar + ((br - ar) * t) / 255;
  int g = ag + ((bg - ag) * t) / 255;
  int bl = ab + ((bb - ab) * t) / 255;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}
// Doan gradient cua mot thanh doc cao full_h bat dau tai full_top. Cho phep ve
// lai MOT PHAN (dong ho, progress, text) ma van dung mau cua ca thanh.
static void s60_grad_slice(int x, int y, int w, int h, int full_top, int full_h,
                           uint16_t c0, uint16_t c1) {
  if (h <= 0 || w <= 0) return;
  for (int i = 0; i < h; i++) {
    int row = y + i - full_top;
    if (row < 0) row = 0; else if (row > full_h - 1) row = full_h - 1;
    int t = full_h > 1 ? (row * 255) / (full_h - 1) : 0;
    disp.fillRect(x, y + i, w, 1, s60_mix565(c0, c1, t));
  }
}
static void s60_grad_v(int x, int y, int w, int h, uint16_t c0, uint16_t c1) {
  s60_grad_slice(x, y, w, h, y, h, c0, c1);
}

// Application pane (status pane + title bar) and softkey bar are single bands:
// every partial repaint must go through these so the gradient stays continuous.
static void s60_pane_fill(int x, int y, int w, int h) {
  s60_grad_slice(x, y, w, h, 0, S60_PANE_H, S60_PANE_TOP, S60_PANE_BOT);
}
static void s60_soft_fill(int x, int y, int w, int h) {
  s60_grad_slice(x, y, w, h, SCR_H - S60_SOFT_H, S60_SOFT_H, S60_SOFT_TOP, S60_SOFT_BOT);
}

// ---- S60 list highlight: blue gradient + light top rule + dark bottom rule ----
static void s60_sel_bar(int x, int y, int w, int h) {
  s60_grad_v(x, y, w, h, S60_SEL_TOP, S60_SEL_BOT);
  if (h > 2) { disp.fillRect(x, y, w, 1, S60_SEL_LINE); disp.fillRect(x, y + h - 1, w, 1, S60_SEL_BOT); }
}

// ---- Rounded S60 panel (menus, notes) with a soft drop shadow ----
static void s60_panel(int x, int y, int w, int h, uint16_t bg, uint16_t line) {
  disp.fillRoundRect(x + 2, y + 2, w, h, 4, S60_SHADOW);
  disp.fillRoundRect(x, y, w, h, 4, bg);
  disp.drawRoundRect(x, y, w, h, 4, line);
}

// ---- S60 key cap used by the on-screen keypad ----
static void s60_key(int x, int y, int w, int h, const char *label, bool sel, bool dim) {
  if (sel) {
    s60_grad_v(x, y, w, h, S60_SEL_TOP, S60_SEL_BOT);
    disp.drawRoundRect(x, y, w, h, 2, S60_SEL_LINE);
  } else {
    s60_grad_v(x, y, w, h, S60_KEY_TOP, S60_KEY_BOT);
    disp.drawRoundRect(x, y, w, h, 2, S60_KEY_LINE);
  }
  set_text_font(0);
  disp.setTextColor(sel ? S60_SEL_TEXT : (dim ? S60_KEY_DIM : S60_KEY_TEXT));
  s60_text_center(label, x + w / 2, y + (h - 8) / 2);
}

// Truncate + draw the title of the current page inside the application pane.
// Dung lc_font (Tahoma VN) de title tieng Viet co dau hien dung.
static void s60_pane_title(const char *t, int x, int maxw, int y) {
  char clip[96];
  const char *src = (t && t[0]) ? t : "Qeafbrowser";
  int n = (int)strlen(src);
  if (n > (int)sizeof clip - 1) n = (int)sizeof clip - 1;
  if (n > 0) memcpy(clip, src, (size_t)n);
  clip[n] = 0;
  while (n > 0 && gui_text_width(clip, 1) > maxw) {
    n--;
    while (n > 0 && ((unsigned char)clip[n] & 0xC0) == 0x80) n--;
    clip[n] = 0;
  }
  // lc_font_vn12 height 14 vs Font0 height 8: canh giua pane 22px
  int ty = y;
  if (y == 5) ty = (UI_HDR_H - gui_font_height(1)) / 2;
  gui_draw_text_bold(x, ty, S60_PANE_TEXT, 0, clip, 1);
}

// ---- ban phim ao D-pad (giong E524546-OS / LegacyOS TextEditor / Pochita Keyboard) ----
static void go_url(const char *url);
static void wifi_result(bool ok);
static bool wifi_try_connect(const char *ssid, const char *pass);
static void open_user_url(const char *raw);
static void open_search_query(const char *raw);
static bool  vkey_open = false;
static int   vkey_row = 1, vkey_col = 0;
static bool  vkey_shift = false, vkey_sym = false;
static char *vkey_buf = nullptr;
static int  *vkey_n = nullptr;
static int   vkey_cap = 0;
static bool  vkey_is_pass = false;
static bool  vkey_is_search = false;

static const char *VK_LOWER[] = {
  "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm./-",
};
static const char *VK_UPPER[] = {
  "1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM./-",
};
static const char *VK_SYM[] = {
  "1234567890", ":/?#&=%@+", "[]{}<>|~^", "_-.,:;'!$",
};
// hang dac biet: SHF SYM SPACE DEL GO
enum { VK_SHF = 0, VK_SYM_K, VK_SPC, VK_DEL, VK_GO, VK_SP_N };
// shortcut TLD khi nhap URL (khong hien khi search/password)
static const char *VK_TLD[] = { ".com", ".net", ".org" };
enum { VK_TLD_N = 3, VK_TLD_ROW = 5 };

static bool vkey_show_tld() {
  return vkey_open && !vkey_is_pass && !vkey_is_search;
}
static int vkey_max_row() {
  return vkey_show_tld() ? VK_TLD_ROW : 4;
}

static const char **vkey_rows() {
  return vkey_sym ? VK_SYM : (vkey_shift ? VK_UPPER : VK_LOWER);
}
static void vkey_bind(char *buf, int *n, int cap, bool is_pass, bool is_search) {
  vkey_buf = buf; vkey_n = n; vkey_cap = cap;
  vkey_is_pass = is_pass; vkey_is_search = is_search;
  vkey_open = true; vkey_row = 1; vkey_col = 0;
  vkey_shift = false; vkey_sym = false;
}
static void vkey_put(char c) {
  if (!vkey_buf || !vkey_n) return;
  if (*vkey_n < vkey_cap - 1) { vkey_buf[(*vkey_n)++] = c; vkey_buf[*vkey_n] = 0; }
}
static void vkey_back() {
  if (!vkey_buf || !vkey_n || *vkey_n <= 0) return;
  vkey_buf[--(*vkey_n)] = 0;
}
static void vkey_submit();

// --- ve ban phim ---
static void vkey_draw_key(int x, int y, int w, int h, const char *label, bool sel, bool dim) {
  s60_key(x, y, w, h, label, sel, dim);
}

static void vkey_draw() {
  // truong nhap kieu S60: nen sang, vien xanh nhat
  disp.fillRect(UI_PAD, UI_HDR_H + 6, SCR_W - 2 * UI_PAD, 30, S60_FIELD);
  disp.drawRoundRect(UI_PAD, UI_HDR_H + 6, SCR_W - 2 * UI_PAD, 30, 3, S60_FIELD_LINE);
  char shown[72];
  int vn = vkey_n ? *vkey_n : 0;
  int copy = vn < 28 ? vn : 28;
  if (vkey_is_pass) { for (int i = 0; i < copy; i++) shown[i] = '*'; }
  else memcpy(shown, vkey_buf, copy);
  shown[copy] = 0;
  if (copy < 28) { shown[copy] = '_'; shown[copy + 1] = 0; }
  set_text_font(0); disp.setTextColor(S60_FG);
  s60_text(shown, UI_PAD + 6, UI_HDR_H + 13);

  // 4 hang chu
  const char **rows = vkey_rows();
  const int kw = (SCR_W - 2 * UI_PAD) / 10;
  const int kh = 28;
  int ky = UI_HDR_H + 42;
  for (int r = 0; r < 4; r++) {
    int n = (int)strlen(rows[r]);
    for (int c = 0; c < n && c < 10; c++) {
      char lab[2] = { rows[r][c], 0 };
      bool sel = (vkey_row == r && vkey_col == c);
      vkey_draw_key(UI_PAD + c * kw, ky + r * kh, kw - 1, kh - 2, lab, sel, false);
    }
  }
  // hang dac biet
  static const char *SP[VK_SP_N] = { "SHF", "SYM", "SPC", "DEL", "GO" };
  const int sw = (SCR_W - 2 * UI_PAD) / VK_SP_N;
  ky += 4 * kh + 4;
  for (int i = 0; i < VK_SP_N; i++) {
    bool sel = (vkey_row == 4 && vkey_col == i);
    bool on = (i == VK_SHF && vkey_shift) || (i == VK_SYM_K && vkey_sym);
    vkey_draw_key(UI_PAD + i * sw, ky, sw - 2, 30, SP[i], sel, on ? false : (i < 2));
    if (on) disp.fillRect(UI_PAD + i * sw + 2, ky + 27, sw - 6, 2, S60_ACCENT);
  }
  // shortcut TLD: .com / .net / .org (chi hop nhap URL)
  if (vkey_show_tld()) {
    const int tw = (SCR_W - 2 * UI_PAD) / VK_TLD_N;
    int ty = ky + 34;
    for (int i = 0; i < VK_TLD_N; i++) {
      bool sel = (vkey_row == VK_TLD_ROW && vkey_col == i);
      vkey_draw_key(UI_PAD + i * tw, ty, tw - 2, 30, VK_TLD[i], sel, false);
    }
    ky = ty;
  }
  // ghi chu
  set_text_font(0); disp.setTextColor(S60_DIM);
  s60_text(vkey_is_search ? "UP/DOWN/LEFT/RIGHT select, OK=go" : "OK=char, A=cancel, GO=done",
           UI_PAD, ky + 36);
}
// vkey_submit nam sau wifi_result (can nets/pass_in)

// ---------------- font/text helper (wml.cpp khai bao extern "C") ----------------
// Do width bang lc_font (Tahoma VN) de wrap khop voi gui_draw_text khi render.
extern "C" int gfx_textWidth(const char *nul_term, int style) {
  int size = (style == 2) ? 2 : 1;
  return gui_text_width(nul_term, size);
}
static int line_h(int style) {
  if (style == 1) return 22;
  if (style == 2) return 28;   // h1/h2
  if (style == 3) return 22;
  if (style == 4) return 22;
  if (style == 5) return 14;
  if (style == 7) return 82;   // image/thumbnail block
  return 16;
}

// ---------------- app state ----------------
enum Scr { SCR_SPLASH, SCR_HOME, SCR_BROWSE, SCR_INPUT, SCR_MSG,
           SCR_WIFI_LIST, SCR_WIFI_PASS, SCR_WIFI_RESULT };
// SCR_INPUT = hop nhap URL (multi-tap + host keyboard o sim). OPTION = menu ngan.
static Scr scr = SCR_SPLASH;

static Doc *g_doc = nullptr;           // PSRAM: giam ~32KB RAM noi bo
#define doc (*g_doc)
static char *doc_buf = nullptr;        // PSRAM: noi dung sau parse (doc.buf)
static char *net_buf = nullptr;        // PSRAM: body tho tu HTTP (khong de de overlap parse)
static char cur_url[256] = "https://qeafivels.com/";
static char last_host[64] = "";
static int  top = 0, cursor_link = 0;
static int  focus_i = 0;   // Focus Block: khung vien xanh chon khoi noi dung
static char msg[192] = "";

// Bo nho cho 2 stack dieu huong lay tu PSRAM (lazy alloc) — giam ~6KB RAM noi bo.
static char (*back_stack)[256] = nullptr; static int back_sp = 0;
static char (*forward_stack)[256] = nullptr; static int forward_sp = 0;
static bool nav_history_suppressed = false;

static char url_in[192]; static int url_in_n = 0;
static bool wifi_up = false;
// Real-time clock: sync NTP when Internet/WiFi is available, then keep time from system RTC.
// Default timezone is ICT-7 (UTC+7 / Viet Nam), configurable in /Qeafbrowser/config.ini.
static bool clock_ntp_started = false;
static bool clock_synced = false;
static uint32_t clock_last_attempt_ms = 0;
static uint32_t clock_last_draw_sec = 0xFFFFFFFFu;
static char clock_cache[8] = "--:--";
static char menu_src_url[256];
static char menu_src_title[MAX_TITLE];
static bool is_search_box = false;
static bool menu_open = false;
static int  menu_idx = 0;      // 0..6 top-level
static int  menu_sub = 0;      // submenu index
static bool menu_in_sub = false;
// Opera Mini 4: virtual mouse + page overview
static bool mouse_on = false;
static int  mouse_x = 120, mouse_y = 160;
#define MOUSE_CURSOR_VIS_W 13
#define MOUSE_CURSOR_VIS_H 19
static bool overview_on = false;
static int  ov_ox = 0, ov_oy = 0;     // target viewport origin in page lines
static int  overview_zoom = 1;          // target zoom 1..8
// Low-RAM overview animation: fixed-point only, no extra framebuffer/sprite.
#define OV_FP 256
#define OV_ANIM_MS 17
static int32_t ov_visual_fp = 0;         // animated viewport origin (line * OV_FP)
static int32_t ov_zoom_fp = OV_FP;       // animated zoom (zoom * OV_FP)
static uint32_t ov_anim_last = 0;
static bool ov_anim_active = false;
static int32_t ov_cursor_x_fp = 0, ov_cursor_y_fp = 0, ov_cursor_w_fp = 0, ov_cursor_h_fp = 0;
static bool ov_cursor_init = false;
static bool ov_cursor_moving = false;

// Low-RAM pixel scrolling for the main page. Uses the same integer ease-out rhythm as Overview.
#define BODY_FP 256
#define BODY_ANIM_MS 17
static int32_t body_scroll_visual_fp = 0;
static int32_t body_scroll_target_fp = 0;
static int32_t body_scroll_velocity_fp = 0;  // fixed-point px/tick, no framebuffer
static uint32_t body_scroll_last = 0;
static bool body_scroll_active = false;
static bool body_scroll_inertia = false;
#define BODY_VEL_KICK_FP   (BODY_FP * 3 / 2)  // 1.5 px/tick initial impulse
#define BODY_VEL_ACCEL_FP  (BODY_FP / 4)      // 0.25 px/tick^2 while held
#define BODY_VEL_MAX_FP    (BODY_FP * 4)      // gentle feature-phone cap
#define BODY_VEL_STOP_FP   (BODY_FP / 20)
#define BODY_FRICTION_NUM  230                // ~0.90 velocity retention per frame
#define BODY_FRICTION_DEN  256
#define BODY_COAST_MARGIN_PX 8                // small post-release coast, keeps focus visible

static bool loading = false;
static int  load_pct = 0;
static long load_got = 0;       // wire bytes da nhan
static long load_total = -1;    // Content-Length (-1 unknown)
static uint32_t load_paint_ms = 0;
static int  load_paint_pct = -1;
static char load_paint_kb[16] = "";
static bool load_phase_recv = false;

// Format KB/MB giong Opera Mini / UC: 12.4K / 1.2M
static void load_fmt_size(long n, char *out, size_t cap) {
  if (n >= 1048576L)
    snprintf(out, cap, "%ld.%ldM", n / 1048576L, ((n % 1048576L) * 10) / 1048576L);
  else
    snprintf(out, cap, "%ld.%ldK", n / 1024L, ((n % 1024L) * 10) / 1024L);
}

// Thanh softkey S60: nen gradient toi, 1 px ke sang, 2 nhan DAM hai ben.
static void s60_softkey_bar(const char *l, const char *r) {
  int fy = SCR_H - S60_SOFT_H;
  s60_soft_fill(0, fy, SCR_W, S60_SOFT_H);
  disp.fillRect(0, fy, SCR_W, 1, S60_SOFT_LINE);
  int sy = fy + (S60_SOFT_H - 8) / 2;
  set_text_font(0);
  disp.setTextColor(S60_SOFT_TEXT);
  s60_text(l, 6, sy);
  s60_text_right(r, SCR_W - 6, sy);
}

// Footer thay dong gio bang progress + so KB khi dang load (Opera Mini 4 / UC style).
static void load_paint_footer() {
  disp.startWrite();
  s60_softkey_bar("Menu", "Back");
  // vung giua: [progress bar] + KB, kieu S60: track toi + fill xanh sang
  int fy = SCR_H - S60_SOFT_H;
  int sy = fy + (S60_SOFT_H - 8) / 2;
  int cx0 = UI_PAD + 32;
  int cx1 = SCR_W - UI_PAD - 28;
  int kb_w = (int)strlen(load_paint_kb) * 6 + 6;
  int bw = cx1 - cx0 - kb_w;
  if (bw < 32) bw = 32;
  int by = fy + (S60_SOFT_H - 8) / 2 - 1;
  if (by < fy + 2) by = fy + 2;
  disp.fillRect(cx0, by, bw, 8, S60_TRACK);
  disp.drawRect(cx0, by, bw, 8, S60_SOFT_LINE);
  int fw = load_pct * (bw - 2) / 100;
  if (fw > 0) disp.fillRect(cx0 + 1, by + 1, fw, 6, S60_ACCENT);
  disp.setTextColor(S60_SOFT_TEXT);
  s60_text(load_paint_kb, cx0 + bw + 4, sy);
  disp.endWrite();
}

// Callback tu http_get: cap nhat KB + thanh progress (throttle ~50ms).
static void on_http_progress(long got, long total) {
  load_got = got;
  load_total = total;
  int pct;
  if (total > 0) {
    pct = (int)((got * 100) / total);
    if (pct > 99) pct = 99;
  } else {
    // unknown size: toi da 95% theo DOC_CAP, khong bao gio dung yen o 45
    long denom = got + DOC_CAP;
    pct = 45 + (int)((got * 50L) / (denom > 0 ? denom : 1));
    if (pct > 95) pct = 95;
  }
  if (pct < 45) pct = 45;

  char kb[16];
  load_fmt_size(got, kb, sizeof kb);

  uint32_t now = millis();
  if (pct == load_paint_pct && !strcmp(kb, load_paint_kb)) return;
  if (now - load_paint_ms < 50) return;
  load_paint_ms = now;
  load_paint_pct = pct;
  strncpy(load_paint_kb, kb, sizeof load_paint_kb - 1);
  load_paint_kb[sizeof load_paint_kb - 1] = 0;
  load_pct = pct;

  if (!load_phase_recv && got > 0) {
    load_phase_recv = true;
    wifi_rssi_poll();
    g_pane_status = "Receiving...";
    draw_status_pane();
  }
  // thanh progress 3px nam trong application pane
  disp.fillRect(0, S60_PANE_H - 3, SCR_W, 3, S60_TRACK);
  disp.fillRect(0, S60_PANE_H - 3, SCR_W * load_pct / 100, 3, S60_ACCENT);
  load_paint_footer();
}

// Page desktop/large-UI: HTML khong co <meta viewport> -> tu bat chuot ao (Opera Mini desktop mode).
static bool html_looks_desktop(const char *html, size_t len) {
  if (!html || len == 0) return false;
  size_t n = len < 8192 ? len : 8192;
  auto ci_has = [&](const char *needle, size_t nl) -> bool {
    for (size_t i = 0; i + nl <= n; i++) {
      size_t j = 0;
      for (; j < nl; j++)
        if (tolower((unsigned char)html[i + j]) != tolower((unsigned char)needle[j])) break;
      if (j == nl) return true;
    }
    return false;
  };
  if (ci_has("viewport", 8)) return false;
  // can thay HTML that (khong phai WML/text doc lap)
  return ci_has("<html", 5) || ci_has("<!doc", 5) || ci_has("<head", 5);
}

#define THUMB_W 56
#define THUMB_H 42
#define THUMB_CACHE 6
struct ThumbCache {
  bool used;
  bool ok;
  bool persistent_hit;
  uint16_t hits;
  uint32_t last_used;
  char url[192];
  uint16_t *pix;              // allocated in PSRAM when available
};
static ThumbCache thumbs[THUMB_CACHE];
static uint32_t thumb_clock = 1;
#if QB_HAS_TJPEG || QB_HAS_PNGDEC
struct ThumbDecodeCtx { ThumbCache *t; int src_w, src_h, out_w, out_h, dx, dy; bool ok; };
static ThumbDecodeCtx g_thumb_ctx;
#endif
#if QB_HAS_PNGDEC
static PNG *g_png = nullptr;           // PSRAM: giam ~45KB RAM noi bo (ucZLIB 32KB...)
static uint16_t *g_png_line = nullptr;
static int g_png_line_pixels = 0;
#endif
static void build_doc(const char *html);
static void resolve_url(const char *href, char *out, size_t cap);
static ThumbCache *thumb_find(const char *abs_url);
static void thumb_prefetch_doc(int max_images);
static void draw_mini_map(int x, int y, int h);
static void draw_scrollbar(int x, int y, int h, int total, int start, int visible);
static void draw_overview_mosaic(int x, int y, int w, int h);
static int render_line_h(int line);
static int doc_px_before_line(int line);
static int doc_total_px();
static int doc_line_at_px(int px, int *line_top_px);
static void body_scroll_reset(int line);
static void body_scroll_target_line(int line);
static bool body_scroll_tick();
static void overview_anim_reset();
static bool overview_anim_tick();
static void build_docf(const char *fmt, ...);
static void render();
static void ensure_focus_visible();
static void urlin_show();
static void cli_doc();

static bool clock_read_local(char out[8], bool blink_colon) {
  struct tm ti;
#if defined(ARDUINO)
  time_t now = time(nullptr);
  if (now < 1700000000) return false; // not synchronized yet
  localtime_r(&now, &ti);
#else
  // Simulator follows Qeafbrowser default timezone (UTC+7) without changing process TZ.
  time_t now = time(nullptr) + 7 * 3600;
  gmtime_r(&now, &ti);
#endif
  snprintf(out, 8, "%02d%c%02d", ti.tm_hour, blink_colon ? ':' : ' ', ti.tm_min);
  return true;
}

static void clock_start_ntp() {
  if (!wifi_up) return;
  clock_last_attempt_ms = millis();
#if defined(ARDUINO)
  const char *tz = cfg_tz.length() ? cfg_tz.c_str() : "ICT-7";
  configTzTime(tz, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  Serial.printf("[clock] NTP start tz=%s\n", tz);
#else
  Serial.println("[clock] simulator realtime UTC+7");
#endif
  clock_ntp_started = true;
}

static void clock_update_cache() {
  if (wifi_up && !clock_ntp_started) clock_start_ntp();
#if defined(ARDUINO)
  if (wifi_up && clock_ntp_started && !clock_synced && millis() - clock_last_attempt_ms > 30000) {
    clock_ntp_started = false; // retry alternate NTP DNS/network failures
    clock_start_ntp();
  }
#endif
  time_t now_s = time(nullptr);
#if !defined(ARDUINO)
  if (now_s < 1) return;
#endif
  uint32_t sec_key = (uint32_t)now_s;
  if (sec_key == clock_last_draw_sec) return;
  char next[8];
  bool blink = (sec_key & 1u) == 0u;
  if (clock_read_local(next, blink)) {
    clock_synced = true;
    strncpy(clock_cache, next, sizeof clock_cache - 1);
    clock_cache[sizeof clock_cache - 1] = 0;
  }
  clock_last_draw_sec = sec_key;
}

#if defined(ARDUINO)
// Ve lai MOT PHAN cua thanh softkey tren mot LovyanGFX bat ky (panel hoac
// backbuffer PSRAM) sao cho gradient cua ca thanh van lien tuc.
static void s60_soft_slice_g(lgfx::LovyanGFX *g, int x, int y, int w, int h) {
  const int top = SCR_H - S60_SOFT_H;
  for (int i = 0; i < h; i++) {
    int row = y + i - top;
    if (row < 0) row = 0; else if (row > S60_SOFT_H - 1) row = S60_SOFT_H - 1;
    int t = S60_SOFT_H > 1 ? (row * 255) / (S60_SOFT_H - 1) : 0;
    g->fillRect(x, y + i, w, 1, s60_mix565(S60_SOFT_TOP, S60_SOFT_BOT, t));
  }
}
// Chu DAM khong phu thuoc doi tuong ve (panel that hoac backbuffer).
static void s60_text_g(lgfx::LovyanGFX *g, const char *s, int x, int y, int fg) {
  g->setTextColor(fg);
  g->drawString(s, x, y);
  if (s60_bold) g->drawString(s, x + S60_BOLD_PX, y);
}
static void clock_paint(lgfx::LovyanGFX *g, int sy, int cx) {
  g->startWrite();
  s60_soft_slice_g(g, cx - 30, SCR_H - UI_FTR_H, 60, UI_FTR_H);
  g->setTextFont(2);
  s60_text_g(g, clock_cache, cx - g->textWidth(clock_cache) / 2, sy, S60_SOFT_TEXT);
  g->endWrite();
}
static void clock_draw_footer_only() {
  if (scr == SCR_SPLASH) return;
  if (loading) return;   // dang load: giu progress + KB, khong ghi de bang gio
  clock_update_cache();
  int sy = SCR_H - UI_FTR_H + (UI_FTR_H - 8) / 2;
  const int cx = SCR_W / 2;
  // Ve len panel NGAY (khong doi frame push) + len backbuffer de render sau
  // khong ve lai dong gio cu (gio bi nhay lui 1 giay).
  clock_paint(g_panel, sy, cx);
  if (frame_ok) clock_paint(&frame_buf, sy, cx);
}
#else
static void clock_draw_footer_only() {
  if (scr == SCR_SPLASH) return;
  if (loading) return;   // dang load: giu progress + KB
  clock_update_cache();
  int sy = SCR_H - UI_FTR_H + (UI_FTR_H - 8) / 2;
  const int cx = SCR_W / 2;
  disp.startWrite();
  s60_soft_fill(cx - 30, SCR_H - UI_FTR_H, 60, UI_FTR_H);
  set_text_font(0); disp.setTextColor(S60_SOFT_TEXT);
  s60_text_center(clock_cache, cx, sy);
  disp.endWrite();
}
#endif

// ---------------- WiFi wizard (quet + nhap mat kieu dien thoai + luu bo nho tam) ----------------
#define WIFI_MAX 12
struct Net { char ssid[40]; int rssi; bool open; };
static Net nets[WIFI_MAX]; static int net_n = 0;
static int net_cursor = 0;
static char pass_in[65]; static int pass_n = 0;
static bool pass_t9_prev = false;
static char connect_msg[128] = "";
// vkey submit dung cac bien nay (dinh nghia day)
extern String cfg_ssid, cfg_pass;

// multi-tap: nhap nhanh 1 phim nhieu lan de chon ky tu ( nhu dien thoai Nokia cu)
struct KeyGroup { const char *keys[2]; const char *grp[2]; };  // [game|T9][lower|shift]
static const KeyGroup MT[] = {
  {{"menu","1"},      {".?1,!", ".?1,!"}},
  {{"up","2"},        {"abc",   "ABC"}},
  {{"a","3"},         {"def",   "DEF"}},
  {{"left","4"},      {"ghi",   "GHI"}},
  {{"ok","5"},        {"jkl",   "JKL"}},
  {{"right","6"},     {"mno",   "MNO"}},
  {{"option","7"},    {"pqrs",  "PQRS"}},
  {{"down","8"},      {"tuv",   "TUV"}},
  {{"delete","9"},    {"wxyz",  "WXYZ"}},
  {{"mode","0"},      {" 0",    " 0"}},
};
// URL: dau cham/slash/2-cham o menu; ky tu dac biet URL o mode
static const KeyGroup MT_URL[] = {
  {{"menu","1"},      {".:/@?!",  ".:/@?!"}},
  {{"up","2"},        {"abc2",    "ABC2"}},
  {{"a","3"},         {"def3",    "DEF3"}},
  {{"left","4"},      {"ghi4",    "GHI4"}},
  {{"ok","5"},        {"jkl5",    "JKL5"}},
  {{"right","6"},     {"mno6",    "MNO6"}},
  {{"option","7"},    {"pqrs7",   "PQRS7"}},
  {{"down","8"},      {"tuv8",    "TUV8"}},
  {{"delete","9"},    {"wxyz9",   "WXYZ9"}},
  {{"mode","0"},      {"-_&=#%+", "-_&=#%+"}},
};
static int  mt_slot = -1;      // mang MT dang cho commit
static int  mt_pos  = 0;
static uint32_t mt_last = 0;
static bool shift_on = false;
static const KeyGroup *mt_tab = MT;
static int mt_tab_n = (int)(sizeof MT / sizeof MT[0]);
static char *mt_buf = nullptr;
static int  *mt_n = nullptr;
static int   mt_cap = 0;

static void mt_bind(char *buf, int *n, int cap, const KeyGroup *tab, int tab_n) {
  mt_buf = buf; mt_n = n; mt_cap = cap; mt_tab = tab; mt_tab_n = tab_n;
  mt_slot = -1; mt_pos = 0; shift_on = false; mt_last = 0;
}

static void mt_commit() {
  if (mt_slot < 0 || !mt_buf || !mt_n) return;
  const char *g = mt_tab[mt_slot].grp[shift_on ? 1 : 0];
  int len = (int)strlen(g);
  char c = g[mt_pos % len];
  if (*mt_n < mt_cap - 1) mt_buf[(*mt_n)++] = c;
  mt_buf[*mt_n] = 0;
  mt_slot = -1;
}
static void mt_key(int slot) {
  uint32_t now = millis();
  if (mt_slot == slot && now - mt_last < 900) {
    mt_pos++;                    // lan cuoc tiep trong cung nhom
  } else {
    mt_commit();
    mt_slot = slot; mt_pos = 0;
  }
  mt_last = now;
}
static int mt_find(const char *k) {
  for (int i = 0; i < mt_tab_n; i++)
    if (!strcmp(k, mt_tab[i].keys[0]) || !strcmp(k, mt_tab[i].keys[1])) return i;
  return -1;
}

static void wifi_scan() {
  Serial.println("[wifi] scan real networks (WiFi.scanNetworks)...");
  WiFi.mode(WIFI_STA);
  // Chi ngat ket noi khi CHUA ket noi — refresh danh sach khong duoc mat WiFi dang dung.
  if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();
  delay(150);
  net_n = WiFi.scanNetworks(false, true);
  if (net_n < 0) net_n = 0;
  if (net_n > WIFI_MAX) net_n = WIFI_MAX;
  for (int i = 0; i < net_n; i++) {
    strncpy(nets[i].ssid, WiFi.SSID(i).c_str(), sizeof nets[i].ssid - 1);
    nets[i].ssid[sizeof nets[i].ssid - 1] = 0;
    nets[i].rssi = WiFi.RSSI(i);
    nets[i].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
  }
  WiFi.scanDelete();
  for (int i = 0; i < net_n; i++)
    for (int j = i + 1; j < net_n; j++)
      if (nets[j].rssi > nets[i].rssi) { Net t = nets[i]; nets[i] = nets[j]; nets[j] = t; }
  net_cursor = 0;
  Serial.printf("[wifi] found %d real networks\n", net_n);
}

// Buffer WML tam dung chung cho History/Bookmark/WiFi list (PSRAM, giam ~14KB noi bo).
static char *page_scratch_buf = nullptr;
static char *page_scratch() {
  if (!page_scratch_buf) page_scratch_buf = (char *)heap_caps_malloc(6000, MALLOC_CAP_SPIRAM);
  if (!page_scratch_buf) page_scratch_buf = (char *)malloc(6000);
  return page_scratch_buf;
}

// Focus Block nhu trang chinh: dua vien xanh den dong cua mang dang chon (net_cursor).
static void wifi_list_focus() {
  focus_i = 0;
  if (net_cursor >= 0 && net_cursor < doc.nlinks)
    focus_i = doc.links[net_cursor].line0;
  while (focus_i < doc.nlines && doc.lines[focus_i].n == 0) focus_i++;
  if (focus_i >= doc.nlines) focus_i = 0;
  cursor_link = (focus_i < doc.nlines) ? doc.lines[focus_i].link : -1;
  ensure_focus_visible();
}

static void wifi_list_show() {
  char *buf = page_scratch();
  if (!buf) {
    build_doc("<wml><card title=\"WiFi\"><p>Not enough RAM.</p></card></wml>");
    scr = SCR_WIFI_LIST; top = 0; focus_i = 0; cursor_link = -1;
    return;
  }
  int o = snprintf(buf, 6000,
    "<wml><card title=\"WiFi\"><p>Select a network:<br/></p><p>");
  for (int i = 0; i < net_n; i++)
    o += snprintf(buf + o, 6000 - o, "<a href=\"mtt:wifisel#%d\">%s %s</a><br/>",
                  i, nets[i].ssid, nets[i].open ? "(open)" : "");
  if (!net_n) o += snprintf(buf + o, 6000 - o, "(no networks — OPTION to rescan)");
  snprintf(buf + o, 6000 - o, "</p></card></wml>");
  build_doc(buf);
  scr = SCR_WIFI_LIST; top = 0;
  wifi_list_focus();
}

static bool wifi_try_connect(const char *ssid, const char *pass) {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass[0] ? pass : NULL);
  uint32_t t0 = millis();
  int last_sec = -1;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
    int st = WiFi.status();
    // That bai ro rang (sai mat khau / khong thay mang) -> ket luan som, khong choi 12s.
    if ((st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) && millis() - t0 >= 3000) break;
    int sec = (int)((millis() - t0) / 1000);
    if (sec != last_sec) {              // chi ve lai khi doi giay -> khong nhap nhay
      last_sec = sec;
      snprintf(connect_msg, sizeof connect_msg, "Connecting to %s...%ds", ssid, sec);
      build_docf("<wml><card title=\"WiFi\"><p>%s</p></card></wml>", connect_msg);
      render();
    }
    delay(200);
  }
  bool ok = (WiFi.status() == WL_CONNECTED);
  if (!ok) { WiFi.disconnect(); wifi_up = false; }
  return ok;
}

static void wifi_pass_show() {
  char buf[256];
  snprintf(buf, sizeof buf,
    "<wml><card title=\"WiFi\"><p>Network: %s%s</p></card></wml>",
    nets[net_cursor].ssid, nets[net_cursor].open ? " (open)" : "");
  build_doc(buf);
  scr = SCR_WIFI_PASS; top = 0; cursor_link = 0;
  vkey_bind(pass_in, &pass_n, (int)sizeof pass_in, true, false);
}

static void wifi_result(bool ok) {
  if (ok) {
    cfg_ssid = nets[net_cursor].ssid;
    cfg_pass = pass_in;
    config_save();                       // luu bo nho tam -> SD
    wifi_up = true;
    clock_start_ntp();
    build_docf("<wml><card title=\"WiFi\"><p>CONNECTED<br/>%s<br/>Pass: %s<br/>IP %s<br/>"
               "Saved to /Qeafbrowser/config.ini</p></card></wml>",
               cfg_ssid.c_str(), cfg_pass.c_str(), WiFi.localIP().toString().c_str());
  } else {
    wifi_up = false;
    build_docf("<wml><card title=\"WiFi\"><p>FAILED<br/>%s<br/>"
               "Check password.<br/>OK = retry, A = back to list</p></card></wml>",
               nets[net_cursor].ssid);
  }
  scr = SCR_WIFI_RESULT; top = 0; cursor_link = 0;
}

static void open_user_url(const char *raw) {
  char u[256];
  if (url_normalize_input(raw, u, sizeof u)) go_url(u);
  else go_url("mtt:start");
}

static void open_search_query(const char *raw) {
  if (!raw || !raw[0]) { go_url("mtt:start"); return; }
  char enc[192], u[256];
  url_encode_query(raw, enc, sizeof enc);
  snprintf(u, sizeof u, "https://www.google.com/search?q=%s&hl=en&gl=us", enc);
  go_url(u);
}

static void vkey_submit() {
  vkey_open = false;
  if (vkey_is_search) {
    if (vkey_buf && vkey_buf[0]) {
      is_search_box = false;
      open_search_query(vkey_buf);
    } else go_url("mtt:start");
    return;
  }
  if (vkey_is_pass) {
    if (net_cursor >= 0 && net_cursor < net_n) {
      Serial.printf("[wifi] try connect '%s' pass='%s'\n", nets[net_cursor].ssid, pass_in);
      wifi_result(wifi_try_connect(nets[net_cursor].ssid, nets[net_cursor].open ? "" : pass_in));
      render();
    }
    return;
  }
  // GO khi chi con prefill https:// -> ve trang chu (khong mo host rong)
  if (vkey_buf && vkey_buf[0] &&
      strcmp(vkey_buf, "https://") && strcmp(vkey_buf, "http://"))
    open_user_url(vkey_buf);
  else go_url("mtt:start");
}

// ---------------- keypad (pattern E524546-OS/src/main.cpp) ----------------
struct KeyDef { int pin; const char *game; const char *t9; };
static const KeyDef KEYS[] = {
  { KEY_MENU,   "menu",   "1" }, { KEY_UP,    "up",     "2" }, { KEY_A,     "back",   "3" },
  { KEY_LEFT,   "left",   "4" }, { KEY_START, "ok",     "5" }, { KEY_RIGHT, "right",  "6" },
  { KEY_OPTION, "option", "7" }, { KEY_DOWN,  "down",   "8" }, { KEY_B,     "delete", "9" },
  { KEY_SELECT, "mode",   "0" },
};
static const int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);
static bool last_state[NKEYS]; static unsigned long last_change[NKEYS], press_start[NKEYS];
static bool t9mode = false;

static void (*key_cb)(const char *k) = nullptr;

// ---- auto-repeat: giu phim cuon (up/down) de list di chuyen lien tuc ----
// Chi ap dung cho launcher (browser cu giu hanh vi edge-triggered nhu cu).
static unsigned long repeat_next[NKEYS];   // moc millis cho lan lap ke tiep

static void keys_init() {
  for (int i = 0; i < NKEYS; i++) {
    pinMode(KEYS[i].pin, INPUT_PULLUP);
    last_state[i] = HIGH; last_change[i] = 0; press_start[i] = 0;
    repeat_next[i] = 0;
  }
}
static void keys_poll() {
  unsigned long now = millis();
  for (int i = 0; i < NKEYS; i++) {
    bool v = digitalRead(KEYS[i].pin);
    if (v == last_state[i]) {
      // giu phim: auto-repeat cho launcher (up/down hoat dong nhieu nhat)
      if (v == LOW && launcher_active() && key_cb &&
          (KEYS[i].pin == KEY_UP || KEYS[i].pin == KEY_DOWN)) {
        if (repeat_next[i] && now >= repeat_next[i]) {
          repeat_next[i] = now + LC_REPEAT_RATE_MS;
          key_cb(t9mode ? KEYS[i].t9 : KEYS[i].game);
        }
      }
      continue;
    }
    if (now - last_change[i] <= 25) continue;
    last_state[i] = v; last_change[i] = now;
    bool is_select = (KEYS[i].pin == KEY_SELECT);
    if (v == LOW) {
      press_start[i] = now;
      repeat_next[i] = now + LC_REPEAT_DELAY_MS;
      if (!is_select && key_cb) key_cb(t9mode ? KEYS[i].t9 : KEYS[i].game);
    } else {
      repeat_next[i] = 0;
      if (is_select) {              // SELECT: phat khi nha (can do thoi gian giu)
        if (now - press_start[i] >= 600) { // giu >600ms: dao che do + phat "mode"
          t9mode = !t9mode;
          Serial.printf("[key] mode: %s\n", t9mode ? "T9" : "GAME");
          if (key_cb) key_cb("mode");
        } else {                           // nhan ngan: "0" (T9) hoac "mode" (Game)
          if (key_cb) key_cb(t9mode ? "0" : "mode");
        }
      }
    }
  }
}

// ---------------- mtt: pages — Opera Mini 4 Java style ----------------
// Speed Dial 3x3 + Search + Bookmarks
static const char *PAGE_START =
  "<wml><card title=\"Qeafbrowser\">"
  "<p><input><a href=\"#\">Enter URL</a></input></p>"
  "<p><input><a href=\"mtt:search\">Google Search</a></input></p>"
  "<p><a href=\"mtt:bookmark\">Bookmarks</a>|<a href=\"mtt:history\">History</a>"
  "|<a href=\"mtt:config\">Settings</a>|<a href=\"mtt:about\">About</a>"
  "|<a href=\"mtt:help\">Help</a></p>"
  "<p><folder><a href=\"https://qeafivels.com/\">Qeafivels</a></folder></p>"
  "<p><folder><a href=\"https://www.google.com/\">Google</a></folder></p>"
  "<p><folder><a href=\"https://m.facebook.com/\">Facebook</a></folder></p>"
  "<p><folder><a href=\"https://en.m.wikipedia.org/\">Wikipedia</a></folder></p>"
  "<p><folder><a href=\"https://simple.wikipedia.org/\">Simple Wiki</a></folder></p>"
  "<p><folder><a href=\"https://m.youtube.com/\">YouTube</a></folder></p>"
  "<p><folder><a href=\"http://m.weather.com/\">Weather</a></folder></p>"
  "<p><folder><a href=\"mtt:feeds\">News RSS</a></folder></p>"
  "<p><folder><a href=\"mtt:web\">More Sites</a></folder></p>"
  "</card></wml>";

static const char *PAGE_ABOUT =
  "<wml><card title=\"About\">"
  "<p>Qeafbrowser 2.1<br/>Opera Mini 4 style mode<br/>"
  "HTTP + HTTPS / HTML + WML + RSS<br/>"
  "Text-only / WAP / XHTML-MP ready<br/><br/>"
  "Default home: https://qeafivels.com/<br/>ESP32-S3 port.</p></card></wml>";

static const char *PAGE_HELP =
  "<wml><card title=\"Help\"><p>UP/DOWN: scroll page<br/>LEFT/RIGHT: prev/next link<br/>"
  "OK: open link<br/>BACK: back<br/>MENU: home<br/>OPTION: menu<br/>"
  "(Bmrk/Optn/Navg/Tool/Sett/Help/Exit)<br/>"
  "Hold SELECT: switch T9<br/><br/>"
  "Enter URL: Tool &gt; Input URL<br/>domains auto-HTTPS<br/>Tools &gt; Forward to go forward<br/>"
  "multi-tap or PC keyboard (sim)<br/>"
  "WiFi: Sett &gt; WiFi<br/>"
  "Text mode: Sett &gt; Text (no images)<br/>"
  "RSS/Atom feeds: Home &gt; News RSS</p></card></wml>";

static const char *PAGE_CONFIG =
  "<wml><card title=\"Settings\"><p>WiFi: %s<br/>SSID: %s<br/>Pass: %s<br/>Home: %s<br/>Timezone: %s<br/>Text mode: %s<br/><br/>"
  "<a href=\"mtt:wifi\">&gt;&gt; Connect WiFi (scan)</a><br/>"
  "<a href=\"mtt:textmode\">&gt;&gt; Toggle text mode (images on/off)</a><br/>"
  "<a href=\"mtt:wifi\">&gt;&gt; Change WiFi password</a><br/><br/>"
  "Text mode = hide images, faster text-only.<br/>"
  "Auto-reconnects to saved network.<br/>"
  "Edit later: /Qeafbrowser/config.ini</p></card></wml>";

static const char *PAGE_WEB =
  "<wml><card title=\"Qeafbrowser Web\"><p>"
  "<a href=\"https://qeafivels.com/\">Qeafivels</a><br/>"
  "<a href=\"http://pokoyo.wapka.mobi/\">PokoyoWap</a><br/>"
  "<a href=\"http://google.com/\">Google</a><br/>"
  "<a href=\"http://wap.yahoo.com/\">Yahoo!</a><br/>"
  "<a href=\"http://tubidy.mobi/\">Tubidy</a><br/>"
  "<a href=\"http://wap.c2.hu/\">C2 Mail</a><br/>"
  "<a href=\"https://m.wikipedia.org/\">Wikipedia Mobile</a><br/>"
  "<a href=\"https://simple.wikipedia.org/\">Simple Wikipedia</a><br/>"
  "<a href=\"https://en.wikipedia.org/wiki/Special:Random\">Wiki Random</a><br/>"
  "<a href=\"mtt:feeds\">News RSS feeds</a><br/>"
  "<a href=\"https://qeafivels.com/\">Qeafivels Home</a></p></card></wml>";

static const char *PAGE_SITES =
  "<wml><card title=\"Mobile Sites\">"
  "<p><a href=\"http://m.google.com/\">Google Mobile</a><br/>"
  "<a href=\"http://m.facebook.com/\">Facebook</a><br/>"
  "<a href=\"http://m.youtube.com/\">YouTube</a><br/>"
  "<a href=\"http://en.m.wikipedia.org/\">Wikipedia</a><br/>"
  "<a href=\"https://m.wikipedia.org/\">Wikipedia Mobile</a><br/>"
  "<a href=\"https://simple.wikipedia.org/\">Simple Wikipedia</a></p></card></wml>";

static const char *PAGE_FEEDS =
  "<wml><card title=\"News RSS\"><p>"
  "<a href=\"https://feeds.bbci.co.uk/news/rss.xml\">BBC News</a><br/>"
  "<a href=\"https://rss.cnn.com/rss/edition.rss\">CNN World</a><br/>"
  "<a href=\"https://www.reddit.com/.rss\">Reddit Front</a><br/>"
  "<a href=\"https://hn.algolia.com/rss\">Hacker News</a><br/>"
  "<a href=\"https://simple.wikipedia.org/wiki/Special:Export\">Simple Wiki XML</a><br/>"
  "<a href=\"mtt:start\">Back to Speed Dial</a></p></card></wml>";

static const char *PAGE_FB =
  "<wml><card title=\"Facebook\"><p>"
  "<a href=\"http://m.facebook.com/\">Facebook Mobile</a><br/>"
  "<a href=\"mtt:start\">Back</a></p></card></wml>";

static const char *PAGE_TOOLS =
  "<wml><card title=\"Tools\"><p>"
  "<a href=\"#\">Input URL</a><br/>"
  "<a href=\"mtt:search\">Search</a><br/>"
  "<a href=\"mtt:menu#refresh\">Refresh</a><br/>"
  "<a href=\"mtt:forward\">Forward</a><br/>"
  "<a href=\"mtt:history\">History</a><br/>"
  "<a href=\"mtt:bookmark\">Bookmark</a><br/>"
  "<a href=\"mtt:feeds\">News RSS</a></p></card></wml>";

static const char *PAGE_DL =
  "<wml><card title=\"Downloads\"><p>No downloads.<br/>"
  "Visited pages in <a href=\"mtt:history\">History</a></p></card></wml>";

static const char *PAGE_ERROR =
  "<wml><card title=\"Connection Timeout\"><p>%s<br/><br/>"
  "<a href=\"#\">Refresh</a><br/><a href=\"mtt:start\">Back</a></p></card></wml>";

static bool thumb_ensure_pixels(ThumbCache *t) {
  if (!t) return false;
  if (t->pix) return true;
  t->pix = (uint16_t*)heap_caps_malloc(THUMB_W * THUMB_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  if (!t->pix) t->pix = (uint16_t*)malloc(THUMB_W * THUMB_H * sizeof(uint16_t));
  if (t->pix) {
    memset(t->pix, 0xFF, THUMB_W * THUMB_H * sizeof(uint16_t));
    return true;
  }
  return false;
}

static void thumb_commit_persistent(ThumbCache *t) {
  if (t && t->ok && t->pix && t->url[0] && !t->persistent_hit)
    thumb_store_save(t->url, t->pix, THUMB_W * THUMB_H);
}

#if QB_HAS_TJPEG
static bool jpg_thumb_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  ThumbDecodeCtx &c = g_thumb_ctx;
  if (!c.t) return false;
  for (uint16_t yy = 0; yy < h; yy++) {
    int dy = c.dy + y + yy;
    if (dy < 0 || dy >= THUMB_H) continue;
    for (uint16_t xx = 0; xx < w; xx++) {
      int dx = c.dx + x + xx;
      if (dx < 0 || dx >= THUMB_W) continue;
      c.t->pix[dy * THUMB_W + dx] = bitmap[yy * w + xx];
    }
  }
  c.ok = true;
  return true;
}
static bool decode_thumb_jpeg_ram(ThumbCache *t, const uint8_t *buf, size_t len) {
  uint16_t sw = 0, sh = 0;
  JRESULT jr = TJpgDec.getJpgSize(&sw, &sh, buf, len);
  if (jr != JDR_OK || !sw || !sh) {
    Serial.printf("[thumb] jpeg size rc=%d %ux%u\n", (int)jr, (unsigned)sw, (unsigned)sh);
    return false;
  }
  uint8_t scale = 1;
  while (scale < 8 && ((sw / scale) > THUMB_W || (sh / scale) > THUMB_H)) scale <<= 1;
  int out_w = sw / scale; if (out_w < 1) out_w = 1;
  int out_h = sh / scale; if (out_h < 1) out_h = 1;
  memset(t->pix, 0xFF, THUMB_W * THUMB_H * sizeof(uint16_t));
  g_thumb_ctx = { t, (int)sw, (int)sh, out_w, out_h, (THUMB_W - out_w) / 2, (THUMB_H - out_h) / 2, false };
  TJpgDec.setCallback(jpg_thumb_output);
  TJpgDec.setJpgScale(scale);
  if (len > 0xFFFFFFFFu) return false;
  jr = TJpgDec.drawJpg(0, 0, buf, (uint32_t)len);
  t->ok = (jr == JDR_OK) && g_thumb_ctx.ok;
  if (!t->ok) Serial.printf("[thumb] jpeg draw rc=%d\n", (int)jr);
  return t->ok;
}
#endif

#if QB_HAS_PNGDEC
// PNGdec giu truc tiep ucZLIB[32KB] trong doi tuong -> dat doi tuong vao PSRAM
// de tra lai ~45KB RAM noi bo cho WiFi/TLS. Cap lan dau, dung lai ve sau.
static PNG *png_obj() {
  if (!g_png) {
    void *p = heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM);
    if (!p) p = malloc(sizeof(PNG));
    if (p) g_png = new (p) PNG;
  }
  return g_png;
}
// PNGdec 1.1.6 defines PNG_DRAW_CALLBACK as int(PNGDRAW*).
// Keep the callback signature exact because older examples on the web often use void.
// Return 1 to continue decoding the next scanline.
static uint16_t *png_line_buffer(int width) {
  if (width < 1) return nullptr;
  if (g_png_line && g_png_line_pixels >= width) return g_png_line;
  if (g_png_line) {
    free(g_png_line);
    g_png_line = nullptr;
    g_png_line_pixels = 0;
  }
  void *p = heap_caps_malloc((size_t)width * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  if (!p) p = malloc((size_t)width * sizeof(uint16_t));
  if (p) {
    g_png_line = (uint16_t*)p;
    g_png_line_pixels = width;
  }
  return g_png_line;
}

static int png_thumb_draw(PNGDRAW *pDraw) {
  ThumbDecodeCtx &c = g_thumb_ctx;
  if (!g_png || !pDraw || !c.t || c.src_w < 1 || c.src_h < 1 ||
      c.src_w > g_png_line_pixels) return 1;
  g_png->getLineAsRGB565(pDraw, g_png_line, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);
  int dy = c.dy + (pDraw->y * c.out_h) / c.src_h;
  if (dy < 0 || dy >= THUMB_H) return 1;
  for (int dx = 0; dx < c.out_w; dx++) {
    int sx = (dx * c.src_w) / c.out_w;
    int tx = c.dx + dx;
    if (tx >= 0 && tx < THUMB_W) c.t->pix[dy * THUMB_W + tx] = g_png_line[sx];
  }
  c.ok = true;
  return 1;
}
static bool decode_thumb_png_ram(ThumbCache *t, const uint8_t *buf, size_t len) {
  if (!t || !t->pix || !buf || len == 0 || len > 0x7fffffffUL) return false;
  PNG *png = png_obj();
  if (!png) return false;
  PNG_DRAW_CALLBACK *draw_cb = png_thumb_draw;  // compile-time signature check against PNGdec.h
  int rc = png->openRAM((uint8_t*)buf, (int)len, draw_cb);
  if (rc != PNG_SUCCESS) return false;
  int sw = png->getWidth(), sh = png->getHeight();
  if (sw < 1 || sh < 1 || !png_line_buffer(sw)) { png->close(); return false; }
  int out_w = THUMB_W, out_h = (sh * THUMB_W) / sw;
  if (out_h > THUMB_H) { out_h = THUMB_H; out_w = (sw * THUMB_H) / sh; }
  if (out_w < 1) out_w = 1;
  if (out_h < 1) out_h = 1;
  memset(t->pix, 0xFF, THUMB_W * THUMB_H * sizeof(uint16_t));
  g_thumb_ctx = { t, sw, sh, out_w, out_h, (THUMB_W - out_w) / 2, (THUMB_H - out_h) / 2, false };
  rc = png->decode(nullptr, 0);
  png->close();
  t->ok = (rc == PNG_SUCCESS) && g_thumb_ctx.ok;
  return t->ok;
}
#endif

static ThumbCache *thumb_find(const char *abs_url) {
  if (!abs_url || !abs_url[0]) return nullptr;
  for (int i = 0; i < THUMB_CACHE; i++) {
    if (thumbs[i].used && !strcmp(thumbs[i].url, abs_url)) {
      thumbs[i].last_used = thumb_clock++;
      if (thumbs[i].hits < 0xFFFF) thumbs[i].hits++;
      Serial.printf("[thumb] PSRAM hit slot=%d hits=%u %s\n", i, (unsigned)thumbs[i].hits, abs_url);
      return &thumbs[i];
    }
  }

  int slot = -1;
  for (int i = 0; i < THUMB_CACHE; i++) if (!thumbs[i].used) { slot = i; break; }
  if (slot < 0) {
    uint32_t oldest = 0xFFFFFFFFu;
    for (int i = 0; i < THUMB_CACHE; i++) {
      if (thumbs[i].last_used < oldest) { oldest = thumbs[i].last_used; slot = i; }
    }
    Serial.printf("[thumb] LRU evict slot=%d old=%s\n", slot, slot >= 0 ? thumbs[slot].url : "?");
  }
  ThumbCache *t = &thumbs[slot];
  uint16_t *keep = t->pix;
  memset(t, 0, sizeof *t);
  t->pix = keep;
  if (!thumb_ensure_pixels(t)) return nullptr;
  t->used = true;
  t->hits = 1;
  t->last_used = thumb_clock++;
  strncpy(t->url, abs_url, sizeof t->url - 1);
  t->url[sizeof t->url - 1] = 0;

  // Tier 2: persistent raw RGB565 thumbnail in LittleFS/selected FS.
  if (thumb_store_load(abs_url, t->pix, THUMB_W * THUMB_H)) {
    t->ok = true;
    t->persistent_hit = true;
    return t;
  }

  auto ends_with = [](const char *a, const char *b) { size_t la = strlen(a), lb = strlen(b); return la >= lb && !strcasecmp(a + la - lb, b); };
#if !defined(ARDUINO) && defined(__linux__)
  static unsigned char ibuf[32768];
  HttpMeta meta; memset(&meta, 0, sizeof meta); size_t len = 0;
  auto load_local = [&](const char *path) -> bool {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    len = fread(ibuf, 1, sizeof ibuf, f);
    fclose(f);
    return len > 0;
  };
  bool got = false;
  if (strstr(abs_url, "hero-banner.jpg")) { strcpy(meta.content_type, "image/jpeg"); got = load_local("sim/assets/hero-banner.jpg"); }
  else if (strstr(abs_url, "products-audio.png")) { strcpy(meta.content_type, "image/png"); got = load_local("sim/assets/products-audio.png"); }
  else got = http_get(abs_url, (char*)ibuf, sizeof ibuf, &len, &meta);
  if (!got || !len) { t->ok = false; return t; }

  auto blit_rgb = [&](const unsigned char *src, int w, int h, int comp) {
    for (int y = 0; y < THUMB_H; y++) {
      int sy = y * h / THUMB_H;
      for (int x = 0; x < THUMB_W; x++) {
        int sx = x * w / THUMB_W;
        const unsigned char *px = src + (sy * w + sx) * comp;
        t->pix[y * THUMB_W + x] = disp.color565(px[0], px[1], px[2]);
      }
    }
    t->ok = true;
  };

  if ((len > 8 && ibuf[0] == 0x89 && ibuf[1] == 'P' && ibuf[2] == 'N' && ibuf[3] == 'G') || strstr(meta.content_type, "png") || ends_with(abs_url, ".png")) {
    png_image img; memset(&img, 0, sizeof img); img.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&img, ibuf, len)) {
      img.format = PNG_FORMAT_RGB;
      png_bytep buf = (png_bytep)malloc(PNG_IMAGE_SIZE(img));
      if (buf && png_image_finish_read(&img, nullptr, buf, 0, nullptr)) blit_rgb(buf, img.width, img.height, 3);
      if (buf) free(buf);
      png_image_free(&img);
      thumb_commit_persistent(t);
      return t;
    }
  }

  if ((len > 2 && ibuf[0] == 0xFF && ibuf[1] == 0xD8) || strstr(meta.content_type, "jpeg") || strstr(meta.content_type, "jpg") || ends_with(abs_url, ".jpg") || ends_with(abs_url, ".jpeg")) {
    jpeg_decompress_struct cinfo; jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, ibuf, len);
    if (jpeg_read_header(&cinfo, TRUE) == 1) {
      jpeg_start_decompress(&cinfo);
      int w = cinfo.output_width, h = cinfo.output_height, comp = cinfo.output_components;
      unsigned long stride = w * comp;
      unsigned char *buf = (unsigned char*)malloc((size_t)h * stride);
      if (buf) {
        while (cinfo.output_scanline < cinfo.output_height) {
          unsigned char *row = buf + cinfo.output_scanline * stride;
          jpeg_read_scanlines(&cinfo, &row, 1);
        }
        blit_rgb(buf, w, h, comp);
        free(buf);
      }
      jpeg_finish_decompress(&cinfo);
    }
    jpeg_destroy_decompress(&cinfo);
    thumb_commit_persistent(t);
    return t;
  }
#elif defined(ARDUINO)
  static uint8_t *ibuf = nullptr;
  const size_t IBUF_CAP = 128 * 1024;
  if (!ibuf) ibuf = (uint8_t*)heap_caps_malloc(IBUF_CAP, MALLOC_CAP_SPIRAM);
  if (!ibuf) ibuf = (uint8_t*)malloc(IBUF_CAP);
  if (!ibuf) { t->ok = false; return t; }
  HttpMeta meta; memset(&meta, 0, sizeof meta); size_t len = 0;
  bool got = http_get(abs_url, (char*)ibuf, IBUF_CAP, &len, &meta);
  if (!got || !len || meta.status < 200 || meta.status >= 300 || meta.gzipped || meta.truncated) {
    Serial.printf("[thumb] fetch fail status=%d len=%u type=%s trunc=%d gz=%d\n",
                  meta.status, (unsigned)len, meta.content_type,
                  meta.truncated ? 1 : 0, meta.gzipped ? 1 : 0);
    t->ok = false;
    return t;
  }
  bool is_png = (len > 8 && ibuf[0] == 0x89 && ibuf[1] == 'P' && ibuf[2] == 'N' && ibuf[3] == 'G') || strstr(meta.content_type, "png") || ends_with(abs_url, ".png");
  bool is_jpg = (len > 2 && ibuf[0] == 0xFF && ibuf[1] == 0xD8) || strstr(meta.content_type, "jpeg") || strstr(meta.content_type, "jpg") || ends_with(abs_url, ".jpg") || ends_with(abs_url, ".jpeg");
  #if QB_HAS_PNGDEC
  if (is_png && decode_thumb_png_ram(t, ibuf, len)) { thumb_commit_persistent(t); return t; }
  #endif
  #if QB_HAS_TJPEG
  if (is_jpg && decode_thumb_jpeg_ram(t, ibuf, len)) { thumb_commit_persistent(t); return t; }
  #endif
  if (!is_png && !is_jpg) Serial.printf("[thumb] unsupported type=%s\n", meta.content_type);
#endif
  t->ok = false;
  return t;
}

static void thumb_prefetch_doc(int max_images) {
  if (max_images <= 0 || doc.nimages <= 0) return;
  int loaded = 0;
  for (int i = 0; i < doc.nimages && loaded < max_images; i++) {
    if (!doc.images[i].url[0]) continue;
    char absu[256]; resolve_url(doc.images[i].url, absu, sizeof absu);
    if (!absu[0]) continue;
    ThumbCache *t = thumb_find(absu);
    if (t && t->ok) loaded++;
  }
  Serial.printf("[thumb] prefetch=%d/%d image(s)\n", loaded, doc.nimages);
}

static int32_t ov_ease_step(int32_t cur, int32_t target) {
  int32_t d = target - cur;
  if (!d) return cur;
  // ~120-160 ms ease-out at ~60 FPS, integer only.
  int32_t step = d / 3;
  if (!step) step = d > 0 ? 1 : -1;
  return cur + step;
}

static void overview_anim_reset() {
  ov_visual_fp = (int32_t)ov_oy * OV_FP;
  ov_zoom_fp = (int32_t)overview_zoom * OV_FP;
  ov_anim_last = millis();
  ov_anim_active = false;
  ov_cursor_init = false;
  ov_cursor_moving = false;
}

static bool overview_anim_tick() {
  if (!overview_on) return false;
  uint32_t now = millis();
  if (now - ov_anim_last < OV_ANIM_MS) return false;
  ov_anim_last = now;
  int32_t ty = (int32_t)ov_oy * OV_FP;
  int32_t tz = (int32_t)overview_zoom * OV_FP;
  int32_t oldy = ov_visual_fp, oldz = ov_zoom_fp;
  ov_visual_fp = ov_ease_step(ov_visual_fp, ty);
  ov_zoom_fp = ov_ease_step(ov_zoom_fp, tz);
  if (abs(ty - ov_visual_fp) <= 2) ov_visual_fp = ty;
  if (abs(tz - ov_zoom_fp) <= 2) ov_zoom_fp = tz;
  bool changed = oldy != ov_visual_fp || oldz != ov_zoom_fp;
  bool moving = ov_visual_fp != ty || ov_zoom_fp != tz;
  ov_anim_active = moving || ov_cursor_moving;
  return changed || moving || ov_cursor_moving;
}

static void draw_mini_map(int x, int y, int h) {
  int n = doc.nlines > 0 ? doc.nlines : 1;
  int w = 10;
  disp.fillRect(x, y, w, h, S60_SCROLL_BG);
  disp.drawRect(x, y, w, h, S60_FIELD_LINE);
  for (int i = 0; i < n; i++) {
    int yy = y + 1 + (i * (h - 2)) / n;
    uint16_t c = (doc.lines[i].style == 7) ? UI_BORDER : (doc.lines[i].link >= 0 ? UI_LINK : UI_DIM);
    int lw = (doc.lines[i].style == 7) ? (w - 4) : ((w - 4) * (8 + (doc.lines[i].n % 12))) / 20;
    if (lw < 2) lw = 2; if (lw > w - 4) lw = w - 4;
    disp.fillRect(x + 2, yy, lw, 1, c);
  }
  int vy = y + 1, vh = h - 2;
  if (overview_on) {
    int start_line = (int)(ov_visual_fp / OV_FP);
    int32_t zfp = ov_zoom_fp < OV_FP ? OV_FP : ov_zoom_fp;
    int visible = (int)(((int64_t)n * OV_FP + zfp - 1) / zfp);
    if (visible < 1) visible = 1;
    if (start_line < 0) start_line = 0;
    if (start_line > n - visible) start_line = n - visible;
    if (start_line < 0) start_line = 0;
    vy = y + 1 + (start_line * (h - 2)) / n;
    vh = (visible * (h - 2)) / n;
  } else {
    int total_px = doc_total_px();
    if (total_px < 1) total_px = 1;
    int start_px = (int)(body_scroll_visual_fp / BODY_FP);
    int view_px = SCR_H - UI_ROWS_Y - UI_FTR_H - 4;
    if (view_px > total_px) view_px = total_px;
    if (start_px < 0) start_px = 0;
    if (start_px > total_px - view_px) start_px = total_px - view_px;
    if (start_px < 0) start_px = 0;
    vy = y + 1 + (start_px * (h - 2)) / total_px;
    vh = (view_px * (h - 2)) / total_px;
  }
  if (vh < 5) vh = 5;
  if (vy + vh > y + h - 1) vh = y + h - 1 - vy;
  disp.drawRect(x + 1, vy, w - 2, vh, S60_ACCENT);
}

static void draw_scrollbar(int x, int y, int h, int total, int start, int visible) {
  if (total < 1) total = 1;
  if (visible < 1) visible = 1;
  if (visible > total) visible = total;
  disp.fillRect(x, y, 6, h, S60_SCROLL_BG);
  disp.drawRect(x, y, 6, h, S60_FIELD_LINE);
  // Small up/down caps like feature-phone browsers.
  disp.drawPixel(x + 2, y + 3, UI_DIM); disp.drawPixel(x + 3, y + 3, UI_DIM);
  disp.drawPixel(x + 2, y + h - 4, UI_DIM); disp.drawPixel(x + 3, y + h - 4, UI_DIM);
  int ty = y + 8, th = h - 16;
  if (th < 8) { ty = y + 1; th = h - 2; }
  int grip = (visible * th) / total;
  if (grip < 8) grip = 8; if (grip > th) grip = th;
  int max_start = total - visible; if (max_start < 0) max_start = 0;
  int target_fp = max_start ? (start * 1024) / max_start : 0;
  static int smooth_fp = 0, last_total = -1;
  if (last_total != total) { smooth_fp = target_fp; last_total = total; }
  else {
    int d = target_fp - smooth_fp;
    if (d) { int step = d / 3; if (!step) step = d > 0 ? 1 : -1; smooth_fp += step; }
  }
  int gy = ty + (smooth_fp * (th - grip)) / 1024;
  disp.fillRect(x + 1, gy, 4, grip, S60_SCROLL_FG);
  disp.fillRect(x + 1, gy, 4, 1, S60_ACCENT);
  disp.fillRect(x + 1, gy + grip - 1, 4, 1, S60_ACCENT);
  // Grip ribs make movement readable on 240x320 LCD.
  for (int yy = gy + 3; yy < gy + grip - 2; yy += 4) disp.fillRect(x + 2, yy, 2, 1, UI_WHITE);
}

static ThumbCache *thumb_peek(const char *abs_url) {
  if (!abs_url || !abs_url[0]) return nullptr;
  for (int i = 0; i < THUMB_CACHE; i++)
    if (thumbs[i].used && thumbs[i].ok && !strcmp(thumbs[i].url, abs_url)) return &thumbs[i];
  return nullptr;
}

static void draw_overview_mosaic(int x, int y, int w, int h) {
  // Java/Symbian page-tile preview: each tile represents roughly one rendered screen,
  // split by real line pixel heights rather than arbitrary line counts.
  const int n = doc.nlines > 0 ? doc.nlines : 1;
  const int MAX_TILES = 8;
  const int PAGE_PX = 218;  // close to the usable content viewport on 240x320
  int p0[MAX_TILES], p1[MAX_TILES], ppx[MAX_TILES];
  int pages = 0, line = 0;
  while (line < n && pages < MAX_TILES) {
    int begin = line, ph = 0;
    while (line < n) {
      int lh = render_line_h(line);
      if (line > begin && ph + lh > PAGE_PX) break;
      ph += lh;
      line++;
    }
    if (pages == MAX_TILES - 1 && line < n) {
      while (line < n) { ph += render_line_h(line); line++; }
    }
    p0[pages] = begin;
    p1[pages] = line;
    ppx[pages] = ph > 0 ? ph : 1;
    pages++;
  }
  if (pages < 1) { pages = 1; p0[0] = 0; p1[0] = n; ppx[0] = PAGE_PX; }

  int cols = pages <= 4 ? 2 : 3;
  int rows = (pages + cols - 1) / cols;
  const int gap = 3;
  int tw = (w - gap * (cols + 1)) / cols;
  int th = (h - gap * (rows + 1)) / rows;
  if (tw < 34) tw = 34;
  if (th < 30) th = 30;

  disp.fillRect(x, y, w, h, S60_SCROLL_BG);
  disp.fillRect(x, y, w, 1, UI_BORDER);
  disp.fillRect(x, y + h - 1, w, 1, UI_BORDER);

  int visual_line = (int)(ov_visual_fp / OV_FP);
  if (visual_line < 0) visual_line = 0;
  if (visual_line >= n) visual_line = n - 1;
  int selected_page = 0;
  for (int p = 0; p < pages; p++) if (visual_line >= p0[p] && visual_line < p1[p]) { selected_page = p; break; }
  int target_page = selected_page;
  int target_line = ov_oy;
  if (target_line < 0) target_line = 0;
  if (target_line >= n) target_line = n - 1;
  for (int p = 0; p < pages; p++) if (target_line >= p0[p] && target_line < p1[p]) { target_page = p; break; }

  // Animated viewport height. Fractional zoom remains fixed-point, so x1->x8 changes smoothly.
  int32_t zfp = ov_zoom_fp < OV_FP ? OV_FP : ov_zoom_fp;
  int32_t view_lines_fp = (int32_t)(((int64_t)n * OV_FP * OV_FP) / zfp);
  int view_lines = (view_lines_fp + OV_FP - 1) / OV_FP;
  if (view_lines < 2) view_lines = 2;
  if (view_lines > n) view_lines = n;
  int sel_col = target_page % cols, sel_row = target_page / cols;
  int target_tx = x + gap + sel_col * (tw + gap);
  int target_ty = y + gap + sel_row * (th + gap);

  for (int t = 0; t < pages; t++) {
    int col = t % cols, row = t / cols;
    int tx = x + gap + col * (tw + gap);
    int ty = y + gap + row * (th + gap);
    int l0 = p0[t], l1 = p1[t];

    // Paper/card shadow and top browser chrome strip.
    disp.fillRect(tx + 2, ty + 2, tw, th, 0xBDF7);
    disp.fillRect(tx, ty, tw, th, UI_BG);
    disp.fillRect(tx, ty, tw, 1, UI_BORDER);
    disp.fillRect(tx, ty + th - 1, tw, 1, UI_BORDER);
    disp.fillRect(tx, ty, 1, th, UI_BORDER);
    disp.fillRect(tx + tw - 1, ty, 1, th, UI_BORDER);
    disp.fillRect(tx + 2, ty + 2, tw - 4, 3, UI_TITLE);
    // Tiny page number tab, like feature-phone page tiles.
    set_text_font(5); disp.setTextColor(S60_DIM);
    char pn[5]; snprintf(pn, sizeof pn, "%d", t + 1);
    s60_text_right(pn, tx + tw - 3, ty + 6);

    int ix = tx + 3, iy = ty + 8, iw = tw - 6, ih = th - 11;
    int py = 0;
    for (int i = l0; i < l1 && i < doc.nlines; i++) {
      int lh = render_line_h(i);
      int ry = iy + (py * ih) / ppx[t];
      int rh = (lh * ih) / ppx[t];
      if (rh < 1) rh = 1;
      if (ry >= iy + ih) break;
      if (ry + rh > iy + ih) rh = iy + ih - ry;
      int st = doc.lines[i].style;

      if (st == 7) {
        int ii = doc.lines[i].image;
        ThumbCache *thm = nullptr;
        if (ii >= 0 && ii < doc.nimages) {
          char absu[256]; resolve_url(doc.images[ii].url, absu, sizeof absu);
          thm = thumb_peek(absu);
        }
        int pw = iw * 3 / 5; if (pw < 12) pw = 12;
        int ph = rh; if (ph < 6) ph = 6; if (ph > ih / 2) ph = ih / 2;
        if (thm && thm->pix) {
          for (int oy = 0; oy < ph; oy += 2)
            for (int ox = 0; ox < pw; ox += 2) {
              int sx = (ox * THUMB_W) / pw, sy = (oy * THUMB_H) / ph;
              disp.fillRect(ix + ox, ry + oy, 2, 2, thm->pix[sy * THUMB_W + sx]);
            }
        } else {
          disp.fillRect(ix, ry, pw, ph, 0xC618);
          for (int d = 2; d < pw - 2; d += 6) disp.drawPixel(ix + d, ry + (d % (ph > 1 ? ph : 1)), UI_WHITE);
        }
        if (ix + pw + 2 < ix + iw) disp.fillRect(ix + pw + 2, ry + 1, iw - pw - 2, rh > 2 ? rh - 2 : 1, UI_DIM);
      } else {
        uint16_t c = UI_DIM;
        if (doc.lines[i].link >= 0) c = UI_LINK;
        else if (st == 3) c = 0x07E0;
        else if (st == 2) c = UI_BORDER;
        int bw = 5 + (doc.lines[i].n * (iw - 5)) / 48;
        if (bw < 5) bw = 5;
        if (bw > iw) bw = iw;
        if (st == 3) bw = iw;
        else if (st == 5) bw = (iw * 3) / 5;
        disp.fillRect(ix, ry, bw, rh > 1 ? rh - 1 : 1, c);
      }
      py += lh;
    }

    if (t == selected_page) {
      // Inner viewport follows the eased line position; the outer tile cursor is animated below.
      int local_start_px = 0;
      for (int i = l0; i < visual_line && i < l1; i++) local_start_px += render_line_h(i);
      int local_view_px = 0;
      int end_line = visual_line + view_lines;
      if (end_line > l1) end_line = l1;
      for (int i = visual_line; i < end_line && i < doc.nlines; i++) local_view_px += render_line_h(i);
      int cy = iy + (local_start_px * ih) / ppx[t];
      int ch = (local_view_px * ih) / ppx[t];
      if (ch < 6) ch = 6;
      if (cy < iy) cy = iy;
      if (cy + ch > iy + ih) ch = iy + ih - cy;
      if (ch < 3) ch = 3;
      int cx = ix + 1, cw = iw - 2;
      disp.fillRect(cx, cy, cw, 2, UI_HOT);
      disp.fillRect(cx, cy + ch - 2, cw, 2, UI_HOT);
      disp.fillRect(cx, cy, 2, ch, UI_HOT);
      disp.fillRect(cx + cw - 2, cy, 2, ch, UI_HOT);
      // 4 square handles.
      disp.fillRect(cx - 1, cy - 1, 4, 4, UI_HOT);
      disp.fillRect(cx + cw - 3, cy - 1, 4, 4, UI_HOT);
      disp.fillRect(cx - 1, cy + ch - 3, 4, 4, UI_HOT);
      disp.fillRect(cx + cw - 3, cy + ch - 3, 4, 4, UI_HOT);
      // Center target/crosshair to make D-Pad pan direction obvious.
      int mx = cx + cw / 2, my = cy + ch / 2;
      disp.fillRect(mx - 4, my, 9, 1, UI_HOT);
      disp.fillRect(mx, my - 4, 1, 9, UI_HOT);
      disp.fillRect(mx - 1, my - 1, 3, 3, UI_WHITE);
      disp.drawPixel(mx, my, UI_HOT);
    }
  }

  // Animated tile-to-tile cursor. Four fixed-point rect values are the only extra state;
  // no off-screen bitmap is allocated.
  int32_t txfp = (target_tx - 2) * OV_FP, tyfp = (target_ty - 2) * OV_FP;
  int32_t twfp = (tw + 4) * OV_FP, thfp = (th + 4) * OV_FP;
  if (!ov_cursor_init) {
    ov_cursor_x_fp = txfp; ov_cursor_y_fp = tyfp;
    ov_cursor_w_fp = twfp; ov_cursor_h_fp = thfp;
    ov_cursor_init = true; ov_cursor_moving = false;
  } else {
    ov_cursor_x_fp = ov_ease_step(ov_cursor_x_fp, txfp);
    ov_cursor_y_fp = ov_ease_step(ov_cursor_y_fp, tyfp);
    ov_cursor_w_fp = ov_ease_step(ov_cursor_w_fp, twfp);
    ov_cursor_h_fp = ov_ease_step(ov_cursor_h_fp, thfp);
    if (abs(ov_cursor_x_fp - txfp) <= 2) ov_cursor_x_fp = txfp;
    if (abs(ov_cursor_y_fp - tyfp) <= 2) ov_cursor_y_fp = tyfp;
    if (abs(ov_cursor_w_fp - twfp) <= 2) ov_cursor_w_fp = twfp;
    if (abs(ov_cursor_h_fp - thfp) <= 2) ov_cursor_h_fp = thfp;
    ov_cursor_moving = ov_cursor_x_fp != txfp || ov_cursor_y_fp != tyfp ||
                       ov_cursor_w_fp != twfp || ov_cursor_h_fp != thfp;
    if (ov_cursor_moving) ov_anim_active = true;
  }
  int cx0 = (int)(ov_cursor_x_fp / OV_FP), cy0 = (int)(ov_cursor_y_fp / OV_FP);
  int cw0 = (int)(ov_cursor_w_fp / OV_FP), ch0 = (int)(ov_cursor_h_fp / OV_FP);
  if (cw0 > 5 && ch0 > 5) {
    disp.fillRect(cx0, cy0, cw0, 2, UI_HOT);
    disp.fillRect(cx0, cy0 + ch0 - 2, cw0, 2, UI_HOT);
    disp.fillRect(cx0, cy0, 2, ch0, UI_HOT);
    disp.fillRect(cx0 + cw0 - 2, cy0, 2, ch0, UI_HOT);
    // Small white keyline and corner grips evoke Opera Mini 4 without alpha/blending.
    disp.fillRect(cx0 + 2, cy0 + 2, cw0 - 4, 1, UI_WHITE);
    disp.fillRect(cx0, cy0, 4, 4, UI_HOT);
    disp.fillRect(cx0 + cw0 - 4, cy0, 4, 4, UI_HOT);
    disp.fillRect(cx0, cy0 + ch0 - 4, 4, 4, UI_HOT);
    disp.fillRect(cx0 + cw0 - 4, cy0 + ch0 - 4, 4, 4, UI_HOT);
  }
}

static void build_doc(const char *html) {
  build_docf("%s", html ? html : "");
}

static void build_docf(const char *fmt, ...) {
  static char buf[2048];      // static: giam stack loopTask (go_url -> show_msg -> build_docf)
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  doc_parse(&doc, buf, strlen(buf));
  body_scroll_reset(0);
}

// ---------------- dieu huong ----------------
static void go_url(const char *url);

static void push_back() {
  if (!cur_url[0]) return;
  if (!back_stack) {
    back_stack = (char (*)[256])heap_caps_malloc(12 * 256, MALLOC_CAP_SPIRAM);
    if (!back_stack) back_stack = (char (*)[256])malloc(12 * 256);
    if (!back_stack) return;
  }
  if (back_sp >= 12) {
    for (int i = 1; i < 12; i++) memcpy(back_stack[i - 1], back_stack[i], 256);
    back_sp = 11;
  }
  strncpy(back_stack[back_sp], cur_url, 255);
  back_stack[back_sp][255] = 0;
  back_sp++;
}
static void push_forward() {
  if (!cur_url[0]) return;
  if (!forward_stack) {
    forward_stack = (char (*)[256])heap_caps_malloc(12 * 256, MALLOC_CAP_SPIRAM);
    if (!forward_stack) forward_stack = (char (*)[256])malloc(12 * 256);
    if (!forward_stack) return;
  }
  if (forward_sp >= 12) {
    for (int i = 1; i < 12; i++) memcpy(forward_stack[i - 1], forward_stack[i], 256);
    forward_sp = 11;
  }
  strncpy(forward_stack[forward_sp], cur_url, 255);
  forward_stack[forward_sp][255] = 0;
  forward_sp++;
}
static void nav_record_new(const char *dest) {
  if (nav_history_suppressed || !dest || !dest[0] || !strcmp(dest, cur_url)) return;
  push_back();
  forward_sp = 0;
}
static bool nav_back() {
  if (!back_sp) return false;
  push_forward();
  char dest[256];
  strncpy(dest, back_stack[--back_sp], sizeof dest - 1); dest[sizeof dest - 1] = 0;
  nav_history_suppressed = true;
  go_url(dest);
  nav_history_suppressed = false;
  return true;
}
static bool nav_forward() {
  if (!forward_sp) return false;
  push_back();
  char dest[256];
  strncpy(dest, forward_stack[--forward_sp], sizeof dest - 1); dest[sizeof dest - 1] = 0;
  nav_history_suppressed = true;
  go_url(dest);
  nav_history_suppressed = false;
  return true;
}
static void resolve_url(const char *href, char *out, size_t cap) {
  // legacy ui_resolve_link: $prev, $refresh, mtt:, http(s), /abs, tuong doi
  if (!href || !href[0]) { out[0] = 0; return; }
  if (!strcmp(href, "$prev")) {
    if (back_sp) strncpy(out, back_stack[back_sp - 1], cap - 1);
    else strncpy(out, "mtt:start", cap - 1);
    out[cap - 1] = 0; return;
  }
  if (!strcmp(href, "$refresh")) {
    strncpy(out, cur_url, cap - 1); out[cap - 1] = 0; return;
  }
  if (!url_resolve(cur_url, href, out, cap)) {
    strncpy(out, href, cap - 1); out[cap - 1] = 0;
  }
}

static void show_msg(const char *m) {
  strncpy(msg, m, sizeof msg - 1); msg[sizeof msg - 1] = 0;
  build_docf(PAGE_ERROR, m);
  scr = SCR_MSG; top = 0; cursor_link = 0;
  loading = false;
  render();          // ve luon: truoc do man hinh dang dung o "Connecting..." lau
}

static void render();
static void ensure_focus_visible();

// Sau khi mo trang: dat focus vao link DAU TIEN (Opera chon phan tu focusable dau),
// khong thi dong dau co noi dung. cursor_link = link dang chon trong khoi.
static void focus_first() {
  focus_i = 0;
  while (focus_i < doc.nlines && doc.lines[focus_i].n == 0) focus_i++;
  int i = focus_i;
  while (i < doc.nlines && doc.lines[i].link < 0) i++;
  if (i < doc.nlines) focus_i = i;
  else if (focus_i >= doc.nlines) focus_i = 0;
  cursor_link = (focus_i >= 0 && focus_i < doc.nlines) ? doc.lines[focus_i].link : -1;
  if (focus_i > 0) ensure_focus_visible();
}

static void go_url(const char *url) {
  Serial.printf("[nav] %s\n", url);
  // legacy: $prev = quay lui that, $refresh = tai lai
  if (url && !strcmp(url, "$prev")) {
    if (!nav_back()) go_url("mtt:start");
    return;
  }
  if (url && !strcmp(url, "$refresh")) {
    char u[256]; strncpy(u, cur_url, sizeof u - 1); u[sizeof u - 1] = 0;
    go_url(u);
    return;
  }
  if (!strcmp(url, "#") || !strcmp(url, "mtt:menu#url")) {
    // hop nhap URL kieu he thong (multi-tap + host keyboard o sim)
    is_search_box = false;
    // mac dinh https:// de user chi can go host
    memcpy(url_in, "https://", 8); url_in[8] = 0; url_in_n = 8;
    mt_bind(url_in, &url_in_n, (int)sizeof url_in, MT_URL, (int)(sizeof MT_URL / sizeof MT_URL[0]));
    pass_t9_prev = t9mode;
    if (!strcmp(url, "#")) {
      strncpy(menu_src_url, cur_url, sizeof menu_src_url - 1);
      menu_src_url[sizeof menu_src_url - 1] = 0;
    }
    urlin_show();
    render(); return;
  }
  char full[256]; resolve_url(url, full, sizeof full);
  if (!strcmp(full, "#")) {
    strncpy(menu_src_url, cur_url, sizeof menu_src_url - 1);
    menu_src_url[sizeof menu_src_url - 1] = 0;
    is_search_box = false;
    memcpy(url_in, "https://", 8); url_in[8] = 0; url_in_n = 8;
    mt_bind(url_in, &url_in_n, (int)sizeof url_in, MT_URL, (int)(sizeof MT_URL / sizeof MT_URL[0]));
    pass_t9_prev = t9mode;
    urlin_show();
    render(); return;
  }
  if (!strncmp(full, "mtt:", 4)) {
    if (!strcmp(full, "mtt:forward")) {
      if (!nav_forward()) show_msg("No Forward page.");
      return;
    }
    nav_record_new(full);
    strncpy(cur_url, full, sizeof cur_url - 1);
    cur_url[sizeof cur_url - 1] = 0;
    if (!strcmp(full, "mtt:start")) build_doc(PAGE_START);
    else if (!strcmp(full, "mtt:about")) build_doc(PAGE_ABOUT);
    else if (!strcmp(full, "mtt:help")) build_doc(PAGE_HELP);
    else if (!strcmp(full, "mtt:config")) build_docf(PAGE_CONFIG, wifi_up ? "CONNECTED" : "OFF", cfg_ssid.c_str(), cfg_pass.c_str(), cfg_home.c_str(), cfg_tz.c_str(), cfg_text_mode ? "ON" : "OFF");
    else if (!strcmp(full, "mtt:textmode")) {
      cfg_text_mode = !cfg_text_mode;
      config_save();
      Serial.printf("[cfg] text_mode=%d\n", cfg_text_mode ? 1 : 0);
      build_docf(PAGE_CONFIG, wifi_up ? "CONNECTED" : "OFF", cfg_ssid.c_str(), cfg_pass.c_str(), cfg_home.c_str(), cfg_tz.c_str(), cfg_text_mode ? "ON" : "OFF");
    }
    else if (!strcmp(full, "mtt:web")) build_doc(PAGE_WEB);
    else if (!strcmp(full, "mtt:sites")) build_doc(PAGE_SITES);
    else if (!strcmp(full, "mtt:feeds")) build_doc(PAGE_FEEDS);
    else if (!strcmp(full, "mtt:fb")) build_doc(PAGE_FB);
    else if (!strcmp(full, "mtt:tools")) build_doc(PAGE_TOOLS);
    else if (!strcmp(full, "mtt:dl")) build_doc(PAGE_DL);
    else if (!strcmp(full, "mtt:search")) {
      // hop nhap Search — dung o URL box, tim qua Google WAP neu khong co scheme
      url_in_n = 0; url_in[0] = 0;
      mt_bind(url_in, &url_in_n, (int)sizeof url_in, MT_URL, (int)(sizeof MT_URL / sizeof MT_URL[0]));
      pass_t9_prev = t9mode;
      strncpy(menu_src_url, "mtt:start", sizeof menu_src_url - 1);
      is_search_box = true;
      urlin_show();
      render(); return;
    }
    else if (!strcmp(full, "mtt:wifi")) {
      wifi_scan(); wifi_list_show(); render(); return;
    } else if (!strncmp(full, "mtt:wifisel#", 12)) {
      net_cursor = atoi(full + 12);
      if (net_cursor < 0 || net_cursor >= net_n) net_cursor = 0;
      pass_n = 0; pass_in[0] = 0; pass_t9_prev = t9mode;
      mt_bind(pass_in, &pass_n, (int)sizeof pass_in, MT, (int)(sizeof MT / sizeof MT[0]));
      wifi_pass_show(); render(); return;
    }
    else if (!strcmp(full, "mtt:history")) {
      char *buf = page_scratch();
      if (!buf) { show_msg("Not enough RAM."); return; }
      int o = snprintf(buf, 6000, "<wml><card title=\"History\"><p>");
      for (int i = 0; i < history_count() && o < 5000; i++)
        o += snprintf(buf + o, 6000 - o, "<a href=\"%s\">%s</a><br/>",
                      history_url(i), history_title(i)[0] ? history_title(i) : history_url(i));
      snprintf(buf + o, 6000 - o, "</p></card></wml>");
      build_doc(buf);
    } else if (!strcmp(full, "mtt:menu")) {
      // menu la popup overlay — khong dung trang
    } else if (!strcmp(full, "mtt:menu#book")) {
      bookmark_add(menu_src_url, menu_src_title[0] ? menu_src_title : menu_src_url);
      show_msg("Bookmark saved. OK to continue.");   // show_msg da render
      return;
    } else if (!strcmp(full, "mtt:menu#refresh")) {
      go_url(menu_src_url); return;
    } else if (!strcmp(full, "mtt:bookmark")) {
      char *buf = page_scratch();
      if (!buf) { show_msg("Not enough RAM."); return; }
      int o = snprintf(buf, 6000, "<wml><card title=\"Bookmark\"><p>");
      for (int i = 0; i < bookmark_count() && o < 5000; i++)
        o += snprintf(buf + o, 6000 - o, "<a href=\"%s\">%s</a><br/>",
                      bookmark_url(i), bookmark_title(i)[0] ? bookmark_title(i) : bookmark_url(i));
      snprintf(buf + o, 6000 - o, "</p></card></wml>");
      build_doc(buf);
    } else build_doc(PAGE_START);
    mouse_on = false;                 // trang mtt: la UI nho, khong dung chuot ao
    history_add(full, doc.title[0] ? doc.title : "mtt");
    scr = SCR_BROWSE; top = 0;
    focus_first();
    render(); return;
  }
  if (strncmp(full, "http://", 7) && strncmp(full, "https://", 8)) {
    show_msg("This protocol/link is not supported.");
    return;
  }
  if (!wifi_up) { show_msg("WiFi is off. Go to Settings > Connect WiFi."); return; }
  nav_record_new(full);
  strncpy(cur_url, full, sizeof cur_url - 1);
  cur_url[sizeof cur_url - 1] = 0;
  // luu host
  static char scheme[8], host[64], path[192]; int port;
  if (url_split(full, scheme, host, &port, path)) strncpy(last_host, host, sizeof last_host - 1);
  // legacy-style status: Connecting... -> Sending request... -> Receiving... (KB)
  loading = true; load_pct = 15;
  load_got = 0; load_total = -1; load_paint_ms = 0;
  load_paint_pct = -1; load_paint_kb[0] = 0; load_phase_recv = false;
  load_fmt_size(0, load_paint_kb, sizeof load_paint_kb);
  disp.fillScreen(UI_BG);
  g_pane_status = "Connecting...";
  draw_status_pane();
  set_text_font(0); disp.setTextColor(S60_FG);
  s60_text(full, 8, UI_HDR_H + 8);
  load_paint_footer();                 // Menu | bar + 0.0K | Back (o cho dong gio)
  load_pct = 45; load_paint_pct = 45;
  g_pane_status = "Sending request...";
  draw_status_pane();
  disp.endWrite();
  HttpMeta meta; size_t len = 0;
  http_set_progress(on_http_progress);
  bool ok = http_get(full, net_buf, DOC_CAP, &len, &meta);
  http_set_progress(nullptr);
  g_pane_status = nullptr;             // tra pane ve title cua trang
  if (!ok) {
    loading = false;
    if (meta.status == -2) show_msg("Cannot connect to server.");
    else if (meta.status == -3) show_msg("Invalid HTTP response.");
    else if (meta.status == -5) show_msg("Simulator has no TLS; ESP32 supports HTTPS.");
    else if (meta.status == -6) show_msg("Too many redirects.");
    else show_msg("Connection failed.");
    return;
  }
  // gzip: khong giai nen duoc tren thiet bi -> bao loi
  if (meta.gzipped) { show_msg("Server sent gzip/br; cannot decode yet."); return; }
  if (meta.final_url[0] && strcmp(meta.final_url, cur_url)) {
    strncpy(cur_url, meta.final_url, sizeof cur_url - 1);
    cur_url[sizeof cur_url - 1] = 0;
  }
  if (url_split(cur_url, scheme, host, &port, path)) {
    strncpy(last_host, host, sizeof last_host - 1);
    last_host[sizeof last_host - 1] = 0;
  }
  if (len == 0) {
    char em[96]; snprintf(em, sizeof em, "HTTP %d - empty response.", meta.status);
    show_msg(em); return;
  }
  if (meta.content_type[0] && strncmp(meta.content_type, "text/", 5) &&
      !strstr(meta.content_type, "html") && !strstr(meta.content_type, "xml") &&
      !strstr(meta.content_type, "wml")) {
    char em[128]; snprintf(em, sizeof em, "Cannot render: %s", meta.content_type);
    show_msg(em); return;
  }
  doc.buf = doc_buf; doc.cap = DOC_CAP;
  doc_parse(&doc, net_buf, len);
  body_scroll_reset(0);
  // Warm a few image thumbnails so Overview can show real page tiles immediately.
  // Memory tier is PSRAM; decoded RGB565 survives reboot through LittleFS tier.
  // Text mode (chi van ban): khong prefetch/decode anh.
  if (!cfg_text_mode) thumb_prefetch_doc(3);
  history_add(cur_url, doc.title[0] ? doc.title : cur_url);
  loading = false; load_pct = 100;
  // Desktop/large-UI (khong viewport meta) -> tu hien chuot ao; mobile viewport -> tat chuot.
  // Trang WML/mtt: LUON tat chuot ao, neu khong chuot bat tu trang desktop truoc do se
  // giu nguyen va nuot phim D-Pad cua UI nho.
  if (!doc.is_wml) {
    if (html_looks_desktop(net_buf, len)) {
      mouse_on = true; mouse_x = SCR_W / 2; mouse_y = SCR_H / 2;
      Serial.println("[mouse] desktop UI detected -> virtual mouse on");
    } else {
      mouse_on = false;
    }
  } else {
    mouse_on = false;
  }
  scr = SCR_BROWSE; top = 0;
  focus_first();
  if (meta.truncated) Serial.printf("[http] body truncated at %u bytes\n", (unsigned)len);
  render();
}

// ---------------- render (giong anh chup legacy browser V1.0) ----------------
static const int HDR_H = UI_HDR_H, FTR_H = UI_FTR_H;

// ---------------- keypad spatial focus (Java/Symbian browser style) ----------------
// Mot link co the wrap qua nhieu dong: xem toan bo vung line0..line1 la MOT focus block.
// Van ban thuong / heading / field / folder: moi dong render la mot block doc lap.
static int focus_block_start(int line) {
  if (line < 0 || line >= doc.nlines) return line;
  int li = doc.lines[line].link;
  if (li >= 0 && li < doc.nlinks) {
    int s = doc.links[li].line0;
    if (s >= 0 && s < doc.nlines) return s;
  }
  uint16_t b = doc.lines[line].block;
  int s = line;
  while (s > 0 && doc.lines[s - 1].n > 0 && doc.lines[s - 1].link < 0 &&
         doc.lines[s - 1].block == b) s--;
  return s;
}
static int focus_block_end(int line) {
  if (line < 0 || line >= doc.nlines) return line;
  int li = doc.lines[line].link;
  if (li >= 0 && li < doc.nlinks) {
    int e = doc.links[li].line1;
    if (e >= 0 && e < doc.nlines) return e;
  }
  uint16_t b = doc.lines[line].block;
  int e = line;
  while (e + 1 < doc.nlines && doc.lines[e + 1].n > 0 && doc.lines[e + 1].link < 0 &&
         doc.lines[e + 1].block == b) e++;
  return e;
}
static int render_line_h(int line) {
  if (line < 0 || line >= doc.nlines) return 18;
  int st = doc.lines[line].style;
  if (st == 1) return 24;
  // Dong trong nam GIUA hai hang danh sach (folder/dir): co lai con 2 px de danh
  // sach S60 lien mach nhu menu Nokia, thay vi 16/22 px trang ngan cach. Phai
  // kiem tra TRUOC nhanh "st == 3" vi parser giu style 3 cho ca dong trong.
  if (doc.lines[line].n == 0 && line > 0 && line + 1 < doc.nlines &&
      doc.lines[line - 1].style == 3 && doc.lines[line - 1].n != 0 &&
      doc.lines[line + 1].style == 3 && doc.lines[line + 1].n != 0)
    return 2;
  if (st == 3) return 22;
  return line_h(st);
}

static int doc_px_before_line(int line) {
  if (line <= 0) return 0;
  if (line > doc.nlines) line = doc.nlines;
  int px = 0;
  for (int i = 0; i < line; i++) px += render_line_h(i);
  return px;
}

static int doc_total_px() {
  return doc_px_before_line(doc.nlines);
}

static int doc_line_at_px(int px, int *line_top_px) {
  if (px < 0) px = 0;
  int acc = 0;
  for (int i = 0; i < doc.nlines; i++) {
    int h = render_line_h(i);
    if (px < acc + h) { if (line_top_px) *line_top_px = acc; return i; }
    acc += h;
  }
  if (line_top_px) *line_top_px = acc;
  return doc.nlines > 0 ? doc.nlines - 1 : 0;
}

static void body_scroll_reset(int line) {
  int px = doc_px_before_line(line);
  body_scroll_visual_fp = body_scroll_target_fp = (int32_t)px * BODY_FP;
  body_scroll_velocity_fp = 0;
  body_scroll_last = millis();
  body_scroll_active = false;
  body_scroll_inertia = false;
}

static int body_scroll_max_px() {
  int total = doc_total_px();
  int viewport = SCR_H - UI_ROWS_Y - UI_FTR_H - 4;
  return total > viewport ? total - viewport : 0;
}

static void body_scroll_target_line(int line) {
  int max_px = body_scroll_max_px();
  int px = doc_px_before_line(line);
  if (px > max_px) px = max_px;
  if (px < 0) px = 0;
  body_scroll_target_fp = (int32_t)px * BODY_FP;
  body_scroll_active = body_scroll_visual_fp != body_scroll_target_fp || body_scroll_velocity_fp != 0;
}

static int body_scroll_held_dir() {
  // Read physical D-Pad level so release is visible even though on_key() is edge-triggered.
  bool neg = digitalRead(KEY_UP) == LOW || digitalRead(KEY_LEFT) == LOW;
  bool pos = digitalRead(KEY_DOWN) == LOW || digitalRead(KEY_RIGHT) == LOW;
  if (pos == neg) return 0;
  return pos ? 1 : -1;
}

static void body_scroll_kick(int dir) {
  if (!dir) return;
  int32_t d = body_scroll_target_fp - body_scroll_visual_fp;
  if (d != 0) {
    // Extend an already-needed autoscroll by only a few pixels. This gives a visible
    // coast after key release without letting the focused block disappear off-screen.
    int max_px = body_scroll_max_px();
    int32_t margin = (int32_t)dir * BODY_COAST_MARGIN_PX * BODY_FP;
    int32_t nt = body_scroll_target_fp + margin;
    if (nt < 0) nt = 0;
    int32_t max_fp = (int32_t)max_px * BODY_FP;
    if (nt > max_fp) nt = max_fp;
    body_scroll_target_fp = nt;
  }
  if ((dir > 0 && body_scroll_velocity_fp < 0) || (dir < 0 && body_scroll_velocity_fp > 0))
    body_scroll_velocity_fp /= 3; // direction reversal: shed old momentum quickly
  body_scroll_velocity_fp += (int32_t)dir * BODY_VEL_KICK_FP;
  if (body_scroll_velocity_fp > BODY_VEL_MAX_FP) body_scroll_velocity_fp = BODY_VEL_MAX_FP;
  if (body_scroll_velocity_fp < -BODY_VEL_MAX_FP) body_scroll_velocity_fp = -BODY_VEL_MAX_FP;
  body_scroll_inertia = true;
  body_scroll_active = true;
}

static void body_scroll_move_by_px(int delta) {
  int max_px = body_scroll_max_px();
  int px = (int)(body_scroll_visual_fp / BODY_FP) + delta;
  if (px < 0) px = 0;
  if (px > max_px) px = max_px;
  body_scroll_visual_fp = body_scroll_target_fp = (int32_t)px * BODY_FP;
  body_scroll_velocity_fp = 0;
  body_scroll_last = millis();
  body_scroll_active = false;
  body_scroll_inertia = false;
}

static bool body_scroll_tick() {
  if (!body_scroll_active && !body_scroll_inertia && body_scroll_velocity_fp == 0) return false;
  uint32_t now = millis();
  if (now - body_scroll_last < BODY_ANIM_MS) return false;
  body_scroll_last = now;

  int32_t old = body_scroll_visual_fp;
  int32_t d = body_scroll_target_fp - body_scroll_visual_fp;
  int desired = d > 0 ? 1 : d < 0 ? -1 : 0;
  int held = body_scroll_held_dir();

  if (held && (!desired || held == desired)) {
    body_scroll_velocity_fp += (int32_t)held * BODY_VEL_ACCEL_FP;
    if (body_scroll_velocity_fp > BODY_VEL_MAX_FP) body_scroll_velocity_fp = BODY_VEL_MAX_FP;
    if (body_scroll_velocity_fp < -BODY_VEL_MAX_FP) body_scroll_velocity_fp = -BODY_VEL_MAX_FP;
    body_scroll_inertia = true;
  } else if (body_scroll_velocity_fp != 0) {
    // Key released: integer friction gives a light Opera Mini / Java-phone coast.
    body_scroll_velocity_fp = (body_scroll_velocity_fp * BODY_FRICTION_NUM) / BODY_FRICTION_DEN;
    if (abs(body_scroll_velocity_fp) <= BODY_VEL_STOP_FP) body_scroll_velocity_fp = 0;
  }

  d = body_scroll_target_fp - body_scroll_visual_fp;
  desired = d > 0 ? 1 : d < 0 ? -1 : 0;
  if (desired) {
    // Never let stale momentum run away from the focus target.
    if ((body_scroll_velocity_fp > 0 && desired < 0) || (body_scroll_velocity_fp < 0 && desired > 0))
      body_scroll_velocity_fp /= 2;

    int32_t step = body_scroll_velocity_fp;
    if (step == 0 || (step > 0 ? 1 : -1) != desired) {
      // Final low-speed settle stays fixed-point and allocation-free.
      int32_t eased = ov_ease_step(body_scroll_visual_fp, body_scroll_target_fp);
      step = eased - body_scroll_visual_fp;
    }
    if (abs(step) > abs(d)) step = d;
    body_scroll_visual_fp += step;
    int32_t remain = body_scroll_target_fp - body_scroll_visual_fp;
    if (abs(remain) <= 2 || (!held && abs(remain) <= BODY_FP * 3 && abs(body_scroll_velocity_fp) <= BODY_FP / 2)) {
      body_scroll_visual_fp = body_scroll_target_fp;
      body_scroll_velocity_fp = 0;
    }
  } else {
    // At target there is nowhere safe to coast without hiding the selected block.
    body_scroll_velocity_fp = 0;
  }

  body_scroll_inertia = body_scroll_velocity_fp != 0;
  body_scroll_active = body_scroll_visual_fp != body_scroll_target_fp || body_scroll_inertia;
  return old != body_scroll_visual_fp || body_scroll_active;
}

static void ensure_focus_visible() {
  if (!doc.nlines || focus_i < 0 || focus_i >= doc.nlines) return;
  int fs = focus_block_start(focus_i), fe = focus_block_end(focus_i);
  focus_i = fs;
  if (fs < top) top = fs;
  if (top < 0) top = 0;

  const int body_bottom = SCR_H - FTR_H - 4;
  // Cuon tung dong cho den khi day focus block nam trong viewport.
  // Neu block cao hon viewport, giu dau block o tren de nguoi dung doc tu dau.
  int block_h = 0;
  for (int i = fs; i <= fe && i < doc.nlines; i++) block_h += render_line_h(i);
  if (UI_ROWS_Y + block_h > body_bottom) { top = fs; body_scroll_target_line(top); return; }

  while (top < fs) {
    int y = UI_ROWS_Y;
    for (int i = top; i <= fe && i < doc.nlines; i++) y += render_line_h(i);
    if (y <= body_bottom) break;
    top++;
  }
  body_scroll_target_line(top);
}

// =================== S60 icon set (hinh hoc co ban, khong can asset) ===================
static void s60_icon_globe(int cx, int cy, int r, uint16_t c) {
  disp.drawCircle(cx, cy, r, c);
  disp.drawFastHLine(cx - r, cy, r * 2 + 1, c);            // xich dao
  for (int dy = -r; dy <= r; dy++) {                       // kinh tuyen
    int rem = r * r - dy * dy;
    int s = 0; while ((s + 1) * (s + 1) <= rem) s++;
    int dx = s / 2;
    disp.drawPixel(cx + dx, cy + dy, c);
    disp.drawPixel(cx - dx, cy + dy, c);
  }
}
static void s60_icon_gear(int cx, int cy, int r, uint16_t c, uint16_t bg) {
  static const int8_t DX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
  static const int8_t DY[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
  for (int i = 0; i < 8; i++) disp.fillRect(cx + DX[i] * r - 1, cy + DY[i] * r - 1, 3, 3, c);
  disp.fillCircle(cx, cy, r, c);
  disp.fillCircle(cx, cy, r / 2, bg);                      // lo o giua
}
static void s60_icon_clock(int cx, int cy, int r, uint16_t c) {
  disp.drawCircle(cx, cy, r, c);
  disp.drawFastVLine(cx, cy - r + 2, r - 1, c);
  disp.drawFastHLine(cx, cy, r - 2, c);
}
static void s60_icon_magnifier(int cx, int cy, int r, uint16_t c) {
  disp.drawCircle(cx - 1, cy - 1, r, c);
  disp.fillRect(cx + r - 2, cy + r - 2, 4, 2, c);
  disp.fillRect(cx + r - 1, cy + r - 1, 2, 2, c);
}
static void s60_icon_ribbon(int x, int y, int w, int h, uint16_t c, uint16_t bg) {
  (void)bg;
  disp.fillRect(x, y, w, h, c);                            // bookmark ribbon + khuyet V
  int n = w / 2;
  for (int i = 0; i < n; i++) {
    disp.fillRect(x + i, y + h - n + i, 1, n - i, bg);
    disp.fillRect(x + w - 1 - i, y + h - n + i, 1, n - i, bg);
  }
}
// 3 cung song + cham tam, cao 11 px, be 2r+1 px, trai deu quanh (cx, cy).
static void s60_icon_wifi_glyph(int cx, int cy, int r, uint16_t c) {
  int t = cy - r;
  int w0 = r * 2 + 1;
  disp.fillRect(cx - r, t, w0, 2, c);
  int w1 = w0 * 2 / 3; if (w1 < 5) w1 = 5;
  disp.fillRect(cx - w1 / 2, t + 3, w1, 2, c);
  int w2 = w0 / 3; if (w2 < 3) w2 = 3;
  disp.fillRect(cx - w2 / 2, t + 6, w2, 2, c);
  disp.fillRect(cx - 1, t + 9, 3, 2, c);
}
static void s60_icon_info(int cx, int cy, int r, uint16_t c) {
  disp.drawCircle(cx, cy, r, c);
  disp.fillRect(cx - 1, cy - 1, 2, r, c);
  disp.fillRect(cx - 1, cy - r + 2, 2, 2, c);
}

// Icon tile S60: khoi vuong bo goc co gradient sang, glyph ben trong.
enum S60IconKind { S60_IC_SITE, S60_IC_SETTINGS, S60_IC_BOOKMARK, S60_IC_HISTORY,
                   S60_IC_SEARCH, S60_IC_WIFI, S60_IC_INFO, S60_IC_HELP };

static S60IconKind s60_icon_kind(const char *label) {
  const char *l = label ? label : "";
  auto has = [l](const char *k) -> bool {
    for (const char *p = l; *p; p++) {
      int i = 0; for (; k[i]; i++) if (tolower((unsigned char)p[i]) != tolower((unsigned char)k[i])) break;
      if (!k[i]) return true;
    }
    return false;
  };
  if (has("wifi") || has("connect") || has("network")) return S60_IC_WIFI;
  if (has("setting") || has("config") || has("tool"))   return S60_IC_SETTINGS;
  if (has("bookmark"))                                    return S60_IC_BOOKMARK;
  if (has("history"))                                     return S60_IC_HISTORY;
  if (has("search") || has("google"))                     return S60_IC_SEARCH;
  if (has("about"))                                       return S60_IC_INFO;
  if (has("help"))                                        return S60_IC_HELP;
  return S60_IC_SITE;
}

// Icon cua mot dong danh sach S60 3rd Edition: chi co glyph 16x16, KHONG co o
// nen — dung nhu menu Nokia (nen chi doi mau khi dong duoc chon).
// bg = mau nen dang nam duoi icon (de khoe lo/cat khuyet).
static void s60_list_icon(int x, int y, int s, S60IconKind k, uint16_t fg, uint16_t bg) {
  int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 1;
  switch (k) {
    case S60_IC_SETTINGS: s60_icon_gear(cx, cy, r, fg, bg); break;
    case S60_IC_BOOKMARK: s60_icon_ribbon(cx - r + 2, y + 1, r * 2 - 3, s - 2, fg, bg); break;
    case S60_IC_HISTORY:  s60_icon_clock(cx, cy, r, fg); break;
    case S60_IC_SEARCH:   s60_icon_magnifier(cx, cy, r, fg); break;
    case S60_IC_WIFI:     s60_icon_wifi_glyph(cx, cy + 1, r - 1, fg); break;
    case S60_IC_INFO:     s60_icon_info(cx, cy, r, fg); break;
    case S60_IC_HELP:     disp.drawCircle(cx, cy, r, fg);
                          set_text_font(0); disp.setTextColor(fg);
                          s60_text_center("?", cx, cy - 4); break;
    default:              s60_icon_globe(cx, cy, r - 1, fg); break;
  }
}

// ---- P2: live WiFi bars in title bar (E524546: WiFi STA, no touch) ----
static int     wifi_rssi_lvl = -1;     // -1=off, 0..4 bars
static uint32_t wifi_rssi_ms = 0;

static void wifi_rssi_poll() {
#if defined(ARDUINO)
  uint32_t now = millis();
  if (now - wifi_rssi_ms < 1000) return;
  wifi_rssi_ms = now;
  if (!wifi_up || WiFi.status() != WL_CONNECTED) { wifi_rssi_lvl = -1; return; }
  int r = WiFi.RSSI();                 // dBm, typically -30..-90
  int lvl;
  if (r >= -55) lvl = 4;
  else if (r >= -66) lvl = 3;
  else if (r >= -75) lvl = 2;
  else if (r >= -85) lvl = 1;
  else lvl = 0;
  wifi_rssi_lvl = lvl;
#else
  wifi_rssi_lvl = wifi_up ? 3 : -1;    // sim: steady 3 bars when "connected"
#endif
}

// ---- S60 application pane: vach song (trai), WiFi + pin (phai), title DAM ----
// Chieu cao thanh duoi cua pane: 4 px cuoi dung cho progress/duong ke.
#define S60_PANE_MAIN_H (S60_PANE_H - 4)
#define S60_PANE_TEXT_Y 5

static void draw_icon_signal(int x, int y) {
  // 5 vach Nokia: be 3 px, cach 1 px, cao 3..11 px, moc day y+11.
  int lvl = (wifi_rssi_lvl < 0) ? 0 : wifi_rssi_lvl + 1;
  for (int i = 0; i < 5; i++) {
    int h = 3 + i * 2;
    uint16_t c = (i < lvl) ? S60_PANE_TEXT : 0x2965;   // slot chua co tin hieu
    disp.fillRect(x + i * 4, y + 11 - h, 3, h, c);
  }
}

// Pin: ESP32-S3 khong do duoc muc pin (khong co chan ADC trong pins.h) nen bieu
// tuong hien "nguon dang cap" nhu dien thoai dang sac: khung + 3 o day.
static void draw_icon_battery(int x, int y) {
  const int w = 18, h = 10;
  disp.drawRect(x, y, w, h, S60_PANE_TEXT);
  disp.fillRect(x + w, y + 3, 2, 4, S60_PANE_TEXT);
  for (int i = 0; i < 3; i++) disp.fillRect(x + 2 + i * 5, y + 2, 4, h - 4, S60_PANE_TEXT);
}

static void draw_icon_wifi(int x, int y) {
  uint16_t c = (wifi_rssi_lvl >= 0) ? S60_PANE_TEXT : 0x2965;
  s60_icon_wifi_glyph(x + 6, y + 5, 5, c);
  if (wifi_rssi_lvl < 0)                                    // gach cheo = mat ket noi
    for (int i = 0; i < 11; i++) disp.drawPixel(x + 1 + i, y + 10 - i, S60_RED);
}

// Status pane + title bar (mot dai gradient lien tuc theo kieu S60).
static void draw_status_pane() {
  wifi_rssi_poll();
  s60_pane_fill(0, 0, SCR_W, S60_PANE_H);
  draw_icon_signal(3, 3);
  const int bat_x = SCR_W - 20;
  draw_icon_battery(bat_x, 3);
  draw_icon_wifi(bat_x - 16, 3);
  const int tx = 25;
  const char *t = g_pane_status ? g_pane_status : (doc.title[0] ? doc.title : "Qeafbrowser");
  s60_pane_title(t, tx, (bat_x - 17) - tx - 2, S60_PANE_TEXT_Y);
  if (loading) {
    disp.fillRect(0, S60_PANE_H - 3, SCR_W, 3, S60_TRACK);
    disp.fillRect(0, S60_PANE_H - 3, SCR_W * load_pct / 100, 3, S60_ACCENT);
  } else {
    disp.fillRect(0, S60_PANE_H - 1, SCR_W, 1, S60_PANE_LINE);
  }
}

static void draw_title_bar(bool loading_bar) {
  (void)loading_bar;
  draw_status_pane();
}
static void draw_icon_pencil(int x, int y, uint16_t c) {
  // but chi kieu S60 cho o nhap URL
  disp.fillRect(x + 1, y + 1, 2, 8, c);
  disp.fillRect(x + 3, y + 0, 2, 3, c);
  disp.fillRect(x + 1, y + 0, 3, 1, c);
}
// Mui ten phai dac (dung cho muc menu co menu con).
static void draw_chevron(int x, int y, uint16_t c) {
  for (int i = 0; i < 5; i++) disp.fillRect(x + i, y + i, 2, 10 - 2 * i, c);
}

static void draw_virtual_mouse_cursor(int x, int y) {
  static const uint8_t outer_x[18] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,8,8
  };
  static const uint8_t outer_w[18] = {
    1,2,3,4,5,6,7,8,9,10,12,7,8,9,10,4,3,1
  };
  static const uint8_t inner_x[14] = { 1,1,1,1,1,1,1,1,1,1,6,6,7,8 };
  static const uint8_t inner_w[14] = { 2,4,5,6,8,9,10,6,7,2,3,2,2,1 };
  for (int row = 0; row < 18; row++)
    disp.fillRect(x + outer_x[row] + 1, y + row + 1, outer_w[row], 1, S60_SHADOW);
  for (int row = 0; row < 18; row++)
    disp.fillRect(x + outer_x[row], y + row, outer_w[row], 1, S60_FG);
  for (int row = 0; row < 14; row++)
    disp.fillRect(x + inner_x[row], y + row + 3, inner_w[row], 1, S60_WHITE);
}

// popup OPTION menu (Bmrk/Optn/Navg/Tool/Sett/Help/Exit) kieu S60 Options
struct MenuTop { const char *label; const char *child; };
static const MenuTop MENUS[8] = {
  { "Bmrk", "SvBmrk" },
  { "Optn", "SpDial" },   // Speed Dial home
  { "Navg", "Overview" }, // Opera Mini page overview
  { "Tool", "Mouse"    }, // Opera Mini virtual mouse
  { "Sett", "WiFi"   },
  { "Text", "NoImg"  },   // toggle text mode (hide images)
  { "Help", "Help"   },
  { "Exit", ""       },
};

// S60 Options: panel tron co bong, muc DAM, thanh chon xanh, mui ten cho menu con.
static void render_menu_popup() {
  const int mx = 16, my = UI_HDR_H + 4;
  const int mw = SCR_W - 2 * mx;
  const int rowh = 20;
  const int mh = 8 * rowh + 8;
  s60_panel(mx, my, mw, mh, S60_FIELD, S60_FIELD_LINE);
  set_text_font(0);
  for (int i = 0; i < 8; i++) {
    int y = my + 4 + rowh * i;
    bool sel = (i == menu_idx);
    bool sub = sel && menu_in_sub;
    if (sel) {
      s60_sel_bar(mx + 2, y, mw - 4, rowh - 2);
      draw_chevron(mx + 6, y + 4, S60_SEL_TEXT);        // con tro chon cua S60
    }
    disp.setTextColor((sel && !menu_in_sub) ? S60_SEL_TEXT : S60_FG);
    s60_text(MENUS[i].label, mx + 14, y + 5);
    if (MENUS[i].child[0]) {
      disp.setTextColor(sub ? S60_SEL_TEXT : (sel ? S60_SEL_TEXT : S60_DIM));
      s60_text_right(MENUS[i].child, mx + mw - 12, y + 5);
      draw_chevron(mx + mw - 11, y + 5, sub ? S60_SEL_TEXT : S60_DIM);
    } else {
      draw_chevron(mx + mw - 11, y + 5, sel ? S60_SEL_TEXT : S60_DIM);
    }
  }
}

static void render() {
#if defined(ARDUINO)
  // Ve toan bo khung vao backbuffer PSRAM, sau do push mot len man hinh.
  // gui_draw_text (lc_font) ve qua launcher LCD() — can redirect target sang frame_buf.
  if (frame_ok) {
    gfx_dst = &frame_buf;
    launcher_set_target(&frame_buf);
  }
#endif
  disp.startWrite();
  disp.fillScreen(UI_BG);
  // title bar + globe + WiFi bars (P2)
  draw_title_bar(loading);

  if (overview_on) {
    int n = doc.nlines > 0 ? doc.nlines : 1;
    int ox = 6, oy = HDR_H + 14, ow = SCR_W - 26, oh = SCR_H - HDR_H - FTR_H - 18;
    set_text_font(5);
    disp.setTextColor(S60_DIM);
    s60_text("Desktop overview", 8, HDR_H + 2);
    char zbuf[16]; snprintf(zbuf, sizeof zbuf, "x%d", overview_zoom);
    s60_text_right(zbuf, SCR_W - 8, HDR_H + 2);
    draw_overview_mosaic(ox, oy, ow, oh);
    int32_t zfp = ov_zoom_fp < OV_FP ? OV_FP : ov_zoom_fp;
    int draw_start = (int)(ov_visual_fp / OV_FP);
    int draw_visible = (int)(((int64_t)n * OV_FP + zfp - 1) / zfp);
    if (draw_visible < 2) draw_visible = 2;
    if (draw_visible > n) draw_visible = n;
    if (draw_start > n - draw_visible) draw_start = n - draw_visible;
    if (draw_start < 0) draw_start = 0;
    // Mini-map and scrollbar follow the eased visual position instead of jumping to the target.
    draw_mini_map(SCR_W - 18, HDR_H + 16, 58);
    draw_scrollbar(SCR_W - 8, oy, oh, n, draw_start, draw_visible);
  } else if (vkey_open) {
    vkey_draw();
  } else {
    // Body scrolls in real pixels. Only fixed-point offsets are stored; no off-screen framebuffer.
    int scroll_px = (int)(body_scroll_visual_fp / BODY_FP);
    int total_px = doc_total_px();
    int view_px = SCR_H - UI_ROWS_Y - UI_FTR_H - 4;
    int max_scroll_px = total_px > view_px ? total_px - view_px : 0;
    if (scroll_px < 0) scroll_px = 0;
    if (scroll_px > max_scroll_px) scroll_px = max_scroll_px;
    int first_top_px = 0;
    int first_line = doc_line_at_px(scroll_px, &first_top_px);
    int y = UI_ROWS_Y - (scroll_px - first_top_px);
    int visible_lines = 0;
    int fs = focus_block_start(focus_i), fe = focus_block_end(focus_i);
    int focus_x0 = SCR_W, focus_x1 = -1, focus_y0 = -1, focus_y1 = -1;
    for (int i = first_line; i < doc.nlines && y < SCR_H - FTR_H - 4; i++) {
      int st = doc.lines[i].style;
      int lh = render_line_h(i);
      if (doc.lines[i].n) {
        const char *txt = doc.buf + doc.lines[i].off;
        bool sel = (i >= fs && i <= fe);   // Focus Block co the gom nhieu dong cua mot link
        if (sel && st != 1 && st != 3 && st != 7) {
          // st 1/3/7 tu ve trang thai chon (o nhap / thanh xanh / vien anh), khong
          // them khung focus de tranh chong len nhau.
          int bx = UI_PAD - 3;
          int bw;
          {
            int tw = gfx_textWidth(txt, st);
            bw = tw + 8;
            if (bw < 16) bw = 16;
            if (bx + bw > SCR_W - 2) bw = SCR_W - 2 - bx;
          }
          if (bx < focus_x0) focus_x0 = bx;
          if (bx + bw > focus_x1) focus_x1 = bx + bw;
          if (focus_y0 < 0) focus_y0 = y - 3;
          focus_y1 = y + lh - 1;
        }
        if (st == 7) {
          // Text mode: hien alt thuong, khong goi thumb_find/decode.
          if (cfg_text_mode) {
            set_text_font(0); disp.setTextColor(UI_FG);
            const char *alt_t = txt;
            int ii_t = doc.lines[i].image;
            if (ii_t >= 0 && ii_t < doc.nimages && doc.images[ii_t].alt[0])
              alt_t = doc.images[ii_t].alt;
            s60_text(alt_t, UI_PAD, y);
            break;
          }
          // Small Screen Rendering: JPEG/PNG thumbnail neu decode duoc, neu khong thi fallback placeholder.
          int bx = UI_PAD, bw = SCR_W - 2 * UI_PAD, bh = 74;
          disp.fillRect(bx, y, bw, bh, S60_SCROLL_BG);
          disp.drawRect(bx, y, bw, bh, sel ? S60_ACCENT : UI_BORDER);
          if (sel) disp.drawRect(bx + 1, y + 1, bw - 2, bh - 2, S60_ACCENT);
          int ii = doc.lines[i].image;
          const char *alt = txt;
          ThumbCache *th = nullptr;
          if (ii >= 0 && ii < doc.nimages) {
            char absu[256]; resolve_url(doc.images[ii].url, absu, sizeof absu);
            alt = doc.images[ii].alt[0] ? doc.images[ii].alt : txt;
            th = thumb_find(absu);
          }
          if (th && th->ok) {
            // pushImage 1 loi thay cho 2352 lan drawPixel -> render nhanh hon nhieu.
            disp.pushImage(bx + 8, y + 10, THUMB_W, THUMB_H, th->pix);
          } else {
            disp.fillRect(bx + 8, y + 10, THUMB_W, THUMB_H, UI_BORDER);
            for (int d = 0; d <= 26; d++) {
              int xx = bx + 8 + d * 2;
              if (xx < bx + 8 + THUMB_W) {
                int yy0 = y + 10 + (d * THUMB_H) / 26;
                int yy1 = y + 10 + THUMB_H - 1 - (d * THUMB_H) / 26;
                disp.drawPixel(xx, yy0, UI_BG);
                disp.drawPixel(xx, yy1, UI_BG);
              }
            }
          }
          set_text_font(5); disp.setTextColor(UI_DIM);
          s60_text("Image thumb 240px", bx + 72, y + 10);
          set_text_font(0); disp.setTextColor(UI_FG);
          s60_text(alt, bx + 72, y + 28);
          set_text_font(5); disp.setTextColor(UI_DIM);
          s60_text((th && th->ok) ? (th->persistent_hit ? "LittleFS -> PSRAM" : "PSRAM image cache") : "thumbnail fallback", bx + 72, y + 48);
        } else if (st == 1) {
          // O nhap kieu S60: nen sang + vien nhat; khi duoc chon thi TO NEN XANH
          // va chu trang, dung nhu text box dang focus cua S60.
          int bx = UI_PAD, bw = SCR_W - 2 * UI_PAD, bh = 24;   // khop render_line_h
          if (sel) {
            s60_sel_bar(bx, y, bw, bh);
            disp.drawRoundRect(bx, y, bw, bh, 3, S60_SEL_LINE);
          } else {
            disp.fillRect(bx, y, bw, bh, S60_FIELD);
            disp.drawRoundRect(bx, y, bw, bh, 3, S60_FIELD_LINE);
          }
          uint16_t ic = sel ? S60_SEL_TEXT : S60_TILE_GLYPH;
          bool url_box = strstr(txt, "URL") || strstr(txt, "Address") || strstr(txt, "Enter");
          if (url_box) draw_icon_pencil(bx + 7, y + 7, ic);
          else s60_icon_magnifier(bx + 10, y + 12, 4, ic);
          set_text_font(0);
          disp.setTextColor(sel ? S60_SEL_TEXT : S60_DIM);
          s60_text(txt, bx + 22, y + 8);
        } else if (st == 3) {
          // Dong danh sach S60: thanh chon xanh + icon glyph + nhan DAM + mui ten.
          const int rh = S60_ROW_H;
          if (sel) s60_sel_bar(0, y, SCR_W, rh - 1);
          s60_list_icon(3, y + 3, S60_TILE, s60_icon_kind(txt),
                        sel ? S60_SEL_TEXT : S60_TILE_GLYPH,
                        sel ? S60_SEL_TOP : S60_BG);
          int label_x = 3 + S60_TILE + 5;
          set_text_font(0);
          disp.setTextColor(sel ? S60_SEL_TEXT : S60_FG);
          char clip[64];
          s60_fit(txt, clip, (int)sizeof clip, SCR_W - label_x - 14);
          s60_text(clip, label_x, y + (rh - 8) / 2);
          draw_chevron(SCR_W - 12, y + (rh - 10) / 2, sel ? S60_SEL_TEXT : S60_DIM);
          if (!sel) disp.fillRect(0, y + rh - 1, SCR_W, 1, S60_RULE);
        } else {
          // Van ban noi dung: lc_font Tahoma VN — tieng Viet co dau + ASCII nhu cu.
          uint16_t fg = S60_FG;
          if (doc.lines[i].link >= 0) fg = S60_LINK;
          else if (st == 2) fg = S60_LINK;                // tieu de bai bao
          else if (st == 5) fg = S60_DIM;                 // byline / timestamp
          int fs = (st == 2) ? 2 : 1;
          gui_draw_text_bold(UI_PAD, y, fg, S60_BG, txt, fs);
          if (doc.lines[i].link >= 0) {
            int tw = gfx_textWidth(txt, st);
            int uy = y + (st == 2 ? 24 : st == 5 ? 11 : 13);
            disp.drawFastHLine(UI_PAD, uy, tw, S60_LINK);
          }
          if (st == 2) disp.fillRect(UI_PAD, y + 26, SCR_W - 2 * UI_PAD, 1, S60_RULE);
        }
      }
      y += lh;
      visible_lines++;
    }
    draw_mini_map(SCR_W - 18, HDR_H + 26, 84);
    draw_scrollbar(SCR_W - 8, HDR_H + 26, 84, total_px > 0 ? total_px : 1, scroll_px, view_px);
    if (menu_open) render_menu_popup();
    // Focus Block: vien xanh om SAT text/link/block, khong che noi dung ben trong.
    if (focus_y0 >= 0 && focus_y1 > focus_y0 && focus_x1 > focus_x0) {
      uint16_t fc = UI_LINK;
      int fw = focus_x1 - focus_x0;
      int fh = focus_y1 - focus_y0 + 1;
      if (focus_y0 < HDR_H) { fh -= (HDR_H - focus_y0); focus_y0 = HDR_H; }
      if (focus_y0 + fh > SCR_H - FTR_H) fh = SCR_H - FTR_H - focus_y0;
      if (fw > 3 && fh > 3) {
        disp.fillRect(focus_x0, focus_y0, fw, 2, fc);
        disp.fillRect(focus_x0, focus_y0 + fh - 2, fw, 2, fc);
        disp.fillRect(focus_x0, focus_y0, 2, fh, fc);
        disp.fillRect(focus_x0 + fw - 2, focus_y0, 2, fh, fc);
      }
    }
    if (mouse_on) draw_virtual_mouse_cursor(mouse_x, mouse_y);
  }

  // Repaint header after pixel scrolling so partially clipped content cannot bleed into title bar.
  draw_title_bar(loading);

  // softkey bar S60 — nhan trai | dong ho | nhan phai, chu DAM
  const char *l = "Menu", *r = "Back";
  if (vkey_open) { l = "OK"; r = "Cancel"; }
  else if (menu_open) { l = "Select"; r = "Cancel"; }
  s60_softkey_bar(l, r);
  int sy = SCR_H - S60_SOFT_H + (S60_SOFT_H - 8) / 2;
  // Real-time clock. NTP syncs when Internet is available; the system RTC keeps running afterwards.
  // Khi dang load: o giua la progress bar + KB (da ve boi load_paint_footer / on_http_progress).
  if (!loading) {
    clock_update_cache();
    disp.setTextColor(S60_SOFT_TEXT);
    s60_text_center(clock_cache, SCR_W / 2, sy);
  }
  disp.endWrite();          // dong giao dich cua frame_buf (neu dang ve frame)
#if defined(ARDUINO)
  if (frame_ok) {
    gfx_dst = g_panel;      // day len man hinh THAT mot lan — khong nhap nhay
    launcher_set_target(nullptr);
    frame_buf.pushSprite(0, 0);
  }
#endif
}

// hop nhap URL / Search — ban phim ao
static void urlin_show() {
  char buf[256];
  snprintf(buf, sizeof buf,
    "<wml><card title=\"%s\"><p>%s</p></card></wml>",
    is_search_box ? "Search" : "Input URL",
    is_search_box ? "Keyword:" : "URL:");
  build_doc(buf);
  scr = SCR_INPUT; top = 0; cursor_link = 0;
  vkey_bind(url_in, &url_in_n, (int)sizeof url_in, false, is_search_box);
}

// Man khoi dong kieu S60: application pane o tren, logo Nokia-style, progress bar.
static void draw_splash() {
  disp.fillScreen(S60_BG);
  disp.startWrite();
  s60_pane_fill(0, 0, SCR_W, S60_PANE_H);
  disp.fillRect(0, S60_PANE_H - 1, SCR_W, 1, S60_PANE_LINE);
  set_text_font(0); disp.setTextColor(S60_PANE_TEXT);
  s60_text("Symbian S60 interface", 6, S60_PANE_TEXT_Y);
  const int lw = 56, lh = 56;
  const int x0 = (SCR_W - lw) / 2, y0 = 84;
  s60_panel(x0, y0, lw, lh, S60_PANE_MID, S60_PANE_LINE);
  disp.drawCircle(x0 + lw / 2, y0 + lh / 2 - 2, 15, S60_WHITE);   // chu Q
  disp.drawCircle(x0 + lw / 2, y0 + lh / 2 - 2, 14, S60_WHITE);
  disp.fillRect(x0 + lw / 2 + 9, y0 + lh / 2 + 8, 8, 3, S60_WHITE);
  disp.fillRect(x0 + lw / 2 + 15, y0 + lh / 2 + 11, 3, 3, S60_WHITE);
  set_text_font(2); disp.setTextColor(S60_PANE_MID);
  s60_text_center("Qeafbrowser", SCR_W / 2, y0 + lh + 12);
  set_text_font(0); disp.setTextColor(S60_DIM);
  s60_text_center("Default: qeafivels.com", SCR_W / 2, y0 + lh + 38);
  s60_text_center("240x320  keypad browser", SCR_W / 2, y0 + lh + 52);
  const int bx = 30, by = SCR_H - 64, bw = SCR_W - 60;
  disp.fillRect(bx, by, bw, 6, S60_SCROLL_BG);
  disp.drawRect(bx, by, bw, 6, S60_FIELD_LINE);
  disp.fillRect(bx + 1, by + 1, bw / 2, 4, S60_ACCENT);
  disp.endWrite();
}

// ---------------- keypad handler ----------------
static void jump_link(int dir) {
  // Java/Symbian spatial focus: moi khoi chi dung MOT lan.
  // Trong layout mot cot, UP/LEFT = block truoc; DOWN/RIGHT = block sau.
  // Neu khoi co NHIEU link tren cung dong (vd Speed Dial Bookmarks|History|Settings|...),
  // di chuyn truoc di giua cac link TRONG khoi; het link moi sang khoi ke.
  if (!doc.nlines || dir == 0) return;
  int fs = focus_block_start(focus_i), fe = focus_block_end(focus_i);
  if (cursor_link >= 0 && cursor_link < doc.nlinks) {
    if (dir > 0) {
      for (int l = cursor_link + 1; l < doc.nlinks; l++) {
        if (doc.links[l].line0 >= fs && doc.links[l].line0 <= fe) {
          cursor_link = l;
          Serial.printf("[focus] line=%d..%d link=%d top=%d dir=%d\n",
                        focus_i, fe, cursor_link, top, dir);
          return;
        }
      }
    } else {
      for (int l = cursor_link - 1; l >= 0; l--) {
        if (doc.links[l].line0 >= fs && doc.links[l].line0 <= fe) {
          cursor_link = l;
          Serial.printf("[focus] line=%d..%d link=%d top=%d dir=%d\n",
                        focus_i, fe, cursor_link, top, dir);
          return;
        }
      }
    }
  }
  int i = (dir > 0) ? fe + 1 : fs - 1;
  int step = (dir > 0) ? 1 : -1;
  while (i >= 0 && i < doc.nlines) {
    if (doc.lines[i].n == 0) { i += step; continue; }           // dong rong
    if (doc_line_is_separator(&doc, i)) { i += step; continue; } // dau "|" giua 2 link
    break;
  }
  if (i < 0 || i >= doc.nlines) return;  // dung tai bien, khong wrap bat ngo
  focus_i = focus_block_start(i);
  // Vao khoi moi: chon link dau tien (sang phai) / link cuoi cung (sang trai).
  fs = focus_block_start(focus_i); fe = focus_block_end(focus_i);
  cursor_link = -1;
  for (int l = 0; l < doc.nlinks; l++) {
    if (doc.links[l].line0 >= fs && doc.links[l].line0 <= fe) {
      cursor_link = l;
      if (dir < 0) continue;   // LEFT/UP: giu link cuoi cung cua khoi
      break;                   // RIGHT/DOWN: dung o link dau tien
    }
  }
  if (cursor_link < 0) cursor_link = doc.lines[focus_i].link;
  ensure_focus_visible();
  Serial.printf("[focus] line=%d..%d link=%d top=%d dir=%d\n",
                focus_i, focus_block_end(focus_i), cursor_link, top, dir);
}
static void on_key(const char *k) {
  // ---- launcher dieu khien man hinh: moi phim di vao launcher truoc ----
  if (launcher_active()) {
    launcher_key(k);
    return;
  }
  // (Man splash khong con nam phim — launcher tu mo ngay sau setup.)

  // ---- Opera Mini: page overview ----
  if (overview_on) {
    int n = doc.nlines > 0 ? doc.nlines : 1;
    auto clamp_overview_target = [&]() {
      int vh = n / overview_zoom;
      if (vh < 2) vh = 2;
      if (vh > n) vh = n;
      if (ov_oy > n - vh) ov_oy = n - vh;
      if (ov_oy < 0) ov_oy = 0;
      return vh;
    };
    if (!strcmp(k, "back") || !strcmp(k, "menu")) { overview_on = false; ov_anim_active = false; ov_cursor_moving = false; render(); return; }
    if (!strcmp(k, "left")) {
      if (overview_zoom > 1) overview_zoom--;
      clamp_overview_target(); ov_anim_active = true; render(); return;
    }
    if (!strcmp(k, "right")) {
      if (overview_zoom < 8) overview_zoom++;
      clamp_overview_target(); ov_anim_active = true; render(); return;
    }
    int vh = clamp_overview_target();
    int pan_step = vh / 4; if (pan_step < 1) pan_step = 1;
    if (!strcmp(k, "up"))   { ov_oy -= pan_step; clamp_overview_target(); ov_anim_active = true; render(); return; }
    if (!strcmp(k, "down")) { ov_oy += pan_step; clamp_overview_target(); ov_anim_active = true; render(); return; }
    if (!strcmp(k, "ok"))   {
      int old_top = top;
      int old_px = (int)(body_scroll_visual_fp / BODY_FP);
      top = (int)((ov_visual_fp + OV_FP / 2) / OV_FP);
      focus_i = top;
      body_scroll_visual_fp = (int32_t)old_px * BODY_FP;
      ensure_focus_visible();
      if (top == old_top) body_scroll_target_line(top);
      overview_on = false; ov_anim_active = false; ov_cursor_moving = false;
      render(); return;
    }
    render(); return;
  }
  // ---- Opera Mini: virtual mouse ----
  // Chi "an" cac phim dieu khong; OPTION/MENU van mo menu duoc khi chuot bat.
  if (mouse_on && !menu_open &&
      (!strcmp(k, "back") || !strcmp(k, "up") || !strcmp(k, "down") ||
       !strcmp(k, "left") || !strcmp(k, "right") || !strcmp(k, "ok"))) {
    if (!strcmp(k, "back")) { mouse_on = false; render(); return; }
    // up/down: cuộn trang (giup doc dai) + van di chuyen con tro chuot
    if (!strcmp(k, "up"))    { body_scroll_move_by_px(-12); mouse_y -= 12; if (mouse_y < HDR_H) mouse_y = HDR_H; render(); return; }
    if (!strcmp(k, "down"))  { body_scroll_move_by_px(12);  mouse_y += 12; if (mouse_y > SCR_H - FTR_H - MOUSE_CURSOR_VIS_H) mouse_y = SCR_H - FTR_H - MOUSE_CURSOR_VIS_H; render(); return; }
    if (!strcmp(k, "left"))  { mouse_x -= 12; if (mouse_x < 2) mouse_x = 2; render(); return; }
    if (!strcmp(k, "right")) { mouse_x += 12; if (mouse_x > SCR_W - MOUSE_CURSOR_VIS_W) mouse_x = SCR_W - MOUSE_CURSOR_VIS_W; render(); return; }
    if (!strcmp(k, "ok")) {
      // click: chon link gan con tro nhat
      int best = -1, best_d = 10000;
      int scroll_px = (int)(body_scroll_visual_fp / BODY_FP);
      int first_top_px = 0;
      int first_line = doc_line_at_px(scroll_px, &first_top_px);
      int y = UI_ROWS_Y - (scroll_px - first_top_px);
      for (int line = first_line; line < doc.nlines && y < SCR_H - FTR_H - 4; line++) {
        int h = render_line_h(line);
        int li = doc.lines[line].link;
        if (li >= 0 && li < doc.nlinks && mouse_y >= y && mouse_y < y + h) {
          int d = abs(mouse_x - SCR_W / 2);
          if (d < best_d) { best_d = d; best = li; }
        }
        y += h;
      }
      if (best >= 0) { mouse_on = false; go_url(doc.links[best].url); }
      return;
    }
  }

  // ---- ban phim ao ----
  if (vkey_open) {
    const char **rows = vkey_rows();
    if (!strcmp(k, "back")) {
      vkey_open = false;
      if (vkey_is_pass) { wifi_list_show(); render(); }
      else if (menu_src_url[0]) go_url(menu_src_url);
      else go_url("mtt:start");
      return;
    }
    if (!strcmp(k, "menu"))  { vkey_submit(); return; }   // MENU = xong
    if (!strcmp(k, "delete")) { vkey_back(); render(); return; }
    if (!strcmp(k, "up"))    { if (vkey_row > 0) { vkey_row--; if (vkey_row < 4) { int n = (int)strlen(rows[vkey_row]); if (vkey_col >= n) vkey_col = n - 1; } else if (vkey_col >= VK_SP_N) vkey_col = VK_SP_N - 1; } render(); return; }
    if (!strcmp(k, "down"))  {
      if (vkey_row < vkey_max_row()) {
        vkey_row++;
        if (vkey_row < 4) { int n = (int)strlen(rows[vkey_row]); if (vkey_col >= n) vkey_col = n - 1; }
        else if (vkey_row == VK_TLD_ROW) { if (vkey_col >= VK_TLD_N) vkey_col = 0; }
        else vkey_col = 0;
      }
      render(); return;
    }
    if (!strcmp(k, "left"))  { if (vkey_col > 0) vkey_col--; render(); return; }
    if (!strcmp(k, "right")) {
      int n;
      if (vkey_row < 4) n = (int)strlen(rows[vkey_row]);
      else if (vkey_row == VK_TLD_ROW) n = VK_TLD_N;
      else n = VK_SP_N;
      if (vkey_col < n - 1) vkey_col++;
      render(); return;
    }
    if (!strcmp(k, "mode"))  { vkey_shift = !vkey_shift; vkey_sym = false; render(); return; }
    if (!strcmp(k, "option")){ vkey_sym = !vkey_sym; render(); return; }
    if (!strcmp(k, "ok")) {
      if (vkey_row < 4) {
        vkey_put(rows[vkey_row][vkey_col]);
        if (vkey_shift) vkey_shift = false;   // 1 chu HOA roi thuong
      } else if (vkey_row == VK_TLD_ROW) {
        if (vkey_col >= 0 && vkey_col < VK_TLD_N) {
          const char *tld = VK_TLD[vkey_col];
          for (const char *p = tld; *p; p++) vkey_put(*p);
        }
      } else {
        if (vkey_col == VK_SHF) vkey_shift = !vkey_shift;
        else if (vkey_col == VK_SYM_K) vkey_sym = !vkey_sym;
        else if (vkey_col == VK_SPC) vkey_put(' ');
        else if (vkey_col == VK_DEL) vkey_back();
        else if (vkey_col == VK_GO)  { vkey_submit(); return; }
      }
      render(); return;
    }
    render(); return;
  }

  // ---- popup OPTION menu (giong anh legacy browser) ----
  if (menu_open) {
    if (!strcmp(k, "option") || !strcmp(k, "back")) { menu_open = false; render(); return; }
    if (!strcmp(k, "up")) {
      if (menu_in_sub) menu_in_sub = false;
      else if (menu_idx > 0) menu_idx--;
      menu_sub = 0; render(); return;
    }
    if (!strcmp(k, "down")) {
      if (menu_in_sub) { /* giu submenu don */
      }       else if (menu_idx < 7) menu_idx++;
      menu_sub = 0; render(); return;
    }
    if (!strcmp(k, "right")) {
      if (MENUS[menu_idx].child[0]) menu_in_sub = true;
      render(); return;
    }
    if (!strcmp(k, "left")) { menu_in_sub = false; render(); return; }
    if (!strcmp(k, "ok")) {
      menu_open = false;
      // xu ly muc
      if (menu_idx == 0) {          // Bmrk > SvBmrk
        bookmark_add(menu_src_url[0] ? menu_src_url : cur_url,
                     menu_src_title[0] ? menu_src_title : doc.title);
        show_msg("SvBmrk: Bookmark saved. OK to continue.");   // show_msg da render
        return;
      } else if (menu_idx == 1) {   // Optn > SpDial
        go_url("mtt:start"); return;
      } else if (menu_idx == 2) {   // Navg > Zoom (overview)
        overview_on = true; ov_oy = top; overview_zoom = 1; overview_anim_reset();
        render(); return;
      } else if (menu_idx == 3) {   // Tool > Mouse
        mouse_on = !mouse_on;
        if (mouse_on) { mouse_x = 20; mouse_y = HDR_H + 20; }
        render(); return;
      } else if (menu_idx == 4) {   // Sett > WiFi
        go_url("mtt:config"); return;
      } else if (menu_idx == 5) {   // Text > NoImg
        cfg_text_mode = !cfg_text_mode;
        config_save();
        Serial.printf("[cfg] text_mode=%d (menu)\n", cfg_text_mode ? 1 : 0);
        char msg[48];
        snprintf(msg, sizeof msg, "Text mode: %s", cfg_text_mode ? "ON" : "OFF");
        show_msg(msg);
        return;
      } else if (menu_idx == 6) {   // Help
        go_url("mtt:help"); return;
      } else {                      // Exit -> ve launcher (Retro-Go style)
        launcher_set_active(true);
        launcher_redraw();
        return;
      }
      render(); return;
    }
    render(); return;
  }

  if (!strcmp(k, "menu")) { menu_open = false; go_url("mtt:start"); return; }
  if (scr == SCR_MSG) {
    // Trang loi (PAGE_ERROR): OK = thu tai lai; cac phim khac van diu huong
    // binh thuong (back/menu/option/d-pad) — khong duoc "nuot" phim.
    if (!strcmp(k, "ok")) { go_url(cur_url); return; }
    if (!strcmp(k, "back")) { if (!nav_back()) go_url("mtt:start"); return; }
    // option/movement: rot xuong xu ly SCR_BROWSE o duoi
  }
  if (scr == SCR_WIFI_LIST) {
    // D-Pad di chuyen Focus Block (len/xuong/trai/phai) — giong trang chinh, khong rebuild doc.
    if (!strcmp(k, "up") || !strcmp(k, "left")) {
      if (net_cursor > 0) { net_cursor--; wifi_list_focus(); }
    } else if (!strcmp(k, "down") || !strcmp(k, "right")) {
      if (net_cursor < net_n - 1) { net_cursor++; wifi_list_focus(); }
    }
    else if (!strcmp(k, "ok")) { char u[24]; snprintf(u, sizeof u, "mtt:wifisel#%d", net_cursor); go_url(u); return; }
    else if (!strcmp(k, "back") || !strcmp(k, "menu")) { go_url("mtt:config"); return; }
    else if (!strcmp(k, "option")) {
      cfg_ssid = ""; cfg_pass = ""; config_save();
      WiFi.disconnect(); wifi_up = false;      // quen mang da luu
      wifi_scan(); wifi_list_show();
    }
    render(); return;   // chi nhung nhanh tren (up/down/option) can render o day
  }
  if (scr == SCR_WIFI_PASS) {
    if (!strcmp(k, "back")) { wifi_scan(); wifi_list_show(); render(); return; }
    if (!strcmp(k, "delete")) {
      if (mt_slot >= 0) mt_slot = -1;
      else if (pass_n) { pass_n--; pass_in[pass_n] = 0; }
      wifi_pass_show(); render(); return;
    }
    if (!strcmp(k, "ok")) {
      mt_commit();
      Serial.printf("[wifi] try connect '%s' pass='%s'\n", nets[net_cursor].ssid, pass_in);
      wifi_result(wifi_try_connect(nets[net_cursor].ssid, nets[net_cursor].open ? "" : pass_in));
      render(); return;
    }
    if (!strcmp(k, "mode")) {            // SELECT: giu>600ms=shift HOA/thuong, nhan=space
      if (t9mode != pass_t9_prev) {      // vua toggle => shift
        shift_on = t9mode;
        pass_t9_prev = t9mode;
      } else {
        mt_commit();
        if (pass_n < (int)sizeof(pass_in) - 1) { pass_in[pass_n++] = ' '; pass_in[pass_n] = 0; }
      }
      wifi_pass_show(); render(); return;
    }
    int slot = mt_find(k);
    if (slot >= 0) { mt_key(slot); wifi_pass_show(); render(); return; }
    return;
  }
  if (scr == SCR_WIFI_RESULT) {
    if (!strcmp(k, "ok")) {
      if (wifi_up) { go_url("mtt:start"); return; }   // go_url da render
      wifi_result(wifi_try_connect(nets[net_cursor].ssid, pass_in));
      render(); return;
    }
    if (!strcmp(k, "back")) { wifi_list_show(); render(); return; }
    if (!strcmp(k, "menu")) { go_url("mtt:start"); return; }
    return;
  }
  if (scr == SCR_INPUT) {
    if (!strcmp(k, "ok")) {           // OK = mo URL / Search
      mt_commit();
      if (is_search_box) {
        if (url_in_n) {
          is_search_box = false;
          open_search_query(url_in);
        } else go_url("mtt:start");
      } else {
        if (url_in_n && strcmp(url_in, "https://") && strcmp(url_in, "http://"))
          open_user_url(url_in);
        else go_url("mtt:start");
      }
      return;                         // cac nhanh tren da go_url -> da render
    } else if (!strcmp(k, "back")) {
      mt_commit();
      if (menu_src_url[0]) go_url(menu_src_url); else go_url("mtt:start");
      return;
    } else if (!strcmp(k, "menu")) {
      mt_commit(); go_url("mtt:start"); return;
    } else if (!strcmp(k, "delete")) {
      if (mt_slot >= 0) mt_slot = -1;
      else if (url_in_n) { url_in_n--; url_in[url_in_n] = 0; }
      urlin_show();
    } else if (!strcmp(k, "mode")) {  // SELECT: giu>600ms=shift, nhan=ky tu trong nhom mode
      if (t9mode != pass_t9_prev) {
        shift_on = t9mode;
        pass_t9_prev = t9mode;
      } else {
        int slot = mt_find("mode");
        if (slot >= 0) mt_key(slot);
      }
      urlin_show();
    } else {
      int slot = mt_find(k);
      if (slot >= 0) mt_key(slot);
      urlin_show();
    }
    render(); return;
  }
  if (!strcmp(k, "back")) {
    if (!nav_back()) go_url("mtt:start");
    return;
  }
  // D-Pad: di chuyen Focus Block giua cac khoi noi dung
  if (!strcmp(k, "up"))    { jump_link(-1); body_scroll_kick(-1); render(); return; }
  if (!strcmp(k, "down"))  { jump_link(1);  body_scroll_kick(1);  render(); return; }
  if (!strcmp(k, "left"))  { jump_link(-1); body_scroll_kick(-1); render(); return; }
  if (!strcmp(k, "right")) { jump_link(1);  body_scroll_kick(1);  render(); return; }
  if (!strcmp(k, "ok")) {
    // OK: mo link dang chon (cursor_link) — ho tro nhieu link tren cung dong
    int li = -1;
    if (focus_i >= 0 && focus_i < doc.nlines) {
      int fs = focus_block_start(focus_i), fe = focus_block_end(focus_i);
      if (cursor_link >= 0 && cursor_link < doc.nlinks &&
          doc.links[cursor_link].line0 >= fs && doc.links[cursor_link].line0 <= fe)
        li = cursor_link;
      else
        li = doc.lines[focus_i].link;
    }
    if (li >= 0 && li < doc.nlinks) go_url(doc.links[li].url);
    else Serial.printf("[focus] select text block line=%d\n", focus_i);
    return;
  }
  if (!strcmp(k, "option")) {
    // popup menu Bmrk/Optn/Navg/Tool/Sett/Help/Exit
    strncpy(menu_src_url, cur_url, sizeof menu_src_url - 1);
    menu_src_url[sizeof menu_src_url - 1] = 0;
    strncpy(menu_src_title, doc.title, sizeof menu_src_title - 1);
    menu_src_title[sizeof menu_src_title - 1] = 0;
    menu_open = true; menu_idx = 0; menu_sub = 0; menu_in_sub = false;
    Serial.println("[menu] open Bmrk/Optn/Navg/Tool/Sett/Help/Exit");
    render(); return;
  }
  if (!strcmp(k, "delete")) { go_url("mtt:history"); return; }
}

// host test: mo URL truc tiep (khong di menu)
extern "C" void sim_go_url(const char *url) { go_url(url); }
// launcher -> browser: dong launcher va ve trang chu ngay (go_url tu render)
extern "C" void sim_key_launch_go_home(void) {
  launcher_set_active(false);
  go_url("mtt:start");
}
// host test: ve lai man khoi dong de regression chup duoc theme S60
// (setup() da ve splash roi di tiep qua WiFi/home nen harness khong kip chup).
extern "C" void sim_draw_splash() { draw_splash(); }
// host test: xuat layout da parse ra stdout (khong can ban phim serial)
extern "C" void sim_doc_dump() { cli_doc(); }
extern "C" int sim_doc_nlinks() { return doc.nlinks; }
extern "C" const char *sim_doc_link(int i) {
  return (i >= 0 && i < doc.nlinks) ? doc.links[i].url : "";
}
extern "C" int sim_doc_nimages() { return doc.nimages; }
extern "C" const char *sim_doc_image_url(int i) {
  return (i >= 0 && i < doc.nimages) ? doc.images[i].url : "";
}
extern "C" const char *sim_doc_title() { return doc.title; }
extern "C" int sim_doc_nlines() { return doc.nlines; }
extern "C" const char *sim_doc_line(int i) {
  if (i < 0 || i >= doc.nlines) return "";
  return doc.buf + doc.lines[i].off;
}
extern "C" int sim_body_scroll_px() { return (int)(body_scroll_visual_fp / BODY_FP); }
extern "C" int sim_body_scroll_target_px() { return (int)(body_scroll_target_fp / BODY_FP); }
extern "C" int sim_body_scroll_max_px() { return body_scroll_max_px(); }
extern "C" int sim_mouse_on() { return mouse_on ? 1 : 0; }
extern "C" int sim_body_scroll_active() { return body_scroll_active ? 1 : 0; }
extern "C" int sim_body_scroll_velocity_fp() { return (int)body_scroll_velocity_fp; }
extern "C" int sim_body_scroll_inertia() { return body_scroll_inertia ? 1 : 0; }
extern "C" int sim_thumb_request(const char *url) {
  ThumbCache *t = thumb_find(url);
  return (t && t->ok) ? 1 : 0;
}

// host bridge: sim go ban phim PC vao ban phim ao / hop nhap
extern "C" void url_input_char(char c) {
  if (vkey_open) {
    if (c == '\r' || c == '\n') { vkey_submit(); return; }
    if (c == 8 || c == 127) { vkey_back(); render(); return; }
    if (isprint((unsigned char)c)) vkey_put(c);
    render(); return;
  }
  if (scr != SCR_INPUT) return;
  if (c == '\r' || c == '\n') {
    mt_commit();
    if (is_search_box) {
      if (url_in_n) {
        is_search_box = false;
        open_search_query(url_in);
      } else go_url("mtt:start");
    } else if (url_in_n && strcmp(url_in, "https://") && strcmp(url_in, "http://"))
      open_user_url(url_in);
    else go_url("mtt:start");
    return;
  }
  if (c == 8 || c == 127) {
    mt_slot = -1;
    if (url_in_n) { url_in_n--; url_in[url_in_n] = 0; }
    urlin_show(); render(); return;
  }
  if (url_in_n < (int)sizeof url_in - 1 && isprint((unsigned char)c)) {
    mt_slot = -1;
    url_in[url_in_n++] = c;
    url_in[url_in_n] = 0;
  }
  urlin_show(); render();
}

// ---------------- serial CLI (dieu khien firmware tu PC) ----------------
// lenh: go <url> | key <name> | scan | status | wifi <ssid> <pass> | fonttest
static char cli_in[160]; static int cli_n = 0;

static void cli_fonttest() {
  static const char *samples[] = {
    "Settings", "Timezone: ICT-7", "Qeafbrowser",
    "ghijklmnop 0123456789", "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  };
  const int styles[] = { 0, 2, 5 };
  for (int si = 0; si < 3; si++) {
    int st = styles[si];
    set_text_font(st);
    Serial.printf("[font] style=%d lh=%d\n", st, line_h(st));
    for (int i = 0; i < 5; i++) {
      int tw = disp.textWidth(samples[i]);
      Serial.printf("  w=%3d \"%s\"\n", tw, samples[i]);
    }
  }
  Serial.println("[font] fonttest done");
}

// Xuat layout da parse (dong / style / chieu cao / vi tri y) — dung de kiem tra
// theme S60: dong style 3 la mot hang danh sach co o icon.
static void cli_doc() {
  Serial.printf("[doc] title=\"%s\" lines=%d links=%d images=%d wml=%d\n",
                doc.title, doc.nlines, doc.nlinks, doc.nimages, doc.is_wml ? 1 : 0);
  int y = UI_ROWS_Y;
  for (int i = 0; i < doc.nlines; i++) {
    char t[44];
    int n = doc.lines[i].n; if (n > 43) n = 43;
    if (n) memcpy(t, doc.buf + doc.lines[i].off, (size_t)n);
    t[n] = 0;
    int h = render_line_h(i);
    Serial.printf("  %3d y=%3d h=%2d st=%d link=%2d blk=%u \"%s\"\n",
                  i, y, h, doc.lines[i].style, doc.lines[i].link,
                  (unsigned)doc.lines[i].block, t);
    y += h;
  }
}

// Xuat framebuffer 240x320 RGB565 qua serial (protocol bin):
//   "SHOT2 <len>\n" + raw BE bytes + "\nEND\n
// Launcher ve truc tiep panel nen khi launcher dang mo phai ve lai vao frame_buf.
static void cli_shot() {
#if defined(ARDUINO)
  if (!frame_ok) { Serial.println("[shot] no frame buffer"); return; }
  if (launcher_active()) {
    launcher_set_target(&frame_buf);
    launcher_redraw();
    launcher_set_target(nullptr);
    // day len man hinh THAT de UI launcher van hien dung sau khi redirect
    gfx_dst = g_panel;
    frame_buf.pushSprite(0, 0);
  }
  const size_t n = (size_t)SCR_W * SCR_H * 2;
  Serial.printf("SHOT2 %u\n", (unsigned)n);
  Serial.flush();
  const uint8_t *p = (const uint8_t *)frame_buf.getBuffer();
  // Gui theo block 512B de khong tran UART FIFO
  size_t off = 0;
  while (off < n) {
    size_t chunk = n - off; if (chunk > 512) chunk = 512;
    Serial.write(p + off, chunk);
    off += chunk;
  }
  Serial.println("\nEND");
  Serial.flush();
#else
  Serial.println("[shot] sim only");
#endif
}

static void cli_exec(const char *line) {
  if (!strncmp(line, "go ", 3)) { go_url(line + 3); return; }
  if (!strncmp(line, "key ", 4)) { on_key(line + 4); return; }
  if (!strcmp(line, "scan")) { wifi_scan(); wifi_list_show(); render(); return; }
  if (!strcmp(line, "doc")) { cli_doc(); return; }
  if (!strcmp(line, "fonttest")) { cli_fonttest(); return; }
  if (!strcmp(line, "shot")) { cli_shot(); return; }
  if (!strncmp(line, "wifi ", 5)) {
    char ssid[33] = {0}, pass[65] = {0};
    const char *p = line + 5;
    int i = 0;
    while (*p && *p != ' ' && i < 32) ssid[i++] = *p++;
    while (*p == ' ') p++;
    i = 0;
    while (*p && i < 64) pass[i++] = *p++;
    if (!ssid[0]) { Serial.println("[cli] wifi <ssid> <pass>"); return; }
    Serial.printf("[cli] wifi try '%s'...\n", ssid);
    bool ok = wifi_try_connect(ssid, pass);
    if (ok) {
      cfg_ssid = ssid; cfg_pass = pass;
      config_save();
      wifi_up = true;
      clock_start_ntp();
      Serial.printf("[cli] wifi OK ip=%s\n", WiFi.localIP().toString().c_str());
      go_url(cfg_home.length() ? cfg_home.c_str() : "https://qeafivels.com/");
    } else {
      wifi_up = false;
      Serial.println("[cli] wifi FAIL");
    }
    return;
  }
  if (!strcmp(line, "status")) {
    clock_update_cache();
    Serial.printf("[status] wifi=%s ip=%s clock=%s%s tz=%s url=%s\n",
                  wifi_up ? "up" : "-",
                  wifi_up ? WiFi.localIP().toString().c_str() : "-",
                  clock_synced ? "sync " : "wait ", clock_cache,
                  cfg_tz.c_str(), cur_url);
    return;
  }
  if (line[0]) Serial.printf("[cli] ? %s\n", line);
}

static void serial_poll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (vkey_open) { url_input_char(c); continue; }
    if (c == '\r') continue;
    if (c == '\n') {
      cli_in[cli_n] = 0; cli_n = 0;
      cli_exec(cli_in);
      continue;
    }
    if (c == 8 || c == 127) { if (cli_n) cli_n--; continue; }
    if (cli_n < (int)sizeof cli_in - 1) cli_in[cli_n++] = c;
  }
}

// ---------------- setup/loop ----------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("[boot] 1 serial");
  Serial.flush();
  Serial.println("[boot] 1b pre-panel");
  Serial.flush();
  Serial.println("[boot] 1c panel.init enter");
  Serial.flush();
  g_panel->init();          // disp.init() khong duoc (macro disp = LovyanGFX*)
  Serial.println("[boot] 2 display");
  Serial.flush();
  g_panel->setRotation(0);                 // doc 240x320 theo E524546-OS
  Serial.println("[boot] 2b rot");
  Serial.flush();
  g_panel->setBrightness(200);
  Serial.println("[boot] 2bb bright");
  Serial.flush();
  g_panel->fillScreen(TFT_BLACK);
  Serial.println("[boot] 2c fill");
  Serial.flush();

  doc_buf = (char *)heap_caps_malloc(DOC_CAP, MALLOC_CAP_SPIRAM);
  net_buf = (char *)heap_caps_malloc(DOC_CAP, MALLOC_CAP_SPIRAM);
  if (!doc_buf) doc_buf = (char*)malloc(DOC_CAP);
  if (!net_buf) net_buf = (char*)malloc(DOC_CAP);
  // Doc struct (~32KB) cung chuyen sang PSRAM de WiFi/TLS du RAM noi bo.
  g_doc = (Doc *)heap_caps_malloc(sizeof(Doc), MALLOC_CAP_SPIRAM);
  if (!g_doc) g_doc = (Doc *)malloc(sizeof(Doc));
  if (g_doc) memset(g_doc, 0, sizeof(Doc));
  if (!g_doc || !doc_buf || !net_buf)
    Serial.println("[mem] CRITICAL: cannot alloc PSRAM/heap for doc buffers");
  doc.buf = doc_buf; doc.cap = DOC_CAP;
#if defined(ARDUINO)
  // Backbuffer 240x320 RGB565 (153600 byte) trong PSRAM — chong nhap nhay man hinh.
  frame_buf.setPsram(true);
  if (frame_buf.createSprite(SCR_W, SCR_H)) frame_ok = true;
  Serial.printf("[gfx] frame buffer PSRAM 240x320: %s\n", frame_ok ? "OK" : "MISSING -> draw direct");
#endif
#if defined(ARDUINO)
  Serial.printf("[mem] boot: heap=%u maxblk=%u psram=%u psram_free=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
#endif
  store_init();
  Serial.println("[boot] 3 store");
  Serial.flush();
  launcher_begin(false);      // nap theme + mount SD + khoi tao art task
  keys_init();
  key_cb = on_key;
  Serial.println("[boot] 4 keys");
  Serial.flush();

  draw_splash();
  delay(1200);
  Serial.println("[boot] 5 splash");
  Serial.flush();
  // Mo dau vao launcher (Retro-Go style) — phim bat ky de vao browser.
  launcher_set_active(true);

  WiFi.persistent(true);                 // luu tai WiFi vao flash (bo nho tam)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                  // khong cho WiFi sleep: ket noi on dinh + CPU khong bi cham
  WiFi.setAutoReconnect(true);           // tu ket noi lai khi mat mang do AP
  if (cfg_ssid.length()) {
    WiFi.begin(cfg_ssid.c_str(), cfg_pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
    wifi_up = (WiFi.status() == WL_CONNECTED);
    if (!wifi_up) WiFi.disconnect();
    Serial.printf("[wifi] %s ip=%s\n", wifi_up ? "OK" : "FAIL",
                  wifi_up ? WiFi.localIP().toString().c_str() : "-");
  }
#if defined(ARDUINO)
  Serial.printf("[mem] after wifi: heap=%u maxblk=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
#endif
  if (wifi_up) clock_start_ntp();
  else Serial.println("[wifi] Go to Settings > Connect WiFi to scan + enter manually");
  if (launcher_active()) {
    // Launcher dang mo (boot): KHONG cho browser go_url de ve lai man — launcher
    // tu chup lai man hinh; browser chi build page khi nguoi mo muc Browser.
    Serial.println("[boot] launcher active -> browser page load da hoan on start");
  } else if (wifi_up) {
    go_url(cfg_home.length() ? cfg_home.c_str() : "https://qeafivels.com/");
  } else {
    go_url("mtt:start");   // offline: Speed Dial, khong dung man "Connection Timeout"
  }
  Serial.println("[boot] setup done. CLI: go <url> | key <name> | scan | status | wifi <ssid> <pass> | shot | doc");
  Serial.flush();
}

void loop() {
  static bool once = false;
  if (!once) { once = true; Serial.println("[boot] loop enter"); Serial.flush(); }
  keys_poll();
  serial_poll();
  // Launcher: ve phan art moi ve (task decode xong) — chi khi launcher dang mo.
  if (launcher_active()) launcher_tick();
  // multi-tap: qua 900ms khong phim -> tu khoa ky tu dang cho (nhan dien Nokia)
  if (scr == SCR_WIFI_PASS && mt_slot >= 0 && millis() - mt_last > 900) {
    mt_commit();
    wifi_pass_show();
    render();
  }
  // Overview animation is incremental and allocation-free. Redraw only while easing.
  if (overview_on && (ov_anim_active || ov_cursor_moving ||
      ov_visual_fp != (int32_t)ov_oy * OV_FP || ov_zoom_fp != (int32_t)overview_zoom * OV_FP)) {
    if (overview_anim_tick()) render();
  } else if (!overview_on && scr == SCR_BROWSE &&
             (body_scroll_active || body_scroll_inertia || body_scroll_velocity_fp != 0)) {
    if (body_scroll_tick()) render();
  }
  // Detect Internet/WiFi becoming available later (auto-reconnect) and start NTP then.
  static uint32_t net_watch_tick = 0;
  if (millis() - net_watch_tick >= 2000) {
    net_watch_tick = millis();
    bool now_up = (WiFi.status() == WL_CONNECTED);
    if (now_up && !wifi_up) {
      wifi_up = true;
      clock_ntp_started = false;
      clock_start_ntp();
      Serial.println("[clock] network restored; NTP requested");
    } else if (!now_up && wifi_up) {
      wifi_up = false;
      if (!clock_synced) clock_ntp_started = false;
      Serial.println("[clock] network lost; RTC keeps last synchronized time");
    }
  }
  // Update only the small clock area once per second; no full-screen redraw/task/framebuffer.
  static uint32_t clock_ui_tick = 0;
  if (millis() - clock_ui_tick >= 1000) {
    clock_ui_tick = millis();
    clock_draw_footer_only();
    // launcher statusbar dung chung gio + wifi (khong ve lai, chi cache)
    launcher_set_clock(clock_cache);
    launcher_set_net(wifi_up);
  }
  static uint32_t hb = 0;
  if (millis() - hb > 15000) { hb = millis(); }   // (no-op giu cho struct tinh)
  delay(5);
}
