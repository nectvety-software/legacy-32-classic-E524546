// headless_main.cpp — portable Linux/CI harness for the exact firmware sources.
// Generates 240x320 BMP screenshots from the LGFX framebuffer while driving the keypad.
#include "sim_arduino.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);

static const char *OUT = "sim_out_keypad";

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
  SIM_KEYS[slot] = 1; pump(70); SIM_KEYS[slot] = 0; pump(180);
}
extern "C" void sim_key_game(const char *name) {
  int s = slot_of(name); if (s >= 0) sim_press(s);
}

static void shot(const char *name) {
  char p[256]; snprintf(p, sizeof p, "%s/%s.bmp", OUT, name);
  if (LGFX::inst) LGFX::inst->dumpBmp(p);
  printf("[shot] %s\n", p);
}

int main() {
#ifdef _WIN32
  ::mkdir(OUT); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir(OUT, 0755); ::mkdir("sim_lfs", 0755); ::mkdir("sim_lfs/Qeafbrowser", 0755);
#endif
  // Persist Wi-Fi so setup() boots in connected state exactly like a configured device.
  FILE *f = fopen("sim_lfs/Qeafbrowser/config.ini", "wb");
  if (f) {
    fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=https://qeafivels.com/\n", f);
    fclose(f);
  }

  sim_http_mock_set(true);
  setup();
  pump(150);

  // Real browser flow through http_get() (mock network transport only): HTML -> parser -> renderer.
  sim_go_url("https://keypad.test/");
  pump(250);
  shot("01_focus_heading");

  sim_key_game("down");
  shot("02_focus_paragraph");

  sim_key_game("down");
  shot("03_focus_long_link");

  // LEFT behaves as previous focus block in a single-column mobile layout.
  sim_key_game("left");
  shot("04_left_previous_block");
  sim_key_game("right");
  shot("05_right_long_link");

  // Center OK activates the focused link and resolves /article.html against HTTPS base URL.
  sim_key_game("ok");
  pump(250);
  shot("06_ok_open_article");

  // Back returns to previous page, then navigate down to force viewport auto-scroll.
  sim_key_game("back");
  pump(200);
  for (int i = 0; i < 6; i++) sim_key_game("down");
  shot("07_autoscroll_lower_block");

  printf("[done] keypad browser navigation test complete\n");
  return 0;
}
