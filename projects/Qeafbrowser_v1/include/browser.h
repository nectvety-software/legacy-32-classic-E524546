// ESP_Browser — WML/HTML browser port tu DMAX_QQ_Browser (MRE VXP) cho E524546.
// Kien truc theo J2ME_to_MRE_Porting_Guide: pool tinh, buffer PSRAM, khong malloc trong loop.
#pragma once
#include <Arduino.h>
#include <WString.h>

#define SCR_W 240
#define SCR_H 320
#define DOC_CAP 49152          // body toi da (PSRAM)
#define MAX_LINES 400
#define MAX_LINKS 96
#define MAX_TITLE 96

// ---------------- Document (WML subset + HTML subset -> dong van ban + link)
struct Link {
  char url[192];
  int16_t line0, line1;        // vung dong chua link
};

struct Doc {
  char  *buf;                  // PSRAM: toan bo text sau parse
  size_t len, cap;
  struct Line { uint32_t off; uint16_t n; int16_t link; uint8_t style; } lines[MAX_LINES];
  int nlines;
  Link  links[MAX_LINKS];
  int nlinks;
  char  title[MAX_TITLE];
  bool  is_wml;
};

bool   doc_parse(Doc *d, const char *html, size_t len);   // wml.cpp
void   doc_free(Doc *d);

// ---------------- HTTP (http.cpp)
struct HttpMeta {
  int status;
  char content_type[48];
  char redirect[256];
  bool gzipped;
};
bool http_get(const char *url, char *buf, size_t cap, size_t *out_len, HttpMeta *meta);

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
// config.ini: wifi_ssid, wifi_pass, home_url
void config_load();
void config_save();            // ghi nguoc bo nho tam ra SD (wizard WiFi dung)
extern String cfg_ssid, cfg_pass, cfg_home;

// ---------------- URL helpers
bool url_split(const char *url, char *scheme, char *host, int *port, char *path);
bool url_is_mtt(const char *url);    // "mtt:xxx"
