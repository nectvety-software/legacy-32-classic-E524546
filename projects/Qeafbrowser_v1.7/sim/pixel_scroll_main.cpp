#include "sim_arduino.h"
#include "browser.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);
extern "C" int sim_body_scroll_px();
extern "C" int sim_body_scroll_target_px();
extern "C" int sim_body_scroll_active();
static const char *OUT="sim/pixel_scroll_out";
static void pump(unsigned ms){ unsigned long t0=millis(); do{ loop(); Sleep(2);}while(millis()-t0<ms); }
static int slot_of(const char *name){ static const char *N[]={"menu","up","back","left","ok","right","option","down","delete","mode"}; for(int i=0;i<10;i++) if(!strcmp(name,N[i])) return i; return -1; }
static void tap(const char *name,unsigned post=170){ int s=slot_of(name); if(s<0)return; SIM_KEYS[s]=1; pump(55); SIM_KEYS[s]=0; pump(post); }
static void trigger(const char *name){ int s=slot_of(name); if(s<0)return; SIM_KEYS[s]=1; pump(30); SIM_KEYS[s]=0; pump(2); }
static void shot(const char *name){ char p[256]; snprintf(p,sizeof p,"%s/%s.bmp",OUT,name); if(LGFX::inst) LGFX::inst->dumpBmp(p); printf("[shot] %s scroll=%d target=%d active=%d\n",p,sim_body_scroll_px(),sim_body_scroll_target_px(),sim_body_scroll_active()); }
int main(){
#ifdef _WIN32
  ::mkdir("sim"); ::mkdir(OUT); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir("sim",0755); ::mkdir(OUT,0755); ::mkdir("sim_lfs",0755); ::mkdir("sim_lfs/Qeafbrowser",0755);
#endif
  FILE *f=fopen("sim_lfs/Qeafbrowser/config.ini","wb");
  if(f){fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=https://qeafivels.com/\n",f);fclose(f);}
  sim_http_mock_set(true);
  setup(); pump(220);

  // 1) Normal browsing: move to a lower content block until autoscroll is required.
  sim_go_url("https://keypad.test/"); pump(180);
  for(int i=0;i<5;i++) tap("down",90);
  shot("01_before_pixel_scroll");
  trigger("down");
  int start=sim_body_scroll_px(), target=sim_body_scroll_target_px();
  printf("[assert] normal start=%d target=%d\n",start,target);
  if(target <= start){ fprintf(stderr,"[FAIL] D-pad did not request downward pixel scroll\n"); return 2; }
  pump(18); int early=sim_body_scroll_px(); shot("02_pixel_scroll_early");
  pump(38); int mid=sim_body_scroll_px(); shot("03_pixel_scroll_mid");
  pump(420); int settled=sim_body_scroll_px(); shot("04_pixel_scroll_settled");
  if(!(start <= early && early <= mid && mid <= settled && settled == sim_body_scroll_target_px())){
    fprintf(stderr,"[FAIL] pixel scroll easing progression invalid: %d %d %d %d target=%d\n",start,early,mid,settled,sim_body_scroll_target_px()); return 3;
  }
  printf("[PASS] D-pad pixel easing progression\n");

  // 2) Overview -> main page handoff: pan overview, OK, then body scroll eases to selected page position.
  sim_go_url("https://qeafivels.com/"); pump(220);
  tap("option"); tap("down"); tap("down"); tap("right"); tap("ok",100); // Overview
  for(int i=0;i<5;i++) tap("right",80); // zoom
  for(int i=0;i<4;i++) tap("down",80);  // pan
  shot("05_overview_before_ok");
  trigger("ok");
  int hand_start=sim_body_scroll_px(), hand_target=sim_body_scroll_target_px();
  shot("06_overview_handoff_start");
  pump(18); int hand_early=sim_body_scroll_px(); shot("07_overview_handoff_early");
  pump(40); int hand_mid=sim_body_scroll_px(); shot("08_overview_handoff_mid");
  pump(420); int hand_end=sim_body_scroll_px(); shot("09_overview_handoff_settled");
  printf("[assert] handoff %d -> %d -> %d -> %d target=%d\n",hand_start,hand_early,hand_mid,hand_end,hand_target);
  if(hand_target != hand_start && hand_end != sim_body_scroll_target_px()){
    fprintf(stderr,"[FAIL] overview handoff did not settle\n"); return 4;
  }
  printf("[PASS] overview -> body pixel-scroll handoff\n");
  printf("[done] pixel scroll regression complete\n");
  return 0;
}
