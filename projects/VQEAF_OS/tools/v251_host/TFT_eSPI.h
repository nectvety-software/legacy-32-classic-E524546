#pragma once
#include <stdint.h>
#include <cassert>
class TFT_eSPI {
  bool swapped=false;
public:
  unsigned calls=0;uint16_t output[1024]={};int16_t x=0,y=0;
  bool getSwapBytes() const{return swapped;}
  void setSwapBytes(bool enabled){swapped=enabled;}
  void pushImage(int16_t xx,int16_t yy,int16_t w,int16_t h,const uint16_t *src){
    assert(w==32&&h==32);assert(src);x=xx;y=yy;calls++;
    for(unsigned i=0;i<1024;i++){
      // Mock semantics: bytes are transmitted in native RGB565 wire order
      // only when TFT_eSPI is configured to swap host uint16 words.
      output[i]=swapped?src[i]:uint16_t((src[i]<<8)|(src[i]>>8));
    }
  }
};
