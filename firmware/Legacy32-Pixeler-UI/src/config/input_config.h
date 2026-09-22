#pragma once
#include <stdint.h>
#include "board_config.h"

// Buttons connect the GPIO to GND when pressed. Pixeler enables INPUT_PULLUP.
namespace pixeler
{
  enum BtnID : uint8_t
  {
    BTN_UP = BOARD_KEY_UP,
    BTN_DOWN = BOARD_KEY_DOWN,
    BTN_LEFT = BOARD_KEY_LEFT,
    BTN_RIGHT = BOARD_KEY_RIGHT,
    BTN_MENU = BOARD_KEY_MENU,       // Home (top-left)
    BTN_OPTION = BOARD_KEY_OPTION,   // Context (bottom-left)
    BTN_OK = BOARD_KEY_START,        // OK = START center (Symbian map moi)
    BTN_START = BOARD_KEY_START,
    BTN_A = BOARD_KEY_A,
    BTN_BACK = BOARD_KEY_A           // Back = A top-right (Symbian map moi)
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
