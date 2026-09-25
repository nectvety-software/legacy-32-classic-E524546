// Qeafbrowser — WML/HTML browser port tu legacy keypad browser (MRE VXP) cho E524546.
// Kien truc theo J2ME_to_MRE_Porting_Guide: pool tinh, buffer PSRAM, khong malloc trong loop.
#pragma once
#include <Arduino.h>
#include <WString.h>

#define SCR_W 240
#define SCR_H 320
#define DOC_CAP (96 * 1024)    // body toi da (PSRAM)
#define MAX_LINES 400
#define MAX_LINKS 96
#define MAX_TITLE 96
#define MAX_IMAGES 48

// ---------------- Document (WML subset + HTML subset -> dong van ban + link)
struct Link {
  char url[192];
  int16_t line0, line1;        // vung dong chua link
};

struct ImageRef {
  char url[192];
  char alt[96];
  int16_t line0, line1;
};

struct Doc {
  char  *buf;                  // PSRAM: toan bo text sau parse
  size_t len, cap;
  struct Line { uint32_t off; uint16_t n; int16_t link; int16_t image; uint16_t block; uint8_t style; } lines[MAX_LINES];
  int nlines;
  Link  links[MAX_LINKS];
  int nlinks;
  ImageRef images[MAX_IMAGES];
  int nimages;
  char  title[MAX_TITLE];
  bool  is_wml;
};

bool   doc_parse(Doc *d, const char *html, size_t len);   // wml.cpp
bool   doc_line_is_separator(Doc *d, int line);           // wml.cpp: dong "|" giua 2 link
void   doc_free(Doc *d);


// ---------------- HTTP (http.cpp)
struct HttpMeta {
  int status;
  char content_type[48];
  char redirect[256];
  char final_url[256];
  long content_length;
  bool gzipped;
  bool truncated;
  bool secure;
};
bool http_get(const char *url, char *buf, size_t cap, size_t *out_len, HttpMeta *meta);
// Progress callback during body read: got = wire bytes, total = content length (-1 unknown).
typedef void (*HttpProgressFn)(long got, long total);
void http_set_progress(HttpProgressFn fn);   // nullptr deactivate

// ---------------- Store tren SD (store.cpp)
#define HIST_MAX 40
#define BOOK_MAX 24
void store_init();
void history_add(const char *url, const char *title);
int  history_count();
const char *history_url(int i);      // i=0 moi nhat
const char *history_title(int i);
void bookmark_add(const char *url, const char *title);
void bookmark_del(int i);
int  bookmark_count();
const char *bookmark_url(int i);
const char *bookmark_title(int i);
void cookie_set(const char *domain, const char *nameval);
void cookie_get(const char *domain, char *out, size_t cap);
// Thumbnail cache: raw RGB565 data keyed by URL. The RAM tier lives in PSRAM in main.cpp;
// these helpers provide the persistent LittleFS/selected-FS tier.
bool thumb_store_load(const char *url, uint16_t *pix, size_t pixel_count);
bool thumb_store_save(const char *url, const uint16_t *pix, size_t pixel_count);
// config.ini: wifi_ssid, wifi_pass, home_url, timezone, text_mode
void config_load();
void config_save();            // ghi nguoc bo nho tam ra SD (wizard WiFi dung)
extern String cfg_ssid, cfg_pass, cfg_home, cfg_tz;
extern bool cfg_text_mode;     // true = chi van ban (khong tai thumbnail anh)

// ---------------- Launcher FS helpers (store.cpp, dung chung fs_sel) --------
// Launcher va theme/art deu di qua 3 ham nay de khong phai biet SD hay LittleFS.
bool store_fs_read(const char *path, char *buf, size_t cap, size_t *len);
bool store_fs_write(const char *path, const char *data, size_t n);
bool store_fs_exists(const char *path);
bool store_fs_mount_sd();      // mount SD_MMC 1-bit; false = khong co the (dung LFS)
// Theme VQEAF: LUON doc tu LittleFS (fs_sel), khong dung SD.
bool store_theme_read(const char *path, char *buf, size_t cap, size_t *len);
int  store_theme_list(char names[][24], int maxn);  // liet ke /launcher/themes/*.vqeaf

// ---------------- URL helpers
bool url_split(const char *url, char *scheme, char *host, int *port, char *path);
bool url_is_mtt(const char *url);    // "mtt:xxx"
// Chuan hoa URL nguoi dung nhap: giu mtt/http/https, domain thuong -> https://domain.
bool url_normalize_input(const char *input, char *out, size_t cap);
// RFC3986 subset du cho browser nho: absolute, //host, /root, ?query, #fragment, ./ va ../.
bool url_resolve(const char *base, const char *href, char *out, size_t cap);
// Encode query UTF-8 theo application/x-www-form-urlencoded (space -> +).
void url_encode_query(const char *src, char *out, size_t cap);
