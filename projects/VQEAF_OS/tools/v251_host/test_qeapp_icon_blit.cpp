#include "../../src/core/QeappIconBlit.h"
#include <cassert>
#include <cstdio>
int main(){
  TFT_eSPI lcd;
  uint16_t src[1024];for(unsigned i=0;i<1024;i++)src[i]=uint16_t((i*307+0xF800)&0xffff);
  QeappIconBlit::draw(lcd,8,35,src);
  assert(lcd.calls==1 && lcd.x==8 && lcd.y==35);
  assert(!lcd.getSwapBytes());
  for(unsigned i=0;i<1024;i++)assert(lcd.output[i]==src[i]);
  lcd.setSwapBytes(true);QeappIconBlit::draw(lcd,103,170,src);
  assert(lcd.calls==2 && lcd.getSwapBytes());
  for(unsigned i=0;i<1024;i++)assert(lcd.output[i]==src[i]);
  QeappIconBlit::draw(lcd,0,0,nullptr);assert(lcd.calls==2);
  puts("PASS: QEAPP icon draw preserves 1024/1024 RGB565 colors, swap state, positions");
}
