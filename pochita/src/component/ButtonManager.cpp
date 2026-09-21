#include "ButtonManager.h"

ButtonManager buttonManager;

// Pin mapping array matching states index
const int pinMap[] = {KEY_UP,     KEY_DOWN,   KEY_LEFT,  KEY_RIGHT, KEY_SELECT,
                      KEY_OPTION, KEY_START, KEY_B, KEY_OPTION,     KEY_A};

const unsigned long BUTTON_REPEAT_DELAY = 350;
const unsigned long BUTTON_REPEAT_INTERVAL = 120;

void ButtonManager::begin() {
  for (int i = 0; i < 10; i++) {
    states[i].pin = pinMap[i];
    states[i].pressed = false;
    states[i].justPressed = false;
    states[i].lastRaw = false;
    states[i].lastChange = 0;
    states[i].repeatAt = 0;
    pinMode(states[i].pin, INPUT_PULLUP);
  }
}

void ButtonManager::update() {
  for (int i = 0; i < 10; i++) {
    // Active LOW
    bool reading = (digitalRead(states[i].pin) == LOW);

    // Reset single-shot flag every frame
    states[i].justPressed = false;

    if (reading != states[i].lastRaw) {
      states[i].lastChange = millis();
    }

    if ((millis() - states[i].lastChange) > 50) { // 50ms debounce
      if (reading != states[i].pressed) {
        states[i].pressed = reading;

        // If we just became pressed, set the flag
        if (states[i].pressed) {
          states[i].justPressed = true;
          states[i].repeatAt = millis() + BUTTON_REPEAT_DELAY;
        } else {
          states[i].repeatAt = 0;
        }
      }
    }

    states[i].lastRaw = reading;
  }
}

int ButtonManager::getIndex(int pin) {
  for (int i = 0; i < 10; i++) {
    if (states[i].pin == pin)
      return i;
  }
  return -1;
}

bool ButtonManager::isPressed(int pin) {
  int idx = getIndex(pin);
  if (idx == -1)
    return false;
  return states[idx].pressed;
}

bool ButtonManager::isJustPressed(int pin) {
  int idx = getIndex(pin);
  if (idx == -1)
    return false;
  return states[idx].justPressed;
}

ButtonAction ButtonManager::actionForIndex(int index) const {
  static const ButtonAction actions[] = {
      ButtonAction::UP,     ButtonAction::DOWN, ButtonAction::LEFT,
      ButtonAction::RIGHT,  ButtonAction::MENU, ButtonAction::OPTION,
      ButtonAction::SELECT, ButtonAction::START, ButtonAction::A,
      ButtonAction::B};
  return (index >= 0 && index < 10) ? actions[index] : ButtonAction::NONE;
}

ButtonAction ButtonManager::getAction(bool allowRepeat) {
  unsigned long now = millis();
  for (int i = 0; i < 10; ++i) {
    if (states[i].justPressed) return actionForIndex(i);
  }
  if (!allowRepeat) return ButtonAction::NONE;
  for (int i = 0; i < 10; ++i) {
    if (states[i].pressed && states[i].repeatAt != 0 &&
        (long)(now - states[i].repeatAt) >= 0) {
      states[i].repeatAt = now + BUTTON_REPEAT_INTERVAL;
      return actionForIndex(i);
    }
  }
  return ButtonAction::NONE;
}
