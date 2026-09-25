#pragma once
#include "Arduino.h"
#include "ReferenceAsciiFont.h"
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#define TFT_WHITE 0xFFFF
#define TFT_BLACK 0x0000
#define TFT_RED 0xF800
class TFT_eSPI {
public:
 std::vector<uint16_t> fb;
 uint16_t fg=0xffff,bg=0;
 int cx=0,cy=0,family=1,scale=1,fullFills=0,partialRects=0;
 bool transparent=false;
 TFT_eSPI():fb(240*320,0){}
 void init(){}void setRotation(int){}void setTextWrap(bool){};
 void setTextFont(int n){family=n;}void setTextSize(int n){scale=n;}
 void setTextColor(uint16_t f,uint16_t b){fg=f;bg=b;transparent=false;}
 void setTextColor(uint16_t f){fg=f;transparent=true;}
 void setCursor(int x,int y){cx=x;cy=y;}
 void fillRect(int x,int y,int w,int h,uint16_t c){++partialRects;
  if(w<1||h<1)return;
  for(int py=std::max(0,y);py<std::min(320,y+h);++py)
   for(int px=std::max(0,x);px<std::min(240,x+w);++px)fb[py*240+px]=c;
 }
 void fillScreen(uint16_t c){++fullFills;std::fill(fb.begin(),fb.end(),c);}
 void drawFastHLine(int x,int y,int w,uint16_t c){fillRect(x,y,w,1,c);}
 void drawFastVLine(int x,int y,int h,uint16_t c){fillRect(x,y,1,h,c);}
 void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
 void drawLine(int a,int b,int c,int d,uint16_t col){int dx=abs(c-a),sx=a<c?1:-1,dy=-abs(d-b),sy=b<d?1:-1,e=dx+dy;
  while(true){fillRect(a,b,1,1,col);if(a==c&&b==d)break;int e2=e*2;if(e2>=dy){e+=dy;a+=sx;}if(e2<=dx){e+=dx;b+=sy;}}
 }
 void drawCircle(int x,int y,int r,uint16_t col){int px=r,py=0,err=1-r;
  while(px>=py){point(x+px,y+py,col);point(x+py,y+px,col);point(x-py,y+px,col);point(x-px,y+py,col);point(x-px,y-py,col);point(x-py,y-px,col);point(x+py,y-px,col);point(x+px,y-py,col);++py;if(err<0)err+=2*py+1;else{--px;err+=2*(py-px+1);}}
 }
 void fillCircle(int x,int y,int r,uint16_t c){for(int dy=-r;dy<=r;++dy){int dx=int(sqrtf(float(r*r-dy*dy)));drawFastHLine(x-dx,y+dy,2*dx+1,c);}}
 void fillTriangle(int x0,int y0,int x1,int y1,int x2,int y2,uint16_t c){
   auto edge=[](int ax,int ay,int bx,int by,int px,int py){return (px-ax)*(by-ay)-(py-ay)*(bx-ax);};
   int mnx=std::min(x0,std::min(x1,x2)),mxx=std::max(x0,std::max(x1,x2)),mny=std::min(y0,std::min(y1,y2)),mxy=std::max(y0,std::max(y1,y2));
   int area=edge(x0,y0,x1,y1,x2,y2);if(!area)return;
   for(int y=mny;y<=mxy;++y)for(int x=mnx;x<=mxx;++x){int a=edge(x0,y0,x1,y1,x,y),b=edge(x1,y1,x2,y2,x,y),d=edge(x2,y2,x0,y0,x,y);if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))point(x,y,c);}
 }
 void point(int x,int y,uint16_t c){if(x>=0&&x<240&&y>=0&&y<320)fb[y*240+x]=c;}
 int textWidth(const char *s){int w=0;for(;s&&*s;++s){int c=(uint8_t)*s;w+=((c>=32&&c<127)?refFont(c).advance:6)*scale;}return w;}
 int textWidth(const String&s){return textWidth(s.c_str());}
 const RefFontGlyph &refFont(int c)const {return family==1?refFont0[c-32]:refFont1[c-32];}
 void print(const String &s){print(s.c_str());}
 void print(const char *s){for(;s&&*s;++s){int c=(uint8_t)*s;if(c<32||c>=127)c='?';auto &g=refFont(c);if(!transparent)fillRect(cx,cy,g.advance*scale,18*scale,bg);
   for(int yy=0;yy<18;++yy)for(int xx=0;xx<16;++xx)if(g.row[yy]&(1u<<xx))fillRect(cx+xx*scale,cy+yy*scale,scale,scale,fg);
   cx+=g.advance*scale;}
 }
 template<class V> void print(const V &v){print(String(v));}
 uint16_t get(int x,int y)const{return fb[y*240+x];}
 void savePPM(const char *path){std::ofstream f(path,std::ios::binary);f<<"P6\n240 320\n255\n";for(auto px:fb){char rgb[3]={char(((px>>11)&31)*255/31),char(((px>>5)&63)*255/63),char((px&31)*255/31)};f.write(rgb,3);}}
};
