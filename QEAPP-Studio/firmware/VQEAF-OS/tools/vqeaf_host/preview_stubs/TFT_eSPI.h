#pragma once
#include "Arduino.h"
#include <vector>
#include <fstream>
#include <cassert>
class TFT_eSPI {
public:
  std::vector<uint16_t> fb;
  uint16_t fg=0xffff, bg=0;
  int cursorX=0,cursorY=0,textfont=1,fullFills=0;
  TFT_eSPI():fb(240*320,0){}
  void setTextFont(int n){textfont=n;}
  void setTextSize(int){}
  void setTextColor(uint16_t f,uint16_t b){fg=f;bg=b;}
  void setTextColor(uint16_t f){fg=f;}
  void setCursor(int x,int y){cursorX=x;cursorY=y;}
  int textWidth(const String &s){return (int)s.length()*(textfont==2?8:6);}
  int textWidth(const char *s){return (int)strlen(s)*(textfont==2?8:6);}
  void print(const String &s){(void)s;}
  void print(const char *s){(void)s;}
  void fillScreen(uint16_t color){++fullFills;std::fill(fb.begin(),fb.end(),color);}
  void fillRect(int x,int y,int w,int h,uint16_t color){for(int yy=std::max(0,y);yy<std::min(320,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(240,x+w);++xx)fb[yy*240+xx]=color;}
  void drawFastHLine(int x,int y,int w,uint16_t c){fillRect(x,y,w,1,c);}
  void drawFastVLine(int x,int y,int h,uint16_t c){fillRect(x,y,1,h,c);}
  void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
  void drawLine(int x0,int y0,int x1,int y1,uint16_t c){
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){fillRect(x0,y0,1,1,c);if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
  }
  void pushImage(int x,int y,int w,int h,const uint16_t *p){for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)fillRect(x+xx,y+yy,1,1,p[yy*w+xx]);}
  uint16_t get(int x,int y)const{return fb[y*240+x];}
  void savePPM(const char *name){
    std::ofstream f(name,std::ios::binary);f<<"P6\n240 320\n255\n";
    for(auto px:fb){char rgb[3]={char(((px>>11)&31)*255/31),char(((px>>5)&63)*255/63),char((px&31)*255/31)};f.write(rgb,3);}
  }
};
