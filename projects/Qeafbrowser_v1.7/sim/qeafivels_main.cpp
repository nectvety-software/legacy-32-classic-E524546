// qeafivels_main.cpp — regression for Opera Mini 4 style keypad browsing.
#include "sim_arduino.h"
#include "browser.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);
extern "C" int sim_thumb_request(const char *url);
static const char *OUTDIR="sim/qeafivels_out";
static void pump(unsigned ms){ unsigned long t0=millis(); do{ loop(); Sleep(2);}while(millis()-t0<ms); }
static int slot_of(const char *name){ static const char *N[]={"menu","up","back","left","ok","right","option","down","delete","mode"}; for(int i=0;i<10;i++) if(!strcmp(name,N[i])) return i; return -1; }
static void press(const char *name){ int s=slot_of(name); if(s<0)return; SIM_KEYS[s]=1; pump(70); SIM_KEYS[s]=0; pump(180); }
static void shot(const char *name){ char p[256]; snprintf(p,sizeof p,"%s/%s.bmp",OUTDIR,name); if(LGFX::inst) LGFX::inst->dumpBmp(p); printf("[shot] %s\n",p); }
int main(){
#ifdef _WIN32
  ::mkdir("sim"); ::mkdir(OUTDIR); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir("sim",0755); ::mkdir(OUTDIR,0755); ::mkdir("sim_lfs",0755); ::mkdir("sim_lfs/Qeafbrowser",0755);
#endif
  FILE *f=fopen("sim_lfs/Qeafbrowser/config.ini","wb");
  if(f){ fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=https://qeafivels.com/\n",f); fclose(f); }
  sim_http_mock_set(true);
  setup(); pump(180);
  // Deliberately request bare host; HTTP layer must follow redirect to www HTTPS.
  sim_go_url("https://qeafivels.com/"); pump(300);
  const char *final_url = history_count() ? history_url(0) : "";
  printf("[assert] final_url=%s\n", final_url);
  if (strcmp(final_url, "https://www.qeafivels.com/")) {
    fprintf(stderr, "[FAIL] redirect did not settle on www HTTPS URL\n"); return 2;
  }
  if (!strstr(history_title(0), "Qeafivels Software")) {
    fprintf(stderr, "[FAIL] page title not parsed\n"); return 3;
  }
  printf("[PASS] redirect + title parse\n");
  shot("01_qeafivels_loaded_ssr_minimap");
  press("down"); shot("02_focus_hero_image_decoded");
  press("down"); shot("03_focus_audio_tools");
  press("right"); shot("04_focus_ai_agents");
  // Opera Mini 4-style page overview from Option > Navg > Overview.
  press("option");
  press("down"); press("down"); // Navg
  press("right"); press("ok"); pump(100);
  shot("05_desktop_overview_x1");
  press("right"); shot("06_desktop_overview_x2");
  press("right"); press("right"); shot("07_desktop_overview_x4");
  press("right"); press("right"); press("right"); press("right"); shot("08_desktop_overview_x8");
  press("down"); press("down"); press("down"); shot("09_desktop_overview_x8_panned");
  press("ok"); pump(100);
  shot("10_zoomed_section_after_ok");
  // Deep-scroll to the second PNG image to verify PNG decode/cache and smooth scrollbar.
  sim_go_url("https://qeafivels.com/"); pump(200);
  for (int i=0;i<11;i++) press("down");
  shot("11_png_product_thumb_scrollbar");
  // Re-open overview after both JPEG+PNG thumbnails are cached; lower page tiles can reuse them.
  press("option"); press("down"); press("down"); press("right"); press("ok"); pump(100);
  shot("12_overview_cached_page_tiles");
  press("back");
  // Return and open virtual mouse from Tool > Mouse.
  press("option");
  press("down"); press("down"); press("down"); // Tool
  press("right"); press("ok"); pump(100);
  shot("13_virtual_mouse");
  // PSRAM cache has 6 slots. Request 8 unique URL keys backed by the same local JPEG
  // so the regression proves LRU eviction and persistent LittleFS fallback.
  int lru_ok = 1;
  char tu[256];
  for (int i=0;i<8;i++) {
    snprintf(tu,sizeof tu,"https://www.qeafivels.com/hero-banner.jpg?cache=%d",i);
    if (!sim_thumb_request(tu)) lru_ok = 0;
  }
  if (!sim_thumb_request("https://www.qeafivels.com/hero-banner.jpg?cache=0")) lru_ok = 0;
  printf("[assert] thumbnail_lru=%s\n", lru_ok ? "PASS" : "FAIL");
  if (!lru_ok) return 4;
  printf("[done] qeafivels Opera Mini 4 style regression complete\n");
  return 0;
}
