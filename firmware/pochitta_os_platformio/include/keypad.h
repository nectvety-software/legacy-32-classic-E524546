#pragma once
#include <Arduino.h>
#include "app_types.h"

class Keypad {
 public:
  void begin();
  Key poll();
 private:
  uint32_t lastPress_ = 0;
};
