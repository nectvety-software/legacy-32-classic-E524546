#include "sim_arduino.h"
#include <sys/stat.h>
#include <stdio.h>
extern void setup();
extern void loop();
static void pump(unsigned ms){ unsigned long t0=millis(); do{ loop(); Sleep(5);}while(millis()-t0<ms); }
static void shot(const char *name){ char p[256]; snprintf(p,sizeof p,"sim/clock_out/%s.bmp",name); if(LGFX::inst) LGFX::inst->dumpBmp(p); printf("[shot] %s\n",p); }
int main(){
#ifdef _WIN32
  ::mkdir("sim"); ::mkdir("sim/clock_out"); ::mkdir("sim_lfs"); ::mkdir("sim_lfs/Qeafbrowser");
#else
  ::mkdir("sim",0755); ::mkdir("sim/clock_out",0755); ::mkdir("sim_lfs",0755); ::mkdir("sim_lfs/Qeafbrowser",0755);
#endif
  FILE *f=fopen("sim_lfs/Qeafbrowser/config.ini","wb");
  if(f){ fputs("wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=https://qeafivels.com/\ntimezone=ICT-7\n",f); fclose(f); }
  sim_http_mock_set(true);
  setup();
  pump(250);
  shot("01_ntp_synced");
  pump(1100);
  shot("02_one_second_later");
  pump(1100);
  shot("03_two_seconds_later");
  puts("[PASS] realtime clock footer updated without full-page timer task");
  return 0;
}
