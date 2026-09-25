#pragma once
#include "PixelSnakeLogic.h"
#include <stdint.h>
namespace PixelSnake {
// Same integer-pixel renderer on PC and ESP32: no scaling, PNG, or sprites at runtime.
struct Colors {
  uint16_t ground=0x19a7, alt=0x21c7, border=0x0a04, body=0x6fe9,
           light=0xcff7, dark=0x2c84, fruit=0xf8c3, fruitLight=0xfdc0,
           ink=0xf7ff, panel=0x10c4, shadow=0x0802;
};
class Canvas {
 public:
  virtual ~Canvas(){}
  virtual void fill(int x,int y,int w,int h,uint16_t rgb565)=0;
};
class Renderer {
 public:
  static constexpr int TILE=12, X=24, Y=60, BW=W*TILE, BH=H*TILE;
  static void text(Canvas &c,int x,int y,const char *s,uint16_t color,int scale=1);
  static void background(Canvas &c,const Colors &colors);
  static void cell(Canvas &c,Point p,const Game &g,const Colors &colors);
  static void scene(Canvas &c,const Game &g,const Colors &colors,uint16_t highScore);
  static void overlay(Canvas &c,Phase phase,const Colors &colors);
  static void hud(Canvas &c,const Game &g,uint16_t highScore,const Colors &colors);
};
}
