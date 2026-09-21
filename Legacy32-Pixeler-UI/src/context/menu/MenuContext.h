#pragma once

#include "context/IContext.h"
#include "widget/image/Image.h"
#include "widget/menu/GridMenu.h"
#include "widget/text/Label.h"

using namespace pixeler;

class MenuContext : public IContext
{
public:
  MenuContext();
  virtual ~MenuContext() = default;

protected:
  virtual bool loop() override;
  virtual void update() override;

private:
  enum Widget_ID : uint8_t
  {
    ID_HEADER = 1,
    ID_MENU,
    ID_FOOTER,
  };

  void addMenuItem(ContextID id, const char* title, Image::IconType icon);
  void up();
  void down();
  void pageUp();
  void pageDown();
  void ok();

private:
  GridMenu* _menu{nullptr};
  static uint8_t _last_sel_item_pos;
};
