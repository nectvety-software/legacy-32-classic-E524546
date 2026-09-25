#include "sim_arduino.h"
#include "browser.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);
static const char *OUTDIR="sim/overview_anim_out";
static void pump(unsigned ms){ unsigned long t0=millis(); do{ loop(); Sleep(2);}while(millis()-t0<ms); }
static int slot_of(const char *name){ static const char *N[]={"menu","up","back","left","ok","right","option","down","delete","mode"}; for(int i=0;i<10;i++) if(!strcmp(name,N[i])) return i; return -1; }
static void tap(const char *name,unsigned post=180){ int s=slot_of(name); if(s<0)return; SIM_KEYS[s]=1; pump(55); SIM_KEYS[s]=0; pump(post); }
static void trigger(const char *name){ int s=slot_of(name); if(s<0)return; SIM_KEYS[s]=1; pump(55); SIM_KEYS[s]=0; pump(4); }
static void shot(const char *name){ char p[256]; snprintf(p,sizeof p,"%s/%s.bmp",OUTDIR,name); if(LGFX::inst) LGFX::inst->dumpBmp(p); printf("[shot] %s\n",p); }
int main(){
#ifdef _WIN32
  ::mkdir("sim"); ::mkdir(OUTDIR); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir("sim",0755); ::mkdir(OUTDIR,0755); ::mkdir("sim_lfs",0755); ::mkdir("sim_lfs/Qeafbrowser",0755);
#endif
  FILE *f=fopen("sim_lfs/Qeafbrowser/config.ini","wb");
  if(f){fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=https://qeafivels.com/\n",f);fclose(f);}
  sim_http_mock_set(true);
  setup(); pump(220); sim_go_url("https://qeafivels.com/"); pump(260);
  // Enter Overview.
  tap("option"); tap("down"); tap("down"); tap("right"); tap("ok",120);
  shot("01_overview_x1_idle");
  // Smooth x1 -> x2: capture three moments.
  trigger("right"); pump(18); shot("02_zoom_x2_early");
  pump(40); shot("03_zoom_x2_mid");
  pump(130); shot("04_zoom_x2_settled");
  // Go to x8, then pan enough to cross into next preview-page tile.
  for(int i=0;i<6;i++) tap("right",110);
  shot("05_zoom_x8_idle");
  trigger("down"); pump(18); shot("06_pan_early");
  pump(40); shot("07_pan_mid");
  pump(130); shot("08_pan_settled");
  // More pans: approach the next preview tile, then capture the cursor glide.
  for(int i=0;i<6;i++) tap("down",100);
  shot("09_before_tile_transition");
  trigger("down"); pump(18); shot("10_tile_transition_early");
  pump(45); shot("11_tile_transition_mid");
  pump(420); shot("12_tile_transition_settled");
  printf("[done] smooth overview animation regression complete\n");
  return 0;
}
