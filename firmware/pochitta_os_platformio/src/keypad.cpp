#include "keypad.h"
#include "board_config.h"

struct KeyPin {
  int pin;
  Key key;
};

static const KeyPin KEYS[] = {
  {PIN_KEY_MENU, Key::MENU},
  {PIN_KEY_UP, Key::UP},
  {PIN_KEY_DOWN, Key::DOWN},
  {PIN_KEY_LEFT, Key::LEFT},
  {PIN_KEY_RIGHT, Key::RIGHT},
  {PIN_KEY_OPTION, Key::OPTION},
  {PIN_KEY_SELECT, Key::SELECT},
  {PIN_KEY_START, Key::START},
  {PIN_KEY_A, Key::A},
  {PIN_KEY_B, Key::B},
};

void Keypad::begin() {
  for (const auto &key : KEYS) {
    if (key.pin >= 0) {
      pinMode(key.pin, KEY_INPUT_MODE);
    }
  }
}

Key Keypad::poll() {
  if (millis() - lastPress_ < KEY_REPEAT_GUARD_MS) {
    return Key::NONE;
  }

  for (const auto &key : KEYS) {
    if (key.pin < 0 || digitalRead(key.pin) != KEY_ACTIVE_LEVEL) {
      continue;
    }

    delay(KEY_DEBOUNCE_MS);
    if (digitalRead(key.pin) == KEY_ACTIVE_LEVEL) {
      lastPress_ = millis();
      return key.key;
    }
  }

  return Key::NONE;
}
