#include "QeLuaRuntime.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <tuple>
#include <iostream>
#include <string>
#include <vector>
struct Screen {
 std::vector<uint16_t> pixels = std::vector<uint16_t>(240*270,0);
 unsigned rectangles=0, texts=0;
};
static uint32_t nowMs(void *) {
 using namespace std::chrono;
 return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
static void fill(void *ctx,int x,int y,int w,int h,uint16_t c) {
 auto *s=static_cast<Screen*>(ctx);
 for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)s->pixels[yy*240+xx]=c;
 ++s->rectangles;
}
struct Glyph {char ch;uint8_t rows[7];};
static const Glyph GLYPHS[] = {
  {'A',{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
  {'B',{0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}},
  {'C',{0b01111,0b10000,0b10000,0b10000,0b10000,0b10000,0b01111}},
  {'D',{0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110}},
  {'E',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}},
  {'F',{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}},
  {'G',{0b01111,0b10000,0b10000,0b10111,0b10001,0b10001,0b01110}},
  {'H',{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
  {'I',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111}},
  {'J',{0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100}},
  {'K',{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001}},
  {'L',{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}},
  {'M',{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}},
  {'N',{0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001}},
  {'O',{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
  {'P',{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000}},
  {'Q',{0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101}},
  {'R',{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}},
  {'S',{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}},
  {'T',{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}},
  {'U',{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
  {'V',{0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100}},
  {'W',{0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010}},
  {'X',{0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001}},
  {'Y',{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100}},
  {'Z',{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111}},
  {'0',{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}},
  {'1',{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}},
  {'2',{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111}},
  {'3',{0b11110,0b00001,0b00001,0b00110,0b00001,0b00001,0b11110}},
  {'4',{0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}},
  {'5',{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110}},
  {'6',{0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110}},
  {'7',{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}},
  {'8',{0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}},
  {'9',{0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100}},
  {'/',{0b00001,0b00001,0b00010,0b00100,0b01000,0b10000,0b10000}},
  {'=',{0b00000,0b11111,0b00000,0b11111,0b00000,0b00000,0b00000}},
  {':',{0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000}},
  {'-',{0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000}},
  {' ',{0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000}},
};
static const uint8_t *glyph(char ch) {
  if(ch>='a'&&ch<='z')ch-=32;
  for(const auto &g:GLYPHS)if(g.ch==ch)return g.rows;
  return GLYPHS[sizeof(GLYPHS)/sizeof(GLYPHS[0])-1].rows;
}
static void text(void *ctx,int x,int y,const char *str,uint16_t color) {
  auto *s=static_cast<Screen*>(ctx);++s->texts;
  for(int n=0;str[n] && n<48;++n) {
    int ox=x+n*6;if(ox>=240)break;
    const uint8_t *glyphRows=glyph(str[n]);
    for(int dy=0;dy<7;++dy)for(int dx=0;dx<5;++dx) {
      if(glyphRows[dy]&(1<<(4-dx))) {
        const int xx=ox+dx,yy=y+dy;
        if(xx>=0&&xx<240&&yy>=0&&yy<270)s->pixels[yy*240+xx]=color;
      }
    }
  }
}

static bool image(const std::string &out,const Screen &s) {
 std::ofstream f(out,std::ios::binary);if(!f)return false;
 f<<"P6\n240 270\n255\n";
 for(uint16_t px:s.pixels){unsigned r=(px>>11)&31,g=(px>>5)&63,b=px&31;
   f.put(char((r*255+15)/31));f.put(char((g*255+31)/63));f.put(char((b*255+15)/31));}
 return f.good();
}
struct KeyEvent {int frame;std::string key;bool down;};
static bool keyAllowed(const std::string &s) {
  return s=="left" || s=="right" || s=="up" || s=="down" ||
         s=="start" || s=="option";
}
static bool readEvents(const char *path,int frames,std::vector<KeyEvent> &events) {
  std::ifstream f(path,std::ios::binary);
  if (!f) return false;
  std::string line;
  while(std::getline(f,line)) {
    if (line.empty() || line.size()>64 || events.size()>=128) return false;
    std::istringstream is(line);int frame,pressed;std::string key,extra;
    if(!(is>>frame>>key>>pressed) || (is>>extra) || frame<0 || frame>=frames ||
       !keyAllowed(key) || (pressed!=0 && pressed!=1)) return false;
    events.push_back(KeyEvent{frame,key,pressed!=0});
  }
  if (!f.eof() || events.empty()) return false;
  std::stable_sort(events.begin(),events.end(),
     [](const KeyEvent &a,const KeyEvent &b){return a.frame<b.frame;});
  return true;
}
int main(int argc,char **argv) {
 if(argc<2||argc>6){std::cerr<<"Usage: qe_lua_host main.lua [image.ppm] [frames=4] [memory_kib=192] [events.txt]\n";return 2;}
 std::ifstream f(argv[1],std::ios::binary);if(!f){std::cerr<<"Source missing\n";return 2;}
 std::string source((std::istreambuf_iterator<char>(f)),{});
 if(source.empty()||source.size()>QeLuaRuntime::kMaxSource){std::cerr<<"Invalid script size\n";return 2;}
 int frames=argc>=4?std::stoi(argv[3]):4;
 int kib=argc>=5?std::stoi(argv[4]):192;
 if(frames<0||frames>300||kib<16||kib>1024){std::cerr<<"Out of range\n";return 2;}
 std::vector<KeyEvent> events;
 if(argc==6 && !readEvents(argv[5],frames,events)) {
   std::cerr<<"Invalid replay file or reserved key; max 128 events\n";
   return 2;
 }
 Screen screen;
 QeLuaRuntime vm;
 QeLuaRuntime::Draw draw{fill,text,nowMs,&screen};
 if(!vm.start(source.data(),source.size(),draw,size_t(kib)*1024)){std::cerr<<"Lua startup FAIL: "<<vm.error()<<"\n";return 1;}
 for(int i=0;i<frames;++i){
   if(argc<6 && i==1 && !vm.key("start",true)) {
      std::cerr<<vm.error()<<"\n";return 1;
   }
   for(const auto &ev: events) if(ev.frame==i && !vm.key(ev.key.c_str(),ev.down)) {
     std::cerr<<vm.error()<<"\n";return 1;
   }
   if(!vm.update(0.033f)||!vm.render()){std::cerr<<"Lua runtime FAIL: "<<vm.error()<<"\n";return 1;}
 }
 size_t heap=vm.heapUsed(),peak=vm.peakHeapUsed();vm.stop();
 if(argc>=3 && !image(argv[2],screen)){std::cerr<<"Cannot save preview\n";return 2;}
 std::cout<<"PASS Lua 5.4 VM frames="<<frames<<" peak_live_last="<<heap
          <<" peak_heap="<<peak<<" input_events="<<events.size()<<" rect="<<screen.rectangles<<" text="<<screen.texts<<"\n";
 return 0;
}
