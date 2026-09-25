#pragma once
#include "Arduino.h"
class TJpgDecoderStub { public:
  bool getJpgSize(uint16_t*w,uint16_t*h,const uint8_t*,uint32_t){if(w)*w=100;if(h)*h=100;return true;}
  void setCallback(bool (*)(int16_t,int16_t,uint16_t,uint16_t,uint16_t*)){} void setJpgScale(uint8_t){}
  bool drawJpg(int,int,const uint8_t*,uint32_t){return true;}
};
static TJpgDecoderStub TJpgDec;
