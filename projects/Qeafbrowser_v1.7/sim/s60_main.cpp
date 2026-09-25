// s60_main.cpp — regression harness cho theme Symbian S60 (ui_s60.h).
// Dung LAI dung src/*.cpp cua firmware: setup()/loop()/keypad/WML/HTTP deu la
// code that, chi thay transport bang mock HTTP. Xuat framebuffer 240x320 ra BMP.
//
// Chay:
//   powershell -Command "$env:PATH='C:\msys64\mingw64\bin;C:\msys64\usr\bin;'+$env:PATH"
//   g++ -O2 -std=gnu++17 -static -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -I shims -I ../include ^
//       -o qb_s60.exe s60_main.cpp sim_arduino.cpp ../src/main.cpp ../src/wml.cpp ^
//       ../src/http.cpp ../src/store.cpp -lws2_32
//   .\qb_s60.exe
#include "sim_arduino.h"
#include "ui_s60.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);
extern "C" void sim_draw_splash();
extern "C" void sim_doc_dump();

// Luu y: KHONG duoc dat ten bien la OUT — <windef.h> dinh nghia macro OUT rong.
static const char *OUTDIR = "s60_out";

static void pump(unsigned ms) {
  unsigned long t0 = millis();
  do { loop(); Sleep(2); } while (millis() - t0 < ms);
}

static int slot_of(const char *name) {
  static const char *N[] = { "menu","up","back","left","ok","right","option","down","delete","mode" };
  for (int i = 0; i < 10; i++) if (!strcmp(name, N[i])) return i;
  return -1;
}
extern "C" void sim_press(int slot) {
  if (slot < 0 || slot > 9) return;
  SIM_KEYS[slot] = 1; pump(70); SIM_KEYS[slot] = 0; pump(220);
}
extern "C" void sim_key_game(const char *name) {
  int s = slot_of(name); if (s >= 0) sim_press(s);
}

// Pixel scroll eases toward the new target over many 17 ms ticks; large jumps
// need more than one keypress worth of pumping, otherwise the shot catches the
// page mid-scroll (and the focused row may still be below the viewport).
static void settle() { pump(1200); }

static void shot(const char *name) {
  settle();
  char p[256];
  snprintf(p, sizeof p, "%s/%s.bmp", OUTDIR, name);
  if (LGFX::inst) LGFX::inst->dumpBmp(p);
  printf("[shot] %s\n", p);
}

// Kiem tra mau chu dao cua theme co thuc su xuat hien tren khung hinh khong.
static int fb_count(uint16_t c) {
  if (!LGFX::inst) return 0;
  int n = 0;
  for (int i = 0; i < 240 * 320; i++) if (LGFX::inst->fb[i] == c) n++;
  return n;
}
static int g_fail = 0;
static void check(bool cond, const char *what) {
  printf("[%s] %s\n", cond ? "PASS" : "FAIL", what);
  if (!cond) g_fail++;
}

int main() {
#ifdef _WIN32
  ::mkdir(OUTDIR); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir(OUTDIR, 0755); ::mkdir("sim_lfs", 0755); ::mkdir("sim_lfs/Qeafbrowser", 0755);
#endif
  FILE *f = fopen("sim_lfs/Qeafbrowser/config.ini", "wb");
  if (f) {
    fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=mtt:start\n", f);
    fclose(f);
  }

  sim_http_mock_set(true);
  setup();
  pump(150);

  // 1) Man khoi dong S60 (setup() da chay qua splash qua nhanh de chup).
  sim_draw_splash();
  pump(40);
  shot("01_splash_s60");
  check(fb_count(S60_PANE_MID) > 400, "splash: application pane + logo tile ve bang S60_PANE_MID");

  // 2) Speed Dial home: danh sach S60 co o icon + nhan DAM.
  // Boot launcher dang mo Apps/Browser (vi tri 0): OK de exit launcher -> browser.
  sim_key_game("ok");
  pump(300);                  // go_url(mtt:start) + render ben browser

  sim_go_url("mtt:start");
  pump(260);
  sim_doc_dump();
  shot("02_speed_dial_list");
  check(fb_count(S60_SEL_TOP) + fb_count(S60_SEL_BOT) > 80, "list: thanh chon xanh S60 hien tren dong focus");

  // 3) Di chuyen focus xuong cac muc S60: dong chi muc (Bookmarks|History|...)
  // roi toi cac hang danh sach co o icon (Qeafivels, Google, ...).
  sim_key_game("down");
  sim_key_game("down");
  shot("03_speed_dial_link_row");
  for (int i = 0; i < 5; i++) sim_key_game("down");
  shot("04_speed_dial_folder_row");
  check(fb_count(S60_SEL_TOP) + fb_count(S60_SEL_BOT) > 200, "list: hang folder duoc chon to thanh xanh S60");

  // 5) Trang web that qua parser: tieu de DAM + than bai DAM.
  sim_go_url("https://keypad.test/");
  pump(300);
  shot("05_page_s60");
  check(fb_count(S60_LINK) > 40, "page: link/heading dung mau S60_LINK");

  // 6) Menu Options S60 (panel tron + thanh chon xanh + mui ten menu con).
  sim_key_game("option");
  shot("06_options_menu");
  check(fb_count(S60_ACCENT) + fb_count(S60_SEL_TOP) > 40, "menu: panel + thanh chon xanh");

  // 7) Menu con: OK vao muc con.
  sim_key_game("ok");
  shot("07_options_submenu");
  sim_key_game("back");
  pump(120);

  // 8) Ban phim ao S60.
  sim_go_url("mtt:start");
  pump(200);
  sim_key_game("ok");      // "Enter URL" -> hop nhap + ban phim ao
  shot("08_virtual_keypad_s60");

  // 9) O nhap lieu / field S60 trong trang web.
  sim_key_game("back");
  pump(200);
  sim_go_url("https://www.google.com/");
  pump(300);
  shot("09_field_and_page");

  printf("[done] Symbian S60 theme regression: %d FAIL\n", g_fail);
  return g_fail;
}
