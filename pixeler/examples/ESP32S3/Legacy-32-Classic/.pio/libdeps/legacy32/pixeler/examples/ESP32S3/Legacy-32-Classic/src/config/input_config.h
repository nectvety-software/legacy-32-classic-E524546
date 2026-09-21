#pragma once
#include <stdint.h>

// Buttons connect the GPIO to GND when pressed. Pixeler enables INPUT_PULLUP.
namespace pixeler
{
  enum BtnID : uint8_t
  {
    BTN_UP = 7,
    BTN_DOWN = 46,
    BTN_LEFT = 45,
    BTN_RIGHT = 6,
    BTN_MENU = 18,
    BTN_OPTION = 8,
    BTN_OK = 16,       // KEY_SELECT
    BTN_START = 17,
    BTN_A = 15,
    BTN_BACK = 5       // KEY_B
  };
}

#define BUTTONS_TMPL                               \
  {                                                \
      {BTN_UP, Button(BTN_UP, false)},             \
      {BTN_DOWN, Button(BTN_DOWN, false)},         \
      {BTN_LEFT, Button(BTN_LEFT, false)},         \
      {BTN_RIGHT, Button(BTN_RIGHT, false)},       \
      {BTN_MENU, Button(BTN_MENU, false)},         \
      {BTN_OPTION, Button(BTN_OPTION, false)},     \
      {BTN_OK, Button(BTN_OK, false)},             \
      {BTN_START, Button(BTN_START, false)},       \
      {BTN_A, Button(BTN_A, false)},               \
      {BTN_BACK, Button(BTN_BACK, false)},         \
  }

#define BTN_TOUCH_TRESHOLD 50000
#define PRESS_DURATION (unsigned long)1000
#define PRESS_LOCK (unsigned long)700
#define CLICK_LOCK (unsigned long)250
#define HOLD_LOCK (unsigned long)150

