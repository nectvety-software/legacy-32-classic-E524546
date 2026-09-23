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

// ---------------- UI palette (RGB565) — theo legacy keypad browser_GFX_Accuracy_Comparison ----------------
// UI — UC Browser / Opera Mini (OSNews)
#define UI_TITLE  0xC800    // red title
#define UI_SOFT   0x0000    // black status bar
#define UI_BG     0xFFFF    // white article
#define UI_FG     0x0000
#define UI_LINK   0x001F    // blue
#define UI_HOT    0xF800    // red
#define UI_SEL    0xC800
#define UI_SELFG  0xFFFF
#define UI_FIELD  0xD6D7
#define UI_BORDER 0x8C71
#define UI_DIM    0x8C71
#define UI_WHITE  0xFFFF
#define UI_ROW    0xEF7C
#define UI_SHADOW 0x4A69
#define UI_ARROW  0xB5B6

#define UI_HDR_H  22
#define UI_FTR_H  18
#define UI_PAD    8       // le trai/phai chuan
#define UI_ROWS_Y (UI_HDR_H + 6)

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
  uint16_t bg = sel ? UI_SEL : UI_FIELD;
  uint16_t fg = sel ? UI_SELFG : (dim ? UI_DIM : UI_FG);
  disp.fillRect(x, y, w, h, bg);
  disp.fillRect(x, y, w, 1, UI_BORDER);
  disp.fillRect(x, y + h - 1, w, 1, UI_BORDER);
  disp.fillRect(x, y, 1, h, UI_BORDER);
  disp.fillRect(x + w - 1, y, 1, h, UI_BORDER);
  disp.setTextFont(2); disp.setTextColor(fg);
  int tw = disp.textWidth(label);
  disp.drawString(label, x + (w - tw) / 2, y + (h - 8) / 2);
}

static void vkey_draw() {
  // truong nhap
  disp.fillRect(UI_PAD, UI_HDR_H + 6, SCR_W - 2 * UI_PAD, 30, UI_FIELD);
  disp.fillRect(UI_PAD, UI_HDR_H + 6, SCR_W - 2 * UI_PAD, 1, UI_BORDER);
  disp.fillRect(UI_PAD, UI_HDR_H + 35, SCR_W - 2 * UI_PAD, 1, UI_BORDER);
  disp.fillRect(UI_PAD, UI_HDR_H + 6, 1, 30, UI_BORDER);
  disp.fillRect(SCR_W - UI_PAD - 1, UI_HDR_H + 6, 1, 30, UI_BORDER);
  char shown[72];
  int vn = vkey_n ? *vkey_n : 0;
  int copy = vn < 28 ? vn : 28;
  if (vkey_is_pass) { for (int i = 0; i < copy; i++) shown[i] = '*'; }
  else memcpy(shown, vkey_buf, copy);
  shown[copy] = 0;
  if (copy < 28) { shown[copy] = '_'; shown[copy + 1] = 0; }
  disp.setTextFont(2); disp.setTextColor(UI_FG);
  disp.drawString(shown, UI_PAD + 6, UI_HDR_H + 13);

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
    if (on) disp.fillRect(UI_PAD + i * sw + 2, ky + 27, sw - 6, 2, UI_SEL);
  }
  // ghi chu
  disp.setTextFont(2); disp.setTextColor(UI_DIM);
  disp.drawString(vkey_is_search ? "UP/DOWN/LEFT/RIGHT chon, OK=go" : "OK=ky tu, A=huy, GO=xong",
                  UI_PAD, ky + 36);
}
// vkey_submit nam sau wifi_result (can nets/pass_in)

// ---------------- font/text helper (wml.cpp khai bao extern "C") ----------------
extern "C" int gfx_textWidth(const char *nul_term, int style) {
  if (style == 2) disp.setTextFont(4);
  else if (style == 5) disp.setTextFont(1);
  else disp.setTextFont(2);
  return disp.textWidth(nul_term);
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

// Footer thay dong gio bang progress + so KB khi dang load (Opera Mini 4 / UC style).
static void load_paint_footer() {
  int fy = SCR_H - UI_FTR_H;
  int sy = fy + (UI_FTR_H - 8) / 2;
  disp.startWrite();
  disp.fillRect(0, fy, SCR_W, UI_FTR_H, UI_SOFT);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString("Menu", UI_PAD, sy);
  const char *r = "Back";
  disp.drawString(r, SCR_W - disp.textWidth(r) - UI_PAD, sy);
  // vung giua: [progress bar] + KB
  int cx0 = UI_PAD + 30;
  int cx1 = SCR_W - UI_PAD - 30;
  int kb_w = (int)strlen(load_paint_kb) * 6 + 6;
  int bw = cx1 - cx0 - kb_w;
  if (bw < 32) bw = 32;
  int by = fy + (UI_FTR_H - 8) / 2 - 1;
  if (by < fy + 2) by = fy + 2;
  disp.fillRect(cx0, by, bw, 8, 0x4208);              // track
  int fw = load_pct * bw / 100;
  if (fw > 0) disp.fillRect(cx0, by, fw, 8, UI_LINK); // blue fill
  disp.setTextColor(UI_WHITE);
  disp.drawString(load_paint_kb, cx0 + bw + 4, sy);
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
    disp.fillRect(16, (UI_HDR_H - 13) / 2, 150, 14, UI_TITLE);
    disp.setTextFont(2); disp.setTextColor(UI_WHITE);
    disp.drawString("Receiving...", 18, (UI_HDR_H - 13) / 2);
  }
  // thanh 3px duoi title
  disp.fillRect(0, UI_HDR_H - 3, SCR_W, 3, UI_TITLE);
  disp.fillRect(0, UI_HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);
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
static uint16_t g_png_line[1024];
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
static void clock_paint(lgfx::LovyanGFX *g, int sy, int cx) {
  g->startWrite();
  g->fillRect(cx - 30, SCR_H - UI_FTR_H, 60, UI_FTR_H, UI_SOFT);
  g->setTextFont(2); g->setTextColor(UI_WHITE);
  g->drawString(clock_cache, cx - g->textWidth(clock_cache) / 2, sy);
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
  disp.fillRect(cx - 30, SCR_H - UI_FTR_H, 60, UI_FTR_H, UI_SOFT);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString(clock_cache, cx - disp.textWidth(clock_cache) / 2, sy);
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
  Serial.println("[wifi] quet mang THAT (WiFi.scanNetworks)...");
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
  Serial.printf("[wifi] thay %d mang THAT\n", net_n);
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
    build_doc("<wml><card title=\"WiFi\"><p>Khong du bo nho RAM.</p></card></wml>");
    scr = SCR_WIFI_LIST; top = 0; focus_i = 0; cursor_link = -1;
    return;
  }
  int o = snprintf(buf, 6000,
    "<wml><card title=\"WiFi\"><p>Chon mang WiFi:<br/></p><p>");
  for (int i = 0; i < net_n; i++)
    o += snprintf(buf + o, 6000 - o, "<a href=\"mtt:wifisel#%d\">%s %s</a><br/>",
                  i, nets[i].ssid, nets[i].open ? "(mo)" : "");
  if (!net_n) o += snprintf(buf + o, 6000 - o, "(khong thay mang nao — OPTION de quen lai)");
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
      snprintf(connect_msg, sizeof connect_msg, "Dang ket noi %s...%ds", ssid, sec);
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
    "<wml><card title=\"WiFi\"><p>Mang: %s%s</p></card></wml>",
    nets[net_cursor].ssid, nets[net_cursor].open ? " (mo)" : "");
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
    build_docf("<wml><card title=\"WiFi\"><p>KET NOI THANH CONG<br/>%s<br/>IP %s<br/>"
               "Cau hinh da luu va /Qeafbrowser/config.ini</p></card></wml>",
               cfg_ssid.c_str(), WiFi.localIP().toString().c_str());
  } else {
    wifi_up = false;
    build_docf("<wml><card title=\"WiFi\"><p>KET NOI THAT BAI<br/>%s<br/>"
               "Kiểm tra mat khau.<br/>OK = thu lai, A = quay lai danh sach</p></card></wml>",
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
  snprintf(u, sizeof u, "https://www.google.com/search?q=%s", enc);
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
      Serial.printf("[wifi] thu ket noi '%s' pass='%s'\n", nets[net_cursor].ssid, pass_in);
      wifi_result(wifi_try_connect(nets[net_cursor].ssid, nets[net_cursor].open ? "" : pass_in));
      render();
    }
    return;
  }
  if (vkey_buf && vkey_buf[0]) open_user_url(vkey_buf);
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

static void keys_init() {
  for (int i = 0; i < NKEYS; i++) {
    pinMode(KEYS[i].pin, INPUT_PULLUP);
    last_state[i] = HIGH; last_change[i] = 0; press_start[i] = 0;
  }
}
static void keys_poll() {
  unsigned long now = millis();
  for (int i = 0; i < NKEYS; i++) {
    bool v = digitalRead(KEYS[i].pin);
    if (v == last_state[i]) continue;
    if (now - last_change[i] <= 25) continue;
    last_state[i] = v; last_change[i] = now;
    bool is_select = (KEYS[i].pin == KEY_SELECT);
    if (v == LOW) {
      press_start[i] = now;
      if (!is_select && key_cb) key_cb(t9mode ? KEYS[i].t9 : KEYS[i].game);
    } else if (is_select) {              // SELECT: phat khi nha (can do thoi gian giu)
      if (now - press_start[i] >= 600) { // giu >600ms: dao che do + phat "mode"
        t9mode = !t9mode;
        Serial.printf("[key] che do: %s\n", t9mode ? "T9" : "GAME");
        if (key_cb) key_cb("mode");
      } else {                           // nhan ngan: "0" (T9) hoac "mode" (Game)
        if (key_cb) key_cb(t9mode ? "0" : "mode");
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
  "<p><folder><a href=\"https://m.youtube.com/\">YouTube</a></folder></p>"
  "<p><folder><a href=\"https://qeafivels.com/\">Qeafivels Home</a></folder></p>"
  "<p><folder><a href=\"http://m.weather.com/\">Weather</a></folder></p>"
  "<p><folder><a href=\"http://tubidy.mobi/\">Tubidy</a></folder></p>"
  "<p><folder><a href=\"mtt:web\">More Sites</a></folder></p>"
  "</card></wml>";

static const char *PAGE_ABOUT =
  "<wml><card title=\"About\">"
  "<p>Qeafbrowser 2.0<br/>Opera Mini 4 style mode<br/>"
  "HTTP + HTTPS / HTML + WML<br/><br/>"
  "Default home: https://qeafivels.com/<br/>ESP32-S3 port.</p></card></wml>";

static const char *PAGE_HELP =
  "<wml><card title=\"Help\"><p>UP/DOWN: cuoc trang<br/>LEFT/RIGHT: link truoc/sau<br/>"
  "OK: mo link<br/>BACK: lui<br/>MENU: ve trang chu<br/>OPTION: menu<br/>"
  "(Bmrk/Optn/Navg/Tool/Sett/Help/Exit)<br/>"
  "SELECT giu: doi T9<br/><br/>"
  "Nhap URL: Tool &gt; InpURL<br/>domain tu dong HTTPS<br/>Tools &gt; Forward de tien trang<br/>"
  "multi-tap hoac ban phim PC (sim)<br/>"
  "WiFi: Sett &gt; WiFi</p></card></wml>";

static const char *PAGE_CONFIG =
  "<wml><card title=\"Settings\"><p>WiFi: %s<br/>SSID: %s<br/>Home: %s<br/>Timezone: %s<br/><br/>"
  "<a href=\"mtt:wifi\">&gt;&gt; Ket noi WiFi (quet mang)</a><br/>"
  "<a href=\"mtt:wifi\">&gt;&gt; Doi mat khau WiFi</a><br/><br/>"
  "Mac dinh tu dong ket noi lai mang da luu.<br/>"
  "Sua sau: /Qeafbrowser/config.ini tren the SD</p></card></wml>";

static const char *PAGE_WEB =
  "<wml><card title=\"Qeafbrowser Web\"><p>"
  "<a href=\"https://qeafivels.com/\">Qeafivels</a><br/>"
  "<a href=\"http://pokoyo.wapka.mobi/\">PokoyoWap</a><br/>"
  "<a href=\"http://google.com/\">Google</a><br/>"
  "<a href=\"http://wap.yahoo.com/\">Yahoo!</a><br/>"
  "<a href=\"http://tubidy.mobi/\">Tubidy</a><br/>"
  "<a href=\"http://wap.c2.hu/\">C2 Mail</a><br/>"
  "<a href=\"https://qeafivels.com/\">Qeafivels Home</a></p></card></wml>";

static const char *PAGE_SITES =
  "<wml><card title=\"Mobile Sites\">"
  "<p><a href=\"http://m.google.com/\">Google Mobile</a><br/>"
  "<a href=\"http://m.facebook.com/\">Facebook</a><br/>"
  "<a href=\"http://m.youtube.com/\">YouTube</a><br/>"
  "<a href=\"http://en.m.wikipedia.org/\">Wikipedia</a></p></card></wml>";

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
  "<a href=\"mtt:bookmark\">Bookmark</a></p></card></wml>";

static const char *PAGE_DL =
  "<wml><card title=\"Downloads\"><p>Khong co download.<br/>"
  "Lich su da xem o <a href=\"mtt:history\">History</a></p></card></wml>";

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
  if (!TJpgDec.getJpgSize(&sw, &sh, buf, len) || !sw || !sh) return false;
  uint8_t scale = 1;
  while (scale < 8 && ((sw / scale) > THUMB_W || (sh / scale) > THUMB_H)) scale <<= 1;
  int out_w = sw / scale; if (out_w < 1) out_w = 1;
  int out_h = sh / scale; if (out_h < 1) out_h = 1;
  memset(t->pix, 0xFF, THUMB_W * THUMB_H * sizeof(uint16_t));
  g_thumb_ctx = { t, (int)sw, (int)sh, out_w, out_h, (THUMB_W - out_w) / 2, (THUMB_H - out_h) / 2, false };
  TJpgDec.setCallback(jpg_thumb_output);
  TJpgDec.setJpgScale(scale);
  // TJpg_Decoder 1.1.0 accepts an in-memory JPEG buffer and uint32_t length.
  if (len > 0xFFFFFFFFu) return false;
  TJpgDec.drawJpg(0, 0, buf, (uint32_t)len);
  t->ok = g_thumb_ctx.ok;
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
static int png_thumb_draw(PNGDRAW *pDraw) {
  ThumbDecodeCtx &c = g_thumb_ctx;
  if (!g_png || !pDraw || !c.t || c.src_w < 1 || c.src_h < 1 ||
      c.src_w > (int)(sizeof g_png_line / sizeof g_png_line[0])) return 1;
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
  if (sw < 1 || sh < 1 || sw > (int)(sizeof g_png_line / sizeof g_png_line[0])) { png->close(); return false; }
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
  // Buffer lay anh 32KB dat trong PSRAM (lazy) — giam 32KB RAM noi bo cho WiFi.
  static uint8_t *ibuf = nullptr;
  const size_t IBUF_CAP = 32768;
  if (!ibuf) ibuf = (uint8_t*)heap_caps_malloc(IBUF_CAP, MALLOC_CAP_SPIRAM);
  if (!ibuf) ibuf = (uint8_t*)malloc(IBUF_CAP);
  if (!ibuf) { t->ok = false; return t; }
  HttpMeta meta; memset(&meta, 0, sizeof meta); size_t len = 0;
  if (!http_get(abs_url, (char*)ibuf, IBUF_CAP, &len, &meta) || !len) { t->ok = false; return t; }
  bool is_png = (len > 8 && ibuf[0] == 0x89 && ibuf[1] == 'P' && ibuf[2] == 'N' && ibuf[3] == 'G') || strstr(meta.content_type, "png") || ends_with(abs_url, ".png");
  bool is_jpg = (len > 2 && ibuf[0] == 0xFF && ibuf[1] == 0xD8) || strstr(meta.content_type, "jpeg") || strstr(meta.content_type, "jpg") || ends_with(abs_url, ".jpg") || ends_with(abs_url, ".jpeg");
  #if QB_HAS_PNGDEC
  if (is_png && decode_thumb_png_ram(t, ibuf, len)) { thumb_commit_persistent(t); return t; }
  #endif
  #if QB_HAS_TJPEG
  if (is_jpg && decode_thumb_jpeg_ram(t, ibuf, len)) { thumb_commit_persistent(t); return t; }
  #endif
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
  disp.fillRect(x, y, w, h, 0xEF7C);
  disp.fillRect(x, y, w, 1, UI_BORDER);
  disp.fillRect(x, y + h - 1, w, 1, UI_BORDER);
  disp.fillRect(x, y, 1, h, UI_BORDER);
  disp.fillRect(x + w - 1, y, 1, h, UI_BORDER);
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
  disp.fillRect(x + 1, vy, w - 2, 1, UI_HOT);
  disp.fillRect(x + 1, vy + vh - 1, w - 2, 1, UI_HOT);
  disp.fillRect(x + 1, vy, 1, vh, UI_HOT);
  disp.fillRect(x + w - 2, vy, 1, vh, UI_HOT);
}

static void draw_scrollbar(int x, int y, int h, int total, int start, int visible) {
  if (total < 1) total = 1;
  if (visible < 1) visible = 1;
  if (visible > total) visible = total;
  disp.fillRect(x, y, 6, h, 0xDEFB);
  disp.fillRect(x, y, 6, 1, UI_BORDER);
  disp.fillRect(x, y + h - 1, 6, 1, UI_BORDER);
  disp.fillRect(x, y, 1, h, UI_BORDER);
  disp.fillRect(x + 5, y, 1, h, UI_BORDER);
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
  disp.fillRect(x + 1, gy, 4, grip, 0x9D9F);
  disp.fillRect(x + 1, gy, 4, 1, UI_LINK);
  disp.fillRect(x + 1, gy + grip - 1, 4, 1, UI_LINK);
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

  disp.fillRect(x, y, w, h, 0xEF7C);
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
    disp.setTextFont(1); disp.setTextColor(UI_DIM);
    char pn[5]; snprintf(pn, sizeof pn, "%d", t + 1);
    disp.drawString(pn, tx + tw - 8, ty + 6);

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
    url_in_n = 0; url_in[0] = 0;
    mt_bind(url_in, &url_in_n, (int)sizeof url_in, MT_URL, (int)(sizeof MT_URL / sizeof MT_URL[0]));
    pass_t9_prev = t9mode;
    is_search_box = false;
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
    url_in_n = 0; url_in[0] = 0;
    mt_bind(url_in, &url_in_n, (int)sizeof url_in, MT_URL, (int)(sizeof MT_URL / sizeof MT_URL[0]));
    pass_t9_prev = t9mode;
    is_search_box = false;
    urlin_show();
    render(); return;
  }
  if (!strncmp(full, "mtt:", 4)) {
    if (!strcmp(full, "mtt:forward")) {
      if (!nav_forward()) show_msg("Khong co trang Forward.");
      return;
    }
    nav_record_new(full);
    strncpy(cur_url, full, sizeof cur_url - 1);
    cur_url[sizeof cur_url - 1] = 0;
    if (!strcmp(full, "mtt:start")) build_doc(PAGE_START);
    else if (!strcmp(full, "mtt:about")) build_doc(PAGE_ABOUT);
    else if (!strcmp(full, "mtt:help")) build_doc(PAGE_HELP);
    else if (!strcmp(full, "mtt:config")) build_docf(PAGE_CONFIG, wifi_up ? "CONNECTED" : "OFF", cfg_ssid.c_str(), cfg_home.c_str(), cfg_tz.c_str());
    else if (!strcmp(full, "mtt:web")) build_doc(PAGE_WEB);
    else if (!strcmp(full, "mtt:sites")) build_doc(PAGE_SITES);
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
      if (!buf) { show_msg("Khong du bo nho RAM."); return; }
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
      show_msg("Da luu Bookmark. OK de tiep.");   // show_msg da render
      return;
    } else if (!strcmp(full, "mtt:menu#refresh")) {
      go_url(menu_src_url); return;
    } else if (!strcmp(full, "mtt:bookmark")) {
      char *buf = page_scratch();
      if (!buf) { show_msg("Khong du bo nho RAM."); return; }
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
    show_msg("Protocol/link nay khong duoc ho tro.");
    return;
  }
  if (!wifi_up) { show_msg("WiFi chua bat. Vao Settings > Ket noi WiFi."); return; }
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
  disp.fillRect(0, 0, SCR_W, UI_HDR_H, UI_TITLE);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString("Connecting...", 18, (UI_HDR_H - 13) / 2);
  disp.fillRect(0, UI_HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);
  disp.setTextColor(UI_FG); disp.drawString(full, 4, UI_HDR_H + 8);
  load_paint_footer();                 // Menu | bar + 0.0K | Back (o cho dong gio)
  load_pct = 45; load_paint_pct = 45;
  disp.fillRect(0, UI_HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);
  disp.fillRect(18, (UI_HDR_H - 13) / 2, 150, 14, UI_TITLE);
  disp.setTextColor(UI_WHITE);
  disp.drawString("Sending request...", 18, (UI_HDR_H - 13) / 2);
  disp.endWrite();
  HttpMeta meta; size_t len = 0;
  http_set_progress(on_http_progress);
  bool ok = http_get(full, net_buf, DOC_CAP, &len, &meta);
  http_set_progress(nullptr);
  if (!ok) {
    loading = false;
    if (meta.status == -2) show_msg("Khong ket noi duoc may chu.");
    else if (meta.status == -3) show_msg("Phan hoi HTTP khong hop le.");
    else if (meta.status == -5) show_msg("Simulator khong co TLS; ESP32 ho tro HTTPS.");
    else if (meta.status == -6) show_msg("Qua nhieu redirect.");
    else show_msg("Ket noi that bai.");
    return;
  }
  // gzip: khong giai nen duoc tren thiet bi -> bao loi
  if (meta.gzipped) { show_msg("Server bo qua identity va tra gzip/br."); return; }
  if (meta.final_url[0] && strcmp(meta.final_url, cur_url)) {
    strncpy(cur_url, meta.final_url, sizeof cur_url - 1);
    cur_url[sizeof cur_url - 1] = 0;
  }
  if (url_split(cur_url, scheme, host, &port, path)) {
    strncpy(last_host, host, sizeof last_host - 1);
    last_host[sizeof last_host - 1] = 0;
  }
  if (len == 0) {
    char em[96]; snprintf(em, sizeof em, "HTTP %d - server khong tra noi dung.", meta.status);
    show_msg(em); return;
  }
  if (meta.content_type[0] && strncmp(meta.content_type, "text/", 5) &&
      !strstr(meta.content_type, "html") && !strstr(meta.content_type, "xml") &&
      !strstr(meta.content_type, "wml")) {
    char em[128]; snprintf(em, sizeof em, "Khong render duoc: %s", meta.content_type);
    show_msg(em); return;
  }
  doc.buf = doc_buf; doc.cap = DOC_CAP;
  doc_parse(&doc, net_buf, len);
  body_scroll_reset(0);
  // Warm a few image thumbnails so Overview can show real page tiles immediately.
  // Memory tier is PSRAM; decoded RGB565 survives reboot through LittleFS tier.
  thumb_prefetch_doc(3);
  history_add(cur_url, doc.title[0] ? doc.title : cur_url);
  loading = false; load_pct = 100;
  // Desktop/large-UI (khong viewport meta) -> tu hien chuot ao; mobile viewport -> tat chuot.
  if (!doc.is_wml) {
    if (html_looks_desktop(net_buf, len)) {
      mouse_on = true; mouse_x = SCR_W / 2; mouse_y = SCR_H / 2;
      Serial.println("[mouse] desktop UI detected -> virtual mouse on");
    } else {
      mouse_on = false;
    }
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
  return (st == 1) ? 24 : (st == 3) ? 22 : line_h(st);
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

// icon nho ve bang fillRect (khong can GIF runtime)
static void draw_icon_globe(int x, int y) {
  disp.fillRect(x + 2, y + 0, 5, 1, UI_WHITE);
  disp.fillRect(x + 1, y + 1, 7, 1, UI_WHITE);
  disp.fillRect(x + 0, y + 2, 9, 5, UI_WHITE);
  disp.fillRect(x + 1, y + 7, 7, 1, UI_WHITE);
  disp.fillRect(x + 2, y + 8, 5, 1, UI_WHITE);
  disp.drawFastHLine(x + 1, y + 4, 7, UI_TITLE);   // equator
  disp.drawPixel(x + 4, y + 1, UI_TITLE);
  disp.drawPixel(x + 4, y + 7, UI_TITLE);
}
static void draw_icon_pencil(int x, int y) {
  disp.fillRect(x + 1, y + 1, 2, 8, 0x6B4D);       // shaft
  disp.fillRect(x + 3, y + 0, 2, 3, 0xC631);       // tip
  disp.fillRect(x + 1, y + 0, 2, 1, 0x8C71);
}
static void draw_icon_search(int x, int y) {
  disp.fillRect(x + 1, y + 1, 5, 5, 0x8C71);
  disp.fillRect(x + 2, y + 2, 3, 3, UI_BG);
  disp.fillRect(x + 5, y + 5, 3, 2, 0x8C71);
}
static void draw_icon_folder(int x, int y) {
  disp.fillRect(x + 0, y + 2, 4, 2, 0x8C71);       // tab
  disp.fillRect(x + 0, y + 3, 10, 7, 0x07E0);      // body (green)
  disp.fillRect(x + 6, y + 5, 2, 2, UI_WHITE);     // check
}
static void draw_chevron(int x, int y, uint16_t c) {
  for (int i = 0; i < 4; i++) {
    disp.drawPixel(x + i, y + i, c);
    disp.drawPixel(x + i, y + 8 - i, c);
    disp.drawPixel(x + 5 + i, y + i, c);
    disp.drawPixel(x + 5 + i, y + 8 - i, c);
  }
}

// popup OPTION menu (giong anh: Bmrk/Optn/Navg/Tool/Sett/Help/Exit)
struct MenuTop { const char *label; const char *child; };
static const MenuTop MENUS[7] = {
  { "Bmrk", "SvBmrk" },
  { "Optn", "SpDial" },   // Speed Dial home
  { "Navg", "Overview" }, // Opera Mini page overview
  { "Tool", "Mouse"    }, // Opera Mini virtual mouse
  { "Sett", "WiFi"   },
  { "Help", "Help"   },
  { "Exit", ""       },
};

static void draw_chevron_btn(int x, int y) {
  // nut mui ten xam nho (giong anh comparison)
  disp.fillRect(x, y, 14, 14, UI_ARROW);
  disp.fillRect(x, y, 14, 1, UI_BORDER);
  disp.fillRect(x, y + 13, 14, 1, UI_BORDER);
  disp.fillRect(x, y, 1, 14, UI_BORDER);
  disp.fillRect(x + 13, y, 1, 14, UI_BORDER);
  for (int i = 0; i < 4; i++) {
    disp.drawPixel(x + 5 + i, y + 4 + i, UI_FG);
    disp.drawPixel(x + 5 + i, y + 10 - i, UI_FG);
  }
}

static void render_menu_popup() {
  // panel dung + mo (khong lo trang duoi)
  const int mx = 2, my = UI_HDR_H + 2;
  const int lw = 78, rw = 90, mh = 7 * 18 + 6;
  disp.fillRect(mx, my, lw + rw, mh, UI_BG);
  disp.fillRect(mx, my, lw + rw, 1, UI_BORDER);
  disp.fillRect(mx, my + mh - 1, lw + rw, 1, UI_BORDER);
  disp.fillRect(mx, my, 1, mh, UI_BORDER);
  disp.fillRect(mx + lw + rw - 1, my, 1, mh, UI_BORDER);
  disp.fillRect(mx + lw, my, 1, mh, UI_BORDER);   // ke 2 cot
  disp.setTextFont(2);
  for (int i = 0; i < 7; i++) {
    int y = my + 3 + 18 * i;
    bool sel = (i == menu_idx);
    if (sel) disp.fillRect(mx + 1, y - 2, lw - 1, 17, UI_SEL);
    disp.setTextColor(sel && !menu_in_sub ? UI_SELFG : UI_FG);
    disp.drawString(MENUS[i].label, mx + 4, y + 1);
    if (MENUS[i].child[0]) {
      draw_chevron_btn(mx + lw - 18, y);
    }
    // child cot phai (SvBmrk / AddBmk)
    if (MENUS[i].child[0]) {
      bool sub_sel = sel && menu_in_sub;
      if (sub_sel) disp.fillRect(mx + lw + 1, y - 2, rw - 2, 17, UI_SEL);
      disp.setTextColor(sub_sel ? UI_SELFG : (sel ? UI_SELFG : UI_FG));
      disp.drawString(MENUS[i].child, mx + lw + 6, y + 1);
    }
  }
}

static void render() {
#if defined(ARDUINO)
  // Ve toan bo khung vao backbuffer PSRAM, sau do push mot len man hinh.
  if (frame_ok) gfx_dst = &frame_buf;
#endif
  disp.startWrite();
  disp.fillScreen(UI_BG);
  // title bar + globe (can giua doc)
  disp.fillRect(0, 0, SCR_W, HDR_H, UI_TITLE);
  draw_icon_globe(UI_PAD, (HDR_H - 9) / 2);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString(doc.title[0] ? doc.title : "Qeafbrowser", UI_PAD + 14, (HDR_H - 8) / 2);
  if (loading) disp.fillRect(0, HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);

  if (overview_on) {
    int n = doc.nlines > 0 ? doc.nlines : 1;
    int ox = 6, oy = HDR_H + 14, ow = SCR_W - 26, oh = SCR_H - HDR_H - FTR_H - 18;
    disp.setTextFont(1);
    disp.setTextColor(UI_DIM);
    disp.drawString("Desktop overview", 8, HDR_H + 2);
    char zbuf[16]; snprintf(zbuf, sizeof zbuf, "x%d", overview_zoom);
    disp.drawString(zbuf, SCR_W - 20, HDR_H + 2);
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
        if (sel) {
          int bx = UI_PAD - 3;
          int bw;
          if (st == 1 || st == 3 || st == 7) { bx = 2; bw = SCR_W - 4; }
          else {
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
          // Small Screen Rendering: JPEG/PNG thumbnail neu decode duoc, neu khong thi fallback placeholder.
          int bx = UI_PAD, bw = SCR_W - 2 * UI_PAD, bh = 74;
          disp.fillRect(bx, y, bw, bh, 0xDEFB);
          disp.fillRect(bx, y, bw, 1, UI_BORDER);
          disp.fillRect(bx, y + bh - 1, bw, 1, UI_BORDER);
          disp.fillRect(bx, y, 1, bh, UI_BORDER);
          disp.fillRect(bx + bw - 1, y, 1, bh, UI_BORDER);
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
          disp.setTextFont(1); disp.setTextColor(UI_DIM);
          disp.drawString("Image thumb 240px", bx + 72, y + 10);
          disp.setTextFont(2); disp.setTextColor(UI_FG);
          disp.drawString(alt, bx + 72, y + 28);
          disp.setTextFont(1); disp.setTextColor(UI_DIM);
          disp.drawString((th && th->ok) ? (th->persistent_hit ? "LittleFS -> PSRAM" : "PSRAM image cache") : "thumbnail fallback", bx + 72, y + 48);
        } else if (st == 1) {
          // field: box deu 2 ben
          int bx = UI_PAD, bw = SCR_W - 2 * UI_PAD, bh = 22;
          disp.fillRect(bx, y, bw, bh, UI_FIELD);
          disp.fillRect(bx, y, bw, 1, UI_BORDER);
          disp.fillRect(bx, y + bh - 1, bw, 1, UI_BORDER);
          disp.fillRect(bx, y, 1, bh, UI_BORDER);
          disp.fillRect(bx + bw - 1, y, 1, bh, UI_BORDER);
          if (strstr(txt, "URL") || strstr(txt, "Address") || strstr(txt, "Enter"))
            draw_icon_pencil(bx + 6, y + 6);
          else draw_icon_search(bx + 6, y + 6);
          disp.setTextFont(2);
          disp.setTextColor(UI_DIM);
          disp.drawString(txt, bx + 22, y + 7);
        } else if (st == 3) {
          int rh = 22;
          disp.fillRect(0, y, SCR_W, rh, UI_ROW);
          draw_icon_folder(UI_PAD, y + 5);
          disp.setTextFont(2);
          disp.setTextColor(UI_FG);
          disp.drawString(txt, UI_PAD + 18, y + 7);
          draw_chevron_btn(SCR_W - UI_PAD - 16, y + 3);
        } else {
          // font + mau theo style (Opera Mini / bao HTML)
          if (st == 2)      disp.setTextFont(4);          // h1/h2
          else if (st == 5) disp.setTextFont(1);          // small/meta
          else              disp.setTextFont(2);
          uint16_t fg = UI_FG;
          if (doc.lines[i].link >= 0) fg = UI_LINK;
          else if (st == 2) fg = UI_LINK;                 // tieu de bai bao
          else if (st == 5) fg = UI_DIM;                  // byline / timestamp
          else if (st == 6) fg = UI_FG;                   // bold
          disp.setTextColor(fg);
          disp.drawString(txt, UI_PAD, y);
          if (doc.lines[i].link >= 0) {
            int tw = gfx_textWidth(txt, st);
            disp.drawFastHLine(UI_PAD, y + (st == 2 ? 24 : st == 5 ? 11 : 13), tw, UI_LINK);
          }
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
    if (mouse_on) {
      // con tro chuot ao (Opera Mini desktop mode)
      disp.fillRect(mouse_x, mouse_y, 3, 12, UI_FG);
      disp.fillRect(mouse_x + 1, mouse_y + 2, 2, 10, UI_FG);
      disp.fillRect(mouse_x + 2, mouse_y + 4, 2, 8, UI_FG);
      disp.fillRect(mouse_x + 3, mouse_y + 6, 2, 6, UI_FG);
      disp.fillRect(mouse_x, mouse_y, 1, 12, UI_WHITE);
    }
  }

  // Repaint header after pixel scrolling so partially clipped content cannot bleed into title bar.
  disp.fillRect(0, 0, SCR_W, HDR_H, UI_TITLE);
  draw_icon_globe(UI_PAD, (HDR_H - 9) / 2);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString(doc.title[0] ? doc.title : "Qeafbrowser", UI_PAD + 14, (HDR_H - 8) / 2);
  if (loading) disp.fillRect(0, HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);

  // softkey bar — Menu | time | Back (giong Opera Mini / K750)
  disp.fillRect(0, SCR_H - FTR_H, SCR_W, FTR_H, UI_SOFT);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  const char *l = "Menu", *r = "Back";
  if (vkey_open) { l = "OK"; r = "Cancel"; }
  else if (menu_open) { l = "Select"; r = "Cancel"; }
  int sy = SCR_H - FTR_H + (FTR_H - 8) / 2;
  disp.drawString(l, UI_PAD, sy);
  // Real-time clock. NTP syncs when Internet is available; the system RTC keeps running afterwards.
  // Khi dang load: o giua la progress bar + KB (da ve boi load_paint_footer / on_http_progress).
  if (loading) {
    // chi ve lai label neu can; progress da o day tu callback
  } else {
    clock_update_cache();
    disp.drawString(clock_cache, (SCR_W - disp.textWidth(clock_cache)) / 2, sy);
  }
  disp.drawString(r, SCR_W - disp.textWidth(r) - UI_PAD, sy);
  disp.endWrite();          // dong giao dich cua frame_buf (neu dang ve frame)
#if defined(ARDUINO)
  if (frame_ok) {
    gfx_dst = &disp;        // day len man hinh THAT mot lan — khong nhap nhay
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
    is_search_box ? "Tu khoa:" : "URL:");
  build_doc(buf);
  scr = SCR_INPUT; top = 0; cursor_link = 0;
  vkey_bind(url_in, &url_in_n, (int)sizeof url_in, false, is_search_box);
}

static void draw_splash() {
  disp.fillScreen(TFT_BLACK);
  const int lw = 46, lh = 46;
  int x0 = (SCR_W - lw) / 2, y0 = (SCR_H - lh) / 2 - 24;
  disp.startWrite();
  // Native Qeafbrowser mark: compact red feature-phone browser tile with a white Q.
  disp.fillRect(x0, y0, lw, lh, UI_TITLE);
  disp.fillRect(x0 + 7, y0 + 7, 30, 5, UI_WHITE);
  disp.fillRect(x0 + 7, y0 + 29, 30, 5, UI_WHITE);
  disp.fillRect(x0 + 7, y0 + 7, 5, 27, UI_WHITE);
  disp.fillRect(x0 + 32, y0 + 7, 5, 27, UI_WHITE);
  disp.fillRect(x0 + 27, y0 + 26, 5, 5, UI_WHITE);
  disp.fillRect(x0 + 32, y0 + 31, 5, 5, UI_WHITE);
  disp.fillRect(x0 + 37, y0 + 36, 4, 4, UI_WHITE);
  disp.setTextColor(TFT_WHITE); disp.setTextFont(4);
  disp.drawString("Qeafbrowser", (SCR_W - disp.textWidth("Qeafbrowser")) / 2, y0 + lh + 8);
  disp.setTextFont(2); disp.setTextColor(TFT_LIGHTGREY);
  disp.drawString("Default: qeafivels.com", (SCR_W - disp.textWidth("Default: qeafivels.com")) / 2, y0 + lh + 40);
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
  if (scr == SCR_SPLASH) return;

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
    if (!strcmp(k, "up"))    { mouse_y -= 12; if (mouse_y < HDR_H) mouse_y = HDR_H; render(); return; }
    if (!strcmp(k, "down"))  { mouse_y += 12; if (mouse_y > SCR_H - FTR_H) mouse_y = SCR_H - FTR_H; render(); return; }
    if (!strcmp(k, "left"))  { mouse_x -= 12; if (mouse_x < 2) mouse_x = 2; render(); return; }
    if (!strcmp(k, "right")) { mouse_x += 12; if (mouse_x > SCR_W - 3) mouse_x = SCR_W - 3; render(); return; }
    if (!strcmp(k, "ok")) {
      // click: chon link gan con tro nhat
      int best = -1, best_d = 10000;
      int vis = (SCR_H - HDR_H - FTR_H) / 16;
      for (int li = 0; li < doc.nlinks; li++) {
        int l0 = doc.links[li].line0;
        if (l0 < top || l0 >= top + vis) continue;
        int ly = HDR_H + 4 + (l0 - top) * 18 + 8;
        int d = abs(ly - mouse_y) + abs(mouse_x - SCR_W / 2);
        if (d < best_d) { best_d = d; best = li; }
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
    if (!strcmp(k, "up"))    { if (vkey_row > 0) { vkey_row--; if (vkey_row < 4) { int n = (int)strlen(rows[vkey_row]); if (vkey_col >= n) vkey_col = n - 1; } } render(); return; }
    if (!strcmp(k, "down"))  {
      if (vkey_row < 4) {
        vkey_row++;
        if (vkey_row < 4) { int n = (int)strlen(rows[vkey_row]); if (vkey_col >= n) vkey_col = n - 1; }
        else vkey_col = 0;
      }
      render(); return;
    }
    if (!strcmp(k, "left"))  { if (vkey_col > 0) vkey_col--; render(); return; }
    if (!strcmp(k, "right")) {
      int n = (vkey_row < 4) ? (int)strlen(rows[vkey_row]) : VK_SP_N;
      if (vkey_col < n - 1) vkey_col++;
      render(); return;
    }
    if (!strcmp(k, "mode"))  { vkey_shift = !vkey_shift; vkey_sym = false; render(); return; }
    if (!strcmp(k, "option")){ vkey_sym = !vkey_sym; render(); return; }
    if (!strcmp(k, "ok")) {
      if (vkey_row < 4) {
        vkey_put(rows[vkey_row][vkey_col]);
        if (vkey_shift) vkey_shift = false;   // 1 chu HOA roi thuong
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
      } else if (menu_idx < 6) menu_idx++;
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
        show_msg("SvBmrk: da luu Bookmark. OK de tiep.");   // show_msg da render
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
      } else if (menu_idx == 5) {   // Help
        go_url("mtt:help"); return;
      } else {                      // Exit
        go_url("mtt:start"); return;
      }
      render(); return;
    }
    render(); return;
  }

  if (!strcmp(k, "menu")) { menu_open = false; go_url("mtt:start"); return; }
  if (scr == SCR_MSG) {                  // trang loi: OK = thu tai lai
    if (!strcmp(k, "ok")) go_url(cur_url);
    return;
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
      Serial.printf("[wifi] thu ket noi '%s' pass='%s'\n", nets[net_cursor].ssid, pass_in);
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
        if (url_in_n) open_user_url(url_in); else go_url("mtt:start");
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
extern "C" int sim_body_scroll_px() { return (int)(body_scroll_visual_fp / BODY_FP); }
extern "C" int sim_body_scroll_target_px() { return (int)(body_scroll_target_fp / BODY_FP); }
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
    } else if (url_in_n) open_user_url(url_in);
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
// lenh: go <url> | key <name> | scan | status | wifi <ssid> <pass>
static char cli_in[160]; static int cli_n = 0;

static void cli_exec(const char *line) {
  if (!strncmp(line, "go ", 3)) { go_url(line + 3); return; }
  if (!strncmp(line, "key ", 4)) { on_key(line + 4); return; }
  if (!strcmp(line, "scan")) { wifi_scan(); wifi_list_show(); render(); return; }
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
  g_panel->init();          // disp.init() khong duoc (macro disp = LovyanGFX*)
  Serial.println("[boot] 2 display");
  Serial.flush();
  g_panel->setRotation(0);                 // doc 240x320 theo E524546-OS
  g_panel->setBrightness(200);
  g_panel->fillScreen(TFT_BLACK);

  doc_buf = (char *)heap_caps_malloc(DOC_CAP, MALLOC_CAP_SPIRAM);
  net_buf = (char *)heap_caps_malloc(DOC_CAP, MALLOC_CAP_SPIRAM);
  if (!doc_buf || !net_buf) { doc_buf = (char*)malloc(DOC_CAP); net_buf = (char*)malloc(DOC_CAP); }
  // Doc struct (~32KB) cung chuyen sang PSRAM de WiFi/TLS du RAM noi bo.
  g_doc = (Doc *)heap_caps_malloc(sizeof(Doc), MALLOC_CAP_SPIRAM);
  if (!g_doc) g_doc = (Doc *)malloc(sizeof(Doc));
  if (g_doc) memset(g_doc, 0, sizeof(Doc));
  if (!g_doc || !doc_buf || !net_buf)
    Serial.println("[mem] NGUY HIEM: khong cap duoc PSRAM/heap cho doc buffers");
  doc.buf = doc_buf; doc.cap = DOC_CAP;
#if defined(ARDUINO)
  // Backbuffer 240x320 RGB565 (153600 byte) trong PSRAM — chong nhap nhay man hinh.
  frame_buf.setPsram(true);
  if (frame_buf.createSprite(SCR_W, SCR_H)) frame_ok = true;
  Serial.printf("[gfx] frame buffer PSRAM 240x320: %s\n", frame_ok ? "OK" : "MISSING -> ve truc tiep");
#endif
#if defined(ARDUINO)
  Serial.printf("[mem] boot: heap=%u maxblk=%u psram=%u psram_free=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram());
#endif
  store_init();
  keys_init();
  key_cb = on_key;

  draw_splash();
  delay(1200);

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
  Serial.printf("[mem] sau wifi: heap=%u maxblk=%u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
#endif
  if (wifi_up) clock_start_ntp();
  else Serial.println("[wifi] Vao Settings > Ket noi WiFi de quet + nhap thu cong");
  if (wifi_up) go_url(cfg_home.length() ? cfg_home.c_str() : "https://qeafivels.com/");
  else go_url("mtt:start");   // offline: Speed Dial, khong dung man "Connection Timeout"
  Serial.println("[boot] setup xong. CLI: go <url> | key <name> | scan | status | wifi <ssid> <pass>");
  Serial.flush();
}

void loop() {
  keys_poll();
  serial_poll();
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
  }
  static uint32_t hb = 0;
  if (millis() - hb > 15000) { hb = millis(); }   // (no-op giu cho struct tinh)
  delay(5);
}
