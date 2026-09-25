#pragma once
#include "Arduino.h"
#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define TFT_RED 0xF800
class TFT_eSPI { bool swapBytes=false; public:
 void init(){} void setRotation(int){} void fillScreen(uint16_t){} void setTextWrap(bool){} void setTextFont(int){} void setTextSize(int){} void setTextColor(uint16_t,uint16_t=0){} void setCursor(int,int){}
 template<class T> void print(const T&){} int textWidth(const String&s){return (int)s.length()*7;} int textWidth(const char*s){return s?(int)strlen(s)*7:0;}
 void fillRect(int,int,int,int,uint16_t){} void drawRect(int,int,int,int,uint16_t){} void drawFastHLine(int,int,int,uint16_t){} void drawFastVLine(int,int,int,uint16_t){} void drawLine(int,int,int,int,uint16_t){} void fillCircle(int,int,int,uint16_t){} void drawCircle(int,int,int,uint16_t){} void fillTriangle(int,int,int,int,int,int,uint16_t){}
 void setSwapBytes(bool b){swapBytes=b;} bool getSwapBytes()const{return swapBytes;} uint16_t color565(uint8_t r,uint8_t g,uint8_t b){return uint16_t(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));}
 void pushImage(int,int,int,int,const uint16_t*){}
 void startWrite(){} void endWrite(){}
};
