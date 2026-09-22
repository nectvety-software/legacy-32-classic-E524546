// engine_esp32.cpp — Port runtime_bridge.c của LuaS30-IDE sang LovyanGFX + LittleFS/SD_MMC.
// Lõi Lua 5.1.5 nguyên bản từ vendor/lua-5.1.5 (không đổi syntax, giữ tương thích Lua 5.1).
#include "engine_esp32.h"
#include "pins.h"  // pins trước để LGFX header thấy đủ chân
#include "LGFX_ESP32S3_ST7789.h"  // (tự include FS/LittleFS/SD_MMC trước LovyanGFX)

static LGFX_ESP32S3_ST7789* G = nullptr;
static LGFX_Sprite* FSp = nullptr;
#define FB (*FSp)          // framebuffer 240x320 RGB565, giống g_buf của LuaS30
static lua_State* L = nullptr;
static int g_fps = 30;
static int g_frame_ms = 33;
static unsigned long g_last = 0;
static bool g_started = false;
static bool g_paused = false;
static bool g_panic = false;
static bool g_sd = false;
static int g_font_px = 8;

void engine_error_screen(const char* msg) {
  if (!G) return;
  G->fillScreen(TFT_BLACK);
  G->setTextSize(2);
  G->setTextColor(TFT_WHITE, TFT_BLACK);
  G->setCursor(4, 4);
  G->print("LUA ERROR");
  G->setTextSize(1);
  G->setCursor(4, 30);
  G->print(msg ? msg : "unknown");
  G->setCursor(4, 300);
  G->print("RESET de nap lai");
}

static int report(int st) {
  if (st != 0 && L) {
    const char* s = lua_tostring(L, -1);
    if (!s) s = "Lua error";
    Serial.printf("[lua] %s\n", s);
    engine_error_screen(s);
    lua_pop(L, 1);
    g_panic = true;
  }
  return st;
}

static int call_hook(const char* name, bool with_dt, double dt) {
  if (!L || g_panic) return -1;
  int top = lua_gettop(L);
  lua_getglobal(L, "engine");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_getglobal(L, "mre"); }
  if (!lua_istable(L, -1)) { lua_settop(L, top); return -1; }
  lua_getfield(L, -1, name);
  if (!lua_isfunction(L, -1)) { lua_settop(L, top); return 0; }
  int nargs = 0;
  if (with_dt) { lua_pushnumber(L, dt); nargs = 1; }
  int st = lua_pcall(L, nargs, 0, 0);
  if (st) report(st);
  lua_settop(L, top);
  return st;
}

// ---------- helpers: file backend LittleFS + SD ----------
static bool is_sd_path(const char* p) { return p && strncmp(p, "sd:", 3) == 0; }

static String fs_path(const char* name) {
  String s = name ? name : "";
  if (is_sd_path(name)) return String("/") + (name + 3); // dùng trên volume SD_MMC
  if (!s.startsWith("/")) s = "/" + s;
  return s;
}

// Đọc toàn bộ file text/binary vào RAM (giới hạn 192KB như LS30_FILE_MAX)
static bool load_text_file(const char* name, String& out) {
  String p = fs_path(name);
  File f;
  if (is_sd_path(name)) { if (!g_sd) return false; f = SD_MMC.open(p, "r"); }
  else f = LittleFS.open(p, "r");
  if (!f) {
    // fallback: thử volume còn lại
    if (is_sd_path(name)) { f = LittleFS.open(String("/") + (name + 3), "r"); }
    else if (g_sd) { f = SD_MMC.open(p, "r"); }
    if (!f) return false;
  }
  out = f.readString();
  f.close();
  return true;
}

static uint16_t rgb565(int r, int g, int b) {
  r = constrain(r, 0, 255); g = constrain(g, 0, 255); b = constrain(b, 0, 255);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ---------- engine.* bindings (tên + ngữ nghĩa Y HỆT LuaS30 API.md) ----------
static int l_color(lua_State* Ls) {
  int r = (int)luaL_checknumber(Ls, 1), g = (int)luaL_checknumber(Ls, 2), b = (int)luaL_checknumber(Ls, 3);
  lua_pushnumber(Ls, rgb565(r, g, b));
  return 1;
}
static int l_clear(lua_State* Ls) {
  uint16_t c = (uint16_t)luaL_checknumber(Ls, 1);
  FB.fillScreen(c);
  return 0;
}
static int l_rect(lua_State* Ls) {
  int x = (int)luaL_checknumber(Ls, 1), y = (int)luaL_checknumber(Ls, 2);
  int w = (int)luaL_checknumber(Ls, 3), h = (int)luaL_checknumber(Ls, 4);
  uint16_t c = (uint16_t)luaL_checknumber(Ls, 5);
  FB.fillRect(x, y, w, h, c);
  return 0;
}
static int l_frame(lua_State* Ls) {
  int x = (int)luaL_checknumber(Ls, 1), y = (int)luaL_checknumber(Ls, 2);
  int w = (int)luaL_checknumber(Ls, 3), h = (int)luaL_checknumber(Ls, 4);
  uint16_t c = (uint16_t)luaL_checknumber(Ls, 5);
  FB.drawRect(x, y, w, h, c);
  return 0;
}
static int l_line(lua_State* Ls) {
  int x0 = (int)luaL_checknumber(Ls, 1), y0 = (int)luaL_checknumber(Ls, 2);
  int x1 = (int)luaL_checknumber(Ls, 3), y1 = (int)luaL_checknumber(Ls, 4);
  uint16_t c = (uint16_t)luaL_checknumber(Ls, 5);
  FB.drawLine(x0, y0, x1, y1, c);
  return 0;
}
static int l_text(lua_State* Ls) {
  int x = (int)luaL_checknumber(Ls, 1), y = (int)luaL_checknumber(Ls, 2);
  const char* s = luaL_checkstring(Ls, 3);
  uint16_t c = lua_gettop(Ls) >= 4 ? (uint16_t)lua_tonumber(Ls, 4) : (uint16_t)TFT_WHITE;
  FB.setTextColor(c);
  FB.setCursor(x, y);
  FB.print(s);
  return 0;
}
static int l_set_font(lua_State* Ls) {
  int n = (int)luaL_optnumber(Ls, 1, 8);
  g_font_px = n;
  FB.setTextSize(n <= 8 ? 1 : (n <= 12 ? 2 : 3));
  return 0;
}
static int l_text_width(lua_State* Ls) {
  const char* s = luaL_checkstring(Ls, 1);
  lua_pushnumber(Ls, FB.textWidth(s));
  return 1;
}
static int l_font_height(lua_State* Ls) {
  lua_pushnumber(Ls, FB.fontHeight());
  return 1;
}

// Vẽ ảnh PNG/JPG/BMP từ LittleFS hoặc SD (tiền tố "sd:"). Trả về true/false như bản gốc.
static bool draw_image_file(const char* name, int dx, int dy) {
  String p = fs_path(name);
  String lp = p;
  bool on_sd = is_sd_path(name);
  auto try_draw = [&](fs::FS& fs, const String& path) -> bool {
    if (!fs.exists(path.c_str())) return false;
    String l = path; l.toLowerCase();
    if (l.endsWith(".png")) { FB.drawPngFile(fs, path.c_str(), dx, dy); return true; }
    if (l.endsWith(".jpg") || l.endsWith(".jpeg")) { FB.drawJpgFile(fs, path.c_str(), dx, dy); return true; }
    if (l.endsWith(".bmp")) { FB.drawBmpFile(fs, path.c_str(), dx, dy); return true; }
    return false;
  };
  if (on_sd && g_sd && try_draw(SD_MMC, p)) return true;
  if (try_draw(LittleFS, on_sd ? String("/") + (name + 3) : p)) return true;
  if (!on_sd && g_sd && try_draw(SD_MMC, p)) return true; // fallback chéo
  return false;
}

static int l_image(lua_State* Ls) {
  int x = (int)luaL_checknumber(Ls, 1), y = (int)luaL_checknumber(Ls, 2);
  const char* name = luaL_checkstring(Ls, 3);
  lua_pushboolean(Ls, draw_image_file(name, x, y));
  return 1;
}
static int l_image_region(lua_State* Ls) {
  const char* name = luaL_checkstring(Ls, 1);
  int sx = (int)luaL_checknumber(Ls, 2), sy = (int)luaL_checknumber(Ls, 3);
  int sw = (int)luaL_checknumber(Ls, 4), sh = (int)luaL_checknumber(Ls, 5);
  int dx = (int)luaL_checknumber(Ls, 6), dy = (int)luaL_checknumber(Ls, 7);
  // Port blit_region bằng clip + vẽ lệch (dx-sx, dy-sy), đúng ngữ nghĩa LuaS30.
  FB.setClipRect(dx, dy, sw, sh);
  bool ok = draw_image_file(name, dx - sx, dy - sy);
  FB.clearClipRect();
  lua_pushboolean(Ls, ok);
  return 1;
}
static int l_flush(lua_State*) { FB.pushSprite(0, 0); return 0; }
static int l_tick(lua_State* Ls) { lua_pushnumber(Ls, (lua_Number)millis()); return 1; }
static int l_exit(lua_State*) { call_hook("quit", false, 0); ESP.restart(); return 0; }
static int l_log(lua_State* Ls) { Serial.printf("[lua] %s\n", luaL_checkstring(Ls, 1)); return 0; }

static int l_file_exists(lua_State* Ls) {
  const char* n = luaL_checkstring(Ls, 1);
  String p = fs_path(n);
  bool ok = LittleFS.exists(p) || (g_sd && SD_MMC.exists(p));
  if (!ok && !is_sd_path(n)) { // thử chéo volume
    ok = (g_sd && SD_MMC.exists(p));
  }
  lua_pushboolean(Ls, ok);
  return 1;
}
static int l_file_write(lua_State* Ls) {
  const char* n = luaL_checkstring(Ls, 1);
  size_t len = 0; const char* d = luaL_checklstring(Ls, 2, &len);
  String p = fs_path(n);
  File f;
  if (is_sd_path(n)) { if (!g_sd) { lua_pushboolean(Ls, 0); return 1; } f = SD_MMC.open(p, "w"); }
  else f = LittleFS.open(p, "w");
  if (!f) { lua_pushboolean(Ls, 0); return 1; }
  size_t w = f.write((const uint8_t*)d, len);
  f.close();
  lua_pushboolean(Ls, w == len);
  return 1;
}
static int l_file_read(lua_State* Ls) {
  const char* n = luaL_checkstring(Ls, 1);
  String s;
  if (!load_text_file(n, s)) { lua_pushnil(Ls); return 1; }
  lua_pushlstring(Ls, s.c_str(), s.length());
  return 1;
}
static int l_file_delete(lua_State* Ls) {
  const char* n = luaL_checkstring(Ls, 1);
  String p = fs_path(n);
  bool ok = false;
  if (is_sd_path(n)) ok = g_sd && SD_MMC.remove(p);
  else ok = LittleFS.remove(p) || (g_sd && SD_MMC.remove(p));
  lua_pushboolean(Ls, ok);
  return 1;
}
// Audio: chưa có chân I2S/DAC trong BOM -> stub an toàn, has_audio=false (đúng rule degrade của LuaS30).
static int l_audio_play(lua_State* Ls) { (void)Ls; lua_pushboolean(Ls, 0); return 1; }
static int l_audio_stop(lua_State*) { return 0; }
static int l_audio_volume(lua_State*) { return 0; }
static int l_audio_playing(lua_State* Ls) { lua_pushboolean(Ls, 0); return 1; }

static int l_capabilities(lua_State* Ls) { lua_pushnumber(Ls, 0); return 1; }
static int l_device_info(lua_State* Ls) {
  lua_newtable(Ls);
  lua_pushstring(Ls, "esp32-s3-st7789"); lua_setfield(Ls, -2, "family");
  lua_pushnumber(Ls, TFT_WIDTH);  lua_setfield(Ls, -2, "width");
  lua_pushnumber(Ls, TFT_HEIGHT); lua_setfield(Ls, -2, "height");
  lua_pushnumber(Ls, g_fps);      lua_setfield(Ls, -2, "preferred_fps");
  lua_pushnumber(Ls, 8192);       lua_setfield(Ls, -2, "recommended_ram_kb");
  lua_pushnumber(Ls, 0);          lua_setfield(Ls, -2, "capabilities");
  lua_pushstring(Ls, "full");     lua_setfield(Ls, -2, "compatibility");
  return 1;
}
static int l_runtime_compat(lua_State* Ls) {
  lua_newtable(Ls);
  lua_pushboolean(Ls, 1); lua_setfield(Ls, -2, "compatible");
  lua_pushstring(Ls, "full"); lua_setfield(Ls, -2, "level");
  lua_pushnumber(Ls, 0); lua_setfield(Ls, -2, "capabilities");
  lua_pushnumber(Ls, 0); lua_setfield(Ls, -2, "fallback_mask");
  lua_pushnumber(Ls, 0); lua_setfield(Ls, -2, "alias_count");
  lua_pushnumber(Ls, 0); lua_setfield(Ls, -2, "missing_required");
  lua_newtable(Ls); lua_setfield(Ls, -2, "fallbacks");
  return 1;
}
static int l_sd_ok(lua_State* Ls) { lua_pushboolean(Ls, g_sd); return 1; }

// require("src.x") tìm trên LittleFS rồi SD: /src/x.lua, /src_x? giữ y hệt ngữ nghĩa resource-based.
static int l_require(lua_State* Ls) {
  const char* mod = luaL_checkstring(Ls, 1);
  char base[112]; size_t bi = 0;
  for (const char* p = mod; *p && bi < sizeof(base) - 5; p++, bi++) base[bi] = (*p == '.') ? '/' : *p;
  base[bi] = 0;
  lua_getglobal(Ls, "package_loaded");
  lua_getfield(Ls, -1, mod);
  if (!lua_isnil(Ls, -1)) { lua_remove(Ls, -2); return 1; }
  lua_pop(Ls, 2);

  char path[128]; snprintf(path, sizeof(path), "%s.lua", base);
  const char* cands[4] = { path, nullptr, nullptr, nullptr };
  char alt[128]; snprintf(alt, sizeof(alt), "/%s", path);
  String code;
  bool found = false;
  const char* tries[4] = { path, alt, nullptr, nullptr };
  // thử "src/" prefix nếu mod không có slash (tương thích templates/basic)
  char alt2[128]; snprintf(alt2, sizeof(alt2), "src/%s", path);
  const char* list[5] = { path, alt, alt2, nullptr, nullptr };
  (void)cands;
  (void)tries;
  for (int i = 0; list[i] && !found; i++) {
    if (load_text_file(list[i], code)) { found = true; snprintf(path, sizeof(path), "%s", list[i]); break; }
  }
  if (!found) return luaL_error(Ls, "module '%s' missing", mod);
  int st = luaL_loadbuffer(Ls, code.c_str(), code.length(), path);
  if (st != 0) return lua_error(Ls);
  st = lua_pcall(Ls, 0, 1, 0);
  if (st != 0) return lua_error(Ls);
  if (lua_isnil(Ls, -1)) { lua_pop(Ls, 1); lua_pushboolean(Ls, 1); }
  lua_getglobal(Ls, "package_loaded");
  lua_pushvalue(Ls, -2);
  lua_setfield(Ls, -2, mod);
  lua_pop(Ls, 1);
  return 1;
}
static int l_print(lua_State* Ls) {
  int n = lua_gettop(Ls);
  String line;
  lua_getglobal(Ls, "tostring");
  for (int i = 1; i <= n; i++) {
    lua_pushvalue(Ls, -1); lua_pushvalue(Ls, i); lua_call(Ls, 1, 1);
    const char* s = lua_tostring(Ls, -1);
    if (s) { if (i > 1) line += "\t"; line += s; }
    lua_pop(Ls, 1);
  }
  Serial.println(line);
  return 0;
}

static const luaL_Reg kFuncs[] = {
  {"color", l_color}, {"clear", l_clear}, {"rect", l_rect}, {"frame", l_frame},
  {"line", l_line}, {"text", l_text}, {"set_font", l_set_font},
  {"text_width", l_text_width}, {"font_height", l_font_height},
  {"image", l_image}, {"image_region", l_image_region},
  {"file_exists", l_file_exists}, {"file_write", l_file_write},
  {"file_read", l_file_read}, {"file_delete", l_file_delete},
  {"audio_play", l_audio_play}, {"audio_stop", l_audio_stop},
  {"audio_set_volume", l_audio_volume}, {"audio_is_playing", l_audio_playing},
  {"flush", l_flush}, {"tick_ms", l_tick}, {"exit", l_exit}, {"log", l_log},
  {"capabilities", l_capabilities}, {"device_info", l_device_info},
  {"runtime_compat", l_runtime_compat}, {"sd_ok", l_sd_ok},
  {0, 0}
};

void engine_init(LGFX_ESP32S3_ST7789* lcd) {
  G = lcd;
  static LGFX_Sprite fb(G);
  FSp = &fb;
  FB.setPsram(true);
  FB.setColorDepth(16);
  FB.createSprite(TFT_WIDTH, TFT_HEIGHT);
  FB.setTextWrap(false);
  FB.setTextSize(1);
}

static void open_engine_table() {
  lua_newtable(L);
  luaL_register(L, 0, kFuncs);
  lua_pushnumber(L, TFT_WIDTH);  lua_setfield(L, -2, "W");
  lua_pushnumber(L, TFT_HEIGHT); lua_setfield(L, -2, "H");
  lua_pushstring(L, "E524546-OS/1.0 (LuaS30-core on ESP32-S3)"); lua_setfield(L, -2, "version");
  lua_pushboolean(L, 0); lua_setfield(L, -2, "has_audio");   // stub, chưa có I2S
  lua_pushboolean(L, 1); lua_setfield(L, -2, "has_files");   // LittleFS + SD
  lua_pushboolean(L, 1); lua_setfield(L, -2, "has_images");  // PNG/JPG/BMP qua LovyanGFX
  lua_pushboolean(L, 0); lua_setfield(L, -2, "has_touch");
  lua_pushboolean(L, 0); lua_setfield(L, -2, "has_rename");
  lua_pushboolean(L, 1); lua_setfield(L, -2, "has_removable"); // SD
  lua_pushboolean(L, 1); lua_setfield(L, -2, "has_log");
  lua_pushboolean(L, 1); lua_setfield(L, -2, "runtime_compatible");
  lua_pushvalue(L, -1); lua_setglobal(L, "engine");
  lua_setglobal(L, "mre");
  lua_pushcfunction(L, l_require); lua_setglobal(L, "require");
  lua_pushcfunction(L, l_print);   lua_setglobal(L, "print");
}

static bool run_file(const char* vpath, const char* chunk, bool run) {
  String code;
  if (!load_text_file(vpath, code)) return false;
  int st = luaL_loadbuffer(L, code.c_str(), code.length(), chunk);
  if (st != 0) { report(st); return true; } // file có nhưng lỗi parse -> đã báo
  if (run && lua_pcall(L, 0, 0, 0) != 0) report(1);
  return true;
}

void engine_open_vm() {
  g_sd = SD_MMC.numSectors() > 0 || SD_MMC.cardType() != CARD_NONE;
  L = lua_open();
  luaopen_base(L);   lua_settop(L, 0);
  luaopen_table(L);  lua_settop(L, 0);
  luaopen_string(L); lua_settop(L, 0);
  luaopen_math(L);   lua_settop(L, 0);
  lua_newtable(L); lua_setglobal(L, "package_loaded");
  open_engine_table();

  // conf.lua (optional): {fps=30}
  if (run_file("conf.lua", "conf", true) || run_file("/conf.lua", "conf", true)) {
    lua_getglobal(L, "config");
    if (lua_istable(L, -1)) {
      lua_getfield(L, -1, "fps");
      if (lua_isnumber(L, -1)) {
        int f = (int)lua_tonumber(L, -1);
        if (f >= 8 && f <= 60) { g_fps = f; g_frame_ms = 1000 / f; }
      }
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  // main.lua bắt buộc (LittleFS trước, SD sau)
  bool ok = run_file("main.lua", "main", true) || run_file("sd:main.lua", "main", true);
  if (!ok) { engine_error_screen("main.lua missing"); g_panic = true; return; }
  if (g_panic) return;
  call_hook("load", false, 0);
  if (!g_panic) {
    call_hook("draw", false, 0);
    FB.pushSprite(0, 0);
    g_started = true;
    g_last = millis();
  }
}

void engine_close_vm() {
  if (L) { if (!g_panic) call_hook("quit", false, 0); lua_close(L); L = nullptr; }
  g_started = false;
}

lua_State* engine_vm() { return L; }
int engine_fps() { return g_fps; }
bool engine_sd_ok() { return g_sd; }

void engine_draw_frame(unsigned long now) {
  if (!g_started || g_paused || g_panic || !L) return;
  if (now - g_last < (unsigned long)g_frame_ms) return;
  double dt = (now - g_last) / 1000.0;
  if (dt > 0.25) dt = 0.25;
  g_last = now;
  call_hook("update", true, dt);
  if (g_panic) return;
  call_hook("draw", false, 0);
  if (g_panic) return;
  FB.pushSprite(0, 0);
}

void engine_key_event(const char* key, bool pressed) {
  if (!L || g_panic || !key) return;
  int top = lua_gettop(L);
  lua_getglobal(L, "engine");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_getglobal(L, "mre"); }
  if (!lua_istable(L, -1)) { lua_settop(L, top); return; }
  lua_getfield(L, -1, pressed ? "keypressed" : "keyreleased");
  if (lua_isfunction(L, -1)) {
    lua_pushstring(L, key);
    if (lua_pcall(L, 1, 0, 0) != 0) report(1);
  }
  lua_settop(L, top);
}

void engine_pause() {
  if (g_paused || !g_started) return;
  g_paused = true;
  call_hook("pause", false, 0);
}
void engine_resume() {
  if (!g_paused) return;
  g_paused = false;
  g_last = millis();
  call_hook("resume", false, 0);
}
