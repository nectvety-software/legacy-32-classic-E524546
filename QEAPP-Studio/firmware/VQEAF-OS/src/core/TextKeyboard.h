#pragma once
#include <Arduino.h>
#include "SymbianUI.h"
#include "Types.h"

class TextKeyboard {
public:
  void open(const String &caption, const String &initial = "", bool password = false);
  bool active() const { return shown; }
  bool handle(const KeyEvent &e);
  void draw(SymbianUI &ui, bool wifi, bool ble, bool sd, bool hour12);
  bool accepted() const { return done; }
  bool cancelled() const { return cancel; }
  String value() const { return text; }
private:
  bool shown = false, done = false, cancel = false, masked = false, upper = false, firstDraw = false;
  String caption, text;
  int cursor = 0;
  Key lastNumeric = Key::None;
  int numericTap = 0;
  uint32_t numericAt = 0;
  bool typeDigit(Key digit);
  static constexpr int COLS = 6;
  static constexpr int COUNT = 52;
  static constexpr int MAX_TEXT = 159;
  const char *layout = "abcdefghijklmnopqrstuvwxyz0123456789-_.!@#$%&*()+?/:";
  void moveVertical(int direction);
};
