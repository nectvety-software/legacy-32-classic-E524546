// headless_main.cpp — portable Linux/CI harness for the exact firmware sources.
// Generates 240x320 BMP screenshots from the LGFX framebuffer while driving the keypad.
#include "sim_arduino.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);
extern "C" int sim_doc_nlinks();
extern "C" const char *sim_doc_link(int i);
extern "C" int sim_doc_nimages();
extern "C" const char *sim_doc_image_url(int i);
extern "C" const char *sim_doc_title();
extern "C" int sim_doc_nlines();
extern "C" const char *sim_doc_line(int i);
extern "C" int sim_body_scroll_px();
extern "C" int sim_body_scroll_max_px();
extern "C" int sim_mouse_on();

static const char *OUTDIR = "sim_out_keypad";

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
  char p[256]; snprintf(p, sizeof p, "%s/%s.bmp", OUTDIR, name);
  if (LGFX::inst) LGFX::inst->dumpBmp(p);
  printf("[shot] %s\n", p);
}

int main() {
#ifdef _WIN32
  ::mkdir(OUTDIR); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir(OUTDIR, 0755); ::mkdir("sim_lfs", 0755); ::mkdir("sim_lfs/Qeafbrowser", 0755);
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

  // Boot dang mo launcher Retro-Go: OK tren Browser de vao browser (khong thi phim di vao launcher).
  sim_key_game("ok");
  pump(300);

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

  // ---- RSS feed parse (text-only news path) ----
  {
    int fails = 0;
    sim_go_url("https://feed.test/rss.xml");
    pump(250);
    shot("08_rss_feed");
    int nl = sim_doc_nlinks();
    printf("[rss] title=%s links=%d\n", sim_doc_title(), nl);
    if (nl < 3) { fprintf(stderr, "[FAIL] RSS expected >=3 links, got %d\n", nl); fails++; }
    else {
      const char *u0 = sim_doc_link(0);
      printf("[rss] link0=%s\n", u0);
      if (strncmp(u0, "http://feed.test/a1", 19) != 0) {
        fprintf(stderr, "[FAIL] RSS link0 mismatch: %s\n", u0); fails++;
      }
      // CDATA title must be captured (not empty page of raw URLs)
      if (strcmp(sim_doc_title(), "Qeaf Test Feed") != 0) {
        fprintf(stderr, "[FAIL] RSS channel title: %s\n", sim_doc_title()); fails++;
      }
    }
    sim_go_url("https://feed.test/atom.xml");
    pump(250);
    shot("09_atom_feed");
    int na = sim_doc_nlinks();
    printf("[atom] title=%s links=%d\n", sim_doc_title(), na);
    if (na < 2) { fprintf(stderr, "[FAIL] Atom expected >=2 links, got %d\n", na); fails++; }
    printf("[done] feed tests: %d FAIL\n", fails);
    if (fails) return 1;
  }

  // ---- Vietnamese numeric entities -> UTF-8 + lc_font render ----
  {
    int fails = 0;
    sim_go_url("https://vn.test/vn.html");
    pump(250);
    shot("10_vietnamese_page");
    const char *t = sim_doc_title();
    int nl = sim_doc_nlinks();
    int nn = sim_doc_nlines();
    printf("[vn] title=%s links=%d lines=%d\n", t, nl, nn);
    // title "Thế giới tiếng Việt" — ế = U+1EBF = E1 BA BF
    if (!strstr(t, "\xE1\xBA\xBF")) {
      fprintf(stderr, "[FAIL] VN title missing UTF-8 ế: %s\n", t); fails++;
    }
    // also must contain ệ U+1EC7 = E1 BB 87 (Việt)
    if (!strstr(t, "\xE1\xBB\x87")) {
      fprintf(stderr, "[FAIL] VN title missing UTF-8 ệ: %s\n", t); fails++;
    }
    if (nl < 3) { fprintf(stderr, "[FAIL] VN expected >=3 links, got %d\n", nl); fails++; }
    // body: "tiếng Việt" / "Danh mục" must contain multi-byte UTF-8
    bool found_vn = false;
    for (int i = 0; i < nn; i++) {
      const char *ln = sim_doc_line(i);
      // Việt: V i ệ t  — ệ U+1EC7 = E1 BB 87
      // tiếng: t i ế n g — ế U+1EBF = E1 BA BF
      // mục: m ụ c — ụ U+1EE5 = E1 B9 A5
      if (strstr(ln, "Vi\xE1\xBB\x87t") || strstr(ln, "ti\xE1\xBA\xBFng") ||
          strstr(ln, "m\xE1\xB9\xA5" "c")) { found_vn = true; break; }
    }
    if (!found_vn) { fprintf(stderr, "[FAIL] VN body text missing UTF-8\n"); fails++; }
    // dump a couple of lines for eyeballing
    for (int i = 0; i < nn && i < 8; i++)
      printf("[vn] line%d=%s\n", i, sim_doc_line(i));
    printf("[done] vn tests: %d FAIL\n", fails);
    if (fails) return 1;
  }

  {
    int fails = 0;
    sim_go_url("https://qeafivels.com/");
    pump(250);
    int enabled = sim_mouse_on();
    int before = sim_body_scroll_px();
    int max_px = sim_body_scroll_max_px();
    sim_key_game("down");
    sim_key_game("down");
    sim_key_game("down");
    int down = sim_body_scroll_px();
    sim_key_game("up");
    int up = sim_body_scroll_px();
    printf("[mouse] on=%d max=%d scroll=%d->%d->%d\n",
           enabled, max_px, before, down, up);
    if (!enabled) {
      fprintf(stderr, "[FAIL] desktop page did not enable virtual mouse\n");
      fails++;
    }
    if (max_px <= 0) {
      fprintf(stderr, "[FAIL] mouse fixture is not scrollable\n");
      fails++;
    }
    if (down <= before || up >= down) {
      fprintf(stderr, "[FAIL] virtual mouse drag did not scroll page\n");
      fails++;
    }
    shot("11_virtual_mouse_scroll");
    printf("[done] mouse tests: %d FAIL\n", fails);
    if (fails) return 1;
  }

  {
    int fails = 0;
    sim_go_url("https://image.test/index.html");
    pump(500);
    int ni = sim_doc_nimages();
    const char *u0 = sim_doc_image_url(0);
    const char *u1 = sim_doc_image_url(1);
    printf("[image] n=%d u0=%s u1=%s\n", ni, u0, u1);
    if (ni != 2) {
      fprintf(stderr, "[FAIL] image source selection expected 2 supported images, got %d\n", ni);
      fails++;
    }
    if (strcmp(u0, "/real.jpg?x=1&y=2") != 0) {
      fprintf(stderr, "[FAIL] data-src selection mismatch: %s\n", u0);
      fails++;
    }
    if (strcmp(u1, "/small.jpg") != 0) {
      fprintf(stderr, "[FAIL] srcset selection mismatch: %s\n", u1);
      fails++;
    }
    shot("12_image_sources");
    printf("[done] image source tests: %d FAIL\n", fails);
    if (fails) return 1;
  }
  return 0;
}
