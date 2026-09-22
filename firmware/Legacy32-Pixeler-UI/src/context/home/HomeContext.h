#pragma once

#include "context/IContext.h"
#include "widget/text/Label.h"

using namespace pixeler;

class HomeContext : public IContext
{
public:
  HomeContext();
  virtual ~HomeContext() = default;

protected:
  virtual bool loop() override;
  virtual void update() override;

private:
  enum Widget_ID : uint8_t
  {
    ID_HEADER = 1,
    ID_HEADER_BADGE,
    ID_HERO,
    ID_DEVICE,
    ID_STORAGE,
    ID_MEMORY,
    ID_SHORTCUT_A,
    ID_SHORTCUT_START,
    ID_STATUS,
    ID_FOOTER,
  };

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
  void refreshStatus();
  void openMenu();

private:
  Label* _storage_lbl{nullptr};
  Label* _memory_lbl{nullptr};
  Label* _status_lbl{nullptr};
  unsigned long _upd_timer{0};
};
