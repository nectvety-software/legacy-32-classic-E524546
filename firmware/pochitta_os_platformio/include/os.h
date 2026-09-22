#pragma once
#include "ui.h"
#include "keypad.h"
#include "services.h"

class PochittaOS {
 public:
  void begin();
  void loop();
 private:
  UI ui_; Keypad keys_; Services services_;
  AppId app_=AppId::HOME; int selected_=0,scroll_=0; std::vector<MenuItem> items_;
  void open(AppId id); void render(); void activate(); void goBack(); void move(int delta);
  std::vector<MenuItem> appMenu(AppId id);
};
