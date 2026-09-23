#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
using String = std::string;
using std::to_string;
typedef uint8_t byte;
inline void delay(unsigned ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline void delayMicroseconds(unsigned us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }
inline unsigned long millis() { static auto t0=std::chrono::steady_clock::now(); auto d=std::chrono::steady_clock::now()-t0; return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(d).count(); }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
#define INPUT_PULLUP 1
#define OUTPUT 1
#define HIGH 1
#define LOW 0

struct SimSerial {
  FILE *log=nullptr;
  SimSerial(){ log=fopen("sim_log.txt","w"); }
  ~SimSerial(){ if(log) fclose(log); }
  void begin(unsigned){}
  int printf(const char *fmt,...){ va_list ap; va_start(ap,fmt); char b[2048]; vsnprintf(b,sizeof b,fmt,ap); va_end(ap); fputs(b,stdout); fflush(stdout); if(log){fputs(b,log);fflush(log);} return 0; }
  void print(const char *s){ printf("%s",s?s:""); }
  void println(const char *s=""){ printf("%s\n",s?s:""); }
  void flush(){ fflush(stdout); if(log) fflush(log); }
  int available(){ return 0; }
  int read(){ return -1; }
};
extern SimSerial Serial;

#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WIFI_AUTH_OPEN 0
struct SimNet { const char *ssid; int rssi; bool open; };
extern SimNet SIM_NETS[]; extern int SIM_NET_COUNT; extern char SIM_CONNECTED[64]; extern char SIM_EXPECT_PASS[64];
struct IPAddress { char ip[16]; String toString(){return String(ip);} };
struct SimWiFi {
  bool connected=false;
  void mode(int){} void persistent(bool){}
  void disconnect(){connected=false;SIM_CONNECTED[0]=0;}
  int scanNetworks(bool=false,bool=false,bool=false,uint32_t=3000,uint8_t=0){return SIM_NET_COUNT;}
  int scanDelete(){return 0;}
  String SSID(int i){return String(SIM_NETS[i].ssid);} int RSSI(int i){return SIM_NETS[i].rssi;} int encryptionType(int i){return SIM_NETS[i].open?WIFI_AUTH_OPEN:1;}
  void begin(const char *ssid,const char *pass=nullptr){connected=false;for(int i=0;i<SIM_NET_COUNT;i++){if(!strcmp(SIM_NETS[i].ssid,ssid)&&(SIM_NETS[i].open||(pass&&!strcmp(pass,SIM_EXPECT_PASS)))){connected=true;strncpy(SIM_CONNECTED,ssid,63);SIM_CONNECTED[63]=0;return;}}}
  int status(){return connected?WL_CONNECTED:WL_DISCONNECTED;}
  IPAddress localIP(){IPAddress a;strcpy(a.ip,"127.0.0.1");return a;}
};
extern SimWiFi WiFi;

struct WiFiClient {
  int fd=-1; bool ok=false; bool tls=false; SSL_CTX *ctx=nullptr; SSL *ssl=nullptr;
  WiFiClient()=default;
  ~WiFiClient(){ stop(); }
  bool connect(const char *host,int port,int timeout_ms=5000);
  void print(const char *s);
  void print(const String &s){print(s.c_str());}
  void print(long v){char b[32];snprintf(b,sizeof b,"%ld",v);print(b);} void print(int v){char b[32];snprintf(b,sizeof b,"%d",v);print(b);}
  int available(); char read(); bool connected() const {return ok;} void stop();
};
struct WiFiClientSecure: public WiFiClient {
  WiFiClientSecure(){tls=true;} void setInsecure(){} void setHandshakeTimeout(unsigned long){}
  bool connect(const char *host,int port){ return WiFiClient::connect(host,port,8000); }
};

struct SimFile { FILE *f=nullptr; operator bool() const{return f!=nullptr;} int available(){if(!f)return 0; long p=ftell(f); fseek(f,0,SEEK_END); long e=ftell(f); fseek(f,p,SEEK_SET); return (int)(e-p);} int read(){return f?fgetc(f):-1;} size_t write(const uint8_t*b,size_t n){return f?fwrite(b,1,n,f):0;} void close(){if(f){fclose(f);f=nullptr;}} };
namespace fs { class FS { public: virtual ~FS(){} virtual SimFile open(const char*, const char *mode = "r")=0; virtual bool mkdir(const char*)=0; }; }
struct SimFS: public fs::FS { std::string root; explicit SimFS(const char*r):root(r){} bool begin(bool=false){return true;} bool begin(const char*,bool=false){return true;} bool mkdir(const char*p) override; int cardType(){return 1;} unsigned long long cardSize(){return 1ULL<<30;} SimFile open(const char*p,const char*mode="r") override; bool remove(const char*p){std::string f=root+p;return ::remove(f.c_str())==0;} };
extern SimFS SD_MMC,LittleFS;
typedef SimFile File;
#define FILE_READ "r"
#define FILE_WRITE "w"
#define MALLOC_CAP_SPIRAM 0
inline void *heap_caps_malloc(size_t n,int=0){return malloc(n);}

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_YELLOW 0xFFE0
#define TFT_CYAN 0x07FF
#define TFT_GREEN 0x07E0
#define TFT_NAVY 0x0010
#define TFT_DARKGREY 0x7BEF
#define TFT_LIGHTGREY 0xD69F
#define TFT_RED 0xF800

class LGFX { public:
  static LGFX *inst; uint16_t fb[240*320]; int font=2,cx=0,cy=0; uint16_t fg=TFT_WHITE,bg_=TFT_BLACK;
  LGFX(){inst=this;memset(fb,0,sizeof fb);} void init(){} void setRotation(int){} void setBrightness(int){} void startWrite(){} void endWrite(){}
  void fillScreen(uint16_t c){for(int i=0;i<240*320;i++)fb[i]=c;}
  void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)if(i>=0&&i<240&&j>=0&&j<320)fb[j*240+i]=c;}
  void drawPixel(int x,int y,uint16_t c){if(x>=0&&x<240&&y>=0&&y<320)fb[y*240+x]=c;} void drawFastHLine(int x,int y,int w,uint16_t c){fillRect(x,y,w,1,c);}
  void setTextFont(int f){font=f;} void setTextColor(uint16_t c){fg=c;} int charW(){return font>=4?12:(font==2?6:5);} int charH(){return font>=4?16:(font==2?8:7);} int textWidth(const char*s){return (int)strlen(s)*charW();} int textWidth(const char*s,int n){return n*charW();} int textcolor(){return fg;}
  uint16_t color565(uint8_t r,uint8_t g,uint8_t b){return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);} void drawString(const char*s,int x,int y); void dumpBmp(const char*path);
};
extern int SIM_KEYS[10];
int sim_digitalRead(int pin);
#define digitalRead(pin) (sim_digitalRead(pin))
extern "C" void sim_go_url(const char *url);
