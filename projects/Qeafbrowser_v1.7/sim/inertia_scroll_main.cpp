#include "sim_arduino.h"
#include "browser.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
extern void setup();
extern void loop();
extern "C" void sim_go_url(const char *url);
extern "C" void sim_key_launch_go_home(void);
extern "C" int sim_body_scroll_px();
extern "C" int sim_body_scroll_target_px();
extern "C" int sim_body_scroll_active();
extern "C" int sim_body_scroll_velocity_fp();
extern "C" int sim_body_scroll_inertia();

static const char *OUTDIR = "sim/inertia_scroll_out";
static void pump(unsigned ms) { unsigned long t0=millis(); do { loop(); Sleep(2); } while (millis()-t0 < ms); }
static int slot_of(const char *name) {
  static const char *N[]={"menu","up","back","left","ok","right","option","down","delete","mode"};
  for (int i=0;i<10;i++) if (!strcmp(name,N[i])) return i;
  return -1;
}
static void tap(const char *name, unsigned post=70) {
  int s=slot_of(name); if(s<0)return;
  SIM_KEYS[s]=1; pump(55); SIM_KEYS[s]=0; pump(post);
}
static void shot(const char *name) {
  char p[256]; snprintf(p,sizeof p,"%s/%s.bmp",OUTDIR,name);
  if (LGFX::inst) LGFX::inst->dumpBmp(p);
  printf("[shot] %s pos=%d target=%d vel=%d inertia=%d active=%d\n",
         p, sim_body_scroll_px(), sim_body_scroll_target_px(),
         sim_body_scroll_velocity_fp(), sim_body_scroll_inertia(), sim_body_scroll_active());
}

int main() {
#ifdef _WIN32
  ::mkdir("sim"); ::mkdir(OUTDIR); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir("sim",0755); ::mkdir(OUTDIR,0755); ::mkdir("sim_lfs",0755); ::mkdir("sim_lfs/Qeafbrowser",0755);
#endif
  FILE *f=fopen("sim_lfs/Qeafbrowser/config.ini","wb");
  if(f){ fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=https://qeafivels.com/\n",f); fclose(f); }
  sim_http_mock_set(true);
  setup(); pump(220);
  sim_key_launch_go_home(); pump(80);
  // /long.html la fixture WML dai: khong bat chuot ao (khong co viewport meta) va
  // du dai de cuon > mot man hinh, nen anim cuon con dang chay khi giu D-Pad.
  sim_go_url("https://keypad.test/long.html"); pump(180);

  // Bring the focus near the bottom so the next DOWN requires visual pixel scrolling.
  for(int i=0;i<5;i++) tap("down",60);
  shot("01_before_inertia");

  int down=slot_of("down");
  SIM_KEYS[down]=1;                 // physical key is held; body scroll accelerates gently
  pump(55);
  int held_pos=sim_body_scroll_px(), held_vel=sim_body_scroll_velocity_fp();
  shot("02_dpad_held");
  SIM_KEYS[down]=0;                 // release: no new key event, friction handles the coast
  pump(2);
  int release_pos=sim_body_scroll_px(), release_vel=sim_body_scroll_velocity_fp();
  shot("03_release_start");

  pump(18);
  int early_pos=sim_body_scroll_px(), early_vel=sim_body_scroll_velocity_fp();
  shot("04_coast_early");
  pump(35);
  int mid_pos=sim_body_scroll_px(), mid_vel=sim_body_scroll_velocity_fp();
  shot("05_coast_mid");
  pump(50);
  int late_pos=sim_body_scroll_px(), late_vel=sim_body_scroll_velocity_fp();
  shot("06_coast_late");
  pump(2000);
  int end_pos=sim_body_scroll_px(), end_vel=sim_body_scroll_velocity_fp();
  int target=sim_body_scroll_target_px();
  shot("07_settled");

  printf("[assert] held=%d/%d release=%d/%d early=%d/%d mid=%d/%d late=%d/%d end=%d/%d target=%d\n",
         held_pos,held_vel,release_pos,release_vel,early_pos,early_vel,
         mid_pos,mid_vel,late_pos,late_vel,end_pos,end_vel,target);
  if (held_vel <= 0 || release_vel <= 0) {
    fprintf(stderr,"[FAIL] D-Pad hold did not create positive fixed-point velocity\n"); return 2;
  }
  if (!(release_pos <= early_pos && early_pos <= mid_pos && mid_pos <= late_pos && late_pos <= end_pos)) {
    fprintf(stderr,"[FAIL] position did not coast monotonically after release\n"); return 3;
  }
  if (!(release_vel >= early_vel && early_vel >= mid_vel && mid_vel >= late_vel && late_vel >= 0)) {
    fprintf(stderr,"[FAIL] fixed-point velocity did not decelerate after release\n"); return 4;
  }
  if (end_pos != target || end_vel != 0 || sim_body_scroll_inertia() != 0 || sim_body_scroll_active() != 0) {
    fprintf(stderr,"[FAIL] inertial scroll did not settle cleanly\n"); return 5;
  }
  printf("[PASS] release inertia + integer friction + final settle\n");
  printf("[done] inertia scroll regression complete\n");
  return 0;
}
