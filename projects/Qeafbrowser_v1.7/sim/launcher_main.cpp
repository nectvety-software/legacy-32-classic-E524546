// launcher_main.cpp — harness Windows cho launcher (chay dung src/launcher.cpp).
// Xuat framebuffer 240x320 ra BMP de kiem tra bo cuc + theme JSON.
// Build: xem sim/README.md (giong s60_main.cpp, them src/launcher.cpp).
#include "sim_arduino.h"
#include "launcher_config.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

extern void setup();
extern void loop();
extern "C" void sim_press(int slot);
extern "C" void sim_key_game(const char *name);

static const char *OUTDIR = "sim/launcher_out";
static void pump(unsigned ms) {
  unsigned long t0 = millis();
  do { loop(); Sleep(2); } while (millis() - t0 < ms);
}
static int slot_of(const char *name) {
  static const char *N[] = { "menu","up","back","left","ok","right","option","down","delete","mode" };
  for (int i = 0; i < 10; i++) if (!strcmp(name, N[i])) return i;
  return -1;
}
static void tap(const char *name, unsigned post = 170) {
  int s = slot_of(name); if (s < 0) return;
  SIM_KEYS[s] = 1; pump(55); SIM_KEYS[s] = 0; pump(post);
}
static void shot(const char *name) {
  char p[256];
  snprintf(p, sizeof p, "%s/%s.bmp", OUTDIR, name);
  if (LGFX::inst) LGFX::inst->dumpBmp(p);
  printf("[shot] %s\n", p);
}
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
extern "C" void sim_key_launch(const char *k);   // forward (main.cpp cung cap)

int main() {
#ifdef _WIN32
  ::mkdir("sim"); ::mkdir(OUTDIR); ::mkdir("sim_sd"); ::mkdir("sim_sd/launcher");
  ::mkdir("sim_sd/launcher/themes"); ::mkdir("sim_sd/launcher/themes/retrogo_dark");
  ::mkdir("sim_lfs"); ::mkdir("sim_lfs/launcher"); ::mkdir("sim_lfs/launcher/themes");
#else
  ::mkdir("sim", 0755); ::mkdir(OUTDIR, 0755);
#endif
  FILE *f = fopen("sim_lfs/Qeafbrowser/config.ini", "wb");
  if (f) { fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=mtt:start\n", f); fclose(f); }
  // theme .vqeaf mau cho sim (store_theme_read -> LittleFS root sim_lfs/)
  static const char *VQEAF =
      "@vqeaf 1.0\n"
      "<theme id=\"retrogo_dark\" name=\"Retro-Go Dark\">\n"
      "  palette {\n"
      "    screen: \"#001028\"\n"
      "    key: \"#182838\"\n"
      "    keyPressed: \"#C8A000\"\n"
      "    keyBorder: \"#6B4D00\"\n"
      "    keyText: \"#FFFFFF\"\n"
      "    subText: \"#C0C0C0\"\n"
      "    shellTop: \"#001028\"\n"
      "    shellBottom: \"#001028\"\n"
      "    accent: \"#FFD700\"\n"
      "    glow: \"#FF4040\"\n"
      "  }\n"
      "</theme>\n";
  f = fopen("sim_lfs/launcher/themes/retrogo_dark.vqeaf", "wb");
  if (f) { fputs(VQEAF, f); fclose(f); }
  // secondary themes for theme_next
  f = fopen("sim_lfs/launcher/themes/s60_green.vqeaf", "wb");
  if (f) {
    fputs("@vqeaf 1.0\n<theme id=\"s60_green\" name=\"S60 Green\">\n"
          "  palette {\n"
          "    screen: \"#8CC822\"\n"
          "    panel: \"#AFE84A\"\n"
          "    selected: \"#DFF29B\"\n"
          "    titlebar: \"#286C18\"\n"
          "    keyText: \"#000000\"\n"
          "    chromeText: \"#FFFFFF\"\n"
          "    accent: \"#FFFF00\"\n"
          "    border: \"#FFFFFF\"\n"
          "  }\n"
          "</theme>\n", f);
    fclose(f);
  }
  f = fopen("sim_lfs/launcher/themes/amoled_red.vqeaf", "wb");
  if (f) {
    fputs("@vqeaf 1.0\n<theme id=\"amoled_red\" name=\"AMOLED Red\">\n"
          "  palette {\n"
          "    shellTop: \"#141414\"\n"
          "    shellBottom: \"#050505\"\n"
          "    shellBorder: \"#54232B\"\n"
          "    screen: \"#000000\"\n"
          "    key: \"#171717\"\n"
          "    keyPressed: \"#38161B\"\n"
          "    keyBorder: \"#8A2E3B\"\n"
          "    keyText: \"#FFFFFF\"\n"
          "    subText: \"#B78E94\"\n"
          "    accent: \"#FF3D5B\"\n"
          "    glow: \"#FF2D4D\"\n"
          "  }\n"
          "</theme>\n", f);
    fclose(f);
  }
  sim_http_mock_set(true);
  setup();               // setup da khoi dong launcher + ve splash
  pump(300);
  shot("01_boot_launcher");
  // Qeafbrowser / S60 chrome: title gradient S60_PANE + content trang
  check(fb_count(S60_PANE_TOP) + fb_count(S60_PANE_BOT) > 100,
        "launcher: title bar gradient S60_PANE (Qeafbrowser)");
  check(fb_count(0xFFFF) > 8000, "launcher: content Top Sites nen trang");

  tap("down");  shot("02_list_move_down");
  // Focus block: gradient S60_SEL_TOP/BOT + khung S60_SEL_LINE phai co mat
  // (gradient interpolation -> dem ca TOP + BOT; vien 2px = nhieu S60_SEL_LINE)
  check(fb_count(S60_SEL_TOP) + fb_count(S60_SEL_BOT) > 150,
        "launcher: focus block selection S60 gradient");
  check(fb_count(S60_SEL_LINE) > 80, "launcher: focus frame S60_SEL_LINE");
  tap("down");  shot("03_list_move_down2");
  tap("right"); shot("04_tab_games");
  tap("right"); shot("05_tab_favorites");
  tap("right"); shot("06_tab_settings");
  // Settings van dung chrome S60 (content trang), khong con nen vqeaf cu
  check(fb_count(0xFFFF) > 8000, "settings tab: content van nen trang S60");
  tap("left");  shot("07_back_to_favorites");
  tap("left");  tap("left");
  tap("ok");    shot("08_open_browser_item");
  pump(300);
  shot("09_after_open");
  tap("option"); tap("down"); tap("down"); // menu Exit cua browser
  tap("ok"); pump(200); shot("10_back_to_launcher");
  printf("[done] launcher regression: %d FAIL\n", g_fail);
  return g_fail;
}
