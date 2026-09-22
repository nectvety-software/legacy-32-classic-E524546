#pragma once
#include <Arduino.h>

enum class AppId : uint8_t {
  HOME, WIFI, BLUETOOTH, FILES, TERMINAL,
  NOTES, LORA, IR, BROWSER, SETTINGS
};

enum class Key : uint8_t {
  NONE,
  MENU,
  UP,
  DOWN,
  LEFT,
  RIGHT,
  OPTION,
  SELECT,
  START,
  A,
  B
};

struct MenuItem {
  String label;
  String value;
  bool arrow = true;
};
