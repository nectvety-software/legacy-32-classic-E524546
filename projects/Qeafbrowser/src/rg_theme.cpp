// rg_theme.cpp — loader theme.json schema Retro-Go (xem rg_theme.h).
// Parser JSON tối thiểu viết tay (không thêm thư viện — luật PROMPT.md §3):
// chỉ cần đọc "section" { "key": value } với value là số / chuỗi — đủ cho THEMING.md.
// Thứ tự tìm file: SD:/retro-go/themes/<name>/theme.json -> LittleFS cùng đường dẫn
// -> mặc định build-in. Mọi lỗi đều rơi về mặc định, không crash.
#include "rg_theme.h"
#include "rg_config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <string.h>
#include <stdlib.h>

static rg_theme_t s_theme;
static bool s_sd_mounted = false;

// ------------------------------------------------- theme mặc định Retro-Go
// Sao y retro-go/themes/default/theme.json (THEMING.md).
void rg_theme_defaults(rg_theme_t *t) {
  memset(t, 0, sizeof(*t));
  t->dialog = {
    0x0010,          // background (navy)
    0xFFFF,          // foreground
    0x6B4D,          // border (dim gray)
    0xFFFF,          // header
    0xFFFF,          // scrollbar
    RG_C_NONE,       // shadow = "none"
    0xFFFF,          // item_standard
    0x8410,          // item_disabled (gray)
    0xBDF7,          // item_message (silver)
  };
  // launcher_1: chọn = chữ trắng trên nền (bg transparent)
  t->launcher[0] = { 0x0000, 0xFFDE, RG_C_TRANSPARENT, 0x8410, RG_C_TRANSPARENT, 0xFFFF };
  // launcher_2: chọn = chữ xanh lá
  t->launcher[1] = { 0x0000, 0xFFDE, RG_C_TRANSPARENT, 0x8410, RG_C_TRANSPARENT, 0x07E0 };
  // launcher_3: chọn = nền trắng chữ đen
  t->launcher[2] = { 0x0000, 0xFFDE, RG_C_TRANSPARENT, 0x8410, 0xFFFF, 0x0000 };
  // launcher_4: như 3, chữ thường xám đậm hơn
  t->launcher[3] = { 0x0000, 0xFFDE, RG_C_TRANSPARENT, 0xAD55, 0xFFFF, 0x0000 };
  strncpy(t->name, RG_THEME_DEFAULT, sizeof(t->name) - 1);
  t->from_file = false;
}

// ------------------------------------------------- parser JSON tối thiểu
// Tìm ranh giới section {"dialog": { ... }} — trả về con trỏ [begin,end) nội dung trong {}.
static bool json_section(char *json, const char *section, char **begin, char **end) {
  char pat[40];
  snprintf(pat, sizeof pat, "\"%s\"", section);
  char *p = strstr(json, pat);
  if (!p) return false;
  p = strchr(p + strlen(pat), '{');
  if (!p) return false;
  int depth = 0; bool in_str = false;
  for (char *q = p; *q; q++) {
    if (*q == '"' && (q == p || q[-1] != '\\')) in_str = !in_str;
    if (in_str) continue;
    if (*q == '{') depth++;
    else if (*q == '}') { if (--depth == 0) { *begin = p + 1; *end = q; return true; } }
  }
  return false;
}

// Đọc giá trị màu của key trong [begin,end). Giữ nguyên *out nếu không thấy.
static void json_color(char *begin, char *end, const char *key, rg_color_t *out) {
  char pat[40];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  char *p = begin;
  while ((p = strstr(p, pat))) {
    // key phải đứng sau ranh giới hợp lệ (tránh khớp chuỗi con trong "__comment")
    if (!(p == begin || p[-1] == ',' || p[-1] == '{' || p[-1] == '\n' ||
          p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r')) { p += strlen(pat); continue; }
    char *v = p + strlen(pat);
    if (v >= end) return;
    while (v < end && (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n' || *v == ':')) v++;
    if (v >= end) return;
    char *e = v;   // value kết thúc tại , hoặc } hoặc xuống dòng
    while (e < end && *e != ',' && *e != '}' && *e != '\n') e++;
    char saved = *e; *e = 0;
    while (*v == ' ') v++;
    if (!strncmp(v, "\"transparent\"", 13))           *out = RG_C_TRANSPARENT;
    else if (!strncmp(v, "\"none\"", 6))              *out = RG_C_NONE;
    else if (*v == '"')                               *out = (rg_color_t)strtoul(v + 1, nullptr, 0);
    else if (*v == '-' || (*v >= '0' && *v <= '9'))   *out = (rg_color_t)strtol(v, nullptr, 0);
    *e = saved;
    return;
  }
}

#define JC(key, field) json_color(b, e, key, &field)

static void theme_parse(char *json, rg_theme_t *t) {
  char *b, *e;
  if (json_section(json, "dialog", &b, &e)) {
    JC("background", t->dialog.background);
    JC("foreground", t->dialog.foreground);
    JC("border", t->dialog.border);
    JC("header", t->dialog.header);
    JC("scrollbar", t->dialog.scrollbar);
    JC("shadow", t->dialog.shadow);
    JC("item_standard", t->dialog.item_standard);
    JC("item_disabled", t->dialog.item_disabled);
    JC("item_message", t->dialog.item_message);
  }
  for (int i = 0; i < 4; i++) {
    char sec[20];
    snprintf(sec, sizeof sec, "launcher_%d", i + 1);
    if (json_section(json, sec, &b, &e)) {
      JC("background", t->launcher[i].background);
      JC("foreground", t->launcher[i].foreground);
      JC("list_standard_bg", t->launcher[i].list_standard_bg);
      JC("list_standard_fg", t->launcher[i].list_standard_fg);
      JC("list_selected_bg", t->launcher[i].list_selected_bg);
      JC("list_selected_fg", t->launcher[i].list_selected_fg);
    }
  }
}

// ------------------------------------------------- đọc file từ SD / LittleFS
static bool read_file(fs::FS &fsys, const char *path, char *buf, size_t cap, size_t *out_len) {
  File f = fsys.open(path, "r");
  if (!f) return false;
  size_t n = f.read((uint8_t *)buf, cap - 1);
  buf[n] = 0;
  f.close();
  if (out_len) *out_len = n;
  return n > 0;
}

// ------------------------------------------------- API
void rg_theme_set_sd_mounted(bool mounted) { s_sd_mounted = mounted; }

void rg_theme_set_name(const char *name) {
  strncpy(s_theme.name, name ? name : RG_THEME_DEFAULT, sizeof(s_theme.name) - 1);
  s_theme.name[sizeof(s_theme.name) - 1] = 0;
}
const char *rg_theme_name() { return s_theme.name; }
const rg_theme_t *rg_theme_current() { return &s_theme; }

bool rg_theme_load(const char *name) {
  rg_theme_defaults(&s_theme);            // luôn bắt đầu từ mặc định
  if (!name || !*name) name = RG_THEME_DEFAULT;

  static char buf[RG_THEME_JSON_MAX];     // buffer tĩnh, không malloc
  char path[96];
  snprintf(path, sizeof path, "%s/%s/theme.json", RG_DIR_SD_THEMES, name);

  size_t len = 0;
  bool ok = false;
  if (s_sd_mounted) ok = read_file(SD_MMC, path, buf, sizeof buf, &len);
  if (!ok) ok = read_file(LittleFS, path, buf, sizeof buf, &len);
  if (!ok) {
    Serial.printf("[rg_theme] khong doc duoc %s -> mac dinh Retro-Go\n", path);
    rg_theme_set_name(name);
    return false;
  }
  theme_parse(buf, &s_theme);
  s_theme.from_file = true;
  rg_theme_set_name(name);
  Serial.printf("[rg_theme] loaded '%s' (%u bytes)\n", name, (unsigned)len);
  return true;
}

int rg_theme_list(char names[][RG_THEME_NAME_MAX], int maxn) {
  int n = 0;
  fs::FS *fsys = s_sd_mounted ? (fs::FS *)&SD_MMC : (fs::FS *)&LittleFS;
  File dir = fsys->open(RG_DIR_SD_THEMES);
  if (!dir && s_sd_mounted) { fsys = &LittleFS; dir = fsys->open(RG_DIR_SD_THEMES); }
  if (!dir) return 0;
  for (File f = dir.openNextFile(); f && n < maxn; f = dir.openNextFile()) {
    if (f.isDirectory()) {
      const char *base = f.name();        // tên thư mục = tên theme
      const char *slash = strrchr(base, '/');
      if (slash) base = slash + 1;
      if (*base) {
        strncpy(names[n], base, RG_THEME_NAME_MAX - 1);
        names[n][RG_THEME_NAME_MAX - 1] = 0;
        n++;
      }
    }
    f.close();
  }
  dir.close();
  return n;
}

bool rg_theme_next(int dir) {
  char names[RG_THEMES_MAX][RG_THEME_NAME_MAX];
  int n = rg_theme_list(names, RG_THEMES_MAX);
  if (n <= 0) return false;
  int cur = -1;
  for (int i = 0; i < n; i++) if (!strcmp(names[i], s_theme.name)) { cur = i; break; }
  cur = (cur < 0) ? 0 : (cur + dir + n) % n;
  return rg_theme_load(names[cur]);
}
