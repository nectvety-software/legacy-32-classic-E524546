// ESP_Browser — main app cho E524546 (ESP32-S3 + ST7789 240x320 doc + keypad Symbian).
// Port tu DMAX_QQ_Browser (MRE VXP): UI WML card-style, mtt: pseudo-URL,
// start page + About/Help/Settings noi dung lay nguyen van tu ban carve .rodata.
// Lua firmware: docs/PROMPT.md — GPIO bat kha xam pham, doc 240x320 (rotation 0).
#include <Arduino.h>
#include <WiFi.h>
#include <stdarg.h>
#include <esp_heap_caps.h>
#include "pins.h"
#include "browser.h"
#include "LGFX_ESP32S3_ST7789.h"
#include "logo_dmax.h"          // 45x45 RGB565 + LOGO_KEY — asset port tu DMAX .vm_res

// ---------------- UI palette (RGB565) — theo DMAX_QQ_Browser_GFX_Accuracy_Comparison ----------------
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
  if (style == 6) return 82;
  return 16;
}

// ---------------- app state ----------------
enum Scr { SCR_SPLASH, SCR_HOME, SCR_BROWSE, SCR_INPUT, SCR_MSG,
           SCR_WIFI_LIST, SCR_WIFI_PASS, SCR_WIFI_RESULT };
// SCR_INPUT = hop nhap URL (multi-tap + host keyboard o sim). OPTION = menu ngan.
static Scr scr = SCR_SPLASH;

static Doc  doc;
static char *doc_buf = nullptr;        // PSRAM: noi dung sau parse (doc.buf)
static char *net_buf = nullptr;        // PSRAM: body tho tu HTTP (khong de de overlap parse)
static char cur_url[256] = "mtt:start";
static char last_host[64] = "";
static int  top = 0, cursor_link = 0;
static int  focus_i = 0;   // Focus Block: khung vien xanh chon khoi noi dung
static char msg[192] = "";

static char back_stack[12][256]; static int back_sp = 0;
static char forward_stack[12][256]; static int forward_sp = 0;
static bool nav_history_suppressed = false;

static char url_in[192]; static int url_in_n = 0;
static bool wifi_up = false;
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
static int  ov_ox = 0, ov_oy = 0;     // viewport origin in page lines
static int  overview_zoom = 1;          // 1..3, Opera Mini 4 style desktop overview zoom
static bool loading = false;
static int  load_pct = 0;

static void build_doc(const char *html);
static void build_docf(const char *fmt, ...);
static void render();
static void urlin_show();

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
  WiFi.disconnect();
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

static void wifi_list_show() {
  char buf[4000]; int o = snprintf(buf, sizeof buf,
    "<wml><card title=\"WiFi\"><p>Chon mang WiFi:<br/></p><p>");
  for (int i = 0; i < net_n; i++)
    o += snprintf(buf + o, sizeof buf - o, "<a href=\"mtt:wifisel#%d\">%s %s</a><br/>",
                  i, nets[i].ssid, nets[i].open ? "(mo)" : "");
  if (!net_n) o += snprintf(buf + o, sizeof buf - o, "(khong thay mang nao — OPTION de quen lai)");
  snprintf(buf + o, sizeof buf - o, "</p></card></wml>");
  build_doc(buf);
  scr = SCR_WIFI_LIST; top = 0; cursor_link = 0;
}

static bool wifi_try_connect(const char *ssid, const char *pass) {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass[0] ? pass : NULL);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) {
    snprintf(connect_msg, sizeof connect_msg, "Dang ket noi %s... %lus", ssid,
             (millis() - t0) / 1000);
    build_docf("<wml><card title=\"WiFi\"><p>%s</p></card></wml>", connect_msg);
    render();
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
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
    build_docf("<wml><card title=\"WiFi\"><p>KET NOI THANH CONG<br/>%s<br/>IP %s<br/>"
               "Cau hinh da luu va /ESPBrowser/config.ini</p></card></wml>",
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
  "<wml><card title=\"Browser V1.3\">"
  "<p><input><a href=\"#\">Enter URL</a></input></p>"
  "<p><input><a href=\"mtt:search\">Google Search</a></input></p>"
  "<p><a href=\"mtt:bookmark\">Bookmarks</a>|<a href=\"mtt:history\">History</a>"
  "|<a href=\"mtt:config\">Settings</a>|<a href=\"mtt:about\">About</a>"
  "|<a href=\"mtt:help\">Help</a></p>"
  "<p><folder><a href=\"https://www.google.com/\">Google</a></folder></p>"
  "<p><folder><a href=\"https://m.facebook.com/\">Facebook</a></folder></p>"
  "<p><folder><a href=\"https://en.m.wikipedia.org/\">Wikipedia</a></folder></p>"
  "<p><folder><a href=\"http://dmaxmrp.tk/\">dmaxmrp.tk</a></folder></p>"
  "<p><folder><a href=\"https://m.youtube.com/\">YouTube</a></folder></p>"
  "<p><folder><a href=\"http://wap.yahoo.com/\">Yahoo</a></folder></p>"
  "<p><folder><a href=\"http://m.weather.com/\">Weather</a></folder></p>"
  "<p><folder><a href=\"http://tubidy.mobi/\">Tubidy</a></folder></p>"
  "<p><folder><a href=\"mtt:web\">More Sites</a></folder></p>"
  "</card></wml>";

static const char *PAGE_ABOUT =
  "<wml><card title=\"About\">"
  "<p>Browser V1 English Build101013<br/>Modified by Nervz<br/>"
  "ESP Port 1.3 - Opera Mini 4 Mode<br/>HTTP + HTTPS / HTML + WML<br/><br/>"
  "Original UI heritage: DMAX<br/>ESP32-S3 port.</p></card></wml>";

static const char *PAGE_HELP =
  "<wml><card title=\"Help\"><p>UP/DOWN: cuoc trang<br/>LEFT/RIGHT: link truoc/sau<br/>"
  "OK: mo link<br/>BACK: lui<br/>MENU: ve trang chu<br/>OPTION: menu<br/>"
  "(Bmrk/Optn/Navg/Tool/Sett/Help/Exit)<br/>"
  "SELECT giu: doi T9<br/><br/>"
  "Nhap URL: Tool &gt; InpURL<br/>domain tu dong HTTPS<br/>Tools &gt; Forward de tien trang<br/>"
  "multi-tap hoac ban phim PC (sim)<br/>"
  "WiFi: Sett &gt; WiFi</p></card></wml>";

static const char *PAGE_CONFIG =
  "<wml><card title=\"Settings\"><p>WiFi: %s<br/>SSID: %s<br/>Home: %s<br/><br/>"
  "<a href=\"mtt:wifi\">&gt;&gt; Ket noi WiFi (quet mang)</a><br/>"
  "<a href=\"mtt:wifi\">&gt;&gt; Doi mat khau WiFi</a><br/><br/>"
  "Mac dinh tu dong ket noi lai mang da luu.<br/>"
  "Sua sau: /ESPBrowser/config.ini tren the SD</p></card></wml>";

static const char *PAGE_WEB =
  "<wml><card title=\"Web Browser\">"
  "<p><a href=\"http://dmaxmrp.tk/\">dmaxmrp.tk</a><br/>"
  "<a href=\"http://pokoyo.wapka.mobi/\">PokoyoWap</a><br/>"
  "<a href=\"http://google.com/\">Google</a><br/>"
  "<a href=\"http://wap.yahoo.com/\">Yahoo!</a><br/>"
  "<a href=\"http://tubidy.mobi/\">Tubidy</a><br/>"
  "<a href=\"http://wap.c2.hu/\">C2 Mail</a><br/>"
  "<a href=\"http://ref.dmax.wapka.mobi/\">Wapka</a></p></card></wml>";

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

// ---------------- build doc tu chuoi ----------------
static void build_doc(const char *html) {
  doc_parse(&doc, html, strlen(html));
}
static void build_docf(const char *fmt, ...) {
  char buf[2048];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
  doc_parse(&doc, buf, strlen(buf));
}

// ---------------- dieu huong ----------------
static void go_url(const char *url);

static void push_back() {
  if (!cur_url[0]) return;
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
  // DMAX ui_resolve_link: $prev, $refresh, mtt:, http(s), /abs, tuong doi
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
}

static void render();

static void go_url(const char *url) {
  Serial.printf("[nav] %s\n", url);
  // DMAX: $prev = quay lui that, $refresh = tai lai
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
    else if (!strcmp(full, "mtt:config")) build_docf(PAGE_CONFIG, wifi_up ? "CONNECTED" : "OFF", cfg_ssid.c_str(), cfg_home.c_str());
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
      static char buf[6000]; int o = snprintf(buf, sizeof buf, "<wml><card title=\"History\"><p>");
      for (int i = 0; i < history_count() && o < 5000; i++)
        o += snprintf(buf + o, sizeof buf - o, "<a href=\"%s\">%s</a><br/>",
                      history_url(i), history_title(i)[0] ? history_title(i) : history_url(i));
      snprintf(buf + o, sizeof buf - o, "</p></card></wml>");
      build_doc(buf);
    } else if (!strcmp(full, "mtt:menu")) {
      // menu la popup overlay — khong dung trang
    } else if (!strcmp(full, "mtt:menu#book")) {
      bookmark_add(menu_src_url, menu_src_title[0] ? menu_src_title : menu_src_url);
      show_msg("Da luu Bookmark. OK de tiep.");
      render(); return;
    } else if (!strcmp(full, "mtt:menu#refresh")) {
      go_url(menu_src_url); return;
    } else if (!strcmp(full, "mtt:bookmark")) {
      static char buf[6000]; int o = snprintf(buf, sizeof buf, "<wml><card title=\"Bookmark\"><p>");
      for (int i = 0; i < bookmark_count() && o < 5000; i++)
        o += snprintf(buf + o, sizeof buf - o, "<a href=\"%s\">%s</a><br/>",
                      bookmark_url(i), bookmark_title(i)[0] ? bookmark_title(i) : bookmark_url(i));
      snprintf(buf + o, sizeof buf - o, "</p></card></wml>");
      build_doc(buf);
    } else build_doc(PAGE_START);
    history_add(full, doc.title[0] ? doc.title : "mtt");
    scr = SCR_BROWSE; top = 0; cursor_link = 0;
    focus_i = 0;
    while (focus_i < doc.nlines && doc.lines[focus_i].n == 0) focus_i++;
    if (focus_i >= doc.nlines) focus_i = 0;
    if (focus_i < doc.nlines) cursor_link = doc.lines[focus_i].link;
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
  char scheme[8], host[64], path[192]; int port;
  if (url_split(full, scheme, host, &port, path)) strncpy(last_host, host, sizeof last_host - 1);
  // DMAX-style status: Connecting... -> Sending request... -> body
  loading = true; load_pct = 15;
  disp.fillScreen(UI_BG);
  disp.fillRect(0, 0, SCR_W, UI_HDR_H, UI_TITLE);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString("Connecting...", 18, (UI_HDR_H - 13) / 2);
  disp.fillRect(0, UI_HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);
  disp.setTextColor(UI_FG); disp.drawString(full, 4, UI_HDR_H + 8);
  load_pct = 45;
  disp.fillRect(0, UI_HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);
  disp.fillRect(18, (UI_HDR_H - 13) / 2, 120, 14, UI_TITLE);
  disp.setTextColor(UI_WHITE);
  disp.drawString("Sending request...", 18, (UI_HDR_H - 13) / 2);
  disp.endWrite();
  HttpMeta meta; size_t len = 0;
  bool ok = http_get(full, net_buf, DOC_CAP, &len, &meta);
  if (!ok) {
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
  history_add(cur_url, doc.title[0] ? doc.title : cur_url);
  loading = false; load_pct = 100;
  scr = SCR_BROWSE; top = 0; cursor_link = 0;
  focus_i = 0;
  while (focus_i < doc.nlines && doc.lines[focus_i].n == 0) focus_i++;
  if (focus_i >= doc.nlines) focus_i = 0;
  if (focus_i < doc.nlines) cursor_link = doc.lines[focus_i].link;
  if (meta.truncated) Serial.printf("[http] body truncated at %u bytes\n", (unsigned)len);
  render();
}

// ---------------- render (giong anh chup DMAX V1.0) ----------------
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
  if (UI_ROWS_Y + block_h > body_bottom) { top = fs; return; }

  while (top < fs) {
    int y = UI_ROWS_Y;
    for (int i = top; i <= fe && i < doc.nlines; i++) y += render_line_h(i);
    if (y <= body_bottom) break;
    top++;
  }
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
  disp.startWrite();
  disp.fillScreen(UI_BG);
  // title bar + globe (can giua doc)
  disp.fillRect(0, 0, SCR_W, HDR_H, UI_TITLE);
  draw_icon_globe(UI_PAD, (HDR_H - 9) / 2);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  disp.drawString(doc.title[0] ? doc.title : "Browser V1.3", UI_PAD + 14, (HDR_H - 8) / 2);
  if (loading) disp.fillRect(0, HDR_H - 3, SCR_W * load_pct / 100, 3, UI_WHITE);

  if (overview_on) {
    // Opera Mini 4 style desktop overview: thu nho ca trang, viewport xanh va zoom bang D-Pad.
    int n = doc.nlines > 0 ? doc.nlines : 1;
    int avail_h = SCR_H - HDR_H - FTR_H - 12;
    int bh = avail_h / (n > 42 ? 42 : n);
    if (bh < 2) bh = 2;
    int max_rows = avail_h / bh;
    disp.setTextFont(1);
    disp.setTextColor(UI_DIM);
    disp.drawString("Desktop overview", 8, HDR_H + 2);
    char zbuf[16]; snprintf(zbuf, sizeof zbuf, "x%d", overview_zoom);
    disp.drawString(zbuf, SCR_W - 20, HDR_H + 2);
    for (int i = 0; i < n && i < max_rows; i++) {
      int yb = HDR_H + 12 + i * bh;
      int w = 12 + (doc.lines[i].n % 38) * 3;
      if (doc.lines[i].style == 6) w = SCR_W - 22;
      if (w > SCR_W - 22) w = SCR_W - 22;
      uint16_t c = UI_DIM;
      if (doc.lines[i].style == 6) c = UI_BORDER;
      else if (doc.lines[i].link >= 0) c = UI_LINK;
      disp.fillRect(11, yb, w, bh > 2 ? bh - 1 : 1, c);
    }
    int vh = max_rows / (overview_zoom + 1);
    if (vh < 4) vh = 4;
    if (vh > max_rows) vh = max_rows;
    if (ov_oy > n - vh) ov_oy = n - vh;
    if (ov_oy < 0) ov_oy = 0;
    int vy = HDR_H + 12 + ov_oy * bh;
    disp.fillRect(6, vy - 1, SCR_W - 12, 1, UI_SEL);
    disp.fillRect(6, vy + vh * bh, SCR_W - 12, 1, UI_SEL);
    disp.fillRect(6, vy - 1, 1, vh * bh + 2, UI_SEL);
    disp.fillRect(SCR_W - 7, vy - 1, 1, vh * bh + 2, UI_SEL);
  } else if (vkey_open) {
    vkey_draw();
  } else {
    // body — le deu UI_PAD
    int y = UI_ROWS_Y;
    int fs = focus_block_start(focus_i), fe = focus_block_end(focus_i);
    int focus_x0 = SCR_W, focus_x1 = -1, focus_y0 = -1, focus_y1 = -1;
    for (int i = top; i < doc.nlines && y < SCR_H - FTR_H - 4; i++) {
      int st = doc.lines[i].style;
      int lh = render_line_h(i);
      if (doc.lines[i].n) {
        const char *txt = doc.buf + doc.lines[i].off;
        bool sel = (i >= fs && i <= fe);   // Focus Block co the gom nhieu dong cua mot link
        if (sel) {
          int bx = UI_PAD - 3;
          int bw;
          if (st == 1 || st == 3 || st == 6) { bx = 2; bw = SCR_W - 4; }
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
        if (st == 6) {
          // Small Screen Rendering image placeholder: thu nho theo chieu rong 240px.
          int bx = UI_PAD, bw = SCR_W - 2 * UI_PAD, bh = 74;
          disp.fillRect(bx, y, bw, bh, UI_FIELD);
          disp.fillRect(bx, y, bw, 1, UI_BORDER);
          disp.fillRect(bx, y + bh - 1, bw, 1, UI_BORDER);
          disp.fillRect(bx, y, 1, bh, UI_BORDER);
          disp.fillRect(bx + bw - 1, y, 1, bh, UI_BORDER);
          disp.fillRect(bx + 10, y + 10, 38, 26, UI_BORDER);
          for (int d = 0; d <= 26; d++) {
            int xx = bx + 10 + d;
            int yy0 = y + 10 + (d * 26) / 38;
            int yy1 = y + 36 - (d * 26) / 38;
            disp.drawPixel(xx, yy0, UI_BORDER);
            disp.drawPixel(xx, yy1, UI_BORDER);
          }
          disp.setTextFont(1);
          disp.setTextColor(UI_DIM);
          disp.drawString("Image scaled to 240px", bx + 56, y + 10);
          disp.setTextFont(2);
          disp.setTextColor(UI_FG);
          disp.drawString(txt, bx + 56, y + 28);
          disp.setTextFont(1);
          disp.setTextColor(UI_DIM);
          disp.drawString("Opera Mini 4 SSR", bx + 56, y + 48);
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
    }
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

  // softkey bar — Menu | time | Back (giong Opera Mini / K750)
  disp.fillRect(0, SCR_H - FTR_H, SCR_W, FTR_H, UI_SOFT);
  disp.setTextFont(2); disp.setTextColor(UI_WHITE);
  const char *l = "Menu", *r = "Back";
  if (vkey_open) { l = "OK"; r = "Cancel"; }
  else if (menu_open) { l = "Select"; r = "Cancel"; }
  int sy = SCR_H - FTR_H + (FTR_H - 8) / 2;
  disp.drawString(l, UI_PAD, sy);
  // clock o giua (millis -> HH:MM tu 00:00)
  char clk[8];
  unsigned mins = (unsigned)(millis() / 60000u) % (24u * 60u);
  snprintf(clk, sizeof clk, "%02u:%02u", mins / 60, mins % 60);
  disp.drawString(clk, (SCR_W - disp.textWidth(clk)) / 2, sy);
  disp.drawString(r, SCR_W - disp.textWidth(r) - UI_PAD, sy);
  disp.endWrite();
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
  int x0 = (SCR_W - LOGO_W) / 2, y0 = (SCR_H - LOGO_H) / 2 - 20;
  disp.startWrite();
  // logo565.h: RGB565LE tu DMAX .vm_res -> 565 native cua LGFX (khong swap tay)
  for (int yy = 0; yy < LOGO_H; yy++)
    for (int xx = 0; xx < LOGO_W; xx++) {
      unsigned short p = logo565[yy * LOGO_W + xx];
      if (p == LOGO_KEY) continue;               // color-key trong suot
      uint8_t r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
      disp.drawPixel(x0 + xx, y0 + yy,
                     disp.color565(r << 3, g << 2, b << 3));
    }
  disp.setTextColor(TFT_WHITE); disp.setTextFont(4);
  disp.drawString("Browser", (SCR_W - disp.textWidth("Browser")) / 2, y0 + LOGO_H + 8);
  disp.setTextFont(2); disp.setTextColor(TFT_LIGHTGREY);
  disp.drawString("WML Browser for E524546", (SCR_W - disp.textWidth("WML Browser for E524546")) / 2, y0 + LOGO_H + 40);
  disp.endWrite();
}

// ---------------- keypad handler ----------------
static void jump_link(int dir) {
  // Java/Symbian spatial focus: moi link/van-ban/block chi dung MOT lan.
  // Trong layout mot cot, UP/LEFT = block truoc; DOWN/RIGHT = block sau.
  if (!doc.nlines || dir == 0) return;
  int fs = focus_block_start(focus_i), fe = focus_block_end(focus_i);
  int i = (dir > 0) ? fe + 1 : fs - 1;
  while (i >= 0 && i < doc.nlines && doc.lines[i].n == 0) i += (dir > 0 ? 1 : -1);
  if (i < 0 || i >= doc.nlines) return;  // dung tai bien, khong wrap bat ngo
  focus_i = focus_block_start(i);
  cursor_link = doc.lines[focus_i].link;
  ensure_focus_visible();
  Serial.printf("[focus] line=%d..%d link=%d top=%d dir=%d\n",
                focus_i, focus_block_end(focus_i), cursor_link, top, dir);
}
static void on_key(const char *k) {
  if (scr == SCR_SPLASH) return;

  // ---- Opera Mini: page overview ----
  if (overview_on) {
    int n = doc.nlines > 0 ? doc.nlines : 1;
    int avail_h = SCR_H - HDR_H - FTR_H - 12;
    int bh = avail_h / (n > 42 ? 42 : n);
    if (bh < 2) bh = 2;
    int max_rows = avail_h / bh;
    int vh = max_rows / (overview_zoom + 1);
    if (vh < 4) vh = 4;
    if (vh > max_rows) vh = max_rows;
    if (!strcmp(k, "back") || !strcmp(k, "menu")) { overview_on = false; render(); return; }
    if (!strcmp(k, "left")) { if (overview_zoom > 1) overview_zoom--; render(); return; }
    if (!strcmp(k, "right")) { if (overview_zoom < 3) overview_zoom++; render(); return; }
    if (!strcmp(k, "up"))   { if (ov_oy > 0) ov_oy--; render(); return; }
    if (!strcmp(k, "down")) { if (ov_oy < n - vh) ov_oy++; render(); return; }
    if (!strcmp(k, "ok"))   { top = ov_oy; focus_i = top; ensure_focus_visible(); overview_on = false; render(); return; }
    render(); return;
  }
  // ---- Opera Mini: virtual mouse ----
  if (mouse_on && !menu_open) {
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
        int d = abs(ly - mouse_y) + abs(20 - mouse_x);
        if (d < best_d) { best_d = d; best = li; }
      }
      if (best >= 0) { mouse_on = false; go_url(doc.links[best].url); }
      return;
    }
    render(); return;
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

  // ---- popup OPTION menu (giong anh DMAX) ----
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
        show_msg("SvBmrk: da luu Bookmark. OK de tiep.");
      } else if (menu_idx == 1) {   // Optn > SpDial
        go_url("mtt:start"); return;
      } else if (menu_idx == 2) {   // Navg > Zoom (overview)
        overview_on = true; ov_oy = top; overview_zoom = 1;
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
    if (!strcmp(k, "up") && net_cursor > 0) { net_cursor--; wifi_list_show(); }
    else if (!strcmp(k, "down") && net_cursor < net_n - 1) { net_cursor++; wifi_list_show(); }
    else if (!strcmp(k, "ok")) { char u[24]; snprintf(u, sizeof u, "mtt:wifisel#%d", net_cursor); go_url(u); }
    else if (!strcmp(k, "back") || !strcmp(k, "menu")) go_url("mtt:config");
    else if (!strcmp(k, "option")) { cfg_ssid = ""; cfg_pass = ""; config_save(); wifi_scan(); wifi_list_show(); }
    render(); return;
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
      if (wifi_up) go_url("mtt:start");
      else wifi_result(wifi_try_connect(nets[net_cursor].ssid, pass_in));
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
    } else if (!strcmp(k, "back")) {
      mt_commit();
      if (menu_src_url[0]) go_url(menu_src_url); else go_url("mtt:start");
    } else if (!strcmp(k, "menu")) {
      mt_commit(); go_url("mtt:start");
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
  if (!strcmp(k, "up"))    { jump_link(-1); render(); return; }
  if (!strcmp(k, "down"))  { jump_link(1);  render(); return; }
  if (!strcmp(k, "left"))  { jump_link(-1); render(); return; }
  if (!strcmp(k, "right")) { jump_link(1);  render(); return; }
  if (!strcmp(k, "ok")) {
    // OK: mo link trong khoi dang chon (neu co)
    int li = (focus_i >= 0 && focus_i < doc.nlines) ? doc.lines[focus_i].link : -1;
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
// lenh: go <url> | key <name> | scan | status
static char cli_in[160]; static int cli_n = 0;

static void cli_exec(const char *line) {
  if (!strncmp(line, "go ", 3)) { go_url(line + 3); return; }
  if (!strncmp(line, "key ", 4)) { on_key(line + 4); return; }
  if (!strcmp(line, "scan")) { wifi_scan(); wifi_list_show(); render(); return; }
  if (!strcmp(line, "status")) {
    Serial.printf("[status] wifi=%s ip=%s url=%s\n",
                  wifi_up ? "up" : "-",
                  wifi_up ? WiFi.localIP().toString().c_str() : "-",
                  cur_url);
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
  disp.init();
  Serial.println("[boot] 2 display");
  Serial.flush();
  disp.setRotation(0);                 // doc 240x320 theo E524546-OS
  disp.setBrightness(200);
  disp.fillScreen(TFT_BLACK);

  doc_buf = (char *)heap_caps_malloc(DOC_CAP, MALLOC_CAP_SPIRAM);
  net_buf = (char *)heap_caps_malloc(DOC_CAP, MALLOC_CAP_SPIRAM);
  if (!doc_buf || !net_buf) { doc_buf = (char*)malloc(DOC_CAP); net_buf = (char*)malloc(DOC_CAP); }
  doc.buf = doc_buf; doc.cap = DOC_CAP;
  store_init();
  keys_init();
  key_cb = on_key;

  draw_splash();
  delay(1200);

  WiFi.persistent(true);                 // luu tai WiFi vao flash (bo nho tam)
  if (cfg_ssid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg_ssid.c_str(), cfg_pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
    wifi_up = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[wifi] %s ip=%s\n", wifi_up ? "OK" : "FAIL",
                  wifi_up ? WiFi.localIP().toString().c_str() : "-");
  }
  if (!wifi_up) Serial.println("[wifi] Vao Settings > Ket noi WiFi de quet + nhap thu cong");
  go_url(cfg_home.length() ? cfg_home.c_str() : "mtt:start");
  Serial.println("[boot] setup xong. CLI: go <url> | key <name> | scan | status");
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
  static uint32_t hb = 0;
  if (millis() - hb > 15000) { hb = millis(); }   // (no-op giu cho struct tinh)
  delay(5);
}
