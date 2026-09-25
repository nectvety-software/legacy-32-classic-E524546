#pragma once
#include <algorithm>
#include <vector>
#include <stdint.h>
class TFT_eSPI {
public:
  enum { W=240, H=320 };
  std::vector<uint16_t> fb;
  TFT_eSPI() : fb(W*H,0x0000){}
  void fillRect(int x,int y,int w,int h,uint16_t color){
    if(w<=0||h<=0)return;
    for(int iy=std::max(0,y);iy<std::min(int(H),y+h);++iy)
      for(int ix=std::max(0,x);ix<std::min(int(W),x+w);++ix)
        fb[iy*W+ix]=color;
  }
  void drawFastHLine(int x,int y,int w,uint16_t color){fillRect(x,y,w,1,color);}
  void drawRect(int x,int y,int w,int h,uint16_t color){
    drawFastHLine(x,y,w,color);drawFastHLine(x,y+h-1,w,color);
    fillRect(x,y,1,h,color);fillRect(x+w-1,y,1,h,color);
  }
};
