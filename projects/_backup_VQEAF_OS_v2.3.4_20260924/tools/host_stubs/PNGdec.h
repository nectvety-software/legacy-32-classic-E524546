#pragma once
#include "Arduino.h"
#define PNG_SUCCESS 0
#define PNG_RGB565_LITTLE_ENDIAN 0
struct PNGDRAW { int y; };
typedef int PNG_DRAW_CALLBACK(PNGDRAW*);
class PNG { public:
  int openRAM(uint8_t*,int,PNG_DRAW_CALLBACK*){return PNG_SUCCESS;} int getWidth()const{return 100;} int getHeight()const{return 100;}
  void getLineAsRGB565(PNGDRAW*,uint16_t*line,int,uint32_t){ if(line) for(int i=0;i<100;i++) line[i]=0; }
  int decode(void*,int){return PNG_SUCCESS;} void close(){}
};
