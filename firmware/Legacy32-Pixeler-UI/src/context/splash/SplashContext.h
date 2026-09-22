#pragma once
#include <stdint.h>

#include "context/IContext.h"
#include "widget/text/Label.h"

using namespace pixeler;

class SplashContext : public IContext
{
public:
  SplashContext();
  virtual ~SplashContext() = default;

protected:
  virtual bool loop() override;
  virtual void update() override;

private:
  Label* createLabel(uint16_t id,
                     const char* text,
                     uint16_t x,
                     uint16_t y,
                     uint16_t width,
                     uint16_t height,
                     uint16_t back_color,
                     uint16_t text_color,
                     const uint8_t* font = font_unifont,
                     uint8_t radius = 6);

private:
  unsigned long _start_time{0};
};
