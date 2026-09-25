// store.cpp — History / Bookmark / Cookie / Config luu tren SD (fallback LittleFS).
// Port co che Bookmark.o / History.o / Cookie.o / Config.o cua DMAX_QQ_Browser.
// File dinh dang text: moi dong "url\ttitle" (history/bookmark), "domain\tname=val" (cookie).
#include "browser.h"
#include <SD_MMC.h>
#include <LittleFS.h>
#include <FS.h>

String cfg_ssid = "", cfg_pass = "", cfg_home = "mtt:start";

static fs::FS *fs_sel = nullptr;
static const char *DIR = "/ESPBrowser";

static bool f_read_all(const char *p, char *buf, size_t cap, size_t *len) {
  if (!fs_sel) return false;
  File f = fs_sel->open(p, FILE_READ);
  if (!f) return false;
  size_t n = 0;
  while (f.available() && n < cap - 1) buf[n++] = (char)f.read();
  buf[n] = 0; *len = n;
  f.close();
  return true;
}
static bool f_write_all(const char *p, const char *data, size_t n) {
  if (!fs_sel) return false;
  File f = fs_sel->open(p, FILE_WRITE);
  if (!f) return false;
  f.write((const uint8_t *)data, n);
  f.close();
  return true;
}

// ------------------------------ pool luu tru ------------------------------
struct Entry { char url[192]; char title[96]; bool used; };
static Entry hist[HIST_MAX];  static int hist_n = 0;   // 0 = moi nhat
static Entry book[BOOK_MAX];  static int book_n = 0;
static char  cookies[8][160]; static int  cookie_n = 0;

static void entries_load(Entry *e, int cap, int *n, const char *path) {
  static char buf[8192];
  size_t len;
  *n = 0;
  if (!f_read_all(path, buf, sizeof buf, &len)) return;
  char *p = strtok(buf, "\n");
  while (p && *n < cap) {
    char *tab = strchr(p, '\t');
    if (tab) {
      *tab = 0;
      strncpy(e[*n].url, p, sizeof e[*n].url - 1);
      strncpy(e[*n].title, tab + 1, sizeof e[*n].title - 1);
      e[*n].used = true;
      (*n)++;
    }
    p = strtok(0, "\n");
  }
}
static void entries_save(Entry *e, int n, const char *path) {
  static char buf[8192];
  size_t o = 0;
  for (int i = 0; i < n && o < sizeof buf - 300; i++)
    o += snprintf(buf + o, sizeof buf - o, "%s\t%s\n", e[i].url, e[i].title);
  f_write_all(path, buf, o);
}

void store_init() {
  // LittleFS.begin(false) = khong format (format co the treo man boot)
  if (LittleFS.begin(false)) {
    fs_sel = &LittleFS;
    Serial.println("[store] FS=LittleFS");
  } else if (LittleFS.begin(true)) {
    fs_sel = &LittleFS;
    Serial.println("[store] FS=LittleFS(fmt)");
  } else {
    fs_sel = nullptr;
    Serial.println("[store] FS=none");
  }
  if (fs_sel) {
    fs_sel->mkdir("/ESPBrowser");
    char pth[64];
    snprintf(pth, sizeof pth, "%s/history.txt", DIR);
    entries_load(hist, HIST_MAX, &hist_n, pth);
    snprintf(pth, sizeof pth, "%s/bookmark.txt", DIR);
    entries_load(book, BOOK_MAX, &book_n, pth);
  }
  {
    char buf[512]; size_t len = 0;
    if (fs_sel && f_read_all("/ESPBrowser/config.ini", buf, sizeof buf, &len)) {
      char *p = strtok(buf, "\r\n");
      while (p) {
        char *eq = strchr(p, '=');
        if (eq) {
          *eq = 0;
          if (!strcmp(p, "wifi_ssid")) cfg_ssid = eq + 1;
          else if (!strcmp(p, "wifi_pass")) cfg_pass = eq + 1;
          else if (!strcmp(p, "home_url")) cfg_home = eq + 1;
        }
        p = strtok(0, "\r\n");
      }
    }
  }
  Serial.printf("[store] fs=%s hist=%d book=%d ssid=%s home=%s\n",
                fs_sel ? (fs_sel == (fs::FS*)&SD_MMC ? "SD" : "LFS") : "none",
                hist_n, book_n, cfg_ssid.c_str(), cfg_home.c_str());
}

// Ghi bo nho tam cau hinh ra SD (wizard WiFi goi sau khi ket noi thanh cong).
// Neu khong co SD/LFS thi van giu trong RAM (cfg_* String) cho den khi reset.
void config_save() {
  if (!fs_sel) { Serial.println("[store] config_save: khong co FS, chi giu RAM"); return; }
  char buf[512];
  int o = snprintf(buf, sizeof buf,
                   "; ESP_Browser config — tu dong sinh boi wizard WiFi\n"
                   "wifi_ssid=%s\nwifi_pass=%s\nhome_url=%s\n",
                   cfg_ssid.c_str(), cfg_pass.c_str(),
                   cfg_home.length() ? cfg_home.c_str() : "mtt:start");
  if (f_write_all("/ESPBrowser/config.ini", buf, o))
    Serial.printf("[store] config.ini da luu (ssid=%s)\n", cfg_ssid.c_str());
}

void history_add(const char *url, const char *title) {
  for (int i = 0; i < hist_n; i++)
    if (!strcmp(hist[i].url, url)) {          // da co -> day len dau
      for (int k = i; k > 0; k--) hist[k] = hist[k - 1];
      strncpy(hist[0].url, url, sizeof hist[0].url - 1);
      strncpy(hist[0].title, title, sizeof hist[0].title - 1);
      goto save;
    }
  if (hist_n < HIST_MAX) hist_n++;
  for (int k = hist_n - 1; k > 0; k--) hist[k] = hist[k - 1];
  hist[0].used = true;
  strncpy(hist[0].url, url, sizeof hist[0].url - 1); hist[0].url[sizeof hist[0].url - 1] = 0;
  strncpy(hist[0].title, title, sizeof hist[0].title - 1); hist[0].title[sizeof hist[0].title - 1] = 0;
save:
  if (fs_sel) { char p[64]; snprintf(p, sizeof p, "%s/history.txt", DIR); entries_save(hist, hist_n, p); }
}
int  history_count() { return hist_n; }
const char *history_url(int i)   { return (i >= 0 && i < hist_n) ? hist[i].url : ""; }
const char *history_title(int i) { return (i >= 0 && i < hist_n) ? hist[i].title : ""; }

void bookmark_add(const char *url, const char *title) {
  if (book_n >= BOOK_MAX) return;
  strncpy(book[book_n].url, url, sizeof book[book_n].url - 1);
  book[book_n].url[sizeof book[book_n].url - 1] = 0;
  strncpy(book[book_n].title, title, sizeof book[book_n].title - 1);
  book[book_n].title[sizeof book[book_n].title - 1] = 0;
  book_n++;
  if (fs_sel) { char p[64]; snprintf(p, sizeof p, "%s/bookmark.txt", DIR); entries_save(book, book_n, p); }
}
void bookmark_del(int i) {
  if (i < 0 || i >= book_n) return;
  for (int k = i; k < book_n - 1; k++) book[k] = book[k + 1];
  book_n--;
  if (fs_sel) { char p[64]; snprintf(p, sizeof p, "%s/bookmark.txt", DIR); entries_save(book, book_n, p); }
}
int  bookmark_count() { return book_n; }
const char *bookmark_url(int i)   { return (i >= 0 && i < book_n) ? book[i].url : ""; }
const char *bookmark_title(int i) { return (i >= 0 && i < book_n) ? book[i].title : ""; }

void cookie_set(const char *domain, const char *nameval) {
  char nv[128];
  const char *semi = strchr(nameval, ';');
  size_t l = semi ? (size_t)(semi - nameval) : strlen(nameval);
  if (l >= sizeof nv) l = sizeof nv - 1;
  memcpy(nv, nameval, l); nv[l] = 0;
  char key[192]; snprintf(key, sizeof key, "%s\t%s", domain, nv);
  for (int i = 0; i < cookie_n; i++) {
    char *tab = strchr(cookies[i], '\t');
    if (tab && !strncmp(cookies[i], domain, tab - cookies[i]) &&
        !strncmp(tab + 1, nv, strcspn(nv, "=") + 1)) {   // cung ten -> ghi de
      strncpy(cookies[i], key, sizeof cookies[i] - 1);
      return;
    }
  }
  if (cookie_n < 8) strncpy(cookies[cookie_n++], key, sizeof cookies[0] - 1);
}
void cookie_get(const char *domain, char *out, size_t cap) {
  out[0] = 0;
  size_t o = 0;
  for (int i = 0; i < cookie_n; i++) {
    char *tab = strchr(cookies[i], '\t');
    if (!tab) continue;
    if (!strcmp(cookies[i], domain) || strstr(cookies[i], domain)) {
      int l = snprintf(out + o, cap - o, "%s%s", o ? "; " : "", tab + 1);
      if (l > 0) o += l;
      if (o >= cap - 2) break;
    }
  }
}
