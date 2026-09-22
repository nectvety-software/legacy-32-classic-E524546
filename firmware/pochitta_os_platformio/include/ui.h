#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "app_types.h"

class UI {
 public:
  void begin();
  void header(const String& title);
  void footer(const String& left="Options", const String& right="Back");
  void menu(const String& title, const std::vector<MenuItem>& items, int selected, int scroll=0);
  void info(const String& title, const std::vector<MenuItem>& rows);
  void message(const String& title, const String& text);
  TFT_eSPI& tft() { return tft_; }
 private:
  TFT_eSPI tft_;
  void battery();
};
