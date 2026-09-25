// store.cpp — History / Bookmark / Cookie / Config luu tren SD (fallback LittleFS).
// Port co che Bookmark.o / History.o / Cookie.o / Config.o cua legacy keypad browser.
// File dinh dang text: moi dong "url\ttitle" (history/bookmark), "domain\tname=val" (cookie).
#include "browser.h"
#include <SD_MMC.h>
#include <LittleFS.h>
#include <FS.h>
#include <esp_heap_caps.h>               // cap bo nho cho hist/book/scratch tu PSRAM
#include "launcher_config.h"             // LC_DIR_THEMES cho store_theme_list

String cfg_ssid = "", cfg_pass = "", cfg_home = "https://qeafivels.com/", cfg_tz = "ICT-7";
bool cfg_text_mode = false;

static fs::FS *fs_sel = nullptr;
static const char *DIR = "/Qeafbrowser";

// PSRAM truoc, fallback RAM noi bo — giam ~35KB static RAM noi bo cho WiFi/TLS.
static void *store_alloc(size_t n) {
  void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
  if (!p) p = malloc(n);
  return p;
}

static bool f_read_all(const char *p, char *buf, size_t cap, size_t *len) {
  if (!fs_sel) return false;
  File f = fs_sel->open(p, FILE_READ);
  if (!f) return false;
  size_t n = 0;
  // Doc den EOF (khong tin available(): ftell/fseek tren sim co the ra 2 byte sai).
  for (;;) {
    int c = f.read();
    if (c < 0) break;
    if (n >= cap - 1) break;
    buf[n++] = (char)c;
  }
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
static Entry *hist = nullptr;  static int hist_n = 0;   // 0 = moi nhat (PSRAM, giam ~18KB noi bo)
static Entry *book = nullptr;  static int book_n = 0;
static char  cookies[8][160]; static int  cookie_n = 0;
// Buffer text dung chung load/save history+bookmark (PSRAM, thay 2 static 8KB noi bo).
static char *entries_buf = nullptr;
static char *entries_scratch() {
  if (!entries_buf) entries_buf = (char *)store_alloc(8192);
  return entries_buf;
}

static void cookies_save() {
  if (!fs_sel) return;
  char buf[2048]; size_t o = 0;
  for (int i = 0; i < cookie_n && o < sizeof buf - 180; i++)
    o += snprintf(buf + o, sizeof buf - o, "%s\n", cookies[i]);
  f_write_all("/Qeafbrowser/cookies.txt", buf, o);
}

static void cookies_load() {
  cookie_n = 0;
  if (!fs_sel) return;
  char buf[2048]; size_t len = 0;
  if (!f_read_all("/Qeafbrowser/cookies.txt", buf, sizeof buf, &len)) return;
  char *p = strtok(buf, "\r\n");
  while (p && cookie_n < 8) {
    if (strchr(p, '\t') && strchr(p, '=')) {
      strncpy(cookies[cookie_n], p, sizeof cookies[0] - 1);
      cookies[cookie_n][sizeof cookies[0] - 1] = 0;
      cookie_n++;
    }
    p = strtok(nullptr, "\r\n");
  }
}

static void entries_load(Entry *e, int cap, int *n, const char *path) {
  const size_t CAP = 8192;
  *n = 0;
  if (!e) return;
  char *buf = entries_scratch();
  if (!buf) return;
  size_t len;
  if (!f_read_all(path, buf, CAP, &len)) return;
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
  const size_t CAP = 8192;
  char *buf = entries_scratch();
  if (!buf || !e) return;
  size_t o = 0;
  for (int i = 0; i < n && o < CAP - 300; i++)
    o += snprintf(buf + o, CAP - o, "%s\t%s\n", e[i].url, e[i].title);
  f_write_all(path, buf, o);
}

void store_init() {
  // Cap bo nho cho hist/book TRUOC khi load (PSRAM, fallback malloc).
  if (!hist) hist = (Entry *)store_alloc(sizeof(Entry) * HIST_MAX);
  if (!book) book = (Entry *)store_alloc(sizeof(Entry) * BOOK_MAX);
  if (!hist || !book) Serial.println("[store] CRITICAL: cannot alloc hist/book in PSRAM");
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
    fs_sel->mkdir("/Qeafbrowser");
    fs_sel->mkdir("/Qeafbrowser/thumbcache");
    fs_sel->mkdir("/launcher");
    fs_sel->mkdir("/launcher/themes");
    char pth[64];
    snprintf(pth, sizeof pth, "%s/history.txt", DIR);
    entries_load(hist, HIST_MAX, &hist_n, pth);
    snprintf(pth, sizeof pth, "%s/bookmark.txt", DIR);
    entries_load(book, BOOK_MAX, &book_n, pth);
    if (book) {
      static const char *DEF_BM[][2] = {
        { "http://tranlong.hexat.com/",  "TL - Bookmark" },
        { "http://schema.mw.lt/",        "Schema Generator" },
        { "https://wapvxp.xtgem.com/",   "WapVXP.XtGem.com" },
      };
      int added = 0;
      for (size_t i = 0; i < sizeof DEF_BM / sizeof DEF_BM[0]; i++) {
        bool have = false;
        for (int j = 0; j < book_n; j++)
          if (!strcmp(book[j].url, DEF_BM[i][0])) { have = true; break; }
        if (have || book_n >= BOOK_MAX) continue;
        Entry &e = book[book_n++];
        strncpy(e.url, DEF_BM[i][0], sizeof e.url - 1); e.url[sizeof e.url - 1] = 0;
        strncpy(e.title, DEF_BM[i][1], sizeof e.title - 1); e.title[sizeof e.title - 1] = 0;
        e.used = true;
        added++;
      }
      if (added) {
        entries_save(book, book_n, pth);
        Serial.printf("[store] seeded %d default bookmarks (total %d)\n", added, book_n);
      }
    }
    cookies_load();
  }
  {
    char buf[512]; size_t len = 0;
    bool ok = fs_sel && f_read_all("/Qeafbrowser/config.ini", buf, sizeof buf, &len);
    if (ok) {
      char *p = strtok(buf, "\r\n");
      while (p) {
        char *eq = strchr(p, '=');
        if (eq) {
          *eq = 0;
          if (!strcmp(p, "wifi_ssid")) cfg_ssid = eq + 1;
          else if (!strcmp(p, "wifi_pass")) cfg_pass = eq + 1;
          else if (!strcmp(p, "home_url")) cfg_home = eq + 1;
          else if (!strcmp(p, "timezone")) cfg_tz = eq + 1;
          else if (!strcmp(p, "text_mode")) cfg_text_mode = (eq[1] == '1');
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
  if (!fs_sel) { Serial.println("[store] config_save: no FS, RAM only"); return; }
  char buf[512];
  int o = snprintf(buf, sizeof buf,
                   "; Qeafbrowser config — tu dong sinh boi wizard WiFi\n"
                   "wifi_ssid=%s\nwifi_pass=%s\nhome_url=%s\ntimezone=%s\n"
                   "text_mode=%d\n",
                   cfg_ssid.c_str(), cfg_pass.c_str(),
                   cfg_home.length() ? cfg_home.c_str() : "https://qeafivels.com/",
                   cfg_tz.c_str(), cfg_text_mode ? 1 : 0);
  if (f_write_all("/Qeafbrowser/config.ini", buf, o))
    Serial.printf("[store] config.ini saved (ssid=%s text_mode=%d)\n",
                  cfg_ssid.c_str(), cfg_text_mode ? 1 : 0);
}


static uint32_t thumb_hash_url(const char *s) {
  uint32_t h = 2166136261u;
  for (; s && *s; ++s) { h ^= (uint8_t)*s; h *= 16777619u; }
  return h;
}

static uint32_t thumb_crc32(const uint8_t *data, size_t n) {
  uint32_t crc = 0xFFFFFFFFu;
  while (n--) {
    crc ^= *data++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

#pragma pack(push,1)
struct ThumbDiskHeaderV1 {
  uint32_t magic;      // 'QBTC'
  uint16_t version;    // 1
  uint16_t pixels;
  uint32_t url_hash;
};
struct ThumbDiskHeaderV2 {
  uint32_t magic;      // 'QBTC'
  uint16_t version;    // 2
  uint16_t pixels;
  uint32_t url_hash;
  uint16_t width;
  uint16_t height;
  uint16_t format;     // 1 = RGB565 native
  uint16_t reserved;
  uint32_t data_crc32;
};
#pragma pack(pop)

static void thumb_path(const char *url, char *out, size_t cap) {
  snprintf(out, cap, "%s/thumbcache/%08lx.qbt", DIR, (unsigned long)thumb_hash_url(url));
}

bool thumb_store_load(const char *url, uint16_t *pix, size_t pixel_count) {
  if (!fs_sel || !url || !pix || !pixel_count || pixel_count > 65535) return false;
  char path[96]; thumb_path(url, path, sizeof path);
  File f = fs_sel->open(path, FILE_READ);
  if (!f) return false;

  ThumbDiskHeaderV1 common{};
  uint8_t *cp = (uint8_t*)&common;
  size_t got = 0;
  while (f.available() && got < sizeof common) cp[got++] = (uint8_t)f.read();
  if (got != sizeof common || common.magic != 0x43544251u ||
      common.pixels != pixel_count || common.url_hash != thumb_hash_url(url)) {
    f.close(); return false;
  }

  uint32_t expected_crc = 0;
  if (common.version == 2) {
    ThumbDiskHeaderV2 h{};
    memcpy(&h, &common, sizeof common);
    uint8_t *rest = ((uint8_t*)&h) + sizeof common;
    size_t need = sizeof h - sizeof common, rg = 0;
    while (f.available() && rg < need) rest[rg++] = (uint8_t)f.read();
    if (rg != need || h.format != 1 || h.width == 0 || h.height == 0 ||
        (size_t)h.width * h.height != pixel_count) { f.close(); return false; }
    expected_crc = h.data_crc32;
  } else if (common.version != 1) {
    f.close(); return false;
  }

  uint8_t *dst = (uint8_t*)pix;
  const size_t bytes = pixel_count * sizeof(uint16_t);
  got = 0;
  while (f.available() && got < bytes) dst[got++] = (uint8_t)f.read();
  f.close();
  if (got != bytes) return false;

  if (common.version == 2) {
    uint32_t crc = thumb_crc32(dst, bytes);
    if (crc != expected_crc) {
      Serial.printf("[thumb] LFS CRC mismatch %s got=%08lx expected=%08lx\n",
                    path, (unsigned long)crc, (unsigned long)expected_crc);
      return false;
    }
  }
  Serial.printf("[thumb] LFS hit v%u %s (%u px)\n", (unsigned)common.version, path, (unsigned)pixel_count);
  return true;
}

bool thumb_store_save(const char *url, const uint16_t *pix, size_t pixel_count) {
  if (!fs_sel || !url || !pix || !pixel_count || pixel_count > 65535) return false;
  fs_sel->mkdir("/Qeafbrowser/thumbcache");
  char path[96]; thumb_path(url, path, sizeof path);
  File f = fs_sel->open(path, FILE_WRITE);
  if (!f) return false;
  const size_t bytes = pixel_count * sizeof(uint16_t);
  uint16_t width = 56, height = (uint16_t)(pixel_count / 56);
  if ((size_t)width * height != pixel_count) { width = (uint16_t)pixel_count; height = 1; }
  ThumbDiskHeaderV2 h{0x43544251u, 2, (uint16_t)pixel_count, thumb_hash_url(url),
                      width, height, 1, 0, thumb_crc32((const uint8_t*)pix, bytes)};
  bool ok = f.write((const uint8_t*)&h, sizeof h) == sizeof h;
  if (ok) ok = f.write((const uint8_t*)pix, bytes) == bytes;
  f.close();
  if (ok) Serial.printf("[thumb] LFS save v2 %s (%u px crc=%08lx)\n",
                        path, (unsigned)pixel_count, (unsigned long)h.data_crc32);
  return ok;
}

void history_add(const char *url, const char *title) {
  if (!hist) return;
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
  if (!book || book_n >= BOOK_MAX) return;
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
  if (!domain || !domain[0] || !nameval || !nameval[0]) return;
  char nv[128];
  const char *semi = strchr(nameval, ';');
  size_t l = semi ? (size_t)(semi - nameval) : strlen(nameval);
  if (l >= sizeof nv) l = sizeof nv - 1;
  memcpy(nv, nameval, l); nv[l] = 0;
  char key[192]; snprintf(key, sizeof key, "%s\t%s", domain, nv);
  const char *eq = strchr(nv, '=');
  if (!eq || eq == nv) return;
  size_t name_len = (size_t)(eq - nv);
  for (int i = 0; i < cookie_n; i++) {
    char *tab = strchr(cookies[i], '\t');
    if (!tab) continue;
    size_t dom_len = (size_t)(tab - cookies[i]);
    const char *old_eq = strchr(tab + 1, '=');
    size_t old_name_len = old_eq ? (size_t)(old_eq - (tab + 1)) : 0;
    if (strlen(domain) == dom_len && !strncmp(cookies[i], domain, dom_len) &&
        old_name_len == name_len && !strncmp(tab + 1, nv, name_len)) {
      strncpy(cookies[i], key, sizeof cookies[i] - 1);
      cookies[i][sizeof cookies[i] - 1] = 0;
      cookies_save();
      return;
    }
  }
  if (cookie_n < 8) {
    strncpy(cookies[cookie_n], key, sizeof cookies[0] - 1);
    cookies[cookie_n][sizeof cookies[0] - 1] = 0;
    cookie_n++;
    cookies_save();
  }
}
void cookie_get(const char *domain, char *out, size_t cap) {
  if (!domain || !out || cap == 0) return;
  out[0] = 0;
  size_t o = 0;
  for (int i = 0; i < cookie_n; i++) {
    char *tab = strchr(cookies[i], '\t');
    if (!tab) continue;
    size_t dom_len = (size_t)(tab - cookies[i]);
    bool exact = strlen(domain) == dom_len && !strncmp(cookies[i], domain, dom_len);
    bool subdomain = strlen(domain) > dom_len &&
                     domain[strlen(domain) - dom_len - 1] == '.' &&
                     !strncmp(domain + strlen(domain) - dom_len, cookies[i], dom_len);
    if (exact || subdomain) {
      int l = snprintf(out + o, cap - o, "%s%s", o ? "; " : "", tab + 1);
      if (l > 0) o += l;
      if (o >= cap - 2) break;
    }
  }
}

// ================= Launcher FS helpers (dung chung fs_sel) =================
// Launcher khong can biet duoi cung la SD_MMC hay LittleFS: moi doc/ghi di qua
// cac ham nay. SD_MMC 1-bit (chan trong pins.h) duoc mount truoc; that bai thi
// van con LittleFS cua store_init().
#include <SD_MMC.h>

static bool sd_mounted = false;

bool store_fs_mount_sd() {
  if (sd_mounted) return true;
#if defined(ARDUINO)
  // SD_MMC 1-bit: DAT0=9, CMD=11, CLK=13; DAT3/CD=10 (khong pull-up trong mount).
  if (SD_MMC.begin("/sdcard", true)) {
    sd_mounted = true;
    Serial.printf("[store] SD mounted, %llu MB\n", (unsigned long long)(SD_MMC.cardSize() / (1024ULL * 1024ULL)));
    SD_MMC.mkdir("/launcher");
    SD_MMC.mkdir("/launcher/themes");
    SD_MMC.mkdir("/launcher/art");
    return true;
  }
  Serial.println("[store] SD mount failed -> chi dung LittleFS");
#else
  // sim: SimFS SD_MMC root la sim_sd/, chi dam bao thu muc ton tai
  SD_MMC.mkdir("/launcher");
  SD_MMC.mkdir("/launcher/themes");
  SD_MMC.mkdir("/launcher/art");
  sd_mounted = true;
#endif
  return false;
}

// Chon FS cho launcher: SD neu co, khong thi LittleFS cua store.
static fs::FS *launcher_fs() {
  if (sd_mounted) return (fs::FS *)&SD_MMC;
  return fs_sel;
}

// Theme VQEAF: LUON LittleFS (fs_sel) — khong doc tu SD.
bool store_theme_read(const char *path, char *buf, size_t cap, size_t *len) {
  if (!fs_sel || !path || !buf || cap < 2) return false;
  File f = fs_sel->open(path, FILE_READ);
  if (!f) return false;
  size_t n = 0;
  for (;;) {
    int c = f.read();
    if (c < 0) break;
    if (n >= cap - 1) break;
    buf[n++] = (char)c;
  }
  buf[n] = 0;
  if (len) *len = n;
  f.close();
  return true;
}

static bool has_ext(const char *name, const char *ext) {
  size_t n = strlen(name), e = strlen(ext);
  if (n <= e) return false;
  return strcasecmp(name + n - e, ext) == 0;
}

// Liet ke /launcher/themes/*.vqeaf (khong gom duong dan, max 8 ten).
int store_theme_list(char names[][24], int maxn) {
  if (!fs_sel || !names || maxn <= 0) return 0;
  int n = 0;
#if defined(ARDUINO)
  File root = fs_sel->open(LC_DIR_THEMES);
  if (root && root.isDirectory()) {
    for (File e = root.openNextFile(); e && n < maxn; e = root.openNextFile()) {
      if (e.isDirectory()) continue;
      const char *nm = e.name();
      const char *slash = strrchr(nm, '/');
      if (slash) nm = slash + 1;
      slash = strrchr(nm, '\\');
      if (slash) nm = slash + 1;
      if (!has_ext(nm, LC_THEME_EXT)) continue;
      // luu ten khong duoi .vqeaf (vd "amoled_red")
      snprintf(names[n], 24, "%s", nm);
      size_t L = strlen(names[n]);
      if (L > 6 && !strcasecmp(names[n] + L - 6, LC_THEME_EXT))
        names[n][L - 6] = 0;
      n++;
    }
    root.close();
  }
#else
  // sim: SimFS khong co Dir — quet ten .vqeaf da biet
  static const char *KNOWN[] = {
    "retrogo_dark", "s60_green", "amoled_red", "blue_s60", "retrogo_light", 0
  };
  for (int i = 0; KNOWN[i] && n < maxn; i++) {
    char p[96];
    char probe[8];
    size_t got = 0;
    snprintf(p, sizeof p, LC_DIR_THEMES "/%s" LC_THEME_EXT, KNOWN[i]);
    if (store_theme_read(p, probe, sizeof probe, &got) && got > 0) {
      snprintf(names[n], 24, "%s", KNOWN[i]);
      n++;
    }
  }
#endif
  return n;
}

bool store_fs_read(const char *path, char *buf, size_t cap, size_t *len) {
  fs::FS *fsx = launcher_fs();
  if (!fsx || !path || !buf || cap < 2) return false;
  File f = fsx->open(path, FILE_READ);
  if (!f) return false;
  size_t n = 0;
  for (;;) {
    int c = f.read();
    if (c < 0) break;
    if (n >= cap - 1) break;
    buf[n++] = (char)c;
  }
  buf[n] = 0;
  if (len) *len = n;
  f.close();
  return true;
}

bool store_fs_write(const char *path, const char *data, size_t n) {
  fs::FS *fsx = launcher_fs();
  if (!fsx || !path || !data) return false;
  File f = fsx->open(path, FILE_WRITE);
  if (!f) return false;
  size_t w = f.write((const uint8_t *)data, n);
  f.close();
  return w == n;
}

bool store_fs_exists(const char *path) {
  fs::FS *fsx = launcher_fs();
  if (!fsx || !path) return false;
  File f = fsx->open(path, FILE_READ);
  if (!f) return false;
  f.close();
  return true;
}
