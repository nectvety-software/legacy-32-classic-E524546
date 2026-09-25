#pragma once
#include "PixelSnakeLogic.h"
#include "PixelSnakeRender.h"
#include "PixelSnakeConfig.h"
#include "Apps.h"

// Built-in, constrained QEAPP/2 game handler for exactly id=snake_pixel.
// The signed .qeapp supplies only configuration + 32x32 icon, never native code.
class PixelSnakeApp {
 public:
  bool enter(AppContext &ctx);
  void draw(AppContext &ctx);
  void tick(AppContext &ctx);
  ScreenId handle(AppContext &ctx,const KeyEvent &key);
 private:
  PixelSnake::Game game_;
  PixelSnake::Config config_={155,false,PixelSnake::Config::Skin::Forest};
  PixelSnake::Colors colors_;
  uint16_t high_=0;
  uint32_t lastStep_=0;
  bool valid_=false;
  void saveHigh(AppContext &ctx);
  void loadHigh(AppContext &ctx);
  void redrawDelta(AppContext &ctx,PixelSnake::Point formerHead,PixelSnake::Point formerTail,PixelSnake::Result result);
  void paintTFT(AppContext &ctx,bool full);
};
