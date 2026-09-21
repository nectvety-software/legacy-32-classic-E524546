#ifndef BUTTONMANAGER_H
#define BUTTONMANAGER_H

#include "Config.h"
#include <Arduino.h>

enum class ButtonAction {
  NONE, UP, DOWN, LEFT, RIGHT, MENU, OPTION, SELECT, START, A, B
};

class ButtonManager {
public:
  void begin();
  void update(); // Call in loop
  bool isPressed(int pin);
  bool isJustPressed(int pin);
  ButtonAction getAction(bool allowRepeat = true);

  // Map specific actions if needed, or just expose raw check
  // Using raw check wrapper for now to keep it simple as per request

private:
  struct ButtonState {
    int pin;
    bool pressed;     // Stable state
    bool justPressed; // Rising edge flag
    bool lastRaw;     // Last raw reading
    unsigned long lastChange;
    unsigned long repeatAt;
  };
  // Map pin to state index or just use fixed array since we know the pins
  ButtonState states[10];
  ButtonAction actionForIndex(int index) const;
  int getIndex(int pin);
};

extern ButtonManager buttonManager;

#endif
