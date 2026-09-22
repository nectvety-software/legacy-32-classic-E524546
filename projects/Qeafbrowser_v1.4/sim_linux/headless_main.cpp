#include "sim_arduino_posix.h"
#include <sys/stat.h>
#include <string>
extern void setup(); extern void loop();
extern "C" void sim_go_url(const char *url);
static int slot_of(const char *name){static const char*N[]={"menu","up","back","left","ok","right","option","down","delete","mode"};for(int i=0;i<10;i++)if(!strcmp(name,N[i]))return i;return -1;}
static void pump(unsigned ms){unsigned long t=millis();while(millis()-t<ms)loop();}
static void press(const char*n){int s=slot_of(n);if(s<0)return;SIM_KEYS[s]=1;pump(80);SIM_KEYS[s]=0;pump(180);}
static void shot(const char*name){mkdir("sim_out_linux",0777);std::string p=std::string("sim_out_linux/")+name+".bmp";LGFX::inst->dumpBmp(p.c_str());printf("[shot] %s\n",p.c_str());}
int main(int argc,char**argv){mkdir("sim_lfs",0777);mkdir("sim_lfs/ESPBrowser",0777);mkdir("sim_sd",0777);FILE*f=fopen("sim_lfs/ESPBrowser/config.ini","w");fprintf(f,"wifi_ssid=VNPT-Home\nwifi_pass=abc\nhome_url=mtt:start\n");fclose(f);setup();pump(100);shot("01_home");
 const char*http=argc>1?argv[1]:"http://127.0.0.1:18080/redirect"; const char*https=argc>2?argv[2]:"https://127.0.0.1:18443/";
 sim_go_url(http);pump(100);shot("02_http_redirect");
 press("ok");pump(100);shot("03_link_click");
 sim_go_url(https);pump(100);shot("04_https_tls");
 sim_go_url("mtt:history");pump(100);shot("05_history");
 sim_go_url("https://127.0.0.1:18443/chunked");pump(100);shot("06_chunked");
 return 0;}
